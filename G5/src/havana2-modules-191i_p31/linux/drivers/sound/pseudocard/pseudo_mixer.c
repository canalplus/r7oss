/*
 * Pseudo ALSA device (mixer and PCM player) implemented (mostly) software
 * 
 * Copied from sound/drivers/dummy.c by Jaroslav Kysela
 * Copyright (c) 2007 STMicroelectronics R&D Limited <daniel.thompson@st.com>
 * Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sound/driver.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/moduleparam.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <asm/io.h>
#include <asm/cacheflush.h>

#include <ACC_Transformers/acc_mmedefines.h>

#include <stmdisplay.h>

#include "alsa_backend_ops.h"
#include "pseudo_mixer.h"

MODULE_AUTHOR("Daniel Thompson <daniel.thompson@st.com>");
MODULE_DESCRIPTION("Pseudo soundcard");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{ALSA,Pseudo soundcard}}");

#if defined (CONFIG_KERNELVERSION) /* STLinux 2.3 */
#warning Need to remove these typedefs
typedef struct snd_pcm_substream snd_pcm_substream_t;
typedef struct snd_pcm_runtime   snd_pcm_runtime_t;
#endif

#define MAX_PCM_DEVICES		4
#define MAX_PCM_SUBSTREAMS	16
#define MAX_MIDI_DEVICES	2
#define MAX_DYNAMIC_CONTROLS	20

#define MAX_BUFFER_SIZE		(2 * 2 * 3 * 2048) /* 2 channel, 16-bit, 3 2048 sample periods */
#define MAX_PERIOD_SIZE		MAX_BUFFER_SIZE
#define USE_FORMATS 		(SNDRV_PCM_FMTBIT_S16_LE)
#define USE_RATE		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000
#define USE_RATE_MIN		5500
#define USE_RATE_MAX		48000
#define USE_CHANNELS_MIN 	1
#define USE_CHANNELS_MAX 	2
#define USE_PERIODS_MIN 	2
#define USE_PERIODS_MAX 	1024
#define add_playback_constraints(x) 0
#define add_capture_constraints(x) 0

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;
static int pcm_devs[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};
static int pcm_substreams[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 8};
static char *bpa2_partition[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = "bigphysarea"};

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for pseudo soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for pseudo soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable this pseudo soundcard.");
module_param_array(pcm_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_devs, "PCM devices # (0-4) for pseudo driver.");
module_param_array(pcm_substreams, int, NULL, 0444);
MODULE_PARM_DESC(pcm_substreams, "PCM substreams # (1-16) for pseudo driver.");
module_param_array(bpa2_partition, charp, NULL, 0444);
MODULE_PARM_DESC(bpa2_partition, "BPA2 partition ID string from which to allocate memory.");
static struct platform_device *devices[SNDRV_CARDS];

#define _CARD(n, major, minor, freq, chan, f) { \
	.name = n, \
	.alsaname = "hw:" #major "," #minor, \
	.flags = f, \
	.max_freq = freq, \
	.num_channels = chan, \
}

#define CARD(n, major, minor, freq, chan) \
	_CARD(n, major, minor, freq, chan, 0)
#define CARD_SPDIF(n, major, minor, freq, chan) \
	_CARD(n, major, minor, freq, chan, SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING)
#define CARD_FATPIPE(n, major, minor, freq, chan) \
	_CARD(n, major, minor, freq, chan, SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING | \
			                   SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE)
// the following macro indicates the card is connected to a hdmi cell through the spdif player
#define CARD_SPDIF_HDMI(n, major, minor, freq, chan) \
	_CARD(n, major, minor, freq, chan, SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING | \
                               SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING)

// the following macro indicates the card is connected to a hdmi cell through the pcm player
#define CARD_HDMI(n, major, minor, freq, chan) \
	_CARD(n, major, minor, freq, chan, SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING)

#if defined CONFIG_CPU_SUBTYPE_STX7200 && !defined CONFIG_DUAL_DISPLAY
			                   
static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
#if defined (CONFIG_KERNELVERSION)
			CARD_FATPIPE("SPDIF",   0, 5, 192000, 2),
			CARD        ("Analog0", 0, 0, 192000, 2),
			CARD        ("Analog1", 0, 1, 192000, 2),
			CARD_HDMI   ("HDMI",    0, 4, 192000, 2),
#else /* STLinux 2.2 */
			CARD_FATPIPE("SPDIF",   4, 0, 192000, 2),
			CARD        ("Analog0", 0, 0, 192000, 2),
			CARD        ("Analog1", 1, 0, 192000, 2),
			CARD_SPDIF  ("HDMI",    5, 0, 192000, 2),
#endif
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STX7200

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
#if defined (CONFIG_KERNELVERSION)
			CARD        ("Analog0", 0, 0, 192000, 2),
			CARD_SPDIF  ("HDMI",    0, 6, 192000, 2),
#else /* STLinux-2.2 */
			CARD        ("Analog0", 0, 0, 192000, 2),
			CARD_SPDIF  ("HDMI",    5, 0, 192000, 2),
#endif
		},
	},
	{
		{
#if defined (CONFIG_KERNELVERSION)
			CARD_SPDIF  ("SPDIF",   0, 5, 192000, 2),
			CARD        ("Analog1", 0, 1, 192000, 2),
#else /* STLinux 2.2 */
			CARD_SPDIF  ("SPDIF",   4, 0, 192000, 2),
			CARD        ("Analog1", 1, 0, 192000, 2),
#endif			
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STB7100 && !defined CONFIG_DUAL_DISPLAY

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
#if defined (CONFIG_KERNELVERSION)
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("Analog",  0, 1,  48000, 2),
			CARD        ("HDMI",    0, 0,  48000, 2),
#else /* STLinux 2.2 */
			CARD_SPDIF  ("SPDIF",   2, 0,  48000, 2),
			CARD        ("Analog",  1, 0,  48000, 2),
			CARD        ("HDMI",    3, 0,  48000, 2),
#endif
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STB7100

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
#if defined (CONFIG_KERNELVERSION)
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("HDMI",    0, 0,  48000, 2),
#else /* STLinux-2.2 */
			CARD_SPDIF  ("SPDIF",   2, 0,  48000, 2),
			CARD        ("Analog",  0, 0,  48000, 2),
#endif
		},
	},
	{
		{
#if defined (CONFIG_KERNELVERSION)
			CARD        ("Analog",  0, 1,  48000, 2),
#else /* STLinux-2.2 */
			CARD        ("Analog",  1, 0,  48000, 2),
#endif
		},
	},
};


#elif defined CONFIG_CPU_SUBTYPE_STX7141 && !defined CONFIG_DUAL_DISPLAY 

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD        ("Analog0", 0, 0,  48000, 2),
			CARD        ("Analog1", 0, 1,  48000, 2),
			CARD        ("HDMI",    0, 2,  48000, 2),
			CARD_SPDIF  ("SPDIF",   0, 3,  48000, 2),
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STX7105 && !defined CONFIG_DUAL_DISPLAY 

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("Analog",  0, 1,  48000, 2),
			CARD        ("HDMI",    0, 0,  48000, 2),
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STX7108 && !defined CONFIG_DUAL_DISPLAY 
#warning "7108 values haven't been tested"
static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 3,  48000, 2),
//			CARD        ("Analog",  0, 1,  48000, 2),
//			CARD        ("HDMI",    0, 0,  48000, 2),
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STX7106 && !defined CONFIG_DUAL_DISPLAY 

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("Analog",  0, 1,  48000, 2),
			CARD        ("HDMI",    0, 0,  48000, 2),
		},
	},
};


#elif defined CONFIG_CPU_SUBTYPE_STX7111 && !defined CONFIG_DUAL_DISPLAY 

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("Analog",  0, 1,  48000, 2),
			CARD        ("HDMI",    0, 0,  48000, 2),
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STX7141 

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 3,  48000, 2),
			CARD        ("HDMI",    0, 2,  48000, 2),
		},
	},
	{
		{
			CARD        ("Analog0", 0, 0,  48000, 2),
			CARD        ("Analog1", 0, 1,  48000, 2),
		},
	},
};

