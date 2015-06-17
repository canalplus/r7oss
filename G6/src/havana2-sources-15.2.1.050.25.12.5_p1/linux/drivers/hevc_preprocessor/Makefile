ccflags-y += -I$(TREE_ROOT)/linux/drivers/osdev_abs

ccflags-y += -I$(SDK2_SOURCE_video_apiheaders_ext)

#to prevent warning in header hevc_video_transformer_types.h
ccflags-y += -Wno-old-style-declaration

sthevcpp-objs := hevcpp.o

obj-$(CONFIG_STM_STREAMINGENGINE) += sthevcpp.o
