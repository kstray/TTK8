#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>
#include <display/cfb.h>
#include <logging/log.h>
#include <stdio.h>


#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd16xxfb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd16xxfb))
#endif


void display_init() {

    const struct device *dev;

    uint16_t rows;
    uint8_t ppt, font_width, font_height;

    // uint8_t *buf;
    // struct display_buffer_descriptor buf_desc;
    // size_t buf_size = 0;

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
    //cfb_framebuffer_clear(dev, false);
    //cfb_framebuffer_finalize(dev);

    //k_sleep(K_SECONDS(10));

    rows = cfb_get_display_parameter(dev, CFB_DISPLAY_ROWS);
    ppt = cfb_get_display_parameter(dev, CFB_DISPLAY_PPT);

    for (int i = 0; i < 42; i++) {
        if (cfb_get_font_size(dev, i, &font_width, &font_height)) {
            break;
        }
        cfb_framebuffer_set_font(dev, i);
        printk("font width %d, font height %d\n", font_width, font_height);
    }

    //cfb_framebuffer_set_font(dev, 0);

    printk("x_res %d, y_res %d, ppt %d, rows %d, cols %d\n",
	       cfb_get_display_parameter(dev, CFB_DISPLAY_WIDTH),
	       cfb_get_display_parameter(dev, CFB_DISPLAY_HEIGH),
	       ppt,
	       rows,
	       cfb_get_display_parameter(dev, CFB_DISPLAY_COLS));
    
    cfb_framebuffer_clear(dev, false);
    for (int i = 0; i < 3; i++) {
        if (cfb_print(dev, "<3<3<3<3", 32, i * font_height)) {
            printk("Failed to print string\n");
            //continue;
        }
    }
    cfb_framebuffer_finalize(dev);



    // buf_size = 64;
    // buf = k_malloc(buf_size);
    // if (buf == NULL) {
    //     printk("Could not allocate buffer memory\n");
    //     return;
    // }

    // buf_desc.buf_size = buf_size;
    // buf_desc.pitch = 8;
    // buf_desc.width = 8;
    // buf_desc.height = 8;

    //(void)memset(buf, 0b01010101, buf_size);
    //display_write(dev, 64, 64, &buf_desc, buf);

    return;
}