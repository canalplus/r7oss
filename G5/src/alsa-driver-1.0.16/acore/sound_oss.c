#define __NO_VERSION__
#include "adriver.h"
#ifndef CONFIG_HAVE_REGISTER_SOUND_SPECIAL_DEVICE
#define register_sound_special_device(ops,unit,dev) register_sound_special(ops,unit)
#endif
#include "../alsa-kernel/core/sound_oss.c"
