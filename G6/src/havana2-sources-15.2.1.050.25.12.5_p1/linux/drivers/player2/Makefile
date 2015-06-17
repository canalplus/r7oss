##infra headers: stm_common.h stm_registry.h stm_data_interface.h stm_event.h stm_memsink.h stm_memsrc.h
ccflags-y += -I$(CONFIG_INFRASTRUCTURE_PATH)/include
##multicom headers: ics.h mme.h
ccflags-y += -I$(CONFIG_MULTICOM_PATH)/source/include
##te header: stm_te_if_tsmux.h
ccflags-y += -I$(CONFIG_TRANSPORT_ENGINE_PATH)/include
##stmfb headers: stm_display.h stm_scaler.h stm_pixel_capture.h
ccflags-y += -I$(DISPLAY_ENGINE)/include
##stmfb private headers: capture_device_priv.h
ccflags-y += -I$(DISPLAY_ENGINE)/private/include
##blitter header: blitter.h
ccflags-y += -I$(CONFIG_BLITTER_PATH)/linux/kernel/include/linux/stm

##audio & video FW headers
ccflags-y += -I$(SDK2_SOURCE_video_apiheaders_ext)
ccflags-y += -I$(SDK2_SOURCE_audio_apiheaders_ext)

##SE linux headers
ccflags-y += -I$(TREE_ROOT)/linux/drivers/allocator
ccflags-y += -I$(TREE_ROOT)/linux/drivers/h264_encode_hard_host_transformer
ccflags-y += -I$(TREE_ROOT)/linux/drivers/h264_preprocessor
ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_preprocessor
ccflags-y += -I$(TREE_ROOT)/linux/drivers/hevc_hard_host_transformer
ccflags-y += -I$(TREE_ROOT)/linux/drivers/ksound
ccflags-y += -I$(TREE_ROOT)/linux/drivers/mmelog
ccflags-y += -I$(TREE_ROOT)/linux/drivers/strelay
ccflags-y += -I$(TREE_ROOT)/linux/drivers/osdev_abs
##SE player headers
ccflags-y += -I$(TREE_ROOT)/player/include
ccflags-y += -I$(TREE_ROOT)/player/infrastructure
ccflags-y += -I$(TREE_ROOT)/player/class_definitions
ccflags-y += -I$(TREE_ROOT)/player/standards
ccflags-y += -I$(TREE_ROOT)/player/buffer
ccflags-y += -I$(TREE_ROOT)/player/wrapper
ccflags-y += -I$(TREE_ROOT)/player/active_edge
ccflags-y += -I$(TREE_ROOT)/player/player
ccflags-y += -I$(TREE_ROOT)/player/player_stream
ccflags-y += -I$(TREE_ROOT)/player/player_playback
ccflags-y += -I$(TREE_ROOT)/player/es_processor
ccflags-y += -I$(TREE_ROOT)/player/decode_buffer_manager
ccflags-y += -I$(TREE_ROOT)/player/collator
ccflags-y += -I$(TREE_ROOT)/player/collator2
ccflags-y += -I$(TREE_ROOT)/player/frame_parser
ccflags-y += -I$(TREE_ROOT)/player/codec
ccflags-y += -I$(TREE_ROOT)/player/audio_reader
ccflags-y += -I$(TREE_ROOT)/player/audio_generator
ccflags-y += -I$(TREE_ROOT)/player/manifestor
ccflags-y += -I$(TREE_ROOT)/player/manifestation_coordinator
ccflags-y += -I$(TREE_ROOT)/player/output_coordinator
ccflags-y += -I$(TREE_ROOT)/player/output_timer
ccflags-y += -I$(TREE_ROOT)/player/encoder
ccflags-y += -I$(TREE_ROOT)/player/encoder/coder
ccflags-y += -I$(TREE_ROOT)/player/encoder/preproc
ccflags-y += -I$(TREE_ROOT)/player/encoder/transporter
ccflags-y += -I$(TREE_ROOT)/player/encode_coordinator

