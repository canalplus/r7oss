ifeq ($(CONFIG_RELAY),y)
ifeq ($(CONFIG_DEBUG_FS),y)
strelayfs-objs += st_relayfs.o st_relayfs_se.o st_relayfs_te.o st_relayfs_de.o
strelaycore-objs += st_relay_core.o

obj-$(CONFIG_STM_STREAMINGENGINE) += strelaycore.o strelayfs.o
endif
endif