#elif defined CONFIG_CPU_SUBTYPE_STX7108
#warning "7108 values haven't been tested"
static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("HDMI",    0, 1,  48000, 2),
		},
	},
	{
		{
			CARD        ("Analog0", 0, 0,  48000, 2),
			CARD        ("Analog1", 0, 1,  48000, 2),
		},
	},
};


#elif defined CONFIG_CPU_SUBTYPE_STX7105 || defined CONFIG_CPU_SUBTYPE_STX7111  || defined CONFIG_CPU_SUBTYPE_STX7106

static const struct snd_pseudo_mixer_downstream_topology default_topology[] = {
	{
		{
			CARD_SPDIF  ("SPDIF",   0, 2,  48000, 2),
			CARD        ("HDMI",    0, 0,  48000, 2),
		},
	},
	{
		{
			CARD        ("Analog0", 0, 1,  48000, 2),
		},
	},
};


#else /* CONFIG_CPU_SUBTYPE_STxXXXX */
#error Unsupported CPU subtype/dual display combination
#endif


#if defined (CONFIG_CPU_SUBTYPE_STX7200)

static const struct snd_pseudo_transformer_name default_transformer_name = {
	SND_PSEUDO_TRANSFORMER_NAME_MAGIC,
	"MME_TRANSFORMER_TYPE_AUDIO_MIXER4",	
};

#else

static const struct snd_pseudo_transformer_name default_transformer_name = {
	0
};

#endif


struct snd_pseudo {
	struct snd_card *card;
	struct snd_pcm *pcm;

	struct bpa2_part *allocator;
	
        int room_identifier;
        struct alsa_backend_operations *backend_ops;
        component_handle_t backend_mixer;

	struct mutex mixer_lock;
	struct snd_pseudo_mixer_settings mixer;

	snd_pseudo_mixer_observer_t *mixer_observer;
	void *mixer_observer_ctx;
	
	struct snd_kcontrol *dynamic_controls[MAX_DYNAMIC_CONTROLS];
	
	const struct firmware *downmix_firmware;
};

struct snd_pseudo_pcm {
	spinlock_t lock;
	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_irq_pos;	/* IRQ position */
	unsigned int pcm_buf_pos;	/* position in buffer */
	struct snd_pcm_substream *substream;
        int substream_identifier;
	int backend_is_setup;
};

static int snd_card_pseudo_register_dynamic_controls_locked(struct snd_pseudo *pseudo);

static inline void snd_card_pseudo_pcm_callback(void *p, unsigned int playp)
{
        struct snd_pcm_substream *substream = p;
        struct snd_pcm_runtime *runtime = substream->runtime;
        struct snd_pseudo_pcm *ppcm = runtime->private_data;
        unsigned long flags;
        unsigned int delta;
        
        spin_lock_irqsave(&ppcm->lock, flags);
        
        /* Work out the delta (in bytes) since the callback was last called
         * noting, of course, that the play pointer may have wrapped.
         */
        delta = playp;
        if (delta <= ppcm->pcm_buf_pos)
                delta += ppcm->pcm_size;
        BUG_ON(delta <= ppcm->pcm_buf_pos);
        delta = delta - ppcm->pcm_buf_pos;

        ppcm->pcm_irq_pos += delta;
        ppcm->pcm_buf_pos = playp;

        if (ppcm->pcm_irq_pos >= ppcm->pcm_count) {
                while (ppcm->pcm_irq_pos >= ppcm->pcm_count)
                        ppcm->pcm_irq_pos -= ppcm->pcm_count;
                spin_unlock_irqrestore(&ppcm->lock, flags);
                snd_pcm_period_elapsed(ppcm->substream);
        } else
                spin_unlock_irqrestore(&ppcm->lock, flags);
}

static int snd_card_pseudo_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo_pcm *ppcm = runtime->private_data;
        struct snd_pseudo *pseudo = substream->private_data;
	int err = 0;

	spin_lock(&ppcm->lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
                err = pseudo->backend_ops->mixer_start_substream(
                                pseudo->backend_mixer, ppcm->substream_identifier);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
                err = pseudo->backend_ops->mixer_stop_substream(
                                pseudo->backend_mixer, ppcm->substream_identifier);
		break;

	default:
		err = -EINVAL;
		break;
	}
	spin_unlock(&ppcm->lock);
	return err;
}

static int snd_card_pseudo_pcm_prepare(struct snd_pcm_substream *substream)
{
        struct snd_pcm_runtime *runtime = substream->runtime;
        struct snd_pseudo_pcm *ppcm = runtime->private_data;
        struct snd_pseudo *pseudo = substream->private_data;

	ppcm->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	ppcm->pcm_count = snd_pcm_lib_period_bytes(substream);
	ppcm->pcm_irq_pos = 0;
	ppcm->pcm_buf_pos = 0;
        
        return pseudo->backend_ops->mixer_prepare_substream(
                        pseudo->backend_mixer, ppcm->substream_identifier);
}

static snd_pcm_uframes_t snd_card_pseudo_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo_pcm *ppcm = runtime->private_data;

	return bytes_to_frames(runtime, ppcm->pcm_buf_pos);
}

static struct snd_pcm_hardware snd_card_pseudo_playback =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		USE_FORMATS,
	.rates =		USE_RATE,
	.rate_min =		USE_RATE_MIN,
	.rate_max =		USE_RATE_MAX,
	.channels_min =		USE_CHANNELS_MIN,
	.channels_max =		USE_CHANNELS_MAX,
	.buffer_bytes_max =	MAX_BUFFER_SIZE,
	.period_bytes_min =	64,
	.period_bytes_max =	MAX_BUFFER_SIZE/2,
	.periods_min =		USE_PERIODS_MIN,
	.periods_max =		USE_PERIODS_MAX,
	.fifo_size =		0,
};

static void snd_card_pseudo_runtime_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
}

static void snd_card_pseudo_free_pages(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo *pseudo = substream->private_data;
	
	if (runtime->dma_area)
		iounmap(runtime->dma_area);
	
	if (runtime->dma_addr)
		bpa2_free_pages(pseudo->allocator, runtime->dma_addr);
	
	runtime->dma_buffer_p = NULL;
	runtime->dma_area = NULL;
	runtime->dma_addr = 0;
	runtime->dma_bytes = 0;
}

static int snd_card_pseudo_alloc_pages(
		struct snd_pcm_substream *substream, unsigned int size)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo *pseudo = substream->private_data;
	int num_pages;
	
	num_pages = (size + (PAGE_SIZE-1)) / PAGE_SIZE;
	

	
	runtime->dma_addr = bpa2_alloc_pages(pseudo->allocator,
			                     num_pages, 0, GFP_KERNEL);
	if (!runtime->dma_addr)
		return -ENOMEM;

	runtime->dma_area = ioremap_nocache(runtime->dma_addr, size);
	if (!runtime->dma_area) {
		snd_card_pseudo_free_pages(substream);
		return -ENOMEM;
	}
	
	runtime->dma_buffer_p = NULL;
	runtime->dma_bytes = size;
	
	return 0;
}

static int snd_card_pseudo_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
        struct snd_pcm_runtime *runtime = substream->runtime;
        struct snd_pseudo_pcm *ppcm = runtime->private_data;
        struct snd_pseudo *pseudo = substream->private_data;
        struct alsa_substream_descriptor descriptor = {
                .callback = snd_card_pseudo_pcm_callback,
                .user_data = substream,
        };
        int err;

        /* allocate the hardware buffer and map it appropriately */
        err = snd_card_pseudo_alloc_pages(substream, params_buffer_bytes(hw_params));
        if (err < 0)
                return err;

        /* ensure stale data does not leak to userspace */
        memset(runtime->dma_area, 0, runtime->dma_bytes);
         
        /* populate the runtime-variable portion of the descriptor */       
        descriptor.hw_buffer = runtime->dma_area;
        descriptor.hw_buffer_size = runtime->dma_bytes;
        descriptor.channels = params_channels(hw_params);
        descriptor.sampling_freq = params_rate(hw_params);
        descriptor.bytes_per_sample = snd_pcm_format_width(params_format(hw_params)) / 8;
    
        err = pseudo->backend_ops->mixer_setup_substream(
                        pseudo->backend_mixer, ppcm->substream_identifier,
                        &descriptor);
        if (err < 0) {
        	snd_card_pseudo_free_pages(substream);
        	return err;
        }
	ppcm->backend_is_setup = 1;
        
	return 0;
}

