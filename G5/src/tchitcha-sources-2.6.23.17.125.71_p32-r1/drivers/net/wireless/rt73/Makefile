obj-$(CONFIG_RT73_WIRELESS) += rt73.o

EXTRA_CFLAGS += -DAGGREGATION_SUPPORT -DWMM_SUPPORT -DDBG

rt73-objs := rtmp_main.o mlme.o connect.o rtusb_bulk.o rtusb_io.o \
      sync.o assoc.o auth.o auth_rsp.o rtusb_data.o \
      rtmp_init.o  sanity.o rtmp_wep.o rtmp_info.o \
      rtmp_tkip.o wpa.o md5.o
