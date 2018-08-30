/*
 * i2c watchdog
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/watchdog.h>
#include <linux/delay.h>
#include <asm/system_misc.h>

#include <linux/mfd/ts-mcu.h>

#define TS_DEFAULT_TIMEOUT 30

static bool nowayout = WATCHDOG_NOWAYOUT;

struct ts_wdt_dev {
	struct ts_mcu_dev	*mcu;
	struct delayed_work	ping_work;
};

static struct ts_wdt_dev *wdev;

/* The WDT expects 3 values:
 * 0 (always)
 * and two bytes for the feed length in deciseconds
 * 1 <MSB>
 * 2 <LSB>
 * there are also 3 special values if they are specified
 * in the LSB with a 0 MSB:
 * 0 - 200ms
 * 1 - 2s
 * 2 - 4s
 * 3 - 10s
 * 4 - disable watchdog
 */

static int ts_wdt_write(u16 deciseconds)
{
	u8 out[3];
	int ret;
	struct i2c_msg msg;

	out[0] = 0;
	out[1] = (deciseconds & 0xff00) >> 8;
	out[2] = deciseconds & 0xff;
	dev_dbg(wdev->mcu->dev, "Writing 0x00, 0x%02x, 0x%02x\n",
		out[1],
		out[2]);

	msg.flags = 0;
	msg.len = 3;
	msg.buf = out;

	ret = ts_mcu_transfer(wdev->mcu, &msg, 1);
	if (ret != 1) {
		dev_err(wdev->mcu->dev, "%s: write error, ret=%d\n",
			__func__, ret);
	}
	return !ret;
}

/* Watchdog is on by default.  We feed every timeout/2 until userspace feeds */
static void ts_wdt_ping_enable(void)
{
	dev_dbg(wdev->mcu->dev, "%s\n", __func__);
	ts_wdt_write(TS_DEFAULT_TIMEOUT * 10);
	schedule_delayed_work(&wdev->ping_work,
			round_jiffies_relative(TS_DEFAULT_TIMEOUT * HZ / 2));
}

static void ts_wdt_ping_disable(void)
{
	dev_dbg(wdev->mcu->dev, "%s\n", __func__);
	ts_wdt_write(TS_DEFAULT_TIMEOUT * 10);
	cancel_delayed_work_sync(&wdev->ping_work);
}

static int ts_wdt_start(struct watchdog_device *wdt)
{
	dev_dbg(wdev->mcu->dev, "%s\n", __func__);
	dev_dbg(wdev->mcu->dev, "Feeding for %d seconds\n", wdt->timeout);

	ts_wdt_ping_disable();
	return ts_wdt_write(wdt->timeout * 10);
}

static int ts_wdt_stop(struct watchdog_device *wdt)
{
	dev_dbg(wdev->mcu->dev, "%s\n", __func__);
	return ts_wdt_write(3);
}

static void do_ts_reboot(enum reboot_mode reboot_mode, const char *cmd)
{
	unsigned long flags;
	static DEFINE_SPINLOCK(wdt_lock);

	dev_dbg(wdev->mcu->dev, "%s\n", __func__);

	spin_lock_irqsave(&wdt_lock, flags);
	ts_wdt_write(0);
	while (1);
}

static void do_ts_halt(void)
{
	unsigned long flags;
	static DEFINE_SPINLOCK(wdt_lock);

	dev_dbg(wdev->mcu->dev, "%s\n", __func__);

	spin_lock_irqsave(&wdt_lock, flags);
	ts_wdt_write(3);
	while (1);
}

static int ts_set_timeout(struct watchdog_device *wdt,
				   unsigned int timeout)
{
	dev_dbg(wdev->mcu->dev, "%s\n", __func__);
	wdt->timeout = timeout;
	return 0;
}

static void ts_wdt_ping_work(struct work_struct *work)
{
	dev_dbg(wdev->mcu->dev, "%s\n", __func__);
	ts_wdt_ping_enable();
}

static struct watchdog_info ts_wdt_ident = {
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | 
				WDIOF_MAGICCLOSE,
	.identity	= "Technologic Micro Watchdog",
};

static struct watchdog_ops ts_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= ts_wdt_start,
	.stop		= ts_wdt_stop,
	.set_timeout	= ts_set_timeout,
};

static struct watchdog_device ts_wdt_wdd = {
	.info			= &ts_wdt_ident,
	.ops			= &ts_wdt_ops,
	.min_timeout		= 1,
	.timeout		= TS_DEFAULT_TIMEOUT,
	.max_timeout		= 6553,
};

static int ts_wdt_probe(struct platform_device *pdev)
{
	int err;
	struct ts_mcu_dev *mcu = dev_get_drvdata(pdev->dev.parent);

	if (wdev) {
		dev_err(&pdev->dev, "Only one instance supported\n");
		return -EALREADY;
	}

	wdev = devm_kzalloc(&pdev->dev, sizeof(*wdev), GFP_KERNEL);
	if (!wdev)
		return -ENOMEM;

	wdev->mcu = mcu;
	arm_pm_restart = do_ts_reboot;
	pm_power_off = do_ts_halt;
	dev_dbg(&pdev->dev, "%s\n", __func__);

	watchdog_set_drvdata(&ts_wdt_wdd, wdev);
	watchdog_set_nowayout(&ts_wdt_wdd, nowayout);

	INIT_DELAYED_WORK(&wdev->ping_work, ts_wdt_ping_work);

	err = watchdog_register_device(&ts_wdt_wdd);
	if (err)
		return err;

	ts_wdt_ping_enable();

	return 0;
}

static struct platform_driver ts_wdt_driver = {
	.driver = {
		.name	= "ts-wdt",
		.owner	= THIS_MODULE,
	},
	.probe		= ts_wdt_probe,
};

static int __init ts_wdt_init(void)
{
	return platform_driver_register(&ts_wdt_driver);
}
subsys_initcall(ts_wdt_init);

static void __exit ts_wdt_exit(void)
{
	platform_driver_unregister(&ts_wdt_driver);
}
module_exit(ts_wdt_exit);

MODULE_AUTHOR("Mark Featherston <mark@embeddedarm.com>");
MODULE_DESCRIPTION("Technologic Systems watchdog driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ts-wdt");
