// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Oleksij Rempel, Pengutronix
 */

#include <asm/memory.h>
#include <bootsource.h>
#include <deep-probe.h>
#include <common.h>
#include <init.h>
#include <linux/phy.h>
#include <linux/sizes.h>
#include <mach/imx/bbu.h>
#include <mach/imx/iomux-mx8mp.h>
#include <mach/imx/generic.h>
#include <gpio.h>
#include <envfs.h>

static bool is_dr_imx8mp_evk = false;
static int nxp_imx8mp_evk_probe(struct device *dev)
{
	u32 val;
	is_dr_imx8mp_evk = true;
	defaultenv_append_directory(defaultenv_dr_imx8mp_evk);
	barebox_set_hostname("imx8mp-dr");
	imx8m_bbu_internal_mmcboot_register_handler("eMMC", "/dev/mmc0", BBU_HANDLER_FLAG_DEFAULT);

	val = readl(MX8MP_IOMUXC_GPR_BASE_ADDR + MX8MP_IOMUXC_GPR1);
	val |= MX8MP_IOMUXC_GPR1_ENET1_RGMII_EN;
	writel(val, MX8MP_IOMUXC_GPR_BASE_ADDR + MX8MP_IOMUXC_GPR1);

	return 0;
}

static int nxp_imx8mp_evk_phy_reset(void)
{
	if (!is_dr_imx8mp_evk)
		return 0;
	gpio_direction_output(IMX_GPIO_NR(4, 22), 0);
	pr_info("Reset EQOS PHY\n");
	mdelay(10);
	gpio_set_value(IMX_GPIO_NR(4, 22), 1);
	mdelay(30);
	return 0;
}
static const struct of_device_id dr_imx8mp_evk_of_match[] = {
	{ .compatible = "fsl,imx8mp-evk" },
	{ /* sentinel */ },
};
BAREBOX_DEEP_PROBE_ENABLE(dr_imx8mp_evk_of_match);

static struct driver_d dr_imx8mp_evkboard_driver = {
	.name = "board-dr-imx8mp-evk",
	.probe = nxp_imx8mp_evk_probe,
	.of_compatible = DRV_OF_COMPAT(dr_imx8mp_evk_of_match),
};
coredevice_platform_driver(dr_imx8mp_evkboard_driver);

late_initcall(nxp_imx8mp_evk_phy_reset);