static int snd_card_pseudo_hw_free(struct snd_pcm_substream *substream)
{
        struct snd_pcm_runtime *runtime = substream->runtime;
        struct snd_pseudo_pcm *ppcm = runtime->private_data;
        struct snd_pseudo *pseudo = substream->private_data;
        int err;
        
	if (ppcm->backend_is_setup) {
		/* must bring the backend to a halt before releasing the h/ware buffer */
		err = pseudo->backend_ops->mixer_prepare_substream(
					pseudo->backend_mixer, ppcm->substream_identifier);
		if (err < 0)
			return err;
	}
        
	snd_card_pseudo_free_pages(substream);

	return 0;
}

/*
 * Copied verbaitum from snd_pcm_mmap_data_nopage()
 */
static struct page *snd_card_pseudo_pcm_mmap_data_nopage(struct vm_area_struct *area,
                                             unsigned long address, int *type)
{
        struct snd_pcm_substream *substream = area->vm_private_data;
        struct snd_pcm_runtime *runtime;
        unsigned long offset;
        struct page * page;
        void *vaddr;
        size_t dma_bytes;

        if (substream == NULL)
                return NOPAGE_SIGBUS;
        runtime = substream->runtime;
        offset = area->vm_pgoff << PAGE_SHIFT;
        offset += address - area->vm_start;
        snd_assert((offset % PAGE_SIZE) == 0, return NOPAGE_SIGBUS);
        dma_bytes = PAGE_ALIGN(runtime->dma_bytes);
        if (offset > dma_bytes - PAGE_SIZE)
                return NOPAGE_SIGBUS;
        if (substream->ops->page) {
                page = substream->ops->page(substream, offset);
                if (! page)
                        return NOPAGE_OOM; /* XXX: is this really due to OOM? */
        } else {
                vaddr = runtime->dma_area + offset;
                page = virt_to_page(vaddr);
        }
        get_page(page);
        if (type)
                *type = VM_FAULT_MINOR;
        return page;
}

static struct vm_operations_struct snd_card_pseudo_pcm_vm_ops_data =
{
        .open =         snd_pcm_mmap_data_open,
        .close =        snd_pcm_mmap_data_close,
        .nopage =       snd_card_pseudo_pcm_mmap_data_nopage,
};

/*
 * Copied from snd_default_pcm_mmap() and modified to ensure the mapping
 * is not cached.
 */
static int snd_card_pseudo_pcm_mmap(struct snd_pcm_substream *substream,
                                    struct vm_area_struct *area)
{
        area->vm_page_prot = pgprot_noncached(area->vm_page_prot);
        area->vm_ops = &snd_card_pseudo_pcm_vm_ops_data;
        area->vm_private_data = substream;
        area->vm_flags |= VM_RESERVED;
#if defined (CONFIG_KERNELVERSION)
        atomic_inc(&substream->mmap_count);
#else /* STLinux 2.2 kernel */
        atomic_inc(&substream->runtime->mmap_count);
#endif
        return 0;
}
        
static struct snd_pseudo_pcm *new_pcm_stream(struct snd_pcm_substream *substream)
{
	struct snd_pseudo_pcm *ppcm;

	ppcm = kzalloc(sizeof(*ppcm), GFP_KERNEL);
	if (! ppcm)
		return ppcm;
	spin_lock_init(&ppcm->lock);
	ppcm->substream = substream;
        ppcm->substream_identifier = -1; /* invalid */
	/*ppcm->backend_is_setup = 0;*/ /* kzalloc... */
	return ppcm;
}

static int snd_card_pseudo_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
        struct snd_pseudo *pseudo = substream->private_data;
	struct snd_pseudo_pcm *ppcm;
	int err;
	sigset_t allset, oldset;
	
	if ((ppcm = new_pcm_stream(substream)) == NULL)
		return -ENOMEM;
	runtime->private_data = ppcm;
	/* settings private_free makes the infrastructure responsible for freeing ppcm */
	runtime->private_free = snd_card_pseudo_runtime_free;
	runtime->hw = snd_card_pseudo_playback;
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2)
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP|SNDRV_PCM_INFO_MMAP_VALID);
	if ((err = add_playback_constraints(runtime)) < 0)
		return err;

	sigfillset(&allset);
	sigprocmask(SIG_BLOCK, &allset, &oldset);
        err = pseudo->backend_ops->mixer_alloc_substream(
                        pseudo->backend_mixer, &ppcm->substream_identifier);
        sigprocmask(SIG_SETMASK, &oldset, NULL);
        if (err < 0)
                return err;

	return 0;
}

static int snd_card_pseudo_playback_close(struct snd_pcm_substream *substream)
{
        struct snd_pcm_runtime *runtime = substream->runtime;
        struct snd_pseudo_pcm *ppcm = runtime->private_data;
        struct snd_pseudo *pseudo = substream->private_data;
	int err;
        
        BUG_ON(-1 == ppcm->substream_identifier);

        err = pseudo->backend_ops->mixer_free_substream(
                        pseudo->backend_mixer, ppcm->substream_identifier);
	if (err < 0)
		return err;

	ppcm->backend_is_setup = 0;

	return 0;
}

static struct snd_pcm_ops snd_card_pseudo_playback_ops = {
	.open =			snd_card_pseudo_playback_open,
	.close =		snd_card_pseudo_playback_close,
	.ioctl =		snd_pcm_lib_ioctl,
	.hw_params =		snd_card_pseudo_hw_params,
	.hw_free =		snd_card_pseudo_hw_free,
	.prepare =		snd_card_pseudo_pcm_prepare,
	.trigger =		snd_card_pseudo_pcm_trigger,
	.pointer =		snd_card_pseudo_pcm_pointer,
        .mmap =                 snd_card_pseudo_pcm_mmap,
};

static int __init snd_card_pseudo_pcm(struct snd_pseudo *pseudo, int device, int substreams)
{
	struct snd_pcm *pcm;
	int err;

	if ((err = snd_pcm_new(pseudo->card, "Pseudo PCM", device,
			       substreams, 0, &pcm)) < 0)
		return err;
	pseudo->pcm = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_card_pseudo_playback_ops);
	pcm->private_data = pseudo;
	pcm->info_flags = 0;
	strcpy(pcm->name, "Pseudo PCM");

	return 0;
}

static void snd_pseudo_mixer_update(struct snd_pseudo *pseudo)
{
        int err;
        
        err = pseudo->backend_ops->mixer_set_module_parameters(
                        pseudo->backend_mixer, &pseudo->mixer, sizeof(pseudo->mixer));
        if (0 != err)
                printk(KERN_ERR "%s: Could not update mixer parameters\n", pseudo->card->shortname);

        /* lock prevents the observer from being deregistered whilst we update the observer */
	BUG_ON(!mutex_is_locked(&pseudo->mixer_lock));
        if (pseudo->mixer_observer)
        	pseudo->mixer_observer(pseudo->mixer_observer_ctx, &pseudo->mixer);
}

#define PSEUDO_ADDR(x) (offsetof(struct snd_pseudo_mixer_settings, x))

#define PSEUDO_INTEGER(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_integer_info, \
  .get = snd_pseudo_integer_get, .put = snd_pseudo_integer_put, \
  .private_value = PSEUDO_ADDR(addr) }

