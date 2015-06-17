CFLAGS_HDRX:= -DSTHDMIRX_IP

#Enable the HDMIRX I2S BLOCK
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_I2S_TX_IP

#enable the FPB1_FPGA Support
#CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_FPB1_FPGA_ENABLE

#enable the CEC IP support.
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_CEC_IP

#enable the Clock Gen Module
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_CLOCK_GEN_ENABLE

#enable the Clock Gen Module
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_I2C_MASTER_ENABLE

#enable the CSM IP
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_CSM_IP

#enable Internal EDID support
#CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_INTERNAL_EDID_SUPPORT

#enable IFM support
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_IFM_IP
#enable the HDCP Key Loading
#CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_HDCP_KEY_LOADING_THRU_ST40

#HDMIRX_ROOT := $(SRC_TOPDIR)/hdmirx
# include paths
HDRX_HDR_DIRS:= $(addprefix $(STG_TOPDIR)/, \
   hdmirx/include \
   hdmirx/src)

# source paths
ifeq ($(CONFIG_MACH_STM_STIH416),y)
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_WA_VCore_1V=0
HDRX_SRC_DIRS:= $(addprefix $(STG_TOPDIR)/hdmirx/, \
   src \
   src/include \
   src/clkgen \
   src/core \
   src/csm \
   src/phy \
   src/system \
   )
endif
ifeq ($(CONFIG_MACH_STM_STIH407),y)
CFLAGS_HDRX:= $(CFLAGS_HDRX) -DSTHDMIRX_WA_VCore_1V=0
HDRX_SRC_DIRS:= $(addprefix $(STG_TOPDIR)/hdmirx/, \
   src \
   src/include \
   src/4fs432 \
   src/core \
   src/csm \
   src/phy \
   src/system \
   )
endif


OBJECTS_HDRX += $(foreach dir,$(HDRX_SRC_DIRS),$(wildcard $(dir)/*.c))
STM_SRC_FILES += $(foreach file,$(OBJECTS_HDRX),$(subst $(STG_TOPDIR),$(SRC_TOPDIR),$(file)))

CFLAGS_HDRX := $(CFLAGS_HDRX) $(addprefix -I,$(HDRX_HDR_DIRS) $(HDRX_SRC_DIRS))
