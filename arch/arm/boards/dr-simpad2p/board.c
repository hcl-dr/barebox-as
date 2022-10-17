// SPDX-License-Identifier: GPL-2.0

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
#include <globalvar.h>

/* Keep the kernel revA file name compatible */
static char *DTBS[8] = { "dr-simpad2p-revB.dtb",
						 "dr-simpad2p.dtb",
						 "dr-simpad2p-revB.dtb",
						 "dr-simpad2p-revC.dtb",
						 "dr-simpad2p-revC.dtb",
						 "NA",
						 "NA",
						 "NA" };

static bool is_simpad2p = false;
static int get_hwrev(void)
{
	int rev = 0;
	gpio_direction_input(IMX_GPIO_NR(5, 2));
	gpio_direction_input(IMX_GPIO_NR(4, 28));
	gpio_direction_input(IMX_GPIO_NR(4, 29));

	rev = gpio_get_value(IMX_GPIO_NR(5, 2));
	rev |= gpio_get_value(IMX_GPIO_NR(4, 28)) << 1;
	rev |= gpio_get_value(IMX_GPIO_NR(4, 29)) << 2;
	return rev;
}

static char fitnode[120] = "conf-freescale_";

static int dr_simpad2p_probe(struct device *dev)
{
	u32 val;

	is_simpad2p = true;
	defaultenv_append_directory(defaultenv_dr_simpad2p);
	barebox_set_hostname("simpad2p");
	imx8m_bbu_internal_mmcboot_register_handler("eMMC", "/dev/mmc0",
												BBU_HANDLER_FLAG_DEFAULT);

	val = readl(MX8MP_IOMUXC_GPR_BASE_ADDR + MX8MP_IOMUXC_GPR1);
	val |= MX8MP_IOMUXC_GPR1_ENET1_RGMII_EN;
	writel(val, MX8MP_IOMUXC_GPR_BASE_ADDR + MX8MP_IOMUXC_GPR1);
	return 0;
}

static int dr_simpad2p_phy_reset(void)
{
	struct device_node *gpio_np;
	if (!is_simpad2p)
		return 0;
	gpio_np = of_find_node_by_name_address(NULL, "gpio@30210000");
	if (gpio_np) {
		int ret = of_device_ensure_probed(gpio_np);
		if (ret)
			pr_warn("Can't probe GPIO node\n");
	} else {
		pr_warn("Can't get GPIO node\n");
	}
	gpio_request(IMX_GPIO_NR(2, 14), "phy-rst-eqos");
	gpio_direction_output(IMX_GPIO_NR(2, 14), 0);
	pr_info("Reset EQOS PHY\n");
	mdelay(10);
	gpio_set_value(IMX_GPIO_NR(2, 14), 1);
	return 0;
}

static int dr_simpad2p_setup_board_late(void)
{
	int val;
	if (!is_simpad2p)
		return 0;

	val = get_hwrev() & 0x7;
	printf("HW revision: %d\n", val);
	globalvar_add_simple("bootm.fdt", DTBS[val]);

	strcat(fitnode,  DTBS[val]);
	globalvar_add_simple("boot.fitnode", fitnode);
	return dr_simpad2p_phy_reset();
}

static const struct of_device_id dr_simpad2p_of_match[] = {
	{ .compatible = "datarespons,simpad2p" },
	{ /* sentinel */ },
};
BAREBOX_DEEP_PROBE_ENABLE(dr_simpad2p_of_match);

static struct driver_d simpad2p_board_driver = {
	.name = "board-dr-simpad2p",
	.probe = dr_simpad2p_probe,
	.of_compatible = DRV_OF_COMPAT(dr_simpad2p_of_match),
};
coredevice_platform_driver(simpad2p_board_driver);
late_initcall(dr_simpad2p_setup_board_late);
