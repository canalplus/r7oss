# Classes required for SoCs containing a HQVDP display
ifneq ($(CONFIG_HQVDPLITE),)

EXTRA_CFLAGS += -DHQVDPLITE_API_FOR_STAPI
ifneq ($(CONFIG_STM_VIRTUAL_PLATFORM),)
EXTRA_CFLAGS += -DVIRTUAL_PLATFORM_TLM
endif

STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/display/ip/hqvdplite/lld/,	\
							 c8fvp3_FW_lld.c 							\
							 c8fvp3_aligned_viewport.c 					\
							 c8fvp3_FW_download_code.c 					\
							 c8fvp3_FW_plug.c 							\
							 c8fvp3_HVSRC_Lut.c 						\
							 c8fvp3_strRegAccess.c						\
							 check_api.c 								\
							 hqvdplite_dmem.c 							\
							 hqvdplite_pmem.c 							\
							 hqvdp_ucode_256_rd.c 						\
							 hqvdp_ucode_256_wr.c)


endif




