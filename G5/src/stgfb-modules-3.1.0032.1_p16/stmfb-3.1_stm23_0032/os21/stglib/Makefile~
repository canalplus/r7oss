STG_TOPDIR = ../..


DEVFBSOURCEFILES := stgos21.c application_helpers.cpp

SRC_TOPDIR := $(STG_TOPDIR)

GENINITSRCS := $(addprefix $(SRC_TOPDIR)/Generic/,                             \
			DisplayDevice.cpp                                      \
			DisplayPlane.cpp                                       \
			Output.cpp                                             \
			MetaDataQueue.cpp)

STGGAMMASRC := $(addprefix $(SRC_TOPDIR)/Gamma/,                               \
			GenericGammaDevice.cpp                                 \
			GammaMixer.cpp                                         \
			GammaCompositorPlane.cpp                               \
			GammaCompositorCursor.cpp                              \
			GammaCompositorGDP.cpp                                 \
			GammaCompositorNULL.cpp                                \
			GammaCompositorVideoPlane.cpp                          \
			GammaVideoPlug.cpp                                     \
			VBIPlane.cpp)


STMCOMMON := $(addprefix $(SRC_TOPDIR)/STMCommon/,                             \
			stmawg.cpp                                             \
			stmdenc.cpp                                            \
			stmfsynth.cpp                                          \
			stmvtg.cpp                                             \
			stmblitter.cpp                                         \
			stmbdisp.cpp                                           \
			stmbdispaq.cpp                                         \
			stmbdispoutput.cpp                                     \
			stmvirtplane.cpp                                       \
			stmteletext.cpp                                        \
			stmmasteroutput.cpp)

HDTVOUTCOMMON := $(addprefix $(SRC_TOPDIR)/STMCommon/,                         \
			stmsdvtg.cpp                                           \
			stmsdvtg_spinlock.c                                    \
			stmfdvo.cpp)

HDTVOUTCOMMON += $(addprefix $(SRC_TOPDIR)/HDTVOutFormatter/,                  \
			stmhdfoutput.cpp                                       \
			stmauxtvoutput.cpp                                     \
			stmhdfhdmi.cpp                                         \
			stmhdfawg.cpp                                          \
			stmhdfdvo.cpp                                          \
			stmtvoutdenc.cpp                                       \
			stmtvoutteletext.cpp)

DEI_IQI := $(addprefix $(SRC_TOPDIR)/STMCommon/, stmiqi.cpp)
DEI_IQI += $(addprefix $(SRC_TOPDIR)/Gamma/,                                   \
			DEIVideoPipe.cpp                                       \
			VDPFilter.cpp                                          \
			DEIVideoPipeV2.cpp)

HDMI_COMMON := $(addprefix $(SRC_TOPDIR)/STMCommon/,                           \
			stmhdmi.cpp                                            \
			stmiframemanager.cpp)

HDMI_FDMA_IFRAME := $(addprefix $(SRC_TOPDIR)/STMCommon/, stmdmaiframes.cpp)

HDMI_V2_9 := $(addprefix $(SRC_TOPDIR)/STMCommon/, stmv29iframes.cpp)

ALLFBSRCS := $(DEVFBSOURCEFILES) $(GENINITSRCS) $(STMCOMMON) $(STGGAMMASRC)

ifeq ($(CONFIG_STB7100),y)

#STb710x
ALLFBSRCS +=  $(HDMI_COMMON) $(HDMI_FDMA_IFRAME)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/Gamma/,                                 \
			GammaDisplayFilter.cpp                                 \
			DEIVideoPipe.cpp                                       \
			VDPFilter.cpp                                          \
			GammaCompositorDISP.cpp                                \
			GammaBlitter.cpp                                       \
			GDPBDispOutput.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/stb7100/,                           \
			stb7100device.cpp                                      \
			stb7100analogueouts.cpp                                \
			stb7100dvo.cpp                                         \
			stb7100hdmi.cpp                                        \
			stb7100denc.cpp                                        \
			stb7100vtg.cpp                                         \
			stb7100mixer.cpp                                       \
			stb710xgdp.cpp                                         \
			stb710xcursor.cpp                                      \
			stb7100AWGAnalog.cpp)