static int snd_pseudo_integer_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	int addr = kcontrol->private_value;
	
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
        
        switch (addr) {
        /* multiple channel, no gain */
        case PSEUDO_ADDR(secondary_pan):
        case PSEUDO_ADDR(interactive_pan[0]):
        case PSEUDO_ADDR(interactive_pan[1]):
        case PSEUDO_ADDR(interactive_pan[2]):
        case PSEUDO_ADDR(interactive_pan[3]):
        case PSEUDO_ADDR(interactive_pan[4]):
        case PSEUDO_ADDR(interactive_pan[5]):
        case PSEUDO_ADDR(interactive_pan[6]):
        case PSEUDO_ADDR(interactive_pan[7]):
                uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
                uinfo->value.integer.min = Q3_13_MIN;
                uinfo->value.integer.max = Q3_13_UNITY;
                break;
        
        /* single channel, no gain */        
        case PSEUDO_ADDR(interactive_gain[0]):
        case PSEUDO_ADDR(interactive_gain[1]):
        case PSEUDO_ADDR(interactive_gain[2]):
        case PSEUDO_ADDR(interactive_gain[3]):
        case PSEUDO_ADDR(interactive_gain[4]):
        case PSEUDO_ADDR(interactive_gain[5]):
        case PSEUDO_ADDR(interactive_gain[6]):
        case PSEUDO_ADDR(interactive_gain[7]):
                uinfo->count = 1;
                uinfo->value.integer.min = Q3_13_MIN;
                uinfo->value.integer.max = Q3_13_UNITY;
                break;

        /* multiple channel, with x8 gain */
        case PSEUDO_ADDR(primary_gain):
        case PSEUDO_ADDR(secondary_gain):
                uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
                uinfo->value.integer.min = Q3_13_MIN;
                uinfo->value.integer.max = Q3_13_MAX;
                break;
         
        /* single channel, with x8 gain */
        case PSEUDO_ADDR(post_mix_gain):
                uinfo->count = 1;
                uinfo->value.integer.min = Q3_13_MIN;
                uinfo->value.integer.max = Q3_13_MAX;
                break;
                
        /* multiple channel, logarithmic, no gain */
        case PSEUDO_ADDR(master_volume):
            	uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
        	uinfo->value.integer.min = -70;
        	uinfo->value.integer.max = 0;
        	break;

	case PSEUDO_ADDR(drc_type):
            	uinfo->count = 1;
        	uinfo->value.integer.min = SND_PSEUDO_MIXER_DRC_CUSTOM0;
        	uinfo->value.integer.max = SND_PSEUDO_MIXER_DRC_RF;
        	break;

	case PSEUDO_ADDR(hdr):
	case PSEUDO_ADDR(ldr):
            	uinfo->count = 1;
        	uinfo->value.integer.min = Q0_8_MIN;
        	uinfo->value.integer.max = Q0_8_MAX;
        	break;

         /* single channels, up to 30ms delay */
	case PSEUDO_ADDR(chain_delay[0]):
        case PSEUDO_ADDR(chain_delay[1]):
        case PSEUDO_ADDR(chain_delay[2]):
        case PSEUDO_ADDR(chain_delay[3]):
               	uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
        	uinfo->value.integer.min = 0;
        	uinfo->value.integer.max = 30000; //usec
        	break;

        /* single channels, logarithmic, 31db gain */
        case PSEUDO_ADDR(chain_volume[0]):
        case PSEUDO_ADDR(chain_volume[1]):
        case PSEUDO_ADDR(chain_volume[2]):
        case PSEUDO_ADDR(chain_volume[3]):
        	uinfo->count = 1;
    		uinfo->value.integer.min = -31;
    		uinfo->value.integer.max = 31;
    		break;
    		
    	/* master latency control, +-150ms */
        case PSEUDO_ADDR(master_latency):
        	uinfo->count = 1;
        	uinfo->value.integer.min = -150;
        	uinfo->value.integer.max = 150;
        	break;
        	
        /* per-output latency control, +150ms */
        case PSEUDO_ADDR(chain_latency[0]):
        case PSEUDO_ADDR(chain_latency[1]):
        case PSEUDO_ADDR(chain_latency[2]):
        case PSEUDO_ADDR(chain_latency[3]):
        	uinfo->count = 1;
        	uinfo->value.integer.min = 0;
        	uinfo->value.integer.max = 150;
        	break;

        default:
                BUG();
                return -EINVAL;
        }      
	
	return 0;
}

/* re-map channels between ALSA and the audio firmware (swap 2,3 with 4,5) */
static unsigned int remap_channels[SND_PSEUDO_MIXER_CHANNELS] = {
	ACC_MAIN_LEFT,
	ACC_MAIN_RGHT,
	ACC_MAIN_LSUR,
	ACC_MAIN_RSUR,
	ACC_MAIN_CNTR,
	ACC_MAIN_LFE,
	ACC_MAIN_CSURL,
	ACC_MAIN_CSURR,
};

static int snd_pseudo_integer_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
        int addr = kcontrol->private_value;
        unsigned char *cp = ((char *) &pseudo->mixer) + addr;
        int *volumesp = (int *) cp;
        struct snd_ctl_elem_info uinfo = { { 0 } };
        int res, i;

        /* use the switched info function to find the number of channels and the max value */
        res = snd_pseudo_integer_info(kcontrol, &uinfo);
        if (res < 0)
                return res;

	mutex_lock(&pseudo->mixer_lock);
	for (i=0; i<uinfo.count; i++)
                if (uinfo.count == SND_PSEUDO_MIXER_CHANNELS)
                	ucontrol->value.integer.value[i] = volumesp[remap_channels[i]];
                else
                        ucontrol->value.integer.value[i] = volumesp[i];
	mutex_unlock(&pseudo->mixer_lock);
	return 0;
}

static int snd_pseudo_integer_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
        int addr = kcontrol->private_value;
        unsigned char *cp = ((char *) &pseudo->mixer) + addr;
        int *volumesp = (int *) cp;
        struct snd_ctl_elem_info uinfo = { { 0 } };
        int update[SND_PSEUDO_MIXER_CHANNELS];
        int res, changed, i, j;
        
        /* use the switched info function to find the number of channels and the max value */
        res = snd_pseudo_integer_info(kcontrol, &uinfo);
        if (res < 0)
                return res;
	
	for (i=0; i<uinfo.count; i++) {
		update[i] = ucontrol->value.integer.value[i];
		if (update[i] < uinfo.value.integer.min)
			update[i] = uinfo.value.integer.min;
		if (update[i] > uinfo.value.integer.max)
			update[i] = uinfo.value.integer.max;
	}

	changed = 0;	
	mutex_lock(&pseudo->mixer_lock);
	for (i=0; i<uinfo.count; i++) {
                j = (uinfo.count == SND_PSEUDO_MIXER_CHANNELS ? remap_channels[i] : i);
		changed = changed || (volumesp[j] != update[i]);
		volumesp[j] = update[i];
	}
        if (changed)
                snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

	return changed;
}

#define PSEUDO_SWITCH(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_switch_info, \
  .get = snd_pseudo_switch_get, .put = snd_pseudo_switch_put, \
  .private_value = PSEUDO_ADDR(addr) }

static int snd_pseudo_switch_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	
	return 0;
}

static int snd_pseudo_switch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
        char *switchp = ((char *) &pseudo->mixer) + addr;

	/* no spinlock (single bit cannot be incoherent) */
	ucontrol->value.integer.value[0] = (*switchp != 0);
	return 0;
}

static int snd_pseudo_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
        int addr = kcontrol->private_value;
        char *switchp = ((char *) &pseudo->mixer) + addr;
        int changed;
	
	mutex_lock(&pseudo->mixer_lock);
	changed = (*switchp == 0) != (ucontrol->value.integer.value[0] == 0);
	*switchp = (ucontrol->value.integer.value[0] != 0);
        if (changed)
                snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

	return changed;
}

#define PSEUDO_ROUTE(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_route_info, \
  .get = snd_pseudo_route_get, .put = snd_pseudo_route_put, \
  .private_value = PSEUDO_ADDR(addr) }

/* Take care with this info function. Unusually it is called from
 * snd_pseudo_route_put to determine safe bounds for enumerations.
 */
static int snd_pseudo_route_info(struct snd_kcontrol *kcontrol,
                                 struct snd_ctl_elem_info *uinfo)
{
	static char *metadata_update[] = {
		"Never", "Primary and secondary only", "Always"
	};
	
	static char *emergency_mute[] = {
		"Source-only", "Pre-mix", "Post-mix", "No-mute"
	};
	
	static char *virtualization_mode[] = {
		"Off", "ST-OmniSurround", "SRS-TruSurround-XT"
	};
	
	static char *spacialization_mode[] = {
		"Off", "ST-WideSurround"
	};

