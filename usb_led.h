/* These are the vendor and product IDs of the USB to TTL adapter, which I'm
 * using to communicate with my Arduino. */
#define LED_VENDOR_ID 0x10c4
#define LED_PRODUCT_ID 0xea60

#define LED_1 0x01 /* LED 1: 0001 */
#define LED_2 0x02 /* LED 2: 0010 */
#define LED_3 0x04 /* LED 3: 0100 */

#define LED_DRIVER_NAME "usb_led"

struct led {
    struct usb_device * udev; /* the usb device for this device */
    struct usb_interface * iface; /* the usb interface for this device */
    unsigned char * bulk_in_buffer; /* buffer for recv data */
    size_t bulk_in_size; /* size of recv buffer */
    uint8_t bulk_in_endpointAddr; /* address of bulk_in endpoint */
    uint8_t bulk_out_endpointAddr; /* address of bulk_out endpoint */
    struct kref kref; /* reference counter */
    uint8_t color; /* LED colours */
};

/* usb related callbacks */
static int usb_led_probe(struct usb_interface*, const struct usb_device_id*);
static void usb_led_disconnect(struct usb_interface*);
static void usb_led_delete(struct kref *);

/* sysfs interface related things */
static ssize_t led1_show(struct device *, struct device_attribute *, char *);
static ssize_t led1_store(struct device *, struct device_attribute *, const char *, size_t);

static ssize_t led2_show(struct device *, struct device_attribute *, char *);
static ssize_t led2_store(struct device *, struct device_attribute *, const char *, size_t);

static ssize_t led3_show(struct device *, struct device_attribute *, char *);
static ssize_t led3_store(struct device *, struct device_attribute *, const char *, size_t);

/* helpers */
static void change_color(struct led *, const char *, uint8_t);
