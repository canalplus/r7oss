CFLAGS+=-I$(TREE_ROOT)/linux/include/linux/stm/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/dvb/stm/dvb/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/dvb/stm/allocator/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/dvb/stm/h264_preprocessor/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/sound/pseudocard/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/stm/mmelog/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/stm/monitor/
CFLAGS+=-I$(TREE_ROOT)/linux/drivers/media/sysfs/stm/
CFLAGS+=-DREPORT

CFLAGS+= -fno-exceptions

## It would be good to make this CXXFLAGS however, this generates some missing cxx symbols???
## Somebody is using c++ in c code.
CFLAGS+= -fno-rtti

##A bit of a hack to stop conflicting functions
ifeq ($(CONFIG_STM_UNIFY_PLAYER2),y)
CFLAGS+=-DStreamChannelSelect=DEF01
CFLAGS+=-DStreamRegisterEventSignalCallback=DEF02
CFLAGS+=-DBackendDelete=DEF03
CFLAGS+=-DStreamSetPlayInterval=DEF04
CFLAGS+=-DPlaybackCreate=DEF05
CFLAGS+=-DDisplaySynchronize=DEF06
CFLAGS+=-DPlaybackSetSpeed=DEF07
CFLAGS+=-DStreamGetPlayInfo=DEF08
CFLAGS+=-DStreamGetDecodeBufferPoolStatus=DEF09
CFLAGS+=-DPlaybackAddStream=DEF010
CFLAGS+=-DStreamSetInputWindow=DEF011
CFLAGS+=-DStreamStep=DEF012
CFLAGS+=-DPlaybackGetPlayerEnvironment=DEF013
CFLAGS+=-DPlaybackSetOption=DEF014
CFLAGS+=-DStreamEnable=DEF015
CFLAGS+=-DDisplayDelete=DEF016
CFLAGS+=-DStreamGetDecodeBuffer=DEF017
CFLAGS+=-DStreamGetPlayerEnvironment=DEF018
CFLAGS+=-DStreamSetId=DEF019
CFLAGS+=-DStreamDiscontinuity=DEF020
CFLAGS+=-DStreamReturnDecodeBuffer=DEF021
CFLAGS+=-DStreamGetOption=DEF022
CFLAGS+=-DStreamSwitch=DEF023
CFLAGS+=-DStreamSetOutputWindow=DEF024
CFLAGS+=-DStreamDrain=DEF025
CFLAGS+=-DBackendInit=DEF026
CFLAGS+=-DPlaybackGetSpeed=DEF027
CFLAGS+=-DPlaybackSetClockDataPoint=DEF028
CFLAGS+=-DPlaybackRemoveStream=DEF029
CFLAGS+=-DPlaybackSetNativePlaybackTime=DEF030
CFLAGS+=-DStreamSetOption=DEF031
CFLAGS+=-DComponentSetAttribute=DEF032
CFLAGS+=-DPlayerRegisterEventSignalCallback=DEF033
CFLAGS+=-DComponentGetAttribute=DEF034
endif

infrastructure-objs := \
			crc32.o \
			report.o \
			ring_generic.o \
			stack_generic.o \
			allocator_simple.o

inline-objs :=          osinline.o \
			osdev_user.o \
			osdev_device.o \
			sh4cache.o 

backend-objs :=         player_module.o \
			display.o \
			player_backend.o \
			player_factory.o \
			havana_player.o \
			havana_factory.o \
			havana_playback.o \
			havana_display.o \
			havana_demux.o \
			havana_stream.o

buffer-objs :=          buffer_individual_generic.o \
			buffer_manager_generic.o \
			buffer_pool_generic.o

player-objs :=          player_coded_frame.o \
			player_construct.o \
			player_events.o \
			player_extraction.o \
			player_in_sequence.o \
			player_input.o \
			player_playback.o \
			player_process_collate_to_parse.o \
			player_process_decode_to_manifest.o \
			player_process_parse_to_decode.o \
			player_process_post_manifest.o \
			player_settings.o \
			player_statistics.o \
			player_switch.o \
			player_time.o \
			player_tostring.o

demultiplexor-objs :=   demultiplexor_base.o \
			demultiplexor_ts.o

collator-objs :=        collator_base.o \
			collator_packet.o \
			collator_packet_dvp.o \
			collator_pes.o \
			collator_pes_video.o \
			collator_pes_video_mpeg2.o \
			collator_pes_video_h264.o \
			collator_pes_video_vc1.o \
			collator_pes_video_mjpeg.o \
			collator_pes_frame.o \
			collator_pes_video_divx.o \
			collator_pes_video_raw.o \
			collator_pes_audio.o \
			collator_pes_audio_aac.o \
			collator_pes_audio_dvd.o \
			collator_pes_audio_mpeg.o \
			collator_pes_audio_eac3.o \
			collator_pes_audio_dtshd.o \
			collator_pes_audio_lpcm.o \
			collator_pes_audio_wma.o \
			collator_pes_audio_mlp.o

