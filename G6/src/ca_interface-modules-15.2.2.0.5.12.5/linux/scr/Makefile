EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/include/
EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/common/
EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/stm_scr/include/
EXTRA_CFLAGS+=-I$(STM_CA_TOPDIR)/wrapper/include/
EXTRA_CFLAGS+=-I$(INFRASTRUCTURE)/include/

EXTRA_CFLAGS+=-Werror -Wall

obj-m           += stm_scr.o
obj-m		+= stm_scr_ca.o


CA_OS_OBJS :=  ca_os_wrapper.o \
	       ca_queue.o	

SCR_OBJS :=	   scr/stm_scr_main.o \
		   scr/stm_scr.o \
		   scr/scr_utils.o \
		   scr/scr_thread.o \
		   scr/scr_io.o \
		   scr/scr_internal.o \
		   scr/scr_debugfs.o \
		   scr/scr_data_parsing.o \
		   scr/scr_protocol_t0.o \
                   scr/scr_protocol_t1.o \
                   scr/scr_protocol_t14.o \
                   scr/scr_plugin.o \
		   asc/asc_hal.o \
		   asc/asc_internal.o \
		   asc/asc_scr_exports.o \
		   asc/stm_asc_scr.o  \
		   scr/scr_user_interface.o

SCR_CA_OBJS :=  ca/scr_ca_main.o
SCR_CA_OBJS += ca/scr_ca$(STM_SCR_CA).o

RELATIVE_ROOT = ../../
stm_scr-objs := $(addprefix $(RELATIVE_ROOT)wrapper/src/, $(CA_OS_OBJS))\
		$(addprefix $(RELATIVE_ROOT)stm_scr/src/, $(SCR_OBJS))
stm_scr_ca-objs := $(addprefix $(RELATIVE_ROOT)stm_scr/src/, $(SCR_CA_OBJS))



