#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kref.h> /* kref */
#include <linux/slab.h> /* kmalloc */
#include <linux/usb.h> /* usb things */

#include "usb_led.h"

/* Table of devices that work with this driver */
static struct usb_device_id usb_led_table[] = {
    { USB_DEVICE(LED_VENDOR_ID, LED_PRODUCT_ID) },
    { } /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, usb_led_table);

#define make_sysfs_file(led_num)                                                                                 \
static ssize_t led_num##_show(struct device *dev, struct device_attribute *attr, char *buf) {                    \
    uint8_t led1, led2, led3, led_mask;                                                                          \
    struct usb_interface *intf;                                                                                  \
    struct led *led_dev;                                                                                         \
                                                                                                                 \
    printk(KERN_ALERT "show_led called\n");                                                                      \
                                                                                                                 \
    led1 = led2 = led3 = 0;                                                                                      \
                                                                                                                 \
    intf = to_usb_interface(dev);                                                                                \
    led_dev = usb_get_intfdata(intf);                                                                            \
                                                                                                                 \
    led_mask = led_dev->color;                                                                                   \
                                                                                                                 \
    /* Doing this thrice is admittedly wasteful but... C macros. */                                              \
    if (led_mask & LED_1)                                                                                        \
        led1 = 1;                                                                                                \
                                                                                                                 \
    if (led_mask & LED_2)                                                                                        \
        led2 = 1;                                                                                                \
                                                                                                                 \
    if (led_mask & LED_3)                                                                                        \
        led3 = 1;                                                                                                \
                                                                                                                 \
    return sprintf(buf, "%d\n", led_num);                                                                        \
}                                                                                                                \
                                                                                                                 \
static ssize_t led_num##_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) { \
    int retval;                                                                                                  \
    uint8_t led_state, led_mask;                                                                                 \
    struct usb_interface *intf;                                                                                  \
    struct led *led_dev;                                                                                         \
                                                                                                                 \
    printk(KERN_ALERT "set_led called\n");                                                                       \
                                                                                                                 \
    /* args: buf, base, *u8 ; retval 0 is success*/                                                              \
    retval = kstrtou8(buf, 10, &led_state);                                                                      \
    printk(KERN_ALERT "set_led: %d\n", led_state);                                                               \
    if (retval || ((led_state != 0) && (led_state != 1))) {                                                      \
        return -EINVAL;                                                                                          \
    }                                                                                                            \
                                                                                                                 \
    intf = to_usb_interface(dev);                                                                                \
    led_dev = usb_get_intfdata(intf);                                                                            \
    led_mask = led_dev->color;                                                                                   \
                                                                                                                 \
    change_color(led_dev, #led_num, led_state);                                                                  \
                                                                                                                 \
    /* TODO check if I should just return 1 here or something */                                                 \
    return count;                                                                                                \
}                                                                                                                \
                                                                                                                 \