##specific compilation flags
ccflags-y += -DREPORT

ifneq ($(LOWMEMORYBANDWIDTH),)
ccflags-y += -DLOWMEMORYBANDWIDTH
endif


ccflags-y += -Wunused-but-set-variable
ccflags-y += -Wno-missing-field-initializers
ccflags-y += -Wno-unused-parameter
#TODO(pht) bz30520 ccflags-y += -Wsign-conversion
ccflags-y += -Wno-sign-compare

##specific compilation flags C++
EXTRA_CXXFLAGS+= -fno-exceptions -fno-rtti
EXTRA_CXXFLAGS+= -Weffc++
#TODO(pht) bz49202 EXTRA_CXXFLAGS+= -Wshadow
#TODO(pht) bz30697 EXTRA_CXXFLAGS+= -Wold-style-cast

##player2 module

inline-objs := \
			osinline.o \
			aeabi_ldivmod.o

module-objs := \
			player_module.o \
			player_threads.o \
			player_sysfs.o \
			audio_mixer_params.o

infrastructure-objs := \
			fatal_error.o \
			report.o \
			ring_generic.o \
			stack_generic.o \
			message.o \
			allocator_simple.o \
			spanning_startcode.o
##allocator_circular.o

buffer-objs := \
			buffer_individual_generic.o \
			buffer_manager_generic.o \
			buffer_pool_generic.o

wrapper-objs := \
			player_wrapper.o \
			player_factory.o \
			player_policy.o \
			havana_player.o \
			havana_factory.o \
			havana_playback.o \
			havana_display.o \
			havana_stream.o \
			havana_user_data.o \
			audio_conversions.o \
			mixer_pcmproctuning.o \
			timestamps.o \
			havana_capture.o

active_edge-objs := \
			active_edge_base.o \
			collate_to_parse_edge.o \
			parse_to_decode_edge.o \
			decode_to_manifest_edge.o \
			post_manifest_edge.o \

player-objs := \
			player_construct.o \
			player_events.o \
			player_input.o \
			player_generic_playback.o \
			player_process_drain.o \
			player_settings.o \
			player_statistics.o \
			player_time.o \
			player_tostring.o

player_stream-objs := \
			player_stream.o

player_playback-objs := \
			player_playback.o

es_processor-objs := \
			es_processor_base.o \
			es_processor_state.o

decode_buffer_manager-objs := \
			decode_buffer_manager_base.o

collator-objs := \
			collator_common.o \
			collator_base.o \
			collator_video_uncompressed.o \
			collator_pes.o \
			collator_pes_video.o \
			collator_pes_video_mpeg2.o \
			collator_pes_video_h264.o \
			collator_pes_video_vc1.o \
			collator_pes_video_avs.o \
			collator_pes_video_mjpeg.o \
			collator_pes_frame.o \
			collator_pes_video_divx.o \
			collator_pes_audio.o \
			collator_pes_audio_aac.o \
			collator_pes_audio_dvd.o \
			collator_pes_audio_mpeg.o \
			collator_pes_audio_eac3.o \
			collator_pes_audio_dtshd.o \
			collator_pes_audio_lpcm.o \
			collator_pes_audio_adpcm.o \
			collator_pes_audio_wma.o \
			collator_pes_audio_mlp.o \
			collator_pes_audio_dra.o

collator2-objs := \
			collator2_base.o \
			collator2_pes.o \
			collator2_pes_video.o \
			collator2_pes_video_mpeg2.o \
			collator2_pes_video_hevc.o \
			collator2_pes_video_vc1.o \
			collator2_pes_video_h264.o \
			collator2_pes_video_mvc.o

