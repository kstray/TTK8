#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>
#include <logging/log.h>
#include <stdio.h>


#if DT_NODE_HAS_STATUS(DT_INST(0, solomon_ssd16xxfb), okay)
#define DISPLAY_DEV_NAME DT_LABEL(DT_INST(0, solomon_ssd16xxfb))
#endif


void display_init() {
    uint8_t *buf;
    const struct device *dev;
    struct display_capabilities capabilities;
    struct display_buffer_descriptor buf_desc;
    size_t buf_size = 0;

    dev = device_get_binding(DISPLAY_DEV_NAME);
    if (dev == NULL) {
        printk("Device %s not found\n", dev->name);
        return;
    }

    k_sleep(K_SECONDS(5));

    display_get_capabilities(dev, &capabilities);

    int h = capabilities.y_resolution;
    int w = capabilities.x_resolution;

    buf_size = 64;

    buf = k_malloc(buf_size);
    if (buf == NULL) {
        printk("Could not allocate buffer memory\n");
        return;
    }

    buf_desc.buf_size = buf_size;
    buf_desc.pitch = 8;
    buf_desc.width = 8;
    buf_desc.height = 8;

    (void)memset(buf, 0b01010101, buf_size);
    display_write(dev, 64, 64, &buf_desc, buf);

    return;
}