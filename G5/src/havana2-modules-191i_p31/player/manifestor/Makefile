manifestor-objs:=       manifestor_base.o \
			manifestor_video.o \
			manifestor_video_stmfb.o \
			manifestor_audio.o \
			manifestor_audio_ksound.o \
			mixer_mme.o \
			pcmplayer_ksound.o \
			manifestor_clone.o

CFLAGS+=-I$(TREE_ROOT)/linux/drivers/sound/pseudocard/

obj-y += $(manifestor-objs)
