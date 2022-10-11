// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPIO based ID detect and supply control
 *
 * Copyright (c) 2022 Hans Christian Lonstad <hcl@datarespons.no>, Data Respons Solutions AS
 */
#include <common.h>
#include <malloc.h>
#include <init.h>
#include <regulator.h>
#include <of.h>
#include <of_gpio.h>
#include <gpio.h>
#include <linux/gpio/consumer.h>
#include <poller.h>
#include <linux/usb/usb.h>

struct gpio_vbus {
    int gpio;
    struct regulator *vbus_reg;
	struct poller_async poll;
	int last_id_val;
	struct device *dev;
	struct device *usb_dev;
};

static void key_poller(void *p)
{
	struct gpio_vbus *gptr = p;
	int val = gpio_get_value(gptr->gpio);
	if (val != gptr->last_id_val) {
		dev_info(gptr->dev, "USB ID: %d\n", val);
		gptr->last_id_val = val;
		if (val == 0) {
			regulator_enable(gptr->vbus_reg);
			mdelay(800);
		}
		else
			regulator_disable(gptr->vbus_reg);
		usb_rescan();
	}

	poller_call_async(&gptr->poll, 100 * MSECOND, key_poller, gptr);
}

static int gpio_vbus_probe(struct device *dev)
{
	int ret = 0;
	struct device_node *np = dev->device_node;
	struct gpio_vbus *gptr = xzalloc(sizeof(struct gpio_vbus));
	gptr->dev = dev;
	ret = of_get_named_gpio(np, "id-gpio", 0);
	if (ret < 0)
		goto err_free;
	gptr->gpio = ret;
	ret = gpio_request_one(gptr->gpio, GPIOF_DIR_IN, "id");
	if (ret < 0)
		goto err_free;
	gptr->vbus_reg = regulator_get(dev, "vbus");
	if (!gptr->vbus_reg) {
		ret = -EINVAL;
		goto err_free;
	}
	gptr->last_id_val = -1;
	poller_async_register(&gptr->poll, dev_name(dev));
	key_poller(gptr);
	return 0;

err_free:
	free(gptr);
	return ret;
}

static struct of_device_id gpio_vbus_of_ids[] = {
	{ .compatible = "gpio-vbus", },
	{ }
};

static struct driver_d gpio_vbus_driver = {
    .name = "gpio-vbus",
    .probe = gpio_vbus_probe,
    .of_compatible = DRV_OF_COMPAT(gpio_vbus_of_ids),
};

device_platform_driver(gpio_vbus_driver);
