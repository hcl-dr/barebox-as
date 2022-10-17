// SPDX-License-Identifier: GPL-2.0

#include <io.h>
#include <common.h>
#include <debug_ll.h>
#include <firmware.h>
#include <image-metadata.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/sections.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <gpio.h>
#include <pbl/i2c.h>
#include <pbl/pmic.h>
#include <linux/sizes.h>
#include <mach/imx/atf.h>
#include <mach/imx/xload.h>
#include <mach/imx/esdctl.h>
#include <mach/imx/generic.h>
#include <mach/imx/imx8mp-regs.h>
#include <mach/imx/iomux-mx8mp.h>
#include <mach/imx/imx-gpio.h>
#include <mach/imx/imx8m-ccm-regs.h>
#include <mfd/pca9450.h>
#include <soc/imx8m/ddr.h>
#include <soc/fsl/fsl_udc.h>

extern char __dtb_z_dr_cpum2_start[];
extern char __dtb_z_dr_cpum2_revB_start[];

#define UART_PAD_CTRL   MUX_PAD_CTRL(MX8MP_PAD_CTL_DSE6 | \
				     MX8MP_PAD_CTL_FSEL)

#define I2C_PAD_CTRL	MUX_PAD_CTRL(MX8MP_PAD_CTL_DSE6 | \
				     MX8MP_PAD_CTL_HYS | \
				     MX8MP_PAD_CTL_PUE | \
				     MX8MP_PAD_CTL_PE)
#define INPUT_NOPULL_CTRL MUX_PAD_CTRL(MX8MP_PAD_CTL_HYS)

static void setup_uart(void)
{
	void __iomem *uart = IOMEM(MX8M_UART2_BASE_ADDR);

	imx8m_early_setup_uart_clock();

	imx8mp_setup_pad(MX8MP_PAD_UART2_TXD__UART2_DCE_TX | UART_PAD_CTRL);
	imx8mp_setup_pad(MX8MP_PAD_UART2_RXD__UART2_DCE_RX | UART_PAD_CTRL);
	imx8m_uart_setup(uart);

	pbl_set_putc(imx_uart_putc, uart);

	printf("Uart initialized\n");
}

#ifdef CONFIG_IMX8MP_VDD_SOC_085
static struct pmic_config pca9450_cfg[] = {
	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	{ PCA9450_BUCK123_DVS, 0x29 },
	/*
	 * VDD_SOC and VDD_ARM at 0.85V - note that this requires DT
	 * setup for applicable frequencies is kernel.
	 * DRAM access, set DVS1 to 0.85v for suspend.
	 * Enable DVS control through PMIC_STBY_REQ and
	 * set B1_ENMODE=1 (ON by PMIC_ON_REQ=H)
	 */
	{ PCA9450_BUCK1OUT_DVS0, 0x14 },
	{ PCA9450_BUCK1OUT_DVS1, 0x14 },
	{ PCA9450_BUCK1CTRL, 0x59 },
	/* set WDOG_B_CFG to cold reset */
	{ PCA9450_RESET_CTRL, 0xA1 },
};

#else
static struct pmic_config pca9450_cfg[] = {
	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	{ PCA9450_BUCK123_DVS, 0x29 },
	/*
	 * increase VDD_SOC to typical value 0.95V before first
	 * DRAM access, set DVS1 to 0.85v for suspend.
	 * Enable DVS control through PMIC_STBY_REQ and
	 * set B1_ENMODE=1 (ON by PMIC_ON_REQ=H)
	 */
	{ PCA9450_BUCK1OUT_DVS0, 0x1C },
	{ PCA9450_BUCK1OUT_DVS1, 0x14 },
	{ PCA9450_BUCK1CTRL, 0x59 },
	/* set WDOG_B_CFG to cold reset */
	{ PCA9450_RESET_CTRL, 0xA1 },
};
#endif
static void power_init_board(void)
{
	struct pbl_i2c *i2c;

	imx8mp_setup_pad(MX8MP_PAD_I2C1_SCL__I2C1_SCL | I2C_PAD_CTRL);
	imx8mp_setup_pad(MX8MP_PAD_I2C1_SDA__I2C1_SDA | I2C_PAD_CTRL);

	imx8mm_early_clock_init();
	imx8m_ccgr_clock_enable(IMX8M_CCM_CCGR_I2C1);

	i2c = imx8m_i2c_early_init(IOMEM(MX8MP_I2C1_BASE_ADDR));

	pmic_configure(i2c, 0x25, pca9450_cfg, ARRAY_SIZE(pca9450_cfg));
}