	/* ordering must be identical to enum snd_pseudo_mixer_spdif_encoding */
	static char *spdif_encoding[] = {
		"PCM", "AC3", "DTS", "FatPipe"
	};

	static char *hdmi_encoding[] = {
		"PCM", "AC3", "DTS", "FatPipe"
	};

	/* ordering must be identical to enum snd_pseudo_mixer_interactive_audio_mode */
	static char *interactive_audio_mode[] = {
		"3/4.0", "3/2.0", "2/0.0"
	};

	static char *drc_type[] = {
		"Custom0", "Custom1", "Line Out", "RF Out"
	};
	
	char **texts;
	int num_texts;

#define C(x) case PSEUDO_ADDR(x):do { texts = (x); num_texts = sizeof((x)) / sizeof(*(x)); } while(0); break
	switch (kcontrol->private_value) {
	C(metadata_update);
	C(emergency_mute);
	C(virtualization_mode);
	C(spacialization_mode);
	C(spdif_encoding);
	C(hdmi_encoding);
	C(interactive_audio_mode);
	C(drc_type);
	default:
		BUG();
		return 0;
	}
#undef C
	
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
        uinfo->count = 1;
        uinfo->value.enumerated.items = num_texts;
        if (uinfo->value.enumerated.item > (num_texts-1))
                uinfo->value.enumerated.item = (num_texts-1);
        strcpy(uinfo->value.enumerated.name,
               texts[uinfo->value.enumerated.item]);

	return 0;
}

static int snd_pseudo_route_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
        char *routep = ((char *) &pseudo->mixer) + addr;

	/* no spinlock (single address cannot be incoherent) */
	ucontrol->value.integer.value[0] = *routep;
	return 0;
}

static int snd_pseudo_route_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
        int addr = kcontrol->private_value;
        char *routep = ((char *) &pseudo->mixer) + addr;
	struct snd_ctl_elem_info uinfo = { { 0 } };
	int update, changed;
	
	/* use the switched info function to find the bounds of the enumeration */
	(void) snd_pseudo_route_info(kcontrol, &uinfo);

	update = ucontrol->value.integer.value[0];
	if (update < 0)
		update = 0;
	if (update > (uinfo.value.enumerated.items-1))
		update = (uinfo.value.enumerated.items-1);
	
	mutex_lock(&pseudo->mixer_lock);
	changed = (*routep != update);
	*routep = update;
        if (changed)
                snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

	return changed;
}

#define PSEUDO_BLOB(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_blob_info, \
  .get = snd_pseudo_blob_get, .put = snd_pseudo_blob_put, \
  .private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_BLOB_READONLY(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_blob_info, \
  .get = snd_pseudo_blob_get, \
  .private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_IEC958(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_iec958_info, \
  .get = snd_pseudo_blob_get, .put = snd_pseudo_blob_put, \
  .private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_IEC958_READONLY(xname, xindex, addr) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
  .info = snd_pseudo_iec958_info, \
  .get = snd_pseudo_blob_get, \
  .private_value = PSEUDO_ADDR(addr) }

static int snd_pseudo_blob_size(unsigned long private_value)
{
	struct snd_pseudo_mixer_settings *mixer = 0;
	
	switch(private_value) {
#define C(x) case PSEUDO_ADDR(x): return sizeof(mixer->x)
	C(iec958_metadata);
	C(iec958_mask);
	C(fatpipe_metadata);
	C(fatpipe_mask);
	C(downstream_topology);
#undef C
	}
	
	BUG();
	return 0;
}

static int snd_pseudo_blob_info(struct snd_kcontrol *kcontrol,
                                 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;	
        uinfo->count = snd_pseudo_blob_size(kcontrol->private_value);

	return 0;
}

static int snd_pseudo_iec958_info(struct snd_kcontrol *kcontrol,
                                 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	
	return 0;
}


static int snd_pseudo_blob_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
	char *blobp = ((char *) &pseudo->mixer) + addr;
	char *controlp = ucontrol->value.bytes.data;
	size_t sz = snd_pseudo_blob_size(kcontrol->private_value);
	
	mutex_lock(&pseudo->mixer_lock);
	memcpy(controlp, blobp, sz);
	mutex_unlock(&pseudo->mixer_lock);
	
	return 0;
}

static int snd_pseudo_blob_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
	char *blobp = ((char *) &pseudo->mixer) + addr;
	char *controlp = ucontrol->value.bytes.data;
	size_t sz = snd_pseudo_blob_size(kcontrol->private_value);
	int changed = 0;
	
	mutex_lock(&pseudo->mixer_lock);
	if (0 != memcmp(blobp, controlp, sz)) {
		memcpy(blobp, controlp, sz);
		changed = 1;
	}
	if (changed) {
		snd_pseudo_mixer_update(pseudo);
		
		/* special case update if we have updated the downstream topology */
		if (addr == PSEUDO_ADDR(downstream_topology)) {
			int err;
			struct snd_card *card = pseudo->card;

			/* the results of this function are already decided so at this point
			 * it is safe to drop the controls_rwsem lock and update
			 * the controls (we cannot called snd_ctl_remove/snd_ctl_new1 with
			 * the controls_rwsem held or we will deadlock).
			 */
			up_read(&card->controls_rwsem);
			
			if ((err = snd_card_pseudo_register_dynamic_controls_locked(pseudo)) < 0)
				printk(KERN_ERR "%s: Failed to update dynamic controls (%d)\n",
					       card->shortname, err);

			down_read(&card->controls_rwsem);
		}
	}
	mutex_unlock(&pseudo->mixer_lock);


	return changed;
}

static struct snd_kcontrol_new snd_pseudo_controls[] = {
/* pre-mix gain control */
PSEUDO_INTEGER("Post Mix Playback Volume", 0, post_mix_gain),
PSEUDO_INTEGER("Primary Playback Volume", 0, primary_gain),
PSEUDO_INTEGER("Secondary Playback Volume", 0, secondary_gain),
PSEUDO_INTEGER("Secondary Pan Playback Volume", 0, secondary_pan),
PSEUDO_INTEGER("PCM0 Playback Volume", 0, interactive_gain[0]),  
PSEUDO_INTEGER("PCM0 Pan Playback Volume", 0, interactive_pan[0]),
PSEUDO_INTEGER("PCM1 Playback Volume", 0, interactive_gain[1]),  
PSEUDO_INTEGER("PCM1 Pan Playback Volume", 0, interactive_pan[1]),
PSEUDO_INTEGER("PCM2 Playback Volume", 0, interactive_gain[2]),  
PSEUDO_INTEGER("PCM2 Pan Playback Volume", 0, interactive_pan[2]),
PSEUDO_INTEGER("PCM3 Playback Volume", 0, interactive_gain[3]),  
PSEUDO_INTEGER("PCM3 Pan Playback Volume", 0, interactive_pan[3]),
PSEUDO_INTEGER("PCM4 Playback Volume", 0, interactive_gain[4]),  
PSEUDO_INTEGER("PCM4 Pan Playback Volume", 0, interactive_pan[4]),
PSEUDO_INTEGER("PCM5 Playback Volume", 0, interactive_gain[5]),  
PSEUDO_INTEGER("PCM5 Pan Playback Volume", 0, interactive_pan[5]),
PSEUDO_INTEGER("PCM6 Playback Volume", 0, interactive_gain[6]),  
PSEUDO_INTEGER("PCM6 Pan Playback Volume", 0, interactive_pan[6]),
PSEUDO_INTEGER("PCM7 Playback Volume", 0, interactive_gain[7]),  
PSEUDO_INTEGER("PCM7 Pan Playback Volume", 0, interactive_pan[7]),
PSEUDO_ROUTE("Metadata Update Playback Route", 0, metadata_update),

/* post-mix gain control */
PSEUDO_INTEGER("Master Playback Volume", 0, master_volume),

/* emergency mute */
PSEUDO_ROUTE("Emergency Mute Playback Route", 0, emergency_mute),

/* switches */
PSEUDO_SWITCH("DC Remove Playback Switch", 0, dc_remove_enable),
PSEUDO_SWITCH("Master Volume Bypass Playback Switch", 0, bass_mgt_bypass),
PSEUDO_SWITCH("Fixed Frequency Playback Switch", 0, fixed_output_frequency),
PSEUDO_SWITCH("All Speaker Stereo Playback Switch", 0, all_speaker_stereo_enable),
PSEUDO_SWITCH("Downmix Promotion Playback Switch", 0, downmix_promotion_enable),

/* routes */
PSEUDO_ROUTE("SPDIF Encoding Playback Route", 0, spdif_encoding),
PSEUDO_ROUTE("HDMI Encoding Playback Route", 0, hdmi_encoding),
PSEUDO_SWITCH("SPDIF Bypass Playback Switch", 0, spdif_bypass),
PSEUDO_SWITCH("HDMI Bypass Playback Switch", 0, hdmi_bypass),
PSEUDO_ROUTE("Interactive Audio Mode Playback Route", 0, interactive_audio_mode),
PSEUDO_ROUTE("Virtualization Playback Route", 0, virtualization_mode),
#if 0
/* this has been (temporarily?) booted out of the spec until it is proven it is required */
PSEUDO_ROUTE("Spacialization Playback Route", 0, spacialization_mode),
#endif

/* latency tuning */
PSEUDO_INTEGER("Master Playback Latency", 0, master_latency),
/* drc settings */
PSEUDO_SWITCH("DRC Enable", 0, drc_enable),
PSEUDO_ROUTE("DRC Type  ", 0, drc_type),
PSEUDO_INTEGER("Boost factor", 0, hdr),
PSEUDO_INTEGER("Cut   factor", 0, ldr),

/* generic spdif meta data */
PSEUDO_IEC958("IEC958 Playback Default", 0, iec958_metadata),
PSEUDO_IEC958_READONLY("IEC958 Playback Mask", 0, iec958_mask),

/* fatpipe meta data */
PSEUDO_BLOB("FatPipe Playback Default", 0, fatpipe_metadata),
PSEUDO_BLOB_READONLY("FatPipe Playback Mask", 0, fatpipe_mask),

/* topological structure */
PSEUDO_BLOB("Downstream Topology Playback Default", 0, downstream_topology),
};

