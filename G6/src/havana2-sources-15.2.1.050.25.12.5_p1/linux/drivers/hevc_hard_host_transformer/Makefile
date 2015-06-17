ccflags-y += -I$(CONFIG_MULTICOM_PATH)/source/include

ccflags-y += -I$(SDK2_SOURCE_video_apiheaders_ext)

ccflags-y += -I$(TREE_ROOT)/linux/drivers/osdev_abs
ccflags-y += -I$(TREE_ROOT)/linux/drivers/strelay

ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_hard_host_transformer/HADES_API
ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_hard_host_transformer/STHORM
ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_hard_host_transformer/CRC
ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_hard_host_transformer/DUMPER
ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_hard_host_transformer/MEMTEST

#to prevent warning in header hevc_video_transformer_types.h
ccflags-y += -Wno-old-style-declaration

#ccflags-y += -g
#ccflags-y += -O0

sthorm-objs      := STHORM/driver.o STHORM/driverEmuCommon.o STHORM/driverEmuHce.o STHORM/lowLoader.o STHORM/elfLoader.o STHORM/elfCopier.o
crc-objs         := CRC/checker.o CRC/ring.o CRC/crcdriver.o CRC/crc.o
dumper-objs      := DUMPER/picture_dumper.o
memtest-objs     := MEMTEST/memtest.o MEMTEST/sbag.o
transformer-objs := hevc_hard_host_transformer.o HADES_API/hades_api.o
stub-objs        := hevc_hard_host_transformer.o HADES_API/hades_stub.o CRC/crc.o

hades-objs:= $(sthorm-objs) $(crc-objs)  $(dumper-objs) $(memtest-objs) $(transformer-objs)
# hades-objs:=  $(stub-objs)

obj-$(CONFIG_STM_STREAMINGENGINE) += hades.o
