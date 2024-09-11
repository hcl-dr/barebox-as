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
#include <boards/tq/tq_eeprom.h>

extern char __dtb_z_as_imx93evk_start[];
extern struct dram_timing_info asimx93evk_dram_timing;

static noinline void as93evk_init(void)
{
	void *base = IOMEM(MX9_UART1_BASE_ADDR);
	void *muxbase = IOMEM(MX9_IOMUXC_BASE_ADDR);

	writel(0x10, muxbase + 0x170);
	writel(0x10, muxbase + 0x174);
	writel(0x0, muxbase + 0x184);
	writel(0xb9e, muxbase + 0x320);
	writel(0xb9e, muxbase + 0x324);

	imx9_uart_setup(IOMEM(base));
	pbl_set_putc(lpuart32_putc, base + 0x10);

	imx93_ddr_init(&asimx93evk_dram_timing, DRAM_TYPE_LPDDR4);
	if (current_el() == 3) {
		imx93_romapi_load_image();
		imx93_load_and_start_image_via_tfa();

	}

	imx93_barebox_entry(__dtb_z_as_imx93evk_start);
}

ENTRY_FUNCTION(start_as93evk, r0, r1, r2)
{
	if (current_el() == 3)
		imx93_cpu_lowlevel_init();

	relocate_to_current_adr();
	setup_c();

	as93evk_init();
}