static struct snd_kcontrol_new snd_pseudo_dynamic_controls[] = {
PSEUDO_INTEGER("Playback Volume", 0, chain_volume[0]),
PSEUDO_INTEGER("Playback Volume", 0, chain_volume[1]),
PSEUDO_INTEGER("Playback Volume", 0, chain_volume[2]),
PSEUDO_INTEGER("Playback Volume", 0, chain_volume[3]),
PSEUDO_SWITCH("Playback Switch", 0, chain_enable[0]),
PSEUDO_SWITCH("Playback Switch", 0, chain_enable[1]),
PSEUDO_SWITCH("Playback Switch", 0, chain_enable[2]),
PSEUDO_SWITCH("Playback Switch", 0, chain_enable[3]),
PSEUDO_SWITCH("Limiter Playback Switch", 0, chain_limiter_enable[0]),
PSEUDO_SWITCH("Limiter Playback Switch", 0, chain_limiter_enable[1]),
PSEUDO_SWITCH("Limiter Playback Switch", 0, chain_limiter_enable[2]),
PSEUDO_SWITCH("Limiter Playback Switch", 0, chain_limiter_enable[3]),
PSEUDO_INTEGER("Playback Latency", 0, chain_latency[0]),
PSEUDO_INTEGER("Playback Latency", 0, chain_latency[1]),
PSEUDO_INTEGER("Playback Latency", 0, chain_latency[2]),
PSEUDO_INTEGER("Playback Latency", 0, chain_latency[3]),
PSEUDO_INTEGER("Per-Speaker Playback Latency", 0, chain_delay[0]),
PSEUDO_INTEGER("Per-Speaker Playback Latency", 0, chain_delay[1]),
PSEUDO_INTEGER("Per-Speaker Playback Latency", 0, chain_delay[2]),
PSEUDO_INTEGER("Per-Speaker Playback Latency", 0, chain_delay[3]),
};

static int __init snd_card_pseudo_new_mixer(struct snd_pseudo *pseudo)
{
	struct snd_card *card = pseudo->card;
	unsigned int idx;
	int err;

	snd_assert(pseudo != NULL, return -EINVAL);
	mutex_init(&pseudo->mixer_lock);
	strcpy(card->mixername, "Pseudo Mixer");

	for (idx = 0; idx < ARRAY_SIZE(snd_pseudo_controls); idx++) {
		if ((err = snd_ctl_add(card, snd_ctl_new1(&snd_pseudo_controls[idx], pseudo))) < 0)
			return err;
	}
	return 0;
}

/**
 * Provide default values for all the mixer controls.
 */
static void __init snd_pseudo_mixer_init(struct snd_pseudo *pseudo, int dev)
{
	struct snd_pseudo_mixer_settings *mixer = &pseudo->mixer;
	int i, j;
	
        /* aggressively zero the structure */
	memset(mixer, 0, sizeof(*mixer));

        mixer->magic = SND_PSEUDO_MIXER_MAGIC;

        /* pre-mix gain control */
        mixer->post_mix_gain = Q3_13_UNITY;
        for (i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++) {
                mixer->primary_gain[i] = Q3_13_UNITY;
                mixer->secondary_gain[i] = Q3_13_UNITY;
                mixer->secondary_pan[i] = Q3_13_UNITY;
        }
        for (i=0; i<SND_PSEUDO_MIXER_INTERACTIVE; i++) {
                mixer->interactive_gain[i] = Q3_13_UNITY;
                for (j=0; j<SND_PSEUDO_MIXER_CHANNELS; j++)
                        mixer->interactive_pan[i][j] = Q3_13_UNITY;
        }
        mixer->metadata_update = SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER;

        /* post-mix gain control */      
        for (i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++)
                mixer->master_volume[i] = 0;
        for (i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++) {
        	mixer->chain_volume[i] = 0;
        	mixer->chain_enable[i] = 1;
        	mixer->chain_limiter_enable[i] = 1;
        }
        for (i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++)
            for (j=0; j<SND_PSEUDO_MIXER_CHANNELS; j++)
        	mixer->chain_delay[i][j] = 0;
        
        /* emergency mute */
        mixer->emergency_mute = SND_PSEUDO_MIXER_EMERGENCY_MUTE_NO_MUTE;
        
        /* switches */
        mixer->dc_remove_enable = 0; /* Off */
	mixer->bass_mgt_bypass = 0; /* Off */
	mixer->fixed_output_frequency = 0; /* Off */
	mixer->all_speaker_stereo_enable = 0; /* Off */
	mixer->downmix_promotion_enable = 0; /* Off */

	/* routes */
	mixer->spdif_encoding = SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM;
	mixer->hdmi_encoding = SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM;
        mixer->spdif_bypass = 0; /* Off */
        mixer->hdmi_bypass = 0; /* Off */
	mixer->interactive_audio_mode = SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4;
        mixer->virtualization_mode = 0; /* Off */
        mixer->spacialization_mode = 0; /* Off */

	/* drc */
	mixer->drc_enable = 0;
	mixer->drc_type   = SND_PSEUDO_MIXER_DRC_LINE_OUT;
        mixer->hdr        = Q0_8_MAX; /* Off */
        mixer->ldr        = Q0_8_MAX; /* Off */
	
        /* latency tuning */
        mixer->master_latency = 0;
        for (i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++) {
        	mixer->chain_latency[i] = 0;
        }
        
        /* generic spdif meta data */
        /* rely on memset */
        
        /* generic spdif mask */
        memset(&mixer->iec958_mask, 0xff, sizeof(mixer->iec958_mask));
        
        /* fatpipe meta data (see FatPipe 1.1 spec, section 6.0 for interpretation) */
        mixer->fatpipe_metadata.md[0] = 0x40;
        
        /* fatpipe mask (see FatPipe 1.1 spec, section 6.0 for interpretation) */ 
        mixer->fatpipe_mask.md[0] = 0x70;
        mixer->fatpipe_mask.md[2] = 0x1f;
        mixer->fatpipe_mask.md[6] = 0x1f;
	mixer->fatpipe_mask.md[14] = 0xffff;
        
        /* topological structure */
        mixer->downstream_topology = default_topology[dev];
        
        mixer->display_device_id = 0;
        mixer->display_output_id = -1;

        /* search for a valid hdmi output in the default display device 0 */
        {
            stm_display_device_t * pDev = stm_display_get_device(mixer->display_device_id);
            if (pDev == NULL)
            {
                printk(KERN_ERR "Cannot get handle to display device %d \n", mixer->display_device_id);
                mixer->display_device_id = -1;
            }
            else
            {
                int i = 0;
                stm_display_output_t *out;
                
                while ( (out = stm_display_get_output(pDev, i++) ) != 0)
                {
                    ULONG caps;
                    stm_display_output_get_capabilities(out, &caps);
                    if ((caps & STM_OUTPUT_CAPS_TMDS) != 0)
                    {
                        mixer->display_output_id = (i-1);
                    }
                    stm_display_output_release(out);
                }
            }
        }
}

