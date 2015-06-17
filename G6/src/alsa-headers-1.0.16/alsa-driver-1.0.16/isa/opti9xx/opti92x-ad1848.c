#include "adriver.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#define isapnp_dev pci_dev
#endif
#define snd_opti9xx_fixup_dma2(pdev) \
	{struct isapnp_dev *b = (struct isapnp_dev *)pdev;\
		memset(&b->dma_resource[1].flags, 0, sizeof(b->dma_resource[1].flags));}
#endif

#include "../../alsa-kernel/isa/opti9xx/opti92x-ad1848.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "opti92x-ad1848.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
