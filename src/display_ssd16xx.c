#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>
#include <display/cfb.h>
#include <stdio.h>
#include <string.h>

#include "cfb_font_weather.h"


#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd16xxfb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd16xxfb))
#endif

static const struct device *dev;


void display_init() {

    uint8_t font_width, font_height;


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

    err = cfb_framebuffer_set_font(dev, 1);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, "Booting system...", 35, 48);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    cfb_framebuffer_finalize(dev);

    return;
}



void display_print_placeholder() {

    int err;

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


char* id_to_icon(char *id) {
    /* Function for converting icon ID received
       from openweathermap.org to weather icon
       supported by weather font.
    */
    char buf[3];
    memcpy(buf, id, 2);
    buf[2] = '\0';
    if (strcmp(buf, "01") == 0) {
        /* Clear sky */
        return "1";
    } else if (strcmp(buf, "02") == 0) {
        /* Few clouds */
        return "2";
    } else if (strcmp(buf, "03") == 0) {
        /* Scattered clouds */
        return "3";
    } else if (strcmp(buf, "04") == 0) {
        /* Broken clouds */
        return "3";
    } else if (strcmp(buf, "09") == 0) {
        /* Shower rain */
        return "4";
    } else if (strcmp(buf, "10") == 0) {
        /* Rain */
        return "4";
    } else if (strcmp(buf, "11") == 0) {
        /* Thunderstorm */
        return "6";
    } else if (strcmp(buf, "13") == 0) {
        /* Snow */
        return "5";
    } else {
        printk("Unsupported weather icon ID %s\n", id);
        return NULL;
    }
}


void display_print_weather(char *weather, char *icon_id, char *temperature, char *location) {

    int err;

    cfb_framebuffer_clear(dev, false);

    err = cfb_framebuffer_set_font(dev, 2);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, location, 35, 16);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    err = cfb_framebuffer_set_font(dev, 1);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, "Temp:", 35, 48);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    err = cfb_print(dev, temperature, 161, 48);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    err = cfb_framebuffer_set_font(dev, 1);
    if (err) {
        printk("Could not set font, err %d\n", err);
    }

    err = cfb_print(dev, weather, 35, 72);
    if (err) {
        printk("Could not display string, err %d\n", err);
    }

    /* Convert icon id to weather icon */
    char *icon = id_to_icon(icon_id);
    if (icon) {
        err = cfb_framebuffer_set_font(dev, 0);
        if (err) {
            printk("Could not set font, err %d\n", err);
        }

        err = cfb_print(dev, icon, 170, 64);
        if (err) {
            printk("Could not display string, err %d\n", err);
        }
    }

    cfb_framebuffer_finalize(dev);

    return;
}