static int snd_card_pseudo_register_dynamic_controls_locked(struct snd_pseudo *pseudo)
{
	struct snd_card *card = pseudo->card;
	int idx, jdx;
	int changed = 0;

	/* ensure the initialization table is not too large */
	BUG_ON(ARRAY_SIZE(snd_pseudo_dynamic_controls) > ARRAY_SIZE(pseudo->dynamic_controls));
	BUG_ON(!mutex_is_locked(&pseudo->mixer_lock));
	
	/* deregister all existing dynamic controls */
	for (idx = 0; idx < ARRAY_SIZE(pseudo->dynamic_controls); idx++)
		if (pseudo->dynamic_controls[idx]) {
			(void) snd_ctl_remove(card, pseudo->dynamic_controls[idx]);
			pseudo->dynamic_controls[idx] = NULL;
		}
	
	/* force a mute on any disabled output (when it is re-enabled we probably want to
	 * change its gain before kicking it into life)
	 */
	for (idx = 0; idx < SND_PSEUDO_MAX_OUTPUTS; idx++)
		if (pseudo->mixer.downstream_topology.card[idx].name[0] == '\0') {
			for (jdx = idx; jdx < SND_PSEUDO_MAX_OUTPUTS; jdx++)
				if (pseudo->mixer.chain_enable[jdx]) {
					pseudo->mixer.chain_enable[jdx] = 0;
					changed = 1;
				}
			break;
		}
	if (changed)
		snd_pseudo_mixer_update(pseudo);
	
	/* register (and appropriately name) the dynamic controls */
	for (idx = 0; idx < ARRAY_SIZE(snd_pseudo_dynamic_controls); idx++) {
		int cardno = idx % SND_PSEUDO_MAX_OUTPUTS;
		const char *name = pseudo->mixer.downstream_topology.card[cardno].name;
		struct snd_kcontrol *kctl;
		int err;
		
		if ('\0' == name[0]) {
			/* skip over cards with a higher number then this one */
			idx += SND_PSEUDO_MAX_OUTPUTS - (1 +  cardno);
			continue;
		}

		kctl = snd_ctl_new1(&snd_pseudo_dynamic_controls[idx], pseudo);
		if (!kctl)
			return -ENOMEM;
		
		/* generate the name (this is the dynamic bit) */
		snprintf(kctl->id.name, sizeof(kctl->id.name),
		         "%s %s", name, snd_pseudo_dynamic_controls[idx].name);

		if ((err = snd_ctl_add(card, kctl)) < 0)
			return err;
		
		pseudo->dynamic_controls[idx] = kctl;
	}
	
	return 0;
}

static int snd_card_pseudo_register_dynamic_controls(struct snd_pseudo *pseudo)
{
	int res;
	mutex_lock(&pseudo->mixer_lock);
	res = snd_card_pseudo_register_dynamic_controls_locked(pseudo);
	mutex_unlock(&pseudo->mixer_lock);
	return res;
}

static int snd_pseudo_default_backend_get_instance(int StreamId, component_handle_t* Classoid)
{
         return -ENODEV;
}

static int snd_pseudo_default_backend_set_module_parameters(component_handle_t Classoid,
                                                     void *Data, unsigned int Size)
{
        return 0;
}

static int snd_pseudo_default_backend_alloc_substream(component_handle_t Component, int *SubStreamId)
{
        return -ENODEV;
}

static struct alsa_backend_operations snd_psuedo_default_backend_ops = {
        .owner                                      = THIS_MODULE,

        .mixer_get_instance          = snd_pseudo_default_backend_get_instance,
        .mixer_set_module_parameters = snd_pseudo_default_backend_set_module_parameters,
        .mixer_alloc_substream       = snd_pseudo_default_backend_alloc_substream,
        /* we ignore the other members since they cannot be reached unless the alloc succeeds */
};

static int __init snd_pseudo_validate_downmix_header(
		struct snd_pseudo_mixer_downmix_header *headerp, int filesize)
{
	if (filesize < sizeof(struct snd_pseudo_mixer_downmix_header))
		return -EINVAL;
	
	if (SND_PSEUDO_MIXER_DOWNMIX_HEADER_MAGIC != headerp->magic)
		return -EINVAL;
	
	if (SND_PSEUDO_MIXER_DOWNMIX_HEADER_VERSION != headerp->version)
		return -EINVAL;
	
	if (SND_PSEUDO_MIXER_DOWNMIX_HEADER_SIZE(*headerp) != filesize)
		return -EINVAL;	

	return 0;
}

static int __init snd_pseudo_validate_downmix_index(
		struct snd_pseudo_mixer_downmix_header *header,
		struct snd_pseudo_mixer_downmix_index *index)
{
	int i;
	uint64_t last_sort_value = 0; /* known to be illegal because pair4 is not NOT_CONNECTED */
	
	for (i=0; i<header->num_index_entries; i++) {
		union {
			struct snd_pseudo_mixer_channel_assignment ca;
			uint32_t n;
		} output_id = { index[i].output_id }, input_id = { index[i].input_id };
		
		uint64_t sort_value = ((uint64_t) input_id.n << 32) + output_id.n;
		
		if (sort_value <= last_sort_value)
			return -EINVAL;

		last_sort_value = sort_value;
			
		if (index[i].input_dimension > 8 || index[i].output_dimension > 8)
			return -EINVAL;
		
		if ((index[i].offset + (index[i].input_dimension * index[i].output_dimension)) >
		    header->data_length)
			return -EINVAL;
	}
	
	return 0;
}

static int __init snd_pseudo_probe(struct platform_device *devptr)
{
	struct snd_card *card;
	struct snd_pseudo *pseudo;
	int idx, err;
	int dev = devptr->id;

	card = snd_card_new(index[dev], id[dev], THIS_MODULE,
			    sizeof(struct snd_pseudo));
	if (card == NULL)
		return -ENOMEM;
	pseudo = card->private_data;

	pseudo->card = card;
	
	pseudo->allocator = bpa2_find_part(bpa2_partition[dev]);
	if (!pseudo->allocator) {
		err = -ENOMEM;
		goto __nodev;
	}
	
	for (idx = 0; idx < MAX_PCM_DEVICES && idx < pcm_devs[dev]; idx++) {
		if (pcm_substreams[dev] < 1)
			pcm_substreams[dev] = 1;
		if (pcm_substreams[dev] > MAX_PCM_SUBSTREAMS)
			pcm_substreams[dev] = MAX_PCM_SUBSTREAMS;
		if ((err = snd_card_pseudo_pcm(pseudo, idx, pcm_substreams[dev])) < 0)
			goto __nodev;
	}

	strcpy(card->driver, "Pseudo Mixer");
	sprintf(card->shortname, "MIXER%d", dev);
        /* TODO: Is there anything else that should be added to the long name? */
	sprintf(card->longname, "MIXER%d", dev);

        pseudo->room_identifier = dev;
        pseudo->backend_ops = &snd_psuedo_default_backend_ops;
        pseudo->backend_mixer = NULL;

	pseudo->mixer_observer = NULL;
	pseudo->mixer_observer_ctx = NULL;
	
	if ((err = snd_card_pseudo_new_mixer(pseudo)) < 0)
		goto __nodev;
	snd_pseudo_mixer_init(pseudo, dev);
	if ((err = snd_card_pseudo_register_dynamic_controls(pseudo)) < 0)
		goto __nodev;

	if (0 == request_firmware(&pseudo->downmix_firmware, "downmix.fw", &devptr->dev)) {
		struct snd_pseudo_mixer_downmix_header *header = (void *) pseudo->downmix_firmware->data;
		struct snd_pseudo_mixer_downmix_index *index = (void *) (header + 1);
		/* WARNING: these pointers are unsafe to dereference until the validation is performed */
		
		printk(KERN_DEBUG "%s: Found downmix firmware\n", pseudo->card->shortname);
		
		if (0 != snd_pseudo_validate_downmix_header(header, pseudo->downmix_firmware->size)) {
			printk(KERN_ERR "%s: Downmix firmware header is corrupt\n", pseudo->card->shortname);
			release_firmware(pseudo->downmix_firmware);
			pseudo->downmix_firmware = NULL;
		} else if (0 != snd_pseudo_validate_downmix_index(header, index)) {
			printk(KERN_ERR "%s: Downmix firmware index is corrupt\n", pseudo->card->shortname);
			release_firmware(pseudo->downmix_firmware);
			pseudo->downmix_firmware = NULL;
		}
	}

	snd_card_set_dev(card, &devptr->dev);
                
	if ((err = snd_card_register(card)) == 0) {
		platform_set_drvdata(devptr, card);
		return 0;
	}
	

      __nodev:
	snd_card_free(card);
	return err;
}