frame_parser-objs := \
			frame_parser_base.o \
			frame_parser_video.o \
			frame_parser_video_mpeg2.o \
			frame_parser_video_h264.o \
			frame_parser_video_hevc.o \
			frame_parser_video_h264_mvc.o \
			frame_parser_video_vc1.o \
			frame_parser_video_vc1_rp227spmp.o \
			frame_parser_video_wmv.o \
			frame_parser_video_divx.o \
			frame_parser_video_divx_hd.o \
			frame_parser_video_uncompressed.o \
			frame_parser_video_h263.o \
			frame_parser_video_flv1.o \
			frame_parser_video_vp6.o \
			frame_parser_video_rmv.o \
			frame_parser_video_theora.o \
			frame_parser_video_avs.o \
			frame_parser_video_mjpeg.o \
			frame_parser_video_vp8.o \
			frame_parser_audio.o \
			frame_parser_audio_aac.o \
			frame_parser_audio_mpeg.o \
			frame_parser_audio_eac3.o \
			frame_parser_audio_dtshd.o \
			frame_parser_audio_lpcm.o \
			frame_parser_audio_adpcm.o \
			asf_guids.o \
			wma_properties.o \
			frame_parser_audio_wma.o \
			frame_parser_audio_mlp.o \
			frame_parser_audio_rma.o \
			frame_parser_audio_vorbis.o \
			frame_parser_audio_pcm.o \
			frame_parser_audio_dra.o

codec-objs := \
			codec_mme_base.o        \
			codec_mme_video.o       \
			codec_mme_video_mpeg2.o \
			codec_mme_video_h264.o  \
			codec_mme_video_hevc.o  \
			codec_mme_video_h264_mvc.o  \
			codec_mme_video_divx_hd.o \
			codec_mme_video_vc1.o   \
			codec_mme_video_h263.o  \
			codec_mme_video_flv1.o  \
			codec_mme_video_vp6.o  \
			codec_mme_video_rmv.o  \
			codec_mme_video_theora.o  \
			codec_mme_video_avs.o  \
			codec_mme_video_mjpeg.o  \
			codec_uncompressed_video.o \
			codec_mme_video_vp8.o  \
			codec_mme_audio.o       \
			codec_mme_audio_stream.o \
			codec_mme_audio_aac.o  \
			codec_mme_audio_mpeg.o  \
			codec_mme_audio_eac3.o  \
			codec_mme_audio_dtshd.o \
			codec_mme_audio_lpcm.o \
			codec_mme_audio_adpcm.o \
			codec_mme_audio_wma.o \
			codec_mme_audio_spdifin.o \
			codec_mme_audio_silence.o \
			codec_mme_audio_mlp.o \
			codec_mme_audio_rma.o \
			codec_mme_audio_vorbis.o \
			codec_mme_audio_pcm.o \
			codec_mme_audio_dra.o \
			codec_capabilities.o

audio_reader-objs := \
			audio_reader.o

audio_generator-objs := \
			audio_generator.o

manifestor-objs := \
			manifestor_base.o \
			manifestor_video.o \
			manifestor_video_stmfb.o \
			manifestor_video_crc.o \
			manifestor_video_grab.o \
			manifestor_video_sourceGrab.o \
			manifestor_video_push_release_grab.o \
			manifestor_audio.o \
			manifestor_audio_ksound.o \
			audio_player.o \
			manifestor_audio_sourceGrab.o \
			manifestor_sourceGrab.o \
			manifestor_encode.o \
			manifestor_encode_video.o \
			manifestor_encode_audio.o \
			metadata_helper.o \
			mixer_mme.o \
			mixer_player.o \
			pcmplayer_ksound.o

manifestation_coordinator-objs := \
			manifestation_coordinator_base.o

output_coordinator-objs := \
			output_coordinator_base.o

output_timer-objs := \
			output_timer_base.o \
			output_timer_video.o \
			output_timer_audio.o

encoder-objs := \
			encode.o \
			encode_stream.o \
			encode_stream_in_sequence.o \
			encoder_process_preprocessor.o \
			encoder_process_coder.o \
			encoder_process_input.o \
			encoder_generic.o

