/**
 * ts-mcu-core.c -- Technologic Systems Companion MCU
 *
 * (C) 2018 Stuart Longland
 *
 * Based on ts_wdt.h
 * (C) 2015 Technologic Systems
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#include <linux/mfd/ts-mcu.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/mfd/core.h>

static const struct mfd_cell ts_mcu_devs[] = {
	{
		.name = "ts-wdt",
		.id = 0,
		.pm_runtime_no_callbacks = true,
	},
	{
		.name = "ts-psu",
		.id = 1,
		.pm_runtime_no_callbacks = true,
	},
};

static void ts_mcu_address(
		struct ts_mcu_dev	*mcu,
		struct i2c_msg		*msgs,
		int num)
{
	while (num) {
		msgs->addr = mcu->client->addr;
		msgs++;
		num--;
	}
}

/**
 * Transfer a message to the companion MCU
 */
int ts_mcu_transfer(
		struct ts_mcu_dev	*mcu,
		struct i2c_msg		*msgs,
		int num)
{
	ts_mcu_address(mcu, msgs, num);
	return i2c_transfer(mcu->client->adapter, msgs, num);
}
EXPORT_SYMBOL(ts_mcu_transfer);

static int ts_mcu_init(struct ts_mcu_dev *mcu)
{
	int ret;

	dev_dbg(mcu->dev, "Initialising TS-MCU core\n");
	ret = mfd_add_devices(mcu->dev, 0,
			      ts_mcu_devs,
			      ARRAY_SIZE(ts_mcu_devs),
			      NULL, 0, NULL);
	if (ret != 0) {
		dev_err(mcu->dev, "Failed to add children: %d\n", ret);
		goto err;
	}

	return 0;
err:
	/* clean-up */
	mfd_remove_devices(mcu->dev);
	return ret;
}

static void ts_mcu_exit(struct ts_mcu_dev *mcu)
{
	mfd_remove_devices(mcu->dev);
}

static int ts_mcu_probe(struct i2c_client *i2c,
			  const struct i2c_device_id *id)
{
	/* Original credit: Mark Featherston, Kris Bahnsen */
	struct ts_mcu_dev *mcu;

	mcu = devm_kzalloc(&i2c->dev, sizeof(*mcu), GFP_KERNEL);
	if (!mcu)
		return -ENOMEM;

	i2c_set_clientdata(i2c, mcu);
	mcu->client = i2c;
	mcu->dev = &i2c->dev;

	return ts_mcu_init(mcu);
}

static int ts_mcu_remove(struct i2c_client *i2c)
{
	struct ts_mcu_dev *mcu = i2c_get_clientdata(i2c);
	ts_mcu_exit(mcu);
	return 0;
}

static const struct i2c_device_id ts_mcu_id[] = {
	{ "ts-mcu", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ts_mcu_id);
MODULE_ALIAS("platform:ts-mcu");

static struct i2c_driver ts_mcu_driver = {
	.driver = {
		.name	= "ts-mcu",
		.owner	= THIS_MODULE,
	},
	.probe		= ts_mcu_probe,
	.remove		= ts_mcu_remove,
	.id_table	= ts_mcu_id,
};

module_i2c_driver(ts_mcu_driver);

MODULE_AUTHOR("Stuart Longland <me@vk4msl.id.au>");
MODULE_DESCRIPTION("Technologic Systems Companion MCU driver");
MODULE_LICENSE("GPL");
