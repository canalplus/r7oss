EXTRA_CXXFLAGS := -fno-rtti -fno-exceptions

ifeq ($(CONFIG_STM_FMDSW),y)
EXTRA_CFLAGS += -DUSE_FMD -DBUILTIN_FMD
FMDSW_SRC := $(SRC_TOPDIR)/STMCommon/fmdsw.cpp
endif


# Common stmcoredisplay Linux specific module files
CORESOURCEFILES := $(addprefix $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/,   \
			devfbOS.c                                              \
			hdmisysfs.c                                            \
			hdmidev.c                                              \
			hdmiedid.c                                             \
			hdmikthread.c                                          \
			coredisplay.c)

# Base class files and C API implementation
GENINITSRCS := $(addprefix $(SRC_TOPDIR)/Generic/,                             \
			DisplayDevice.cpp                                      \
			DisplayPlane.cpp                                       \
			Output.cpp                                             \
			MetaDataQueue.cpp)

# Classes common to all ST SoCs regardless of the display architecture
STM_COMMON := $(addprefix $(SRC_TOPDIR)/STMCommon/,                            \
			stmmasteroutput.cpp                                    \
			stmawg.cpp                                             \
			stmdenc.cpp                                            \
			stmfsynth.cpp                                          \
			stmvtg.cpp                                             \
			stmblitter.cpp                                         \
			stmteletext.cpp)

STM_HDMI_COMMON := $(addprefix $(SRC_TOPDIR)/STMCommon/,                       \
			stmhdmi.cpp                                            \
			stmiframemanager.cpp)

# Classes required for all SoCs containing Gamma based hardware video
# composition
STM_GAMMA := $(addprefix $(SRC_TOPDIR)/Gamma/,                                 \
			GenericGammaDevice.cpp                                 \
			GammaMixer.cpp                                         \
			GammaCompositorPlane.cpp                               \
			GammaCompositorGDP.cpp                                 \
			GammaCompositorNULL.cpp                                \
			GammaCompositorCursor.cpp                              \
			GammaCompositorVideoPlane.cpp                          \
			GammaVideoPlug.cpp                                     \
			VBIPlane.cpp)

# Classes for SoCs containing the BDispII IP
STM_BDISP := $(addprefix $(SRC_TOPDIR)/STMCommon/, stmbdisp.cpp stmbdispaq.cpp \
			stmbdispoutput.cpp stmvirtplane.cpp )

# Classes for SoCs containing the HDFormatter/TVOut style video output stage.
STM_HDF_TVOUT := $(addprefix $(SRC_TOPDIR)/STMCommon/,                         \
			stmsdvtg.cpp                                           \
			stmsdvtg_spinlock.c                                    \
			stmfdvo.cpp)

STM_HDF_TVOUT += $(addprefix $(SRC_TOPDIR)/HDTVOutFormatter/,                  \
			stmhdfoutput.cpp                                       \
			stmauxtvoutput.cpp                                     \
			stmhdfhdmi.cpp                                         \
			stmhdfawg.cpp                                          \
			stmhdfdvo.cpp                                          \
			stmtvoutdenc.cpp                                       \
			stmtvoutteletext.cpp)

STM_DEI += $(addprefix $(SRC_TOPDIR)/Gamma/,                                   \
			DEIVideoPipe.cpp                                       \
			VDPFilter.cpp)

STM_DEI_IQI := $(STM_DEI)
STM_DEI_IQI += $(addprefix $(SRC_TOPDIR)/STMCommon/, stmiqi.cpp)
STM_DEI_IQI += $(addprefix $(SRC_TOPDIR)/Gamma/, DEIVideoPipeV2.cpp)

STM_HDMI_DMA_IFRAME := $(addprefix $(SRC_TOPDIR)/STMCommon/, stmdmaiframes.cpp)
STM_HDMI_V2_9 := $(addprefix $(SRC_TOPDIR)/STMCommon/, stmv29iframes.cpp)

ALLSRCS := $(CORESOURCEFILES) $(FMDSW_SRC) $(GENINITSRCS) $(STM_COMMON) $(STM_GAMMA)

ifeq ($(CONFIG_CPU_SUBTYPE_STB7100),y)

#STx710x specific
STX710XSRCS = $(STM_BDISP) $(STM_HDMI_COMMON) $(STM_HDMI_DMA_IFRAME)

STX710XSRCS += $(addprefix $(SRC_TOPDIR)/STMCommon/, stmdmaiframes.cpp)

STX710XSRCS += $(addprefix $(SRC_TOPDIR)/Gamma/,                               \
			GammaDisplayFilter.cpp                                 \
			DEIVideoPipe.cpp                                       \
			VDPFilter.cpp                                          \
			GammaCompositorDISP.cpp                                \
			GammaBlitter.cpp                                       \
			GDPBDispOutput.cpp)

