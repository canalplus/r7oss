#include <linux/config.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)
#define SND_NEED_USB_SET_INTERFACE
#endif
// #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define SND_NEED_USB_WRAPPER
// #endif
#include "adriver.h"
#include <linux/usb.h>

#ifndef USB_DT_CS_DEVICE
#define USB_DT_CS_DEVICE		0x21
#define USB_DT_CS_CONFIG		0x22
#define USB_DT_CS_STRING		0x23
#define USB_DT_CS_INTERFACE		0x24
#define USB_DT_CS_ENDPOINT		0x25
#endif

#ifdef OLD_USB
static void * usb_audio_probe(struct usb_device *dev, unsigned int ifnum,
                              const struct usb_device_id *id);
static void usb_audio_disconnect(struct usb_device *dev, void *ptr);
static int usb_reset_configuration(struct usb_device *dev);
#endif

// vim: ft=c