extern struct dram_timing_info dr_cpum2_dram_timing;

static void start_atf(void)
{
	/*
	 * If we are in EL3 we are running for the first time and need to
	 * initialize the DRAM and run TF-A (BL31). The TF-A will then jump
	 * to DRAM in EL2.
	 */
	if (current_el() != 3)
		return;

	printf("Init power\n");
	power_init_board();
	printf("Init DDR\n");
	imx8mp_ddr_init(&dr_cpum2_dram_timing, DRAM_TYPE_LPDDR4);
	printf("Handover to ATF\n");
	imx8mp_load_and_start_image_via_tfa();
}

static int get_hwrev(void)
{
	int ver = 0;
	void __iomem *gpiobase4 = IOMEM(MX8MP_GPIO4_BASE_ADDR);
	void __iomem *gpiobase5 = IOMEM(MX8MP_GPIO5_BASE_ADDR);
	imx8mp_setup_pad(MX8MP_PAD_SAI3_TXFS__GPIO4_IO31 | INPUT_NOPULL_CTRL);
	imx8mp_setup_pad(MX8MP_PAD_SAI3_TXC__GPIO5_IO00 | INPUT_NOPULL_CTRL);
	imx8mp_setup_pad(MX8MP_PAD_SAI3_TXD__GPIO5_IO01 | INPUT_NOPULL_CTRL);

	imx8m_gpio_direction_input(gpiobase4, 31);
	imx8m_gpio_direction_input(gpiobase5, 0);
	imx8m_gpio_direction_input(gpiobase5, 0);

	ver |= imx8m_gpio_val(gpiobase4, 31);
	ver |= imx8m_gpio_val(gpiobase5, 0) << 1;
	ver |= imx8m_gpio_val(gpiobase5, 1) << 2;

	return ver;
}

/*
 * Power-on execution flow of start_nxp_imx8mp_evk() might not be
 * obvious for a very first read, so here's, hopefully helpful,
 * summary:
 *
 * 1. MaskROM uploads PBL into OCRAM and that's where this function is
 *    executed for the first time. At entry the exception level is EL3.
 *
 * 2. DDR is initialized and the image is loaded from storage into DRAM. The PBL
 *    part is copied from OCRAM to the TF-A return address in DRAM.
 *
 * 3. TF-A is executed and exits into the PBL code in DRAM. TF-A has taken us
 *    from EL3 to EL2.
 *
 * 4. Standard barebox boot flow continues
 */
static __noreturn noinline void dr_cpum2_start(void)
{
	int hwrev;
	setup_uart();

	start_atf();
	hwrev = get_hwrev();
	printf("HW revision %d\n", hwrev);

	switch (hwrev) {
	case 0:
	case 1:
		imx8mp_barebox_entry(__dtb_z_dr_cpum2_start);
		break;

	default:
		imx8mp_barebox_entry(__dtb_z_dr_cpum2_revB_start);
		break;
	}
}

ENTRY_FUNCTION(start_dr_cpum2, r0, r1, r2)
{
	imx8mp_cpu_lowlevel_init();

	relocate_to_current_adr();
	setup_c();

	IMD_USED_OF(dr_cpum2_revB);

	dr_cpum2_start();
}
