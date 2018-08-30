/**
 * ts-mcu.h -- Technologic Systems Companion MCU Core definitions
 *
 * This file defines an interface for the companion MCU found in Technologic
 * Systems' TS-7670 and TS-7400v2 single-board computers.  This MCU
 * implements a power management IC and a watchdog.
 *
 * (C) 2018 Stuart Longland
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __MFD_TS_MCU_H__
#define __MFD_TS_MCU_H__

#include <linux/i2c.h>

/**
 * struct ts_mcu_dev
 *
 * @dev:	Parent device pointer
 * @client:	I²C device pointer
 */
struct ts_mcu_dev {
	struct device		*dev;
	struct i2c_client	*client;
};

/**
 * Transfer a message to the companion MCU
 */
int ts_mcu_transfer(
		struct ts_mcu_dev	*mcu,
		struct i2c_msg		*msgs,
		int num);

#endif