else
ifeq ($(CONFIG_STI7200C2),y)
ALLFBSRCS += $(HDTVOUTCOMMON)  $(HDMI_COMMON) $(HDMI_FDMA_IFRAME) $(DEI_IQI)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7200/,                           \
			sti7200device.cpp                                      \
			sti7200cut2devicefactory.cpp                           \
			sti7200cut2remotedevice.cpp                            \
			sti7200cut2localdevice.cpp                             \
			sti7200cut2bdisp.cpp                                   \
			sti7200remoteoutput.cpp                                \
			sti7200hdmi.cpp                                        \
			sti7200cut2localauxoutput.cpp                          \
			sti7200cut2localmainoutput.cpp                         \
			sti7200denc.cpp                                        \
			sti7200gdp.cpp                                         \
			sti7200mixer.cpp                                       \
			sti7200hdfdvo.cpp                                      \
			sti7200flexvp.cpp                                      \
			sti7200xvp.cpp)

else
ifeq ($(CONFIG_STI7111)$(CONFIG_STI7141),y)
ALLFBSRCS += $(HDTVOUTCOMMON)  $(HDMI_COMMON) $(HDMI_FDMA_IFRAME) $(DEI_IQI)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7111/,                           \
			sti7111gdp.cpp                                         \
			sti7111bdisp.cpp                                       \
			sti7111mixer.cpp                                       \
			sti7111dvo.cpp                                         \
			sti7111mainoutput.cpp                                  \
			sti7111auxoutput.cpp                                   \
			sti7111hdmi.cpp                                        \
			sti7111device.cpp                                      \
			sti7111devicecreate.cpp)

else
ifeq ($(CONFIG_STI7105),y)
ALLFBSRCS += $(HDTVOUTCOMMON)  $(HDMI_COMMON) $(HDMI_FDMA_IFRAME) $(DEI_IQI)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7111/,                           \
			sti7111gdp.cpp                                         \
			sti7111bdisp.cpp                                       \
			sti7111mixer.cpp                                       \
			sti7111dvo.cpp                                         \
			sti7111mainoutput.cpp                                  \
			sti7111auxoutput.cpp                                   \
			sti7111hdmi.cpp                                        \
			sti7111device.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7105/,                           \
			sti7105device.cpp                                      \
			sti7105devicecreate.cpp)

else
ifeq ($(CONFIG_STI7106),y)
ALLFBSRCS += $(HDTVOUTCOMMON) $(HDMI_COMMON) $(HDMI_V2_9) $(DEI_IQI)
# Because we subclass the 7111 hdmi implementation we have to include
# the DMA IFrame manager even though we are not going to use it.
ALLFBSRCS += $(HDMI_FDMA_IFRAME)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/Gamma/, GDPBDispOutput.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7111/,                           \
			sti7111gdp.cpp                                         \
			sti7111bdisp.cpp                                       \
			sti7111mixer.cpp                                       \
			sti7111dvo.cpp                                         \
			sti7111mainoutput.cpp                                  \
			sti7111auxoutput.cpp                                   \
			sti7111hdmi.cpp                                        \
			sti7111device.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7105/, sti7105device.cpp)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7106/,                           \
			sti7106hdmi.cpp                                        \
			sti7106device.cpp)

else
ifeq ($(CONFIG_STI5206),y)
# Note: no HDMI on this device
ALLFBSRCS += $(HDTVOUTCOMMON) $(DEI_IQI)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti5206/,                           \
			sti5206bdisp.cpp                                       \
			sti5206dvo.cpp                                         \
			sti5206mainoutput.cpp                                  \
			sti5206auxoutput.cpp                                   \
			sti5206clkdivider.cpp                                  \
			sti5206videopipes.cpp                                  \
			sti5206device.cpp)

else
ifeq ($(CONFIG_STI7108),y)
# TODO: Add SoC HDMI and HQVDP when written
ALLFBSRCS += $(HDTVOUTCOMMON) $(HDMI_COMMON) $(HDMI_V2_9) $(DEI_IQI)

ALLFBSRCS += $(addprefix $(SRC_TOPDIR)/soc/sti7108/,                           \
			sti7108bdisp.cpp                                       \
			sti7108dvo.cpp                                         \
			sti7108hdmi.cpp                                        \
			sti7108mainoutput.cpp                                  \
			sti7108auxoutput.cpp                                   \
			sti7108clkdivider.cpp                                  \
			sti7108device.cpp)

else
$(error Unknown chip configuration)
endif
endif
endif
endif
endif
endif
endif

ALLFBOBJS := $(patsubst %.cpp,%.o,$(ALLFBSRCS))
ALLFBOBJS := $(patsubst %.c,%.o,$(ALLFBOBJS))

all: libstgos21.a

libstgos21.a: $(ALLFBOBJS)
	$(AR) $(ARFLAGS) $@ $(ALLFBOBJS)

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
%.o:%.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

