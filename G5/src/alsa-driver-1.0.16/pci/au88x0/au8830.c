#include "adriver.h"
#include "../../alsa-kernel/pci/au88x0/au8830.h"
#include "../../alsa-kernel/pci/au88x0/au88x0.h"
static struct pci_device_id snd_vortex_ids[] = {
	{PCI_VENDOR_ID_AUREAL, PCI_DEVICE_ID_AUREAL_VORTEX_2,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0,},
	{0,}
};

#include "../../alsa-kernel/pci/au88x0/au88x0_synth.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_core.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_pcm.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_mixer.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_mpu401.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_game.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_eq.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_a3d.c"
#include "../../alsa-kernel/pci/au88x0/au88x0_xtalk.c"
#include "au88x0.c"

EXPORT_NO_SYMBOLS;
