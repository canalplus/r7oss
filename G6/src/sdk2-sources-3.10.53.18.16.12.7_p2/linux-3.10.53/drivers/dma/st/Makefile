obj-$(CONFIG_ST_FDMA) += st-fdma.o

st-fdma-y:= st_fdma.o st_fdma_desc.o st_fdma_dreq.o st_fdma_fw.o \
		st_fdma_hw.o st_fdma_xbar.o

st-fdma-$(CONFIG_DEBUG_FS) += st_fdma_debugfs.o
st-fdma-$(CONFIG_ST_FDMA_AUDIO) += st_fdma_audio.o
st-fdma-$(CONFIG_ST_FDMA_TELSS) += st_fdma_telss.o
st-fdma-$(CONFIG_ST_FDMA_MCHI) += st_fdma_mchi.o