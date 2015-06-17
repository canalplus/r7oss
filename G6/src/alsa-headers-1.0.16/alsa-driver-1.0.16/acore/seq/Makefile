ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := seq.c seq_clientmgr.c seq_memory.c

export-objs  := seq_device.o seq_clientmgr.o seq_lock.o seq_memory.o \
	seq_ports.o seq_midi_emul.o seq_midi_event.o seq_virmidi.o

include $(SND_TOPDIR)/alsa-kernel/core/seq/Makefile

EXTRA_CFLAGS += -I$(SND_TOPDIR)/alsa-kernel/core/seq

include $(SND_TOPDIR)/Rules.make

seq.c: seq.patch $(SND_TOPDIR)/alsa-kernel/core/seq/seq.c
seq_clientmgr.c: seq_clientmgr.patch $(SND_TOPDIR)/alsa-kernel/core/seq/seq_clientmgr.c
seq_memory.c: seq_memory.patch $(SND_TOPDIR)/alsa-kernel/core/seq/seq_memory.c
