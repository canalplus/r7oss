#
# (C) COPYRIGHT 2012 STMicroelectronics R&D Ltd
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#

obj-$(CONFIG_MTT) := mtt.o
mtt-objs := \
		core.o \
		lib.o \
		components.o \
		packets.o \
		relay.o

ifdef CONFIG_STM_SYSTRACE
mtt-objs +=	systrace.o
endif

ifdef CONFIG_KPTRACE
mtt-objs += kptrace.o \
	../../arch/$(ARCH)/kernel/mtt-kptrace.o
endif
