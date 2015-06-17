#define __NO_VERSION__
#include "adriver.h"

#include "../alsa-kernel/core/pcm_lib.c"

#ifdef CONFIG_SND_BIT32_EMUL_MODULE
int snd_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params);
EXPORT_SYMBOL(snd_pcm_hw_params);
#endif