static int snd_pseudo_remove(struct platform_device *devptr)
{
        struct snd_card *card = platform_get_drvdata(devptr);
        struct snd_pseudo *pseudo = card->private_data;
        
        if (pseudo->downmix_firmware)
        	release_firmware(pseudo->downmix_firmware);

	snd_card_free(card);
	platform_set_drvdata(devptr, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int snd_pseudo_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	struct snd_pseudo *pseudo = card->private_data;

	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	snd_pcm_suspend_all(pseudo->pcm);
	return 0;
}
	
static int snd_pseudo_resume(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);

	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}
#endif

#define SND_PSEUDO_DRIVER	"snd_pseudo"

static struct platform_driver snd_pseudo_driver = {
	.probe		= snd_pseudo_probe,
	.remove		= snd_pseudo_remove,
#ifdef CONFIG_PM
	.suspend	= snd_pseudo_suspend,
	.resume		= snd_pseudo_resume,
#endif
	.driver		= {
		.name	= SND_PSEUDO_DRIVER
	},
};




static int snd_card_pseudo_register_backend(struct platform_device *pdev,
                                            struct alsa_backend_operations *alsa_backend_ops)
{
        struct snd_card *card = platform_get_drvdata(pdev);
        struct snd_pseudo *pseudo = card->private_data;
        int err;
        component_handle_t mixer;
        
        /* register the backend and aquire a handle on the mixer instance */
        pseudo->backend_ops = alsa_backend_ops;
        err = pseudo->backend_ops->mixer_get_instance(pseudo->room_identifier, &mixer);
        if (err != 0)
                return err;
        pseudo->backend_mixer = mixer;
        
        if (default_transformer_name.magic)
        	(void) pseudo->backend_ops->mixer_set_module_parameters(
        			pseudo->backend_mixer,
        			(void *) &default_transformer_name, sizeof(default_transformer_name));

        /* only only update of the downmix firmware (if it exists) */
        if (pseudo->downmix_firmware) {
                err = pseudo->backend_ops->mixer_set_module_parameters(
                        pseudo->backend_mixer,
                        pseudo->downmix_firmware->data, pseudo->downmix_firmware->size);
                if (0 != err)
                    	printk(KERN_ERR "%s: Can not pass downmix firmware to mixer\n", pseudo->card->shortname);
        		/* do not propagate error */
        }
        
        /* update the mixer instance with our parameters */
	mutex_lock(&pseudo->mixer_lock);
        snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

        return 0;
}

int register_alsa_backend       (char                           *name,
                                 struct alsa_backend_operations *alsa_backend_ops)
{
        int i;
        
        for (i=0; i<SNDRV_CARDS; i++) {
                if (devices[i]) {
                        snd_card_pseudo_register_backend(devices[i], alsa_backend_ops);
                }
        }
        
        return 0;
}
EXPORT_SYMBOL_GPL(register_alsa_backend);

/**
 * Register a mixer observer which will receive a callback whenever the mixer settings change.
 *
 * The API (register/deregister) supports multiple clients but at present the implementation
 * supports only one because there is only a single client at the moment.
 */
int snd_pseudo_register_mixer_observer(int mixer_num,
		snd_pseudo_mixer_observer_t *observer, void *ctx)
{
    	struct snd_card *card;
    	struct snd_pseudo *pseudo;
    	
	if (!devices[mixer_num])
		return -ENODEV;
	
	card = platform_get_drvdata(devices[mixer_num]);
	pseudo = card->private_data;
	
	mutex_lock(&pseudo->mixer_lock);
	
        if (pseudo->mixer_observer) {
        	mutex_unlock(&pseudo->mixer_lock);
        	return -EBUSY;
        }
        
        pseudo->mixer_observer = observer;
        pseudo->mixer_observer_ctx = ctx;
        
        observer(ctx, &pseudo->mixer);
        
        mutex_unlock(&pseudo->mixer_lock);
        return 0;
}
EXPORT_SYMBOL_GPL(snd_pseudo_register_mixer_observer);

int snd_pseudo_deregister_mixer_observer(int mixer_num,
		snd_pseudo_mixer_observer_t *observer, void *ctx)
{
    	struct snd_card *card;
    	struct snd_pseudo *pseudo;
    	
	if (!devices[mixer_num])
		return -ENODEV;
	
	card = platform_get_drvdata(devices[mixer_num]);
	pseudo = card->private_data;
	
	mutex_lock(&pseudo->mixer_lock);
	
        if (pseudo->mixer_observer != observer && pseudo->mixer_observer_ctx != ctx) {
        	mutex_unlock(&pseudo->mixer_lock);
        	return -EINVAL;
        }
        
        pseudo->mixer_observer = NULL;
        pseudo->mixer_observer_ctx = NULL;
        
        mutex_unlock(&pseudo->mixer_lock);
        return 0;
}
EXPORT_SYMBOL_GPL(snd_pseudo_deregister_mixer_observer);

struct snd_pseudo_mixer_downmix_rom *snd_pseudo_get_downmix_rom(int mixer_num)
{
    	struct snd_card *card;
    	struct snd_pseudo *pseudo;
    	
	if (!devices[mixer_num])
		return ERR_PTR(-ENODEV);
	
	card = platform_get_drvdata(devices[mixer_num]);
	pseudo = card->private_data;

	if (pseudo->downmix_firmware)
		return (void *) pseudo->downmix_firmware->data;

	return NULL;
}
EXPORT_SYMBOL_GPL(snd_pseudo_get_downmix_rom);

static void __init_or_module snd_pseudo_unregister_all(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devices); ++i)
		platform_device_unregister(devices[i]);
	platform_driver_unregister(&snd_pseudo_driver);
}

static int __init alsa_card_pseudo_init(void)
{
	int i, cards, err;

	if ((err = platform_driver_register(&snd_pseudo_driver)) < 0)
		return err;

	cards = 0;
	for (i = 0; i < ARRAY_SIZE(default_topology); i++) {
		struct platform_device *device;
		if (! enable[i])
			continue;
		device = platform_device_register_simple(SND_PSEUDO_DRIVER,
							 i, NULL, 0);
		if (IS_ERR(device))
			continue;
		devices[i] = device;
		cards++;
	}
	if (!cards) {
#ifdef MODULE
		printk(KERN_ERR "%s: Pseudo soundcard not found or device busy\n",
                       KBUILD_MODNAME);
#endif
		snd_pseudo_unregister_all();
		return -ENODEV;
	}

#ifdef MODULE
	printk(KERN_INFO "%s: %d pseudo soundcard(s) found\n",
	       KBUILD_MODNAME, cards);
#endif
	
	return 0;
	
}

static void __exit alsa_card_pseudo_exit(void)
{
	snd_pseudo_unregister_all();
}

module_init(alsa_card_pseudo_init)
module_exit(alsa_card_pseudo_exit)
