# USB LED Driver

A toy Linux device driver that controls 3 LEDs over USB.

## How This Works

### Assumptions About The Device

The driver assumes a simple interface for controlling LEDs over USB. It assumes that the USB gadget (i.e. the thing that has the LEDs) can accept a single byte containing the state of at most 3 LEDs as the data over a BULK endpoint, and do the necessary to light them up. The 3 LEDs are represented by the 3 least significant bits of the byte. These are:

```
LED 1: 0x01 (0b00000001)
LED 2: 0x02 (0b00000010)
LED 3: 0x04 (0b00000100)
```

Each of those last 3 bits, when equal to 1, indicates that the corresponding LED should be turned on. When 0, it should be turned off. For combinations of LEDs, multiple bits at a time can be set. For example, to turn on LEDs 1 and 3, the bitmask should be `0b00000101`, which is `0x05` in hex.

### Driver Interface

The driver allows controlling these LEDs via a sysfs interface. Writing `1` to `/sys/bus/usb/drivers/usb_led/<your-device-here>/led1` will turn LED1 off, writing `0` to it will turn it off. Same for the files `led2` and `led3`. The `<your-device-here>` placeholder represents your USB device, and is usually something like `1-1:1.0`.

To see the current values of the LEDs, simply read these files.

> Note: the driver keeps track of the LEDs internally, and does NOT try to somehow solicit information about their state from the USB device. So if something other than this driver turns the LEDs on or off, the driver will have an incorrect view of the state of the LEDs. Regardless, writing to the files should still give the right results.

## Compiling And Running

> **Caution**: This is a toy project I used to familiarize myself with driver development and the Linux kernel interfaces. The code is NOT guaranteed to be bug free. I strongly recommend running it in a Virtual Machine (instructions later in the README). Bugs in kernel modules are far more severe than bugs in usual userspace applications - they can cause your system to crash/hang, and often a reboot is the only way to get a clean slate. They can also open serious security holes. If you're running this module in your host kernel, do so at your own risk (and make sure you know what you're doing).

First, please edit the `LED_VENDOR_ID` and `LED_PRODUCT_ID` in `usb_led.h` to match the vendor ID and product ID of the USB device you want to control. This information can be found via the `lsusb` command. Then follow the steps below:

```bash
# Compile the kernel module
make

# Insert the module into the kernel
insmod usb_led.ko
```

Ensure your device is plugged in, and make sure it isn't being managed by a different driver. If it is being managed by a different driver, either unload the driver using `rmmod <driver-name` or `modprobe -r <driver-name>`, or unbind it from the driver by writing the bus ID of the device to `/sys/bus/usb/driver/<driver-name>/unbind`, and bind it to this driver by writing the same bus ID to `/sys/bus/usb/drivers/usb_led/bind`. More information about this can be found [here](https://lwn.net/Articles/143397/).

And that should be enough - you should have the `led1`, `led2`, and `led3` files you need in `/sys/bus/usb/drivers/usb_led/<bus-id>/`.


## My Setup

This section describes how I'm playing with this driver, including a description of the development environment.

### Development Environment

All testing happens in a Qemu virtual machine running Ubuntu. Since I wanted to use my host for development, I'm using NFS to share the directory containing the source code over the local network, and having the Qemu guest mount this fileshare in order to compile and test the code.

NFS config:

```
[root@ajmal ~]# cat /etc/exports
/home/ajmal/Projects/usb-led-driver 127.0.0.1/24(rw,sync,insecure,anonuid=1000,anongid=1000)
```

Make sure to run `exportfs -a` after making changes, and make sure that the NFS server is running (this should usually be a systemd service; on my Fedora machine I can start it using `systemctl start nfs-server.service`.

Launch the Qemu VM using these parameters:

```bash
qemu-system-x86_64 -boot c -drive file=./ubuntu-kernel.vdi,format=vdi -nic user -m 4096 -cpu max -accel kvm -usb -monitor stdio
```

The important parameters are:
`-nic user`: Allows internet access, and also allows communicating with the host over the IP address `10.0.2.2` by default. We'll need this for the NFS mount.
`-usb`: Adds a USB bus to the guest. Needed to access our USB device.
`-monitor stdio`: Gives us a useful shell that lets us interact with the Qemu guest in advanced ways. We'll use this to passthrough our USB device.

Find out the vendor and product IDs of your USB LED device using `lsusb`, and pass these through to the qemu guest using this command in the **qemu shell** (replace `vendorid` and `productid` with what you have, obviously):

```
(qemu) device_add usb-host,vendorid=0x10c4,productid=0xea60
```

On the guest side:

```bash
# Check that your USB device is showing correctly in the guest
lsusb

# Make a mount point
mkdir -p /mnt/usb-driver

# Mount the NFS file share
mount -t nfs4 10.0.2.2:/home/ajmal/Projects/usb-led-driver /mnt/usb-driver
```

And now you should be able to build and run the driver!


### The Arduino Rig

I used an Arduino to control the LEDs, and a USB-to-TTL chip to act as the USB LED device and communicate with the Arduino. The hardcoded vendor and product IDs in the `usb_led.h` file correspond to the vendor and product IDs of my USB-to-TTL chip.

This simple sketch is all you need on the Arduino side (note that we're using the builtin Serial port on the Arduino board for debug messages):

```c
#include <SoftwareSerial.h>

#define SERIAL_BAUD_RATE 9600
#define SERIAL_RX 2
#define SERIAL_TX 6

#define LED_1_PIN LED_BUILTIN
#define LED_2_PIN 7
#define LED_3_PIN 8

#define LED_1 0x01
#define LED_2 0x02
#define LED_3 0x04

SoftwareSerial customSerial(SERIAL_RX, SERIAL_TX);

char serialOutBuf[10];
char c;

// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);
  
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("Main serial port ready!");

  customSerial.begin(SERIAL_BAUD_RATE);
  customSerial.println("Custom serial ready!");
}

// the loop function runs over and over again forever
void loop() {
  if (customSerial.available()) {
    c = customSerial.read();

    sprintf(serialOutBuf, "%x", c);
    Serial.println(serialOutBuf);

    if (c & LED_1) {
      Serial.println("matching byte for LED1");
      digitalWrite(LED_1_PIN, HIGH);
    } else {
      digitalWrite(LED_1_PIN, LOW);
    }

    if (c & LED_2) {
      Serial.println("matching byte for LED2");
      digitalWrite(LED_2_PIN, HIGH);
    } else {
      digitalWrite(LED_2_PIN, LOW);
    }

    if (c & LED_3) {
      Serial.println("matching byte for LED3");
      digitalWrite(LED_3_PIN, HIGH);
    } else {
      digitalWrite(LED_3_PIN, LOW);
    }
  }
}
```

Once you've compiled and flashed this onto the Arduino, connected the LEDs to the right pins (pins 7, 8 and the builtin LED pin 13 on my Arduino Uno, in the sample sketch), and connected the TTL-to-USB adapter correctly (TX pin of the TTL-to-USB chip goes to SERIAL_RX pin on the board, RX pin goes to SERIAL_TX), you should be able to control these 3 LEDs with this device driver.
