/**
 * ts-mcu-psu.c -- Technologic Systems Companion MCU: Power supply functions
 *
 * (C) 2018 Stuart Longland
 *
 * Based on ts_wdt.h and get_vin.c
 * Authors: Kris Bahnsen, Mark Featherston
 * https://www.embeddedarm.com/blog/getting-current-voltage-input-vin-on-ts-7670-or-ts-7400-v2/
 *
 * (C) 2015 Technologic Systems
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include <linux/mfd/ts-mcu.h>

struct ts_psu_dev {
	struct ts_mcu_dev	*mcu;
	struct power_supply	*psy;
};

static int ts_psu_read_voltage_in(
		struct ts_psu_dev *psu,
		int *value)
{
	u8 raw[4];
	int ret;
	struct i2c_msg msg;

	msg.flags = I2C_M_RD;
	msg.len = sizeof(raw);
	msg.buf = raw;

	ret = ts_mcu_transfer(psu->mcu, &msg, 1);
	if (ret != 1) {
		dev_err(psu->mcu->dev, "%s: read error, ret=%d\n",
				__func__, ret);
	} else {
		/* 5.82% div., 2.5 Vref, 10b acc. */
		*value = (((raw[3] << 8) | raw[2]) * 4203336) / 100;
	}

	return ret;
}

static int ts_psu_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *value) {
	struct ts_psu_dev *psu = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return ts_psu_read_voltage_in(psu, &value->intval);
	default:
		return -EINVAL;
	}
}

static enum power_supply_property ts_psu_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static const struct power_supply_desc ts_psu_desc = {
	.name		= "ts-psu",
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.properties	= ts_psu_props,
	.num_properties	= ARRAY_SIZE(ts_psu_props),
	.get_property	= ts_psu_get_prop,
};

static int ts_psu_probe(struct platform_device *pdev)
{
	int err;
	struct ts_psu_dev *psu;
	struct ts_mcu_dev *mcu = dev_get_drvdata(pdev->dev.parent);
	struct power_supply_config cfg;

	psu = devm_kzalloc(&pdev->dev, sizeof(*psu), GFP_KERNEL);
	if (!psu)
		return -ENOMEM;

	memset(&cfg, 0, sizeof(cfg));
	cfg.drv_data = psu;

	psu->mcu = mcu;
	psu->psy = power_supply_register(&pdev->dev, &ts_psu_desc, &cfg);

	if (IS_ERR(psu->psy)) {
		err = PTR_ERR(psu->psy);
		goto err;
	}

	dev_set_drvdata(&pdev->dev, psu);
	return 0;

err:
	devm_kfree(&pdev->dev, psu);
	return err;
}

static int ts_psu_remove(struct platform_device *pdev)
{
	struct ts_psu_dev *psu = dev_get_drvdata(&pdev->dev);
	power_supply_unregister(psu->psy);
	devm_kfree(&pdev->dev, psu);
	return 0;
}

static struct platform_driver ts_psu_driver = {
	.driver = {
		.name	= "ts-psu",
		.owner	= THIS_MODULE,
	},
	.probe		= ts_psu_probe,
	.remove		= ts_psu_remove,
};

static int __init ts_psu_init(void)
{
	return platform_driver_register(&ts_psu_driver);
}
subsys_initcall(ts_psu_init);

static void __exit ts_psu_exit(void)
{
	platform_driver_unregister(&ts_psu_driver);
}
module_exit(ts_psu_exit);

MODULE_AUTHOR("Stuart Longland <me@vk4msl.id.au>");
MODULE_DESCRIPTION("Technologic Systems Power Supply driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ts-psu");