STX710XSRCS += $(addprefix $(SRC_TOPDIR)/soc/stb7100/,                         \
			stb7100device.cpp                                      \
			stb7100AWGAnalog.cpp                                   \
			stb7100analogueouts.cpp                                \
			stb7100dvo.cpp                                         \
			stb7100hdmi.cpp                                        \
			stb7100denc.cpp                                        \
			stb7100vtg.cpp                                         \
			stb7100mixer.cpp                                       \
			stb710xgdp.cpp                                         \
			stb710xcursor.cpp)

stx710x-display-objs := $(patsubst %.cpp,%.o,$(ALLSRCS) $(STX710XSRCS))
stx710x-display-objs := $(patsubst %.c,%.o,$(stx710x-display-objs))

ifeq ($(CONFIG_SH_ST_MB602),y)
obj-m += stmcore-display-sti5202.o
stmcore-display-sti5202-objs   := $(stx710x-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/sti5202.o
else
obj-m += stmcore-display-stx7100.o
obj-m += stmcore-display-stx7109c2.o
obj-m += stmcore-display-stx7109c3.o

stmcore-display-stx7100-objs   := $(stx710x-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/stx7100.o
stmcore-display-stx7109c2-objs := $(stx710x-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/stx7109c2.o
stmcore-display-stx7109c3-objs := $(stx710x-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/stx7109c3.o
endif

endif

ifeq ($(CONFIG_CPU_SUBTYPE_STX7200),y)

STI7200SRCS = $(STM_HDF_TVOUT) $(STM_HDMI_COMMON) $(STM_HDMI_DMA_IFRAME) $(STM_BDISP) $(STM_DEI_IQI)

STI7200SRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

STI7200SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7200/,                         \
			sti7200device.cpp                                      \
			sti7200remoteoutput.cpp                                \
			sti7200hdmi.cpp                                        \
			sti7200denc.cpp                                        \
			sti7200gdp.cpp                                         \
			sti7200mixer.cpp                                       \
			sti7200hdfdvo.cpp                                      \
			sti7200flexvp.cpp                                      \
			sti7200xvp.cpp)

STI7200C2SRCS = $(addprefix $(SRC_TOPDIR)/soc/sti7200/,                        \
			sti7200cut2devicefactory.cpp                           \
			sti7200cut2remotedevice.cpp                            \
			sti7200cut2bdisp.cpp                                   \
			sti7200cut2localdevice.cpp                             \
			sti7200cut2localauxoutput.cpp                          \
			sti7200cut2localmainoutput.cpp)

sti7200-display-objs := $(patsubst %.cpp,%.o,$(ALLSRCS) $(STI7200SRCS))
sti7200-display-objs := $(patsubst %.c,%.o,$(sti7200-display-objs))

sti7200c2-display-objs := $(patsubst %.cpp,%.o,$(STI7200C2SRCS))
sti7200c2-display-objs := $(patsubst %.c,%.o,$(sti7200c2-display-objs))

obj-m += stmcore-display-sti7200c2.o

stmcore-display-sti7200c2-objs := $(sti7200-display-objs) $(sti7200c2-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/sti7200c2.o

endif

# The display hardware and clocking in 7111 and 7141 is identical enough that
# we can use exactly the same driver objects, we just need a different
# coredriver file to deal with board and HDMI hotplug PIO differences.
ifeq ($(CONFIG_CPU_SUBTYPE_STX7111)$(CONFIG_CPU_SUBTYPE_STX7141),y)

STI7111SRCS = $(STM_HDF_TVOUT) $(STM_HDMI_COMMON) $(STM_HDMI_DMA_IFRAME) $(STM_BDISP) $(STM_DEI_IQI)

STI7111SRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

STI7111SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7111/,                         \
			sti7111gdp.cpp                                         \
			sti7111bdisp.cpp                                       \
			sti7111mixer.cpp                                       \
			sti7111dvo.cpp                                         \
			sti7111mainoutput.cpp                                  \
			sti7111auxoutput.cpp                                   \
			sti7111hdmi.cpp                                        \
			sti7111device.cpp                                      \
			sti7111devicecreate.cpp)

sti7111-display-objs := $(patsubst %.cpp,%.o,$(ALLSRCS) $(STI7111SRCS))
sti7111-display-objs := $(patsubst %.c,%.o,$(sti7111-display-objs))


ifeq ($(CONFIG_CPU_SUBTYPE_STX7111),y)
obj-m += stmcore-display-sti7111.o
stmcore-display-sti7111-objs := $(sti7111-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/sti7111.o
endif

ifeq ($(CONFIG_CPU_SUBTYPE_STX7141),y)
obj-m += stmcore-display-stx7141.o
stmcore-display-stx7141-objs := $(sti7111-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/stx7141.o
endif

endif


# The 7105 is almost identical to 7111, the only real difference being an
# added 2nd DVO (not currently supported). However we have to also support
# 7106 here, which has a different HDMI, because it hasn't been given its own
# CPU subtype in KConfig, you can only tell it is a 7106 at runtime.
ifeq ($(CONFIG_CPU_SUBTYPE_STX7105),y)

STI7111SRCS = $(STM_HDF_TVOUT) $(STM_HDMI_COMMON) $(STM_HDMI_DMA_IFRAME) $(STM_BDISP) $(STM_DEI_IQI)

STI7111SRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

STI7111SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7111/,                         \
			sti7111gdp.cpp                                         \
			sti7111bdisp.cpp                                       \
			sti7111mixer.cpp                                       \
			sti7111mainoutput.cpp                                  \
			sti7111auxoutput.cpp                                   \
			sti7111dvo.cpp                                         \
			sti7111hdmi.cpp                                        \
			sti7111device.cpp)

STI7105SRCS = $(STI7111SRCS)
STI7105SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7105/,                         \
			sti7105device.cpp                                      \
			sti7105devicecreate.cpp)

STI7106SRCS = $(STI7111SRCS) $(STM_HDMI_V2_9)
STI7106SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7105/, sti7105device.cpp)
STI7106SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7106/,                         \
			sti7106hdmi.cpp                                        \
			sti7106device.cpp)

sti7105-display-objs := $(patsubst %.cpp,%.o,$(ALLSRCS) $(STI7105SRCS))
sti7105-display-objs := $(patsubst %.c,%.o,$(sti7105-display-objs))

sti7106-display-objs := $(patsubst %.cpp,%.o,$(ALLSRCS) $(STI7106SRCS))
sti7106-display-objs := $(patsubst %.c,%.o,$(sti7106-display-objs))


obj-m += stmcore-display-sti7105.o
obj-m += stmcore-display-sti7106.o

stmcore-display-sti7105-objs := $(sti7105-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/sti7105.o
stmcore-display-sti7106-objs := $(sti7106-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/sti7106.o

endif

ifeq ($(CONFIG_CPU_SUBTYPE_STX7108),y)

STI7108SRCS = $(STM_HDF_TVOUT) $(STM_HDMI_COMMON) $(STM_HDMI_V2_9) $(STM_BDISP) $(STM_DEI)

STI7108SRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7108/,                         \
			sti7108bdisp.cpp                                       \
			sti7108dvo.cpp                                         \
			sti7108hdmi.cpp                                        \
			sti7108mainoutput.cpp                                  \
			sti7108auxoutput.cpp                                   \
			sti7108clkdivider.cpp                                  \
			sti7108device.cpp)


sti7108-display-objs := $(patsubst %.cpp,%.o,$(ALLSRCS) $(STI7108SRCS))
sti7108-display-objs := $(patsubst %.c,%.o,$(sti7108-display-objs))

obj-m += stmcore-display-sti7108.o

stmcore-display-sti7108-objs := $(sti7108-display-objs) $(SRC_TOPDIR)/linux/drivers/stm/coredisplay/sti7108.o

endif


# Add build information defines for just the coredisplay object which will
# appear in sysfs. Because of the time information this file will rebuild
# every time.
CFLAGS_coredisplay.o := -DKBUILD_SYSTEM_INFO="KBUILD_STR($(BUILD_SYSTEM_INFO))"     \
                        -DKBUILD_USER="KBUILD_STR($(BUILD_USER))"                   \
                        -DKBUILD_SOURCE="KBUILD_STR($(STMFB_ORIGINAL_SOURCE_PATH))" \
                        -DKBUILD_DATE="KBUILD_STR($(BUILD_DATE))"


# C++ build magic
EXTRA_CFLAGS += -DINSERT_EXTRA_CXXFLAGS_HERE
mould_cxx_cflags = $(subst -ffreestanding,,\
		   $(subst -Wstrict-prototypes,,\
		   $(subst -Wno-pointer-sign,,\
		   $(subst -Wdeclaration-after-statement,,\
		   $(subst -Werror-implicit-function-declaration,,\
		   $(subst -DINSERT_EXTRA_CXXFLAGS_HERE,$(EXTRA_CXXFLAGS),\
		   $(1)))))))


quiet_cmd_cc_o_cpp = CC $(quiet_modtab) $@

cmd_cc_o_cpp = $(call mould_cxx_cflags,$(cmd_cc_o_c))

define rule_cc_o_cpp
	$(call echo-cmd,checksrc) $(cmd_checksrc)                         \
	$(call echo-cmd,cc_o_cpp)                                         \
	$(cmd_cc_o_cpp);                                                  \
	$(cmd_modversions)                                                \
	scripts/basic/fixdep $(depfile) $@ '$(call make-cmd,cc_o_cpp)' > $(@D)/.$(@F).tmp;  \
	rm -f $(depfile);                                                 \
	mv -f $(@D)/.$(@F).tmp $(@D)/.$(@F).cmd
endef

%.o: %.cpp FORCE
	$(call cmd,force_checksrc)
	$(call if_changed_rule,cc_o_cpp)