collator2-objs :=       collator2_base.o \
			collator2_pes.o \
			collator2_pes_video.o \
			collator2_pes_video_h264.o

frame_parser-objs :=    frame_parser_base.o \
			frame_parser_video.o \
			frame_parser_video_mpeg2.o \
			frame_parser_video_h264.o \
			frame_parser_video_vc1.o \
			frame_parser_video_wmv.o \
			frame_parser_video_divx.o \
			frame_parser_video_divx_hd.o \
			frame_parser_video_dvp.o \
			frame_parser_video_h263.o \
			frame_parser_video_flv1.o \
			frame_parser_video_vp6.o \
			frame_parser_video_rmv.o \
			frame_parser_video_theora.o \
			frame_parser_video_mjpeg.o \
			frame_parser_audio.o \
			frame_parser_audio_aac.o \
			frame_parser_audio_mpeg.o \
			frame_parser_audio_eac3.o \
			frame_parser_audio_dtshd.o \
			frame_parser_audio_lpcm.o \
			asf_guids.o \
			wma_properties.o \
			frame_parser_audio_wma.o \
			frame_parser_audio_mlp.o \
			frame_parser_audio_rma.o \
			frame_parser_audio_vorbis.o \
			frame_parser_audio_pcm.o

codec-objs :=           codec_mme_base.o        \
			codec_mme_video.o       \
			codec_mme_video_mpeg2.o \
			codec_mme_video_h264.o  \
			codec_mme_video_divx.o \
			codec_mme_video_divx_hd.o \
			codec_mme_video_vc1.o   \
			codec_mme_video_h263.o  \
			codec_mme_video_flv1.o  \
			codec_mme_video_vp6.o  \
			codec_mme_video_rmv.o  \
			codec_mme_video_theora.o  \
			codec_mme_video_mjpeg.o  \
			codec_dvp_video.o       \
			codec_mme_audio.o       \
			codec_mme_audio_stream.o \
			codec_mme_audio_aac.o  \
			codec_mme_audio_mpeg.o  \
			codec_mme_audio_eac3.o  \
			codec_mme_audio_dtshd.o \
			codec_mme_audio_lpcm.o \
			codec_mme_audio_wma.o \
			codec_mme_audio_spdifin.o \
			codec_mme_audio_silence.o \
			codec_mme_audio_mlp.o \
			codec_mme_audio_rma.o \
			codec_mme_audio_vorbis.o \
			codec_mme_audio_pcm.o

manifestor-objs :=      manifestor_base.o \
			manifestor_video.o \
			manifestor_video_stmfb.o \
			manifestor_audio.o \
			manifestor_audio_ksound.o \
			mixer_mme.o \
			pcmplayer_ksound.o \
			manifestor_clone.o

output_coordinator-objs :=      output_coordinator_base.o

output_timer-objs :=    output_timer_base.o \
			output_timer_video.o \
			output_timer_audio.o

PLAYER_ROOT=../../../../../../

player2-objs :=         $(backend-objs) \
			$(addprefix $(PLAYER_ROOT)/player/demultiplexor/, $(demultiplexor-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/collator/, $(collator-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/collator2/, $(collator2-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/frame_parser/, $(frame_parser-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/codec/, $(codec-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/output_coordinator/, $(output_coordinator-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/output_timer/, $(output_timer-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/manifestor/, $(manifestor-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/buffer/, $(buffer-objs)) \
			$(addprefix $(PLAYER_ROOT)/player/player/, $(player-objs))  \
			$(addprefix $(PLAYER_ROOT)/components/infrastructure/, $(infrastructure-objs)) \
			$(addprefix $(PLAYER_ROOT)/linux/components/inline/, $(inline-objs))


obj-$(CONFIG_STM_UNIFY_PLAYER2) += player2.o

ifeq ($(CONFIG_DEBUG_INFO_CPP),y)
debug_info_cpp = -O0
endif

# C++ build magic
EXTRA_CFLAGS += -DINSERT_EXTRA_CXXFLAGS_HERE
mould_cxx_cflags = $(subst -ffreestanding,,\
		   $(subst -Wstrict-prototypes,,\
		   $(subst -Wno-pointer-sign,,\
		   $(subst -Wdeclaration-after-statement,,\
		   $(subst -Werror-implicit-function-declaration,,\
		   $(subst  -g , -g $(debug_info_cpp) ,\
		   $(subst -DINSERT_EXTRA_CXXFLAGS_HERE,$(EXTRA_CXXFLAGS),\
		   $(1))))))))


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

$(single-used-m): %.o: %.cpp FORCE
	$(call cmd,force_checksrc)
	$(call if_changed_rule,cc_o_cpp)
	@{ echo $(@:.o=.ko); echo $@; } > $(MODVERDIR)/$(@F:.o=.mod)

