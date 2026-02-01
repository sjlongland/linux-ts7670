/* SPDX-License-Identifier: GPL-2.0 */
/* Renesas R-Car Gen4 gPTP device driver
 *
 * Copyright (C) 2022 Renesas Electronics Corporation
 */

#ifndef __RCAR_GEN4_PTP_H__
#define __RCAR_GEN4_PTP_H__

struct rcar_gen4_ptp_private;

int rcar_gen4_ptp_register(struct rcar_gen4_ptp_private *ptp_priv, u32 rate);
int rcar_gen4_ptp_unregister(struct rcar_gen4_ptp_private *ptp_priv);
struct rcar_gen4_ptp_private *rcar_gen4_ptp_alloc(struct platform_device *pdev);

#endif	/* #ifndef __RCAR_GEN4_PTP_H__ */
