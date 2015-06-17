# We should ensure that *all* modules we build are built cleanly,
# without any warnings, so we use subdir-ccflags to provide this
# option throughout the build
subdir-ccflags-y := -Werror
subdir-ccflags-y += -Wextra
subdir-ccflags-y += -Wunused-but-set-variable
subdir-ccflags-y += -Wno-missing-field-initializers
subdir-ccflags-y += -Wno-unused-parameter
subdir-ccflags-y += -Wno-sign-compare

## SE memory checkers
# if < 1: no memory checkers
# if > 0: dump memory checkers
# cf also osdev_abs module parameters
subdir-ccflags-y += -DSE_DEBUG_MEMORY=0

# attributes & statistics support required when DEBUG_FS not set
ifeq ($(SDK2_ENABLE_STATISTICS),y)
subdir-ccflags-y += -DSDK2_ENABLE_STATISTICS
endif
ifeq ($(SDK2_ENABLE_ATTRIBUTES),y)
subdir-ccflags-y += -DSDK2_ENABLE_ATTRIBUTES
endif

#Cannes+; note that CONFIG_MACH_STM_STIH407 is expected to be set for all familly: H305, H407, H310, H410, ..
ifeq ($(CONFIG_MACH_STM_STIH407),y)
# SOC define for VideoCompanion FW header
subdir-ccflags-y += -DBUILD_DEFINED_SOC_STIH407
ifeq ($(CONFIG_ARCH_STI),y)
#STI does not provide config defines anymore: but needed in source code
subdir-ccflags-y += -DCONFIG_MACH_STM_STIH407
endif
HADES_SUPPORTED-$(CONFIG_MACH_STM_STIH407):=y
endif

ifeq ($(CONFIG_HEVC_HADES_CUT1),y)
subdir-ccflags-y += -DHEVC_HADES_CUT1
endif

obj-y += drivers/player2/
obj-y += drivers/allocator/
obj-y += drivers/h264_preprocessor/
obj-$(HADES_SUPPORTED-y) += drivers/hevc_preprocessor/
obj-$(HADES_SUPPORTED-y) += drivers/hevc_hard_host_transformer/
obj-y += drivers/vp8_hard_host_transformer/
obj-y += drivers/h264_encode_hard_host_transformer/
ifeq ($(CONFIG_SOUND),y)
obj-y += drivers/ksound/
endif
ifeq ($(CONFIG_DEBUG_FS),y)
obj-$(CONFIG_RELAY) += drivers/strelay/
endif
obj-y += drivers/mmelog/

ifeq ($(CONFIG_STM_STREAMINGENGINE),)
export CONFIG_STM_STREAMINGENGINE:=m
endif
ifeq ($(CONFIG_DEBUG_FS),y)
ifeq ($(CONFIG_STM_STREAMINGENGINE_TEST),)
export CONFIG_STM_STREAMINGENGINE_TEST:=m
endif
endif
ifeq ($(CONFIG_STM_STREAMINGENGINE_MME),)
export CONFIG_STM_STREAMINGENGINE_MME:=m
endif

platform-objs += se.o
obj-$(CONFIG_STM_STREAMINGENGINE) += platform.o

KBUILD_BUILTIN:=1
