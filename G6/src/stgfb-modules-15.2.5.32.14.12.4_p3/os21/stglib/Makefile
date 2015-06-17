STG_TOPDIR = ../..


DEVFBSOURCEFILES := stgos21.c application_helpers.c

SRC_TOPDIR := $(STG_TOPDIR)

CFLAGS += -include os21-platform.h

GENINITSRCS := $(addprefix $(SRC_TOPDIR)/display/generic/,                     \
			DisplayDevice.cpp                                      \
			DisplayPlane.cpp                                       \
			DisplaySource.cpp                                      \
			SourceInterface.cpp                                    \
			QueueBufferInterface.cpp                               \
			Output.cpp                                             \
			PixelStreamInterface.cpp                               \
			MetaDataQueue.cpp)

STGGAMMASRC := $(addprefix $(SRC_TOPDIR)/display/gamma/,                       \
			GammaMixer.cpp)

STMCOMMON := $(addprefix $(SRC_TOPDIR)/display/ip/,                            \
			stmawg.cpp                                             \
			stmdenc.cpp                                            \
			stmteletext.cpp                                        \
			stmmasteroutput.cpp)


DEI_IQI := $(addprefix $(SRC_TOPDIR)/display/ip/, stmiqi.cpp)
DEI_IQI += $(addprefix $(SRC_TOPDIR)/display/gamma/,                           \
			DEIVideoPipe.cpp                                       \
			VDPFilter.cpp                                          \
			DEIVideoPipeV2.cpp)

STM_SRC_FILES := $(DEVFBSOURCEFILES) $(GENINITSRCS) $(STMCOMMON) $(STGGAMMASRC)

ifneq ($(CONFIG_STG_OPTLEVEL),)
CFLAGS += -O$(CONFIG_STG_OPTLEVEL)
endif



ifeq ($(CONFIG_STIH407),y)
# This is a dummy build target to get the code through a compiler
CONFIG_FLEXCLKGEN      := y
CONFIG_C8VTG           := y

CONFIG_HDMI_TX         := y
CONFIG_HDMI_V29_IFRAME := y
CONFIG_HDMITX3G4_PHY   := y
CONFIG_HDF_V5_0        := y
CONFIG_TVOUT_HD        := y
CONFIG_TVOUT_SD        := y
CONFIG_FDVO            := y
CONFIG_TVOUT_TELETEXT  := y
CONFIG_TVOUT_DENC      := y

STM_SRC_FILES += $(DEI_IQI) $(HQVDPSRC)

include $(SRC_TOPDIR)/display/soc/stiH407/soc.mak

else
$(error Unknown chip configuration)
endif

include $(SRC_TOPDIR)/display/ip/ip.mak

ALLFBOBJS := $(patsubst %.cpp,%.o,$(STM_SRC_FILES))
ALLFBOBJS := $(patsubst %.c,%.o,$(ALLFBOBJS))

CXXFLAGS += -D__BUILD_COREDISPLAY__

all: libstgos21.a

libstgos21.a: $(ALLFBOBJS)
	$(AR) $(ARFLAGS) $@ $(ALLFBOBJS)

clean: $(ALLFBOBJS)
	-find . -name "*.o" | xargs rm -f
	-find . -name "*.a" | xargs rm -f

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o:%.cpp
	$(CC) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<