preprocessor-objs := \
			preproc_base.o \
			preproc_video.o \
			preproc_video_blitter.o \
			preproc_video_scaler.o \
			preproc_video_generic.o \
			preproc_audio.o \
			preproc_audio_null.o \
			preproc_audio_mme.o \
			preproc_null.o

coder-objs := \
			coder_base.o \
			coder_video.o \
			coder_video_mme_h264.o \
			coder_audio.o \
			coder_audio_mme.o \
			coder_audio_mme_ddce51.o \
			coder_audio_mme_aace.o \
			coder_audio_mme_mp3e.o \
			coder_null.o \
			coder_capabilities.o

transporter-objs := \
			transporter_base.o \
			transporter_memsrc.o \
			transporter_tsmux.o \
			transporter_null.o

encode_coordinator-objs := \
			encode_coordinator.o \
			encode_coordinator_process.o \
			encode_coordinator_stream.o \
			encode_coordinator_buffer.o

BUILD_SOURCE_PATH := $(shell dirname `readlink -e $(lastword $(MAKEFILE_LIST))`)
PLAYER_VERSION := $(shell cd $(BUILD_SOURCE_PATH) && git describe --long --dirty --tags --always 2>/dev/null)
# We only define PLAYER_VERSION to the source tree if we were successful from git-describe
ifneq ($(PLAYER_VERSION),)
CFLAGS_player_construct.o += -DPLAYER_VERSION="KBUILD_STR($(PLAYER_VERSION))"
CFLAGS_player_module.o += -DPLAYER_VERSION="KBUILD_STR($(PLAYER_VERSION))"
endif
CFLAGS_module.o += -DPLAYER_VERSION="KBUILD_STR($(PLAYER_VERSION))"

SE_ROOT=../../../
player2-objs := $(module-objs) \
			$(addprefix ../osdev_abs/, $(inline-objs)) \
			$(addprefix $(SE_ROOT)/player/infrastructure/, $(infrastructure-objs)) \
			$(addprefix $(SE_ROOT)/player/wrapper/, $(wrapper-objs)) \
			$(addprefix $(SE_ROOT)/player/active_edge/, $(active_edge-objs)) \
			$(addprefix $(SE_ROOT)/player/audio_reader/, $(audio_reader-objs)) \
			$(addprefix $(SE_ROOT)/player/audio_generator/, $(audio_generator-objs)) \
			$(addprefix $(SE_ROOT)/player/collator/, $(collator-objs)) \
			$(addprefix $(SE_ROOT)/player/collator2/, $(collator2-objs)) \
			$(addprefix $(SE_ROOT)/player/frame_parser/, $(frame_parser-objs)) \
			$(addprefix $(SE_ROOT)/player/codec/, $(codec-objs)) \
			$(addprefix $(SE_ROOT)/player/output_coordinator/, $(output_coordinator-objs)) \
			$(addprefix $(SE_ROOT)/player/output_timer/, $(output_timer-objs)) \
			$(addprefix $(SE_ROOT)/player/decode_buffer_manager/, $(decode_buffer_manager-objs)) \
			$(addprefix $(SE_ROOT)/player/manifestation_coordinator/, $(manifestation_coordinator-objs)) \
			$(addprefix $(SE_ROOT)/player/manifestor/, $(manifestor-objs)) \
			$(addprefix $(SE_ROOT)/player/buffer/, $(buffer-objs)) \
			$(addprefix $(SE_ROOT)/player/player/, $(player-objs))  \
			$(addprefix $(SE_ROOT)/player/player_stream/, $(player_stream-objs))  \
			$(addprefix $(SE_ROOT)/player/player_playback/, $(player_playback-objs))  \
			$(addprefix $(SE_ROOT)/player/es_processor/, $(es_processor-objs)) \
			$(addprefix $(SE_ROOT)/player/encode_coordinator/, $(encode_coordinator-objs)) \
			$(addprefix $(SE_ROOT)/player/encoder/, $(encoder-objs)) \
			$(addprefix $(SE_ROOT)/player/encoder/preproc/, $(preprocessor-objs)) \
			$(addprefix $(SE_ROOT)/player/encoder/coder/, $(coder-objs)) \
			$(addprefix $(SE_ROOT)/player/encoder/transporter/, $(transporter-objs))