static DEVICE_ATTR(led_num, S_IWUSR | S_IRUGO, led_num##_show, led_num##_store);

make_sysfs_file(led1);
make_sysfs_file(led2);
make_sysfs_file(led3);

// TODO if this func fails, return an error
static void change_color(struct led * led_dev, const char * led_name, uint8_t val) {
    // The LED names are all "ledx" where x is a number, so don't account for more than 4 bytes
    int retval, actual_len;
    uint8_t led_val, led_mask = 0;
    // a single byte is all we need
    char *data = kmalloc(sizeof(uint8_t), GFP_KERNEL);

    printk(KERN_ALERT "change_color called: %s\n", led_name);
    /* printk(KERN_ALERT "change_color called: %hhx\n", LED_1); */

    if (!strncmp(led_name, "led1", 4)) {
        printk(KERN_ALERT "change_color led1");
        led_mask = LED_1;
    } else if (!strncmp(led_name, "led2", 4)) {
        printk(KERN_ALERT "change_color led2");
        led_mask = LED_2;
    } else if (!strncmp(led_name, "led3", 4)) {
        printk(KERN_ALERT "change_color led3");
        led_mask = LED_3;
    } else {
        // This shouldn't ever happen since this func is called only from the
        // make_sysfs_file macro, so we'll just early return here
        printk(KERN_ALERT "Invalid led value received");
        return;
    }

    led_val = led_dev->color;

    printk(KERN_ALERT "current value: %hhx\n", led_val);

    /* if val is non-zero, turn LED on */
    if (val) {
        led_val |= led_mask;
    } else {
        led_val &= ~led_mask;
    }

    printk(KERN_ALERT "new value: %hhx\n", led_val);

    data[0] = led_val;

    retval = usb_bulk_msg(led_dev->udev, /* usb device */
                          usb_sndbulkpipe(led_dev->udev, led_dev->bulk_out_endpointAddr), /* endpoint */
                          data, /* data */
                          1, /* data len: our data is just a single char, 1 byte */
                          &actual_len, /* actual number of bytes transferred */
                          HZ*100); /* timeout */

    printk(KERN_ALERT "actual_len: %d\n", actual_len);

    led_dev->color = led_val;

    kfree(data);
}

static struct usb_driver usb_led_driver = {
    .name = LED_DRIVER_NAME,
    .probe = usb_led_probe,
    .disconnect = usb_led_disconnect,
    .id_table = usb_led_table,
};

static int usb_led_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct led *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    size_t buffer_size;
    int i;
    int retval = -ENOMEM;

    printk(KERN_ALERT "usb_led_probe called: %x:%x!\n", id->idVendor, id->idProduct);

    dev = kzalloc(sizeof(struct led), GFP_KERNEL);
    if (!dev) {
        printk(KERN_ERR "failed to allocate memory!\n");
        goto error;
    }

    dev->color = 0x00;

    kref_init(&dev->kref);

    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->iface = interface;

    iface_desc = interface->cur_altsetting;

    for (i = 0; i<iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;

        // Check for first IN endpoint
        if (!dev->bulk_in_endpointAddr &&
             (endpoint->bEndpointAddress & USB_DIR_IN) &&
             ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)) {
            // We found an IN endpoint
            buffer_size = endpoint->wMaxPacketSize;
            dev->bulk_in_size = buffer_size;
            dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
            dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
            if (!dev->bulk_in_buffer) {
                printk(KERN_ERR "Failed to allocate buffer for IN endpoint\n");
                goto error;
            }

            printk(KERN_ALERT "Found IN endpoint\n");
        }

        // Now check for an OUT endpoint
        if (!dev->bulk_out_endpointAddr &&
             !(endpoint->bEndpointAddress & USB_DIR_IN) && // Notice that this is a ! of the condition for the IN endpoint!
             ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)) {
            // We found an OUT endpoint
            dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;

            printk(KERN_ALERT "Found OUT endpoint\n");
        }

        if (dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr) {
            break;
        }
    }

    if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
        printk(KERN_ERR "Failed to allocate both IN and OUT endpoints\n");
        goto error;
    }

    /* Save our data within this interface struct for access later */
    usb_set_intfdata(interface, dev);

    // Create sysfs file
    device_create_file(&interface->dev, &dev_attr_led1);
    device_create_file(&interface->dev, &dev_attr_led2);
    device_create_file(&interface->dev, &dev_attr_led3);

    // TODO register device using usb_register_dev
    printk(KERN_ALERT "Probe successful\n");

    return 0;

error:
    if (dev) {
        kref_put(&dev->kref, usb_led_delete);
    }

    return retval;
}

static void usb_led_disconnect(struct usb_interface *interface) {
    struct led *dev;
    printk(KERN_ALERT "usb_led_disconnect called!\n");

    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    // TODO usb_deregister_dev

    // decrement usage count
    kref_put(&dev->kref, usb_led_delete);

    // Remove sysfs file
    device_remove_file(&interface->dev, &dev_attr_led1);
    device_remove_file(&interface->dev, &dev_attr_led2);
    device_remove_file(&interface->dev, &dev_attr_led3);

    printk(KERN_ALERT "usb_led_disconnect successful!\n");
}

static void usb_led_delete(struct kref *ref) {
    struct led *dev = container_of(ref, struct led, kref);
    usb_put_dev(dev->udev);
    kfree(dev->bulk_in_buffer);
    kfree(dev);
}

// TODO check this for memory safety: if errors happen, I need to deallocate things
static int __init usb_led_init(void) {
    int retval;

    /* This registers our driver with the USB subsystem of the kernel */
    retval = usb_register(&usb_led_driver);
    if (retval) {
        printk(KERN_ERR "usb_led registration with usb core failed: %d\n", retval);
        return retval;
    }

    printk(KERN_ALERT "usb_led init complete\n");

    return 0;
}

static void __exit usb_led_exit(void) {
    /* deregister the usb driver */
    usb_deregister(&usb_led_driver);

    printk(KERN_ALERT "usb_led exit complete\n");
}

MODULE_LICENSE("GPL");

module_init(usb_led_init);
module_exit(usb_led_exit);
