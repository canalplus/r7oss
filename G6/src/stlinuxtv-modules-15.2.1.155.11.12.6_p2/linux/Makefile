# STM Infrastructure include path
ccflags-y +=-I$(CONFIG_INFRA_PATH)/include

# STM Frontend-engine include path
ccflags-y +=-I$(CONFIG_FRONTEND_ENGINE_PATH)/include
ccflags-y +=-I$(CONFIG_FRONTEND_ENGINE_PATH)/linux/include

# STM Transport-engine include path
ccflags-y +=-I$(CONFIG_TRANSPORT_ENGINE_PATH)/include

# STM Kernel include path
ifeq "$(shell [ $(VERSION) -ge 3 -a $(PATCHLEVEL) -ge 7 ] && echo true)" "true"
 ccflags-y +=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb-core/
 ccflags-y +=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb-frontends/
else
 ccflags-y +=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb/dvb-core/
 ccflags-y +=-I$(CONFIG_KERNEL_PATH)/drivers/media/dvb/frontends/
endif

# STM CoreDisplay include path
ccflags-y += -I$(CONFIG_STGFB_PATH)
ccflags-y += -I$(CONFIG_STGFB_PATH)/include
ccflags-y += -I$(CONFIG_STGFB_PATH)/linux/kernel/include/

# STM Blitter include path
ccflags-y += -I$(CONFIG_BLITTER_PATH)/linux/kernel/include

# Local tree include path
ccflags-y +=-I$(TREE_ROOT)/linux/
ccflags-y +=-I$(TREE_ROOT)/common/include/
ccflags-y +=-I$(TREE_ROOT)/linux/include/
ccflags-y +=-I$(TREE_ROOT)/linux/include/linux
ccflags-y +=-I$(TREE_ROOT)/linux/include/linux/stm
ccflags-y +=-I$(TREE_ROOT)/linux/include/linux/dvb/
ccflags-y +=-I$(TREE_ROOT)/linux/stmedia/
ccflags-y +=-I$(TREE_ROOT)/common/include

ifeq ($(CONFIG_STM_VIRTUAL_PLATFORM),y)
# As VSOC uses another target directory for module installation, we use a
# specific variable to point to original include directory (avoid useless copy
# as contains mme, transformers .h files)
ccflags-y += -I$(THD_USER_INCLUDE)/usr/include
else
ccflags-y += -I$(TARGET_DIR)/usr/include
endif

ccflags-y += -DDEBUG_DVB
ccflags-y += -Werror

# Include /usr/include on the target so that we can access LinuxDVB headers
# which are not part of STLinuxTV.
ccflags-y += -I$(TARGET_DIR)/usr/include

# Extract and Provide Version Information from git if available
BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
STLINUXTV_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)

# If our git-describe is unsuccessful, we should fall back to an RPMBuild generated file
ifeq ($(STLINUXTV_VERSION),)
STLINUXTV_VERSION := $(shell cat $(TREE_ROOT)/VERSION)
endif

CFLAGS_module_init.o := -DSTLINUXTV_VERSION="KBUILD_STR($(STLINUXTV_VERSION))"

obj-m += stlinuxtv.o

stlinuxtv-objs:= 	module_init.o \
			stat.o

ccflags-y +=-I$(TREE_ROOT)/linux/v4l2/
ccflags-y +=-I$(TREE_ROOT)/linux/v4l2/common
ccflags-y +=-I$(TREE_ROOT)/linux/v4l2/encoder
ccflags-y +=-I$(TREE_ROOT)/linux/v4l2/tsmux
ccflags-y +=-I$(TREE_ROOT)/linux/alsa/
ccflags-y +=-I$(TREE_ROOT)/linux/player2_dvb/
ccflags-y +=-I$(TREE_ROOT)/linux/dvb/
ccflags-y +=-I$(TREE_ROOT)/linux/demux/


# STM Streaming-Engine include path
ccflags-y +=-I$(CONFIG_PLAYER2_PATH)/player/standards
ccflags-y +=-I$(CONFIG_PLAYER2_PATH)/player/include/
ccflags-y +=-I$(CONFIG_PLAYER2_PATH)/linux/drivers/sound/ksound/

SDK2_ENABLE_STLINUXTV_STATISTICS ?=y

ifeq (y, $(CONFIG_SYSFS))
 ifeq (y, $(filter y, $(SDK2_ENABLE_STATISTICS)))
  ifeq (y, $(filter y, $(SDK2_ENABLE_STLINUXTV_STATISTICS)))
   ccflags-y +=-DSDK2_ENABLE_STLINUXTV_STATISTICS
  endif
 endif
endif

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_DECODE_DISPLAY)))
ccflags-y +=-DCONFIG_STLINUXTV_DECODE_DISPLAY

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_SOFTWARE_DEMUX)))
ccflags-y +=-DCONFIG_STLINUXTV_SOFTWARE_DEMUX
stlinuxtv-objs+= 	player2_dvb/dvb_demux.o \
			player2_dvb/dvb_dvr.o \
			player2_dvb/dvb_module.o
endif

stlinuxtv-objs+= 	player2_dvb/backend.o \
			player2_dvb/display.o \
			player2_dvb/audio_out.o \
			player2_dvb/dvb_audio.o \
			player2_dvb/dvb_video.o \
			player2_dvb/dvb_v4l2.o	\
			player2_dvb/dvb_v4l2_ctrls.o \
			v4l2/common/stm_v4l2_common.o


obj-y += v4l2/

ifeq ($(CONFIG_SOUND),y)
obj-y += alsa/
endif
endif

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_FRONTEND_DEMUX)))
ccflags-y +=-DCONFIG_STLINUXTV_FRONTEND_DEMUX
stlinuxtv-objs +=	frontend/dvb_stm_fe.o \
			frontend/dvb_ipfe.o \
			dvb/dvb_data.o \
			dvb/dvb_demod.o \
			dvb/dvb_lnb.o \
			dvb/dvb_tuner.o \
			dvb/dvb_diseqc.o \
			dvb/dvb_rf_matrix.o \
			dvb/dvb_init.o \
			dvb/dvb_demux.o \
			dvb/dvb_ipfec.o \
			demux/demux_feeds.o \
			demux/demux_filter.o \
			demux/stm_dvb_demux_ctrl.o

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_CRYPTO)))
ccflags-y +=-DCONFIG_STLINUXTV_CRYPTO
ifeq ($(CONFIG_MACH_STM_STIH416),y)
ccflags-y += -DCONFIG_MACH_STM_STIH416
endif
# STM Crypto-Engine include path
ccflags-y +=-I$(CONFIG_CRYPTO_ENGINE_PATH)/include
stlinuxtv-objs +=	ca/ca_transform.o \
			ca/dvb_ca.o
endif
endif

obj-m += stmedia.o
stmedia-objs:=		stmedia/stmedia.o

ifeq (y, $(filter y, $(CONFIG_STLINUXTV_CEC)))
obj-m+=	stmcec_io.o
stmcec_io-objs:=	cec/cec_dev_main.o\
			cec/cec_ioctl.o
endif

