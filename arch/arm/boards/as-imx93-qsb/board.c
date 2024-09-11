// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "as93qsb: " fmt

#include <common.h>
#include <gpio.h>
#include <init.h>
#include <i2c/i2c.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <environment.h>
#include <mfd/pca9450.h>
#include <deep-probe.h>
#include <mach/imx/bbu.h>
#include <envfs.h>

static int as93qsb_probe(struct device *dev)
{

	imx9_bbu_internal_mmcboot_register_handler("eMMC", "/dev/mmc0", 0);
	defaultenv_append_directory(defaultenv_as_imx93_qsb);

	return 0;
}

static const struct of_device_id as93qsb_of_match[] = {
	{
		.compatible = "as,as-imx93-qsb",
	},
	{ /* sentinel */ },
};

static struct driver as93qsb_board_driver = {
	.name = "board-as93qsb",
	.probe = as93qsb_probe,
	.of_compatible = as93qsb_of_match,
};
coredevice_platform_driver(as93qsb_board_driver);

BAREBOX_DEEP_PROBE_ENABLE(as93qsb_of_match);
