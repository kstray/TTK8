#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>
#include <display/cfb.h>
#include <stdio.h>

#include "cfb_font_weather.h"


#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd16xxfb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd16xxfb))
#endif



void display_init() {

    const struct device *dev;

    uint16_t rows;
    uint8_t ppt, font_width, font_height;


    dev = device_get_binding(DISPLAY_DEV_NAME);
    if (dev == NULL) {
        printk("Device %s not found\n", dev->name);
        return;
    }

    k_sleep(K_SECONDS(5));

    if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
        printk("Failed to set required pixel format\n");
        return;
    }

    int err = cfb_framebuffer_init(dev);
    if (err != 0) {
        printk("CFB init error\n");
        return;
    }

    for (int i = 0; i < 42; i++) {
        if (cfb_get_font_size(dev, i, &font_width, &font_height)) {
            break;
        }
        printk("font width %d, font height %d\n", font_width, font_height);
    }

    cfb_framebuffer_clear(dev, false);

    err = cfb_framebuffer_set_font(dev, 2);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, "Press button", 35, 8);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    err = cfb_framebuffer_set_font(dev, 1);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, "to get weather forecast", 10, 40);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    err = cfb_framebuffer_set_font(dev, 0);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, "123456", 29, 72);
    if (err) {
        printk("Could not display custom font, err %d\n", err);
    }

    cfb_framebuffer_finalize(dev);

    return;
}