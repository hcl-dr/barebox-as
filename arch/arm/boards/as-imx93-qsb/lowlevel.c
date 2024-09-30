// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <debug_ll.h>
#include <mach/imx/debug_ll.h>
#include <mach/imx/generic.h>
#include <mach/imx/xload.h>
#include <asm/barebox-arm.h>
#include <soc/imx9/ddr.h>
#include <mach/imx/atf.h>
#include <mach/imx/xload.h>
#include <mach/imx/romapi.h>
#include <mach/imx/esdctl.h>
#include <pbl/i2c.h>
#include <mfd/pca9450.h>
#include <pbl/pmic.h>

extern char __dtb_z_as_imx93qsb_start[];
extern struct dram_timing_info asimx93qsb_dram_timing;

#define PCA9450_REG_PWRCTRL_TOFF_DEB    BIT(5)
static int pca9450_config(struct pbl_i2c *i2c) {
	u8 val;
	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	pmic_reg_write8(i2c, 0x25, PCA9450_BUCK123_DVS, 0x29);

	/* enable DVS control through PMIC_STBY_REQ */

	pmic_reg_write8(i2c, 0x25, PCA9450_BUCK1CTRL, 0x59);
	pmic_reg_read8(i2c, 0x25, PCA9450_PWR_CTRL, &val);
	pr_debug("PCA9450_PWR_CTRL =0x%01x\n", val);
	pmic_reg_read8(i2c, 0x25, PCA9450_STATUS1, &val);
	pr_debug("PCA9450_STATUS1 = 0x%01x\n", val);
	pmic_reg_read8(i2c, 0x25, PCA9450_STATUS2, &val);
	pr_debug("PCA9450_STATUS2 = 0x%01x\n", val);

#if 0
	if (val & PCA9450_REG_PWRCTRL_TOFF_DEB) {
		pmic_reg_write8(i2c, 0x25, PCA9450_BUCK1OUT_DVS0, 0x14);
		pmic_reg_write8(i2c, 0x25, PCA9450_BUCK3OUT_DVS0, 0x14);
	} else {
		pmic_reg_write8(i2c, 0x25, PCA9450_BUCK1OUT_DVS0, 0x14 + 0x4);
		pmic_reg_write8(i2c, 0x25, PCA9450_BUCK3OUT_DVS0, 0x14 + 0x4);
	}

	/* set standby voltage to 0.65v */
	if (val & PCA9450_REG_PWRCTRL_TOFF_DEB)
		pmic_reg_write8(i2c, 0x25, PCA9450_BUCK1OUT_DVS1, 0x0);
	else
		pmic_reg_write8(i2c, 0x25, PCA9450_BUCK1OUT_DVS1, 0x4);
#endif
	/* 1.1v for LPDDR4 */
	pmic_reg_write8(i2c, 0x25, PCA9450_BUCK2OUT_DVS0, 0x28);

	/* I2C_LT_EN*/
	pmic_reg_write8(i2c, 0x25, 0xa, 0x3);

	/* set WDOG_B_CFG to cold reset */
	pmic_reg_write8(i2c, 0x25, PCA9450_RESET_CTRL, 0xA1);
	return 0;
}

static noinline void as93qsb_init(void)
{
	void *base = IOMEM(MX9_UART1_BASE_ADDR);
	void *muxbase = IOMEM(MX9_IOMUXC_BASE_ADDR);
	struct pbl_i2c *i2c;

	/* I2C2 */
	writel(0x10, muxbase + 0x178);
	writel(0x10, muxbase + 0x17C);

	writel(0x0, muxbase + 0x184);	/* Uart1 TxD */
	writel(0xb9e, muxbase + 0x320);
	writel(0xb9e, muxbase + 0x324);


	pbl_set_putc(lpuart32_putc, base + 0x10);
	imx9_uart_setup(IOMEM(base));
	pr_debug("Config i2c\n");
	i2c = imx93_i2c_early_init(IOMEM(MX9_I2C2_BASE_ADDR));
	pca9450_config(i2c);


	if (current_el() == 3) {
		i2c = imx93_i2c_early_init(IOMEM(MX9_I2C2_BASE_ADDR));
		pca9450_config(i2c);
		imx93_ddr_init(&asimx93qsb_dram_timing, DRAM_TYPE_LPDDR4);
		pr_debug("DDR in EL3 after\n");
		imx93_romapi_load_image();
		imx93_load_and_start_image_via_tfa();

	}

	imx93_barebox_entry(__dtb_z_as_imx93qsb_start);
}

ENTRY_FUNCTION(start_as93qsb, r0, r1, r2)
{
	if (current_el() == 3)
		imx93_cpu_lowlevel_init();

	relocate_to_current_adr();
	setup_c();

	as93qsb_init();
}
