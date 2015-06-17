EXTRA_CFLAGS+=-Idrivers/media/dvb/dvb-core/
EXTRA_CFLAGS+=-I$(INCLUDE_PATH)/usr/include/linux/

########################
########################

ifeq ($(CONFIG_SH_WYMDBOX01),y)
FRONTEND_DEMOD_STV0360=y
endif

ifeq ($(CONFIG_SH_NANGMAA),y)
FRONTEND_DEMOD_DNOD44QZV102A=y
FRONTEND_TUNER_DNOD44QZV102A=y
endif

ifeq ($(CONFIG_SH_NANGMAB),y)
FRONTEND_DEMOD_DNOD44QZV102A=y
FRONTEND_TUNER_DNOD44QZV102A=y
endif

ifeq ($(CONFIG_SH_NANGMAC),y)
FRONTEND_DEMOD_MXL101SF=1
FRONTEND_TUNER_MXL101SF=1
endif

ifeq ($(CONFIG_SH_PANDA2B),y)
FRONTEND_DEMOD_MXL101SF=1
FRONTEND_TUNER_MXL101SF=1
endif

ifeq ($(CONFIG_SH_MAROONA),y)
FRONTEND_DEMOD_TDA10048=1
endif

ifeq ($(CONFIG_SH_DORADE),y)
FRONTEND_DEMOD_TDA10048=1
endif

#########################
#########################


ifneq ($(FRONTEND_DEMOD_STV0299),)
obj-m += stv0299.o
endif

ifneq ($(FRONTEND_DEMOD_STV0360),)
obj-m += stv0360.o
endif

ifneq ($(FRONTEND_TUNER_STB6000),)
obj-m += stb6000.o
endif

ifneq ($(FRONTEND_DEMOD_STV900),)
stv0900-objs := stv0900_core.o stv0900_sw.o
obj-m += stv0900.o
obj-m += stv090x.o
endif

ifneq ($(FRONTEND_DEMOD_DNOD44QZV102A),)
  ifneq ($(FRONTEND_TUNER_DNOD44QZV102A),)
### REFERENCE: DNOD44QZV102A
### TYPE:      DVB-T
### BOARDS:    NANGMAA, NANGMAB
mxl5007-ce6353-objs:= \
	./intel/mxl5007-ce6353.o \
	./intel/sembsl_ce6353.o \
	./intel/sembsl_mxl5007.o \
	./intel/sembsl_mxl5007ce6353.o \
	./intel/platform_dependent.o \
	./intel/semapal_fe_simplex.o
obj-m += mxl5007-ce6353.o
  endif
endif

ifneq ($(FRONTEND_DEMOD_MXL101SF),)
  ifneq ($(FRONTEND_TUNER_MXL101SF),)
### REFERENCE: MXL101SF
### TYPE:      DVB-T
### BOARDS:    PANDA2B, NANGMAC
mxl101sf-objs := \
	./mxl101sf/src/MxL101SF_PhyCfg.o \
	./mxl101sf/src/MxL101SF_PhyCtrlApi.o \
	./mxl101sf/src/MxL101SF_OEM_Drv.o \
	./mxl101sf/mxl101sf.o 

obj-m += mxl101sf.o
  endif
endif

ifneq ($(FRONTEND_DEMOD_TDA10048),)
obj-m += tda10048.o
endif
