KBUILD_CPPFLAGS += -I$(CONFIG_INFRASTRUCTURE_PATH)/include -I$(CONFIG_INFRASTRUCTURE_PATH)/stm_event/include -I$(CONFIG_INFRASTRUCTURE_PATH)/common

ifeq ($(CONFIG_CPU_SUBTYPE_STIH416),y)
 obj-m += vxi/
endif

obj-m += pixel_capture/
obj-m += hdmirx/
obj-m += hdmi/
obj-m += displaylink/
obj-m += cec/
obj-m += coredisplay/
obj-m += scaler/
obj-m += vibe_os/

ifneq ($(DISABLE_DISPLAY_TESTS),y)
 ifeq ($(CONFIG_CPU_SUBTYPE_STIH416),y)
  obj-m += vxitest/
 endif
 obj-m += pixelcapturetest/
 obj-m += hdmirxtest/
 obj-m += cectest/
 ifeq ($(CONFIG_RELAY)$(CONFIG_DEBUG_FS),yy)
  obj-m += coredisplaytest/
 endif
 obj-m += vibe_ostest/
endif

