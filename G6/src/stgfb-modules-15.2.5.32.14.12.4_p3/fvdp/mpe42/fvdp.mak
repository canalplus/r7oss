# Classes required for SoCs containing a FVDP display
ifeq ($(CONFIG_MPE42),y)

EXTRA_CFLAGS += -DCONFIG_MPE42

EXTRA_CFLAGS += -I$(STG_TOPDIR)/fvdp -I$(STG_TOPDIR)/fvdp/mpe42


STM_SRC_FILES += $(addprefix $(SRC_TOPDIR)/fvdp/mpe42/,                         \
                        fvdp.c                                                  \
                        fvdp_system.c                                           \
                        fvdp_system_vmux.c                                      \
                        fvdp_system_fe.c                                        \
                        fvdp_system_be.c                                        \
                        fvdp_system_lite.c                                      \
                        fvdp_system_enc.c                                       \
                        fvdp_update.c                                           \
                        fvdp_services.c                                         \
                        fvdp_common.c                                           \
                        fvdp_hostupdate.c                                       \
                        fvdp_ivp.c                                              \
                        fvdp_prepost.c                                          \
                        fvdp_router.c                                           \
                        fvdp_colormatrix.c                                      \
                        fvdp_colorfull.c                                        \
                        fvdp_colorlight.c                                       \
                        fvdp_debug.c                                            \
                        fvdp_mcc.c                                              \
                        fvdp_dfr.c                                              \
                        fvdp_datapath.c                                         \
                        fvdp_hflite.c                                           \
                        fvdp_vflite.c                                           \
                        fvdp_hqscaler.c                                         \
                        fvdp_mbox.c                                             \
                        fvdp_fbm_in.c                                           \
                        fvdp_fbm_out.c                                          \
                        fvdp_vcd.c                                              \
                        fvdp_vcpu.c                                             \
                        fvdp_tnr.c                                              \
                        fvdp_madi.c                                             \
                        fvdp_mcdi.c                                             \
                        fvdp_tnrlite.c                                          \
                        fvdp_test.c                                             \
                        fvdp_vcpu_hex.c                                         \
                        fvdp_bcrop.c                                            \
                        fvdp_eventctrl.c                                        \
                        fvdp_filtertab.c                                        \
                        fvdp_sharpness.c                                        \
                        fvdp_system_sharpness.c                                 \
                        fvdp_ccs.c                                              \
                        fvdp_system_ccs.c)
endif
