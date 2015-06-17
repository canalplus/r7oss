ccflags-$(CONFIG_SND_DEBUG)  := -DDEBUG
ccflags-$(CONFIG_SND_DEBUG_VERBOSE) += -DVERBOSE_DEBUG

# Core

obj-$(CONFIG_SND_STM) += snd-stm-core.o
snd-stm-core-y := core.o clock.o conv.o

# Players, Readers

obj-$(CONFIG_SND_STM_UNIPERIF_PLAYER) += snd-stm-uniperif-player.o
snd-stm-uniperif-player-y := uniperif_player.o

obj-$(CONFIG_SND_STM_UNIPERIF_READER) += snd-stm-uniperif-reader.o
snd-stm-uniperif-reader-y := uniperif_reader.o

obj-$(CONFIG_SND_STM_UNIPERIF_TDM) += snd-stm-uniperif-tdm.o
snd-stm-uniperif-tdm-y := uniperif_tdm.o

# Internal converters

obj-$(CONFIG_SND_STM_CONV_DAC_SYSCONF) += snd-stm-conv-dac-sc.o
snd-stm-conv-dac-sc-y := conv_dac_sc.o

obj-$(CONFIG_SND_STM_CONV_DAC_BUFFER) += snd-stm-conv_dac_buffer.o
snd-stm-conv_dac_buffer-y := conv_dac_buffer.o

obj-$(CONFIG_SND_STM_CONV_BIPHASE) += snd-stm-conv-biphase.o
snd-stm-conv-biphase-y := conv_biphase.o

# SoC glue

obj-$(CONFIG_SND_STM_AUDIO_GLUE) += snd-stm-audio-glue.o
snd-stm-audio-glue-y := audio_glue.o

# TELSS glue

obj-$(CONFIG_SND_STM_TELSS_GLUE) += snd-stm-telss-glue.o
snd-stm-telss-glue-y := telss_glue.o