obj-$(CONFIG_STM_STREAMINGENGINE) += player2.o

##osdev char abstraction: has to be in separated module
osdev_abs-objs := $(addprefix ../osdev_abs/, osdev_user.o osdev_mem.o)
obj-$(CONFIG_STM_STREAMINGENGINE) += osdev_abs.o

# test modules: DEBUFS_FS conditional
ifeq ($(CONFIG_DEBUG_FS),y)
## KTM Control data tests module
ControlData-objs := \
	KTM_ControlData.o\
	KTM_ControlData_dbgfs.o \
	KTM_ControlData_event.o
KTM_ControlData-objs += $(addprefix $(SE_ROOT)/player/collator2/test/, $(ControlData-objs))
obj-$(CONFIG_STM_STREAMINGENGINE_TEST) += KTM_ControlData.o

## encoder stkpi tests module
# Tests will be run in the link order of this list, first and last must remain where they are
# Your tests must be within first and last or they will not run.
encoder_test-objs := \
			module.o \
			common.o \
			video_buffer_traffic320yuv.o \
			compressed_video_buffer.o \
			compressed_audio_buffer.o \
			audio_buffer_lolita_18432smp_48_2ch_s32se.o \
			test_first.o \
			test_001_stream.o \
			test_002_dual_stream.o \
			test_003_repeat_dual_stream.o \
			test_004_test_frame_inject.o \
			test_005_repeat_injection.o \
			test_006_parameter_checks.o \
			test_007_encode_stream_checks.o \
			test_008_memsink_connect.o \
			test_009_memsink_dataflow.o \
			test_010_static_encode_h264.o \
			test_011_set_get_controls.o \
			test_012_static_encode_ddce51.o \
			test_013_set_get_controls_h264.o \
			test_014_set_get_controls_video_preproc.o\
			test_015_frame_encode_event.o\
			test_016_static_encode_aace.o \
			test_017_static_encode_mp3e.o \
			test_018_test_audio_encode_sfc.o \
			test_019_discontinuity_checks.o \
			test_020_eos_discontinuity_check.o \
			test_021_memory_profile.o \
			test_022_fade_out_in_checks.o \
			test_023_multi_video_encode_stream.o \
			test_024_inject_frame_video_preproc.o \
			test_025_inject_frame_h264.o \
			test_026_video_encode_long_run.o \
			test_027_multi_video_encode_long_run.o \
			test_028_video_encode_new_delete_loop.o \
			test_029_video_encode_memory_full.o \
			test_030_multi_video_encode_memory_full.o \
			test_last.o
			# 'test_last.o' MUST be last in this list

encoderstkpi-objs += $(addprefix $(SE_ROOT)/player/encoder/test/, $(encoder_test-objs))
obj-$(CONFIG_STM_STREAMINGENGINE_TEST) += encoderstkpi.o

## encoder stkpi dbgfs tests module
encoderdbgfs-objs := $(SE_ROOT)/player/encoder/test/encoder_stkpi_dbgfs.o
obj-$(CONFIG_STM_STREAMINGENGINE_TEST) += encoderdbgfs.o
endif

## C++ build magic
ifeq ($(CONFIG_DEBUG_INFO_CPP),y)
debug_info_cpp = -O0
endif

ccflags-y += -DINSERT_EXTRA_CXXFLAGS_HERE
mould_cxx_cflags = \
		   $(subst -ffreestanding,,\
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
