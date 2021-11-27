/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>

/* The devicetree node identifier for the "led2" alias (blue LED). */
#define LED2_NODE DT_ALIAS(led2)

#if DT_NODE_HAS_STATUS(LED2_NODE, okay)
#define LED2	DT_GPIO_LABEL(LED2_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED2_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED2_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED2	""
#define PIN	0
#define FLAGS	0
#endif


static const struct device *dev;

void gpio_led_init()
{
	int ret;

	dev = device_get_binding(LED2);
	if (dev == NULL) {
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		return;
	}

    return;
}


void gpio_led_on_off(int on) {
    gpio_pin_set(dev, PIN, on);
}



