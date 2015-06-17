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

#include <linux/module.h>
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
#include <linux/version.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <linux/io.h>
#include <asm/cacheflush.h>

#include <ACC_Transformers/acc_mmedefines.h>

#include <stm_display.h>

#include "stm_se.h"
#include "pseudocard.h"
#include "pseudo_mixer.h"
#include "stm_event.h"

MODULE_AUTHOR("Daniel Thompson <daniel.thompson@st.com>");
MODULE_DESCRIPTION("Pseudo soundcard");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{ALSA,Pseudo soundcard}}");

#define PSEUDO_COMPOUND_CTRL(x)   (x & 0xFFFF)
#define PSEUDO_COMPOUND_ELT(x, y) \
		(((unsigned long) y << 16) | (unsigned long) x)

#define SND_PSEUDO_MIXER_INPUT_GAIN \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT, 0)
#define SND_PSEUDO_MIXER_INPUT_PANNING \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT, 1)

#define SND_PSEUDO_MIXER_DRC_TYPE \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, 0)
#define SND_PSEUDO_MIXER_DRC_BOOST \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, 1)
#define SND_PSEUDO_MIXER_DRC_CUT \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, 2)

#define PSEUDO_COMPOUND(xname, xctrl, xindex) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name  = xname, .index = xindex,     \
	.info  = snd_pseudo_compound_info,   \
	.get   = snd_pseudo_compound_get,    \
	.put   = snd_pseudo_compound_put,    \
	.private_value = (unsigned long) xctrl }

#define MAX_AUDIOGENERATOR_PCM_DEVICES		16
#define MAX_AUDIOGENERATOR_SUBDEVICES		1
#define MAX_INTERACTIVEAUDIO_PCM_DEVICES	4
#define MAX_PCM_DEVICES         (MAX_AUDIOGENERATOR_PCM_DEVICES + \
					MAX_INTERACTIVEAUDIO_PCM_DEVICES)

#define INTERACTIVE_AUDIO_PCM_NAME		"Interactive Audio"
#define AUDIO_GENERATOR_PCM_NAME		"Audio Generator"

#define MAX_PCM_SUBSTREAMS      16
#define MAX_MIDI_DEVICES        2

/* 2 channel, 32-bit, (96kPCM for 32Khz main output) sample periods */
#define MAX_BUFFER_SIZE         (2 * 4 * 2 * 4608)
#define MAX_PERIOD_SIZE         MAX_BUFFER_SIZE
#define USE_FORMATS             (SNDRV_PCM_FMTBIT_S8		| \
				SNDRV_PCM_FMTBIT_MU_LAW		| \
				SNDRV_PCM_FMTBIT_A_LAW		| \
				SNDRV_PCM_FMTBIT_U8		| \
				SNDRV_PCM_FMTBIT_S16_LE		| \
				SNDRV_PCM_FMTBIT_S16_BE		| \
				SNDRV_PCM_FMTBIT_U16_LE		| \
				SNDRV_PCM_FMTBIT_U16_BE		| \
				SNDRV_PCM_FMTBIT_S24_3LE	| \
				SNDRV_PCM_FMTBIT_S24_3BE	| \
				SNDRV_PCM_FMTBIT_S32_LE		| \
				SNDRV_PCM_FMTBIT_S32_BE)

#define USE_RATE                (SNDRV_PCM_RATE_KNOT | \
					SNDRV_PCM_RATE_8000_96000)
#define USE_RATE_MIN            5500
#define USE_RATE_MAX            96000
#define USE_CHANNELS_MIN        1
#define USE_CHANNELS_MAX        2
#define USE_PERIODS_MIN         2
#define USE_PERIODS_MAX         4608
#define add_capture_constraints(x) 0

static unsigned int rates[] = { 8000, 9600, 11025, 12000, 16000, 22050, 24000,
				32000, 44100, 48000, 64000, 88200, 96000 };

static struct snd_pcm_hw_constraint_list hw_constraints_rates = {
	.count = ARRAY_SIZE(rates),
	.list = rates,
	.mask = 0,
};

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

static int nb_analog_player;

static int pcm_audiogenerator_devs[SNDRV_CARDS] = {
	[0 ... (SNDRV_CARDS - 1)] = STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS };
static int pcm_interactiveaudio_devs[SNDRV_CARDS] = {
	[0 ... (SNDRV_CARDS - 1)] = 1 };
static int pcm_interactiveaudio_substreams[MAX_INTERACTIVEAUDIO_PCM_DEVICES] = {
	[0 ... (MAX_INTERACTIVEAUDIO_PCM_DEVICES - 1)] = STM_SE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS };

static int bcast_audiogenerator_devs[SNDRV_CARDS] = {
	[0 ... (SNDRV_CARDS - 1)] = STM_SE_DUAL_STAGE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS };

static char *bpa2_partition[SNDRV_CARDS] = {
	[0 ... (SNDRV_CARDS - 1)] =
#ifdef CONFIG_SND_STM_BPA2_PARTITION_NAME
	    CONFIG_SND_STM_BPA2_PARTITION_NAME
#else
	    "bigphysarea"
#endif
	};

// +18dB is ehe biggest gain that can be represented by a Q3_13 linear gain
#define Q3_13_MAX_DB +18
#define Q3_13_MAX_MB (Q3_13_MAX_DB * 100)
#define MAX_Q3_13_DB(x) ((x) < Q3_13_MAX_DB) ? (x) : Q3_13_MAX_DB

#define __fract32_mulr_fract32_fract32(x, y)	\
		((int)(((long long) (x) * (y)) >> 32) << 1)
#define convert_mb2linear_q13(val) ((val < Q3_13_MAX_MB) ? (convert_mb2linear_q23(val) >> 10) : Q3_13_MAX)

#define Q31_0dB          0x7FFFFFFF
#define Q31_M0p16dB      0x7DA9EAA6
#define Q31_M0p5dB       0x78D6FC9E
#define Q31_M32DB        0x0337184e
#define Q31_BY100        0x51EB852
#define mB_to_qdB(mB) __fract32_mulr_fract32_fract32(mB, Q31_BY100)
#define dB_to_mB(dB)  (dB * 100)


const int ag_32dB[] = {
	/* up to -32dB  */	Q31_0dB,
	/* beyond -32dB */	Q31_M32DB,
	/* beyond -64dB */	(((long long)Q31_M32DB * Q31_M32DB) >> 31),
	/* beyond -96dB */  0
};

static const int ag_8dB[]	=	{
	/* up to  - 8dB */ Q31_0dB,
	/* up to  -16dB */ 0x32f52cfe,
	/* up to  -24dB */ 0x144960c5,
	/* up to  -32dB */ 0x08138561,
};

#define AG_MIN_QDB (8*4)
const int ag_db[AG_MIN_QDB + 1] = /* Q31([-8 , + 0 dB] */
{
	/* -8.00 dB */  0x32F52CFE,  0x34721A0D,  0x35FA26A9,  0x378DA5F8,
	/* -7.00 dB */  0x392CED8D,  0x3AD8557E,  0x3C90386F,  0x3E54F3AD,
	/* -6.00 dB */  0x4026E73C,  0x420675F0,  0x43F4057E,  0x45EFFE95,
	/* -5.00 dB */  0x47FACCF0,  0x4A14DF72,  0x4C3EA838,  0x4E789CB8,
	/* -4.00 dB */  0x50C335D3,  0x531EEFF3,  0x558C4B22,  0x580BCB29,
	/* -3.00 dB */  0x5A9DF7AB,  0x5D435C3F,  0x5FFC8890,  0x62CA107B,
	/* -2.00 dB */  0x65AC8C2F,  0x68A4984B,  0x6BB2D604,  0x6ED7EB40,
	/* -1.00 dB */  0x721482BF,  0x75694C3F,  0x78D6FC9E,  0x7C5E4E01,
	/*  0.00 dB */  Q31_0dB
};

#define AG_MAX_DB (6 * 8) /* Q23 ==> max = 48dB */

int convert_mb2linear_q23(int mb)
{
	int qdb, n32, n8, n0;
	int linear32, linear8, linear;
	if (mb > AG_MAX_DB * 100)
		return Q31_0dB;

	if (mb > 0) {
		/* compute value for mb - 48dB then apply a shift of 8 bit */
		linear = convert_mb2linear_q23(mb - AG_MAX_DB * 100) << 8;

		/* apply a correction of -0.16 dB to the value computed value */
		linear = __fract32_mulr_fract32_fract32(linear, Q31_M0p16dB);

		return linear;
	}

	qdb = mB_to_qdB(mb);

	if (qdb >= -AG_MIN_QDB) {	/* 8*4 quater dB */
		linear = ag_db[AG_MIN_QDB + qdb];
	} else {
		n32 = ((-qdb) >> (5+2))       ; /* number of 32dB slices. */
		n8  = ((-qdb) >> (3+2)) & 0x03; /* number of 8dB slices */
		n0  = ((-qdb) >>     0) & 0x1F; /* number of quater dB */
		linear32 = ag_32dB[n32];
		linear8  = ag_8dB[n8];
		linear   = __fract32_mulr_fract32_fract32(linear32 , linear8);
		linear   = __fract32_mulr_fract32_fract32(linear ,
				ag_db[AG_MIN_QDB - n0]);
	}

	return linear >> 8 ; /* convert to Q23 */
}


/* module parameters shared with other files */
int *card_enables;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for pseudo soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for pseudo soundcard.");
module_param_array(enable, int, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable this pseudo soundcard.");

module_param_array(pcm_audiogenerator_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_audiogenerator_devs,
		 "PCM Audio Generator devices # (0-16) for pseudo driver.");
module_param_array(pcm_interactiveaudio_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_interactiveaudio_devs,
		 "PCM Interactive Audio devices # (0-16) for pseudo driver.");
module_param_array(pcm_interactiveaudio_substreams, int, NULL, 0444);
MODULE_PARM_DESC(pcm_interactiveaudio_substreams,
		 "PCM substreams # (1-16) for pseudo driver.");

module_param_array(bpa2_partition, charp, NULL, 0444);
MODULE_PARM_DESC(bpa2_partition,
		 "BPA2 partition ID string from which to allocate memory.");
static struct platform_device *devices[SNDRV_CARDS];

#define MAX_PCM_PROC_FILE_LENGTH 64

char *tuningFw[MAX_PCM_PROC_FILE_LENGTH];
static int tuningFwNumber;

module_param_array(tuningFw, charp, &tuningFwNumber, 0);
MODULE_PARM_DESC(tuningFw,
" A comma separated list of PcmProcessing tuning firmwares relative path\n"
"                ex: tuningFw=\"dolbyVol.fw,limiter.fw\"");

static struct pcmproc_FW_s pcmproc_fw_ctx;

/* maximum index into the devices[] array for a mixer sound card */
static int mixer_max_index;

static struct snd_pseudo_mixer_downstream_topology default_topology;

struct snd_pseudo_pcm {
	spinlock_t lock;
	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_irq_pos;	/* IRQ position */
	unsigned int pcm_buf_pos;	/* position in buffer */
	unsigned int bytespersample;
	struct snd_pcm_substream *substream;
	component_handle_t audio_generator;
	stm_event_subscription_h event_subscription;
	int backend_is_setup;
	int generator_attached;
};

static void snd_card_pseudo_pcm_event_callback(unsigned int nbevent,
					       stm_event_info_t *events)
{
	unsigned int i;
	stm_event_info_t *eventinfo = events;
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	struct snd_pseudo_pcm *ppcm;
	stm_se_audio_generator_info_t info;
	int err = 0;
	unsigned long flags;
	unsigned int delta, cur_buf_pos;

	if (nbevent < 1)
		return;

	for (i = 0; i < nbevent; i++) {
		eventinfo = (events + i);
		if (!
		    (eventinfo->event.
		     event_id & STM_SE_AUDIO_GENERATOR_EVENT_DATA_CONSUMED)) {
			printk(KERN_INFO "%s Event ID %d not expected\n",
			       __func__, eventinfo->event.event_id);
			continue;
		}

		substream = (struct snd_pcm_substream *)eventinfo->cookie;
		runtime = substream->runtime;
		ppcm = runtime->private_data;

		spin_lock_irqsave(&ppcm->lock, flags);

		err =
		    stm_se_audio_generator_get_info(ppcm->audio_generator,
						    &info);
		if (err != 0)
			printk(KERN_ERR " getinfo error: %d, %x\n", err,
			       (unsigned int)ppcm->audio_generator);

		stm_se_audio_generator_commit(ppcm->audio_generator,
					      info.avail);

		/* Work out the delta (in bytes) since the callback was
		 * last called noting, of course, that the play pointer
		 * may have wrapped.
		 */
		cur_buf_pos =
		    info.head_offset + info.avail * ppcm->bytespersample;
		if (cur_buf_pos >= ppcm->pcm_size)
			cur_buf_pos -= ppcm->pcm_size;

		delta = cur_buf_pos;
		if (delta <= ppcm->pcm_buf_pos)
			delta += ppcm->pcm_size;
		BUG_ON(delta <= ppcm->pcm_buf_pos);
		delta = delta - ppcm->pcm_buf_pos;

		ppcm->pcm_irq_pos += delta;
		ppcm->pcm_buf_pos = cur_buf_pos;

		if (ppcm->pcm_irq_pos >= ppcm->pcm_count) {
			while (ppcm->pcm_irq_pos >= ppcm->pcm_count)
				ppcm->pcm_irq_pos -= ppcm->pcm_count;
			spin_unlock_irqrestore(&ppcm->lock, flags);

			snd_pcm_period_elapsed(ppcm->substream);
		} else
			spin_unlock_irqrestore(&ppcm->lock, flags);
	}
}

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

static int snd_card_pseudo_pcm_trigger(struct snd_pcm_substream *substream,
				       int cmd)
{
	struct snd_pseudo *pseudo = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo_pcm *ppcm = runtime->private_data;
	stm_se_audio_generator_info_t info;
	stm_se_audio_mixer_value_t value;
	int err = 0;
	int idx;

	spin_lock(&ppcm->lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		stm_se_audio_generator_get_info(ppcm->audio_generator, &info);
		stm_se_audio_generator_commit(ppcm->audio_generator,
					      info.avail);

		if (!strcmp(substream->pcm->name, AUDIO_GENERATOR_PCM_NAME)) {
			value.input_gain.input = (stm_object_h)
				(substream->pcm->device +
				STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0);

			value.input_gain.objecthandle =
					(stm_object_h) ppcm->audio_generator;

			for (idx = 0; idx < SND_PSEUDO_MIXER_CHANNELS; idx++)
				value.input_gain.gain[idx] =
					pseudo->audiogengainrestore
						[substream->pcm->device];

			err =
				stm_se_audio_mixer_set_compound_control(pseudo->
						backend_mixer,
						SND_PSEUDO_MIXER_INPUT_GAIN,
						(const void *)&value);
			if (0 != err)
				printk(KERN_ERR "%s\n: Could not get mixer parameter",
						pseudo->card->shortname);
		}

		err = stm_se_audio_generator_start(ppcm->audio_generator);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		err = stm_se_audio_generator_stop(ppcm->audio_generator);
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

	ppcm->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	ppcm->pcm_count = snd_pcm_lib_period_bytes(substream);
	ppcm->pcm_irq_pos = 0;
	ppcm->pcm_buf_pos = 0;

	return 0;
}

static snd_pcm_uframes_t snd_card_pseudo_pcm_pointer(struct snd_pcm_substream
						     *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo_pcm *ppcm = runtime->private_data;

	return bytes_to_frames(runtime, ppcm->pcm_buf_pos);
}

static struct snd_pcm_hardware snd_card_pseudo_playback = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = USE_FORMATS,
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max = USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = MAX_BUFFER_SIZE / 2,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 0,
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
		if (bpa2_low_part(pseudo->allocator))
			iounmap(runtime->dma_area);

	if (runtime->dma_addr)
		bpa2_free_pages(pseudo->allocator, runtime->dma_addr);

	runtime->dma_buffer_p = NULL;
	runtime->dma_area = NULL;
	runtime->dma_addr = 0;
	runtime->dma_bytes = 0;
}

static int snd_card_pseudo_alloc_pages(struct snd_pcm_substream *substream,
				       unsigned int size)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo *pseudo = substream->private_data;
	int num_pages;

	num_pages = (size + (PAGE_SIZE - 1)) / PAGE_SIZE;

	runtime->dma_addr = bpa2_alloc_pages(pseudo->allocator,
					     num_pages, 0, GFP_KERNEL);
	if (!runtime->dma_addr)
		return -ENOMEM;

	if (bpa2_low_part(pseudo->allocator))
		runtime->dma_area = phys_to_virt(runtime->dma_addr);
	else
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
	stm_se_audio_generator_buffer_t audio_gen_buffer;
	stm_se_audio_core_format_t audio_gen_format;
	int i;

	int err, err1;

	/* allocate the hardware buffer and map it appropriately */
	err =
	    snd_card_pseudo_alloc_pages(substream,
					params_buffer_bytes(hw_params));
	if (err < 0)
		return err;

	/* ensure stale data does not leak to userspace */
	memset(runtime->dma_area, 0, runtime->dma_bytes);

	/* populate the runtime-variable portion of audio_gen_buffer */
	audio_gen_buffer.audio_buffer = runtime->dma_area;
	audio_gen_buffer.audio_buffer_size = runtime->dma_bytes;

	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_U8:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_U8;
		ppcm->bytespersample = 1;
		break;
	case SNDRV_PCM_FORMAT_S8:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S8;
		ppcm->bytespersample = 1;
		break;
	case SNDRV_PCM_FORMAT_A_LAW:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_ALAW_8;
		ppcm->bytespersample = 1;
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_ULAW_8;
		ppcm->bytespersample = 1;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_U16LE;
		ppcm->bytespersample = 2;
		break;
	case SNDRV_PCM_FORMAT_U16_BE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_U16BE;
		ppcm->bytespersample = 2;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S16LE;
		ppcm->bytespersample = 2;
		break;
	case SNDRV_PCM_FORMAT_S16_BE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S16BE;
		ppcm->bytespersample = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S24LE;
		ppcm->bytespersample = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3BE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S24BE;
		ppcm->bytespersample = 3;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S32LE;
		ppcm->bytespersample = 4;
		break;
	case SNDRV_PCM_FORMAT_S32_BE:
	default:
		audio_gen_buffer.format = STM_SE_AUDIO_PCM_FMT_S32BE;
		ppcm->bytespersample = 4;
		break;
	}

	audio_gen_format.channel_placement.channel_count =
	    params_channels(hw_params);
	ppcm->bytespersample *= params_channels(hw_params);

	for (i = 0; i < audio_gen_format.channel_placement.channel_count; i++)
		audio_gen_format.channel_placement.chan[i] =
		    STM_SE_AUDIO_CHAN_UNKNOWN;

	audio_gen_format.sample_rate = params_rate(hw_params);

	err = stm_se_audio_generator_set_compound_control(ppcm->audio_generator,
					STM_SE_CTRL_AUDIO_INPUT_FORMAT,
					&audio_gen_format);

	err |=
	    stm_se_audio_generator_set_compound_control(ppcm->audio_generator,
					STM_SE_CTRL_AUDIO_GENERATOR_BUFFER,
					&audio_gen_buffer);

	err |= stm_se_audio_generator_set_control(ppcm->audio_generator,
					STM_SE_CTRL_AUDIO_INPUT_EMPHASIS,
					STM_SE_NO_EMPHASIS);

	err1 = stm_se_audio_generator_attach(ppcm->audio_generator,
					     pseudo->backend_mixer);

	if (!err1)
		ppcm->generator_attached = true;

	if (err1 || err) {
		snd_card_pseudo_free_pages(substream);
		return err1 ? err1 : err;
	}

	ppcm->backend_is_setup = 1;

	return 0;
}

static int snd_card_pseudo_hw_free(struct snd_pcm_substream *substream)
{
	int err = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo_pcm *ppcm = runtime->private_data;
	struct snd_pseudo *pseudo = substream->private_data;

	if (ppcm->generator_attached) {
		ppcm->generator_attached = false;
		err = stm_se_audio_generator_detach(ppcm->audio_generator,
						    pseudo->backend_mixer);
	}
	snd_card_pseudo_free_pages(substream);

	return err;
}

/*
 * Copied verbaitum from snd_pcm_mmap_data_fault()
 */
static int snd_card_pseudo_pcm_mmap_data_fault(struct vm_area_struct *area,
					       struct vm_fault *vmf)
{
	struct snd_pcm_substream *substream = area->vm_private_data;
	struct snd_pcm_runtime *runtime;
	unsigned long offset;
	struct page *page;
	void *vaddr;
	size_t dma_bytes;

	if (substream == NULL)
		return VM_FAULT_SIGBUS;
	runtime = substream->runtime;
	offset = vmf->pgoff << PAGE_SHIFT;
	dma_bytes = PAGE_ALIGN(runtime->dma_bytes);
	if (offset > dma_bytes - PAGE_SIZE)
		return VM_FAULT_SIGBUS;
	if (substream->ops->page) {
		page = substream->ops->page(substream, offset);
		if (!page)
			return VM_FAULT_SIGBUS;
	} else {
		vaddr = runtime->dma_area + offset;
		page = virt_to_page(vaddr);
	}
	get_page(page);
	vmf->page = page;
	return 0;
}

static struct vm_operations_struct snd_card_pseudo_pcm_vm_ops_data = {
	.open = snd_pcm_mmap_data_open,
	.close = snd_pcm_mmap_data_close,
	.fault = snd_card_pseudo_pcm_mmap_data_fault,
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
	area->vm_flags |= VM_IO;
	atomic_inc(&substream->mmap_count);
	return 0;
}

static struct snd_pseudo_pcm *new_pcm_stream(struct snd_pcm_substream
					     *substream)
{
	struct snd_pseudo_pcm *ppcm;

	ppcm = kzalloc(sizeof(*ppcm), GFP_KERNEL);
	if (!ppcm)
		return ppcm;
	spin_lock_init(&ppcm->lock);
	ppcm->substream = substream;
	/*ppcm->backend_is_setup = 0; *//* kzalloc... */
	return ppcm;
}

static int snd_card_pseudo_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo *pseudo = substream->private_data;
	struct snd_pseudo_pcm *ppcm;
	int err;
	sigset_t allset, oldset;
	char se_name[32];
	int32_t ctrlvalue;
	stm_event_subscription_entry_t event_entry = { 0 };

	ppcm = new_pcm_stream(substream);
	if (ppcm == NULL)
		return -ENOMEM;
	runtime->private_data = ppcm;

	/* settings private_free makes the infrastructure
	 * responsible for freeing ppcm */
	runtime->private_free = snd_card_pseudo_runtime_free;
	runtime->hw = snd_card_pseudo_playback;

	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
						&hw_constraints_rates);

	snprintf(se_name, 31, "%s,%d,%d", pseudo->card->shortname,
		 substream->pcm->device, substream->number);
	se_name[31] = '\0';

	if (!strcmp(substream->pcm->name, INTERACTIVE_AUDIO_PCM_NAME))
		ctrlvalue = STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD;
	else
		ctrlvalue = STM_SE_CTRL_VALUE_AUDIO_APPLICATION_ISO;

	sigfillset(&allset);
	sigprocmask(SIG_BLOCK, &allset, &oldset);

	err = stm_se_audio_generator_new(se_name, &ppcm->audio_generator);

	if (!strcmp(substream->pcm->name, AUDIO_GENERATOR_PCM_NAME))
		pseudo->audiogeneratorhandle[substream->pcm->device] =
			ppcm->audio_generator;

	err |= stm_se_audio_generator_set_control(ppcm->audio_generator,
			STM_SE_CTRL_AUDIO_APPLICATION_TYPE,
			ctrlvalue);

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	if (err)
		return err;

	/*setup event subscription */
	event_entry.object = ppcm->audio_generator;
	event_entry.event_mask = STM_SE_AUDIO_GENERATOR_EVENT_DATA_CONSUMED;
	event_entry.cookie = substream;

	err =
	    stm_event_subscription_create(&event_entry, 1,
					  &ppcm->event_subscription);
	if (err < 0) {
		ppcm->event_subscription = NULL;
		printk(KERN_ERR
		       "%s: Failed to Create Event Subscription: AudioGen %x\n",
		       __func__, (uint32_t) ppcm->audio_generator);
		return err;
	}

	/*Set up Callback Function */
	err =
	    stm_event_set_handler(ppcm->event_subscription,
				  &snd_card_pseudo_pcm_event_callback);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to Set Event Handler\n", __func__);
		if (0 > stm_event_subscription_delete(ppcm->event_subscription))
			printk(KERN_ERR
			       "%s stm_event_subscription_delete failed\n",
			       __func__);
		ppcm->event_subscription = NULL;

		return err;
	}

	return 0;
}

static int snd_card_pseudo_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pseudo *pseudo = substream->private_data;
	struct snd_pseudo_pcm *ppcm = runtime->private_data;

	int err;

	if (ppcm->event_subscription)
		if (stm_event_subscription_delete(ppcm->event_subscription) < 0)
			printk(KERN_ERR
			       "%s stm_event_subscription_delete failed\n",
			       __func__);

	err = stm_se_audio_generator_delete(ppcm->audio_generator);
	if (err)
		return err;

	if (!strcmp(substream->pcm->name, AUDIO_GENERATOR_PCM_NAME))
		pseudo->audiogeneratorhandle[substream->pcm->device] = NULL;

	ppcm->event_subscription = NULL;

	ppcm->backend_is_setup = 0;

	return 0;
}

static struct snd_pcm_ops snd_card_pseudo_playback_ops = {
	.open = snd_card_pseudo_playback_open,
	.close = snd_card_pseudo_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_card_pseudo_hw_params,
	.hw_free = snd_card_pseudo_hw_free,
	.prepare = snd_card_pseudo_pcm_prepare,
	.trigger = snd_card_pseudo_pcm_trigger,
	.pointer = snd_card_pseudo_pcm_pointer,
	.mmap = snd_card_pseudo_pcm_mmap,
};

static int snd_card_pseudo_pcm(struct snd_pseudo *pseudo, int device,
				      const char *name, int substreams)
{
	struct snd_pcm *pcm;
	int err;

	err = snd_pcm_new(pseudo->card, name, device, substreams, 0, &pcm);
	if (err < 0)
		return err;
	pseudo->pcm = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_card_pseudo_playback_ops);
	pcm->private_data = pseudo;
	pcm->info_flags = 0;
	strlcpy(pcm->name, name, sizeof(pcm->name));

	return 0;
}

static void snd_pseudo_mixer_update(struct snd_pseudo *pseudo)
{
	int err;

	err = stm_se_component_set_module_parameters(pseudo->backend_mixer,
						   &pseudo->mixer,
						   sizeof(pseudo->mixer));
	if (0 != err)
		printk(KERN_ERR "%s: Could not update mixer parameters\n",
		       pseudo->card->shortname);

	/* lock prevents the observer from being deregistered
	 * whilst we update the observer */
	BUG_ON(!mutex_is_locked(&pseudo->mixer_lock));
	if (pseudo->mixer_observer)
		pseudo->mixer_observer(pseudo->mixer_observer_ctx,
				       &pseudo->mixer);
}

#define PSEUDO_ADDR(x) (offsetof(struct snd_pseudo_mixer_settings, x))

#define PSEUDO_INTEGER(xname, xindex, addr) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_integer_info, \
	.get = snd_pseudo_integer_get, \
	.put = snd_pseudo_integer_put, \
	.private_value = PSEUDO_ADDR(addr) }

static int snd_pseudo_integer_info(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_info *uinfo)
{
	int addr = kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	switch (addr) {

		/* master latency control, +-150ms */
	case PSEUDO_ADDR(master_latency):
		uinfo->count = 1;
		uinfo->value.integer.min = -150;
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
	unsigned char *cp = ((char *)&pseudo->mixer) + addr;
	int *volumesp = (int *)cp;
	struct snd_ctl_elem_info uinfo = { {0} };
	int res, i;

	/* use the switched info function to find the number of
	 * channels and the max value */
	res = snd_pseudo_integer_info(kcontrol, &uinfo);
	if (res < 0)
		return res;

	mutex_lock(&pseudo->mixer_lock);
	for (i = 0; i < uinfo.count; i++)
		if (uinfo.count == SND_PSEUDO_MIXER_CHANNELS)
			ucontrol->value.integer.value[i] =
			    volumesp[remap_channels[i]];
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
	unsigned char *cp = ((char *)&pseudo->mixer) + addr;
	int *volumesp = (int *)cp;
	struct snd_ctl_elem_info uinfo = { {0} };
	int update[SND_PSEUDO_MIXER_CHANNELS];
	int res, changed, i, j;

	/* use the switched info function to find the number of
	 * channels and the max value */
	res = snd_pseudo_integer_info(kcontrol, &uinfo);
	if (res < 0)
		return res;

	for (i = 0; i < uinfo.count; i++) {
		update[i] = ucontrol->value.integer.value[i];
		if (update[i] < uinfo.value.integer.min)
			update[i] = uinfo.value.integer.min;
		if (update[i] > uinfo.value.integer.max)
			update[i] = uinfo.value.integer.max;
	}

	changed = 0;
	mutex_lock(&pseudo->mixer_lock);
	for (i = 0; i < uinfo.count; i++) {
		j = (uinfo.count ==
		     SND_PSEUDO_MIXER_CHANNELS ? remap_channels[i] : i);
		changed = changed || (volumesp[j] != update[i]);
		volumesp[j] = update[i];
	}
	if (changed)
		snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

	return changed;
}

#define PSEUDO_SWITCH(xname, xindex, addr) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_switch_info, \
	.get = snd_pseudo_switch_get, \
	.put = snd_pseudo_switch_put, \
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
	char *switchp = ((char *)&pseudo->mixer) + addr;

	/* no spinlock (single bit cannot be incoherent) */
	ucontrol->value.integer.value[0] = (*switchp != 0);
	return 0;
}

static int snd_pseudo_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
	char *switchp = ((char *)&pseudo->mixer) + addr;
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
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_route_info, \
	.get = snd_pseudo_route_get, \
	.put = snd_pseudo_route_put, \
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

	/* ordering must be identical to enum
	 * snd_pseudo_mixer_interactive_audio_mode */
	static char *interactive_audio_mode[] = {
		"3/4.0", "3/2.0", "2/0.0"
	};

	char **texts;
	int num_texts;

#define C(x) do {	texts = (x);\
			num_texts = sizeof((x)) / sizeof(*(x));\
		} while (0);

	switch (kcontrol->private_value) {
	case PSEUDO_ADDR(metadata_update):
		C(metadata_update);
		break;
	case PSEUDO_ADDR(interactive_audio_mode):
		C(interactive_audio_mode);
		break;
	default:
		BUG();
		return 0;
	}
#undef C

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = num_texts;
	if (uinfo->value.enumerated.item > (num_texts - 1))
		uinfo->value.enumerated.item = (num_texts - 1);
	strlcpy(uinfo->value.enumerated.name,
		texts[uinfo->value.enumerated.item],
		sizeof(uinfo->value.enumerated.name));

	return 0;
}

static int snd_pseudo_route_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
	char *routep = ((char *)&pseudo->mixer) + addr;

	/* no spinlock (single address cannot be incoherent) */
	ucontrol->value.integer.value[0] = *routep;
	return 0;
}

static int snd_pseudo_route_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int addr = kcontrol->private_value;
	char *routep = ((char *)&pseudo->mixer) + addr;
	struct snd_ctl_elem_info uinfo = { {0} };
	int update, changed;

	/* use the switched info function to find the
	 * bounds of the enumeration */
	(void)snd_pseudo_route_info(kcontrol, &uinfo);

	update = ucontrol->value.integer.value[0];
	if (update < 0)
		update = 0;
	if (update > (uinfo.value.enumerated.items - 1))
		update = (uinfo.value.enumerated.items - 1);

	mutex_lock(&pseudo->mixer_lock);
	changed = (*routep != update);
	*routep = update;
	if (changed)
		snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

	return changed;
}

#define PSEUDO_BLOB(xname, xindex, addr) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_blob_info, \
	.get = snd_pseudo_blob_get, \
	.put = snd_pseudo_blob_put, \
	.private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_BLOB_READONLY(xname, xindex, addr) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_blob_info, \
	.get = snd_pseudo_blob_get, \
	.private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_IEC958(xname, xindex, addr) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_iec958_info, \
	.get = snd_pseudo_blob_get, \
	.put = snd_pseudo_blob_put, \
	.private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_IEC958_READONLY(xname, xindex, addr) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_iec958_info, \
	.get = snd_pseudo_blob_get, \
	.private_value = PSEUDO_ADDR(addr) }

#define PSEUDO_DUALMONO_OVERRIDE(xname, xindex) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = xindex, \
	.info = snd_pseudo_switch_info, \
	.get = snd_pseudo_dualmono_metadata_override_get, \
	.put = snd_pseudo_dualmono_metadata_override_put, \
	.private_value = 0 }

static int snd_pseudo_blob_size(unsigned long private_value)
{
	struct snd_pseudo_mixer_settings *mixer = 0;

	switch (private_value) {
	case PSEUDO_ADDR(iec958_metadata):
		return sizeof(mixer->iec958_metadata);
		/* NOT REACHED */
		break;
	case PSEUDO_ADDR(iec958_mask):
		return sizeof(mixer->iec958_mask);
		/* NOT REACHED */
		break;
	case PSEUDO_ADDR(fatpipe_metadata):
		return sizeof(mixer->fatpipe_metadata);
		/* NOT REACHED */
		break;
	case PSEUDO_ADDR(fatpipe_mask):
		return sizeof(mixer->fatpipe_mask);
		/* NOT REACHED */
		break;
	case PSEUDO_ADDR(downstream_topology):
		return sizeof(mixer->downstream_topology);
		/* NOT REACHED */
		break;
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
	char *blobp = ((char *)&pseudo->mixer) + addr;
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
	char *blobp = ((char *)&pseudo->mixer) + addr;
	char *controlp = ucontrol->value.bytes.data;
	size_t sz = snd_pseudo_blob_size(kcontrol->private_value);
	int changed = 0;

	mutex_lock(&pseudo->mixer_lock);
	if (0 != memcmp(blobp, controlp, sz)) {
		memcpy(blobp, controlp, sz);
		changed = 1;
	}

	if (changed)
		snd_pseudo_mixer_update(pseudo);

	mutex_unlock(&pseudo->mixer_lock);

	return changed;
}

static int snd_pseudo_dualmono_metadata_override_get(struct snd_kcontrol
						     *kcontrol,
						     struct snd_ctl_elem_value
						     *ucontrol)
{
	unsigned int value;
	int res;

	res = stm_se_get_control(STM_SE_CTRL_STREAM_DRIVEN_DUALMONO, &value);
	if (0 == res)
		ucontrol->value.integer.value[0] = !value;

	return res;
}

static int snd_pseudo_dualmono_metadata_override_put(struct snd_kcontrol
						     *kcontrol,
						     struct snd_ctl_elem_value
						     *ucontrol)
{
	unsigned int value;
	int res;
	int changed;

	res = stm_se_get_control(STM_SE_CTRL_STREAM_DRIVEN_DUALMONO, &value);
	if (0 != res)
		return res;

	changed = (ucontrol->value.integer.value[0] != !value);
	if (changed) {
		res =
		    stm_se_set_control
		    (STM_SE_CTRL_STREAM_DRIVEN_DUALMONO,
		     !ucontrol->value.integer.value[0]);
		if (0 != res)
			return res;
	}

	return changed;
}

#define PSEUDO_SIMPLE(xname, xctrl, xindex) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name  = xname, .index = xindex,     \
	.info  = snd_pseudo_simple_info,     \
	.get   = snd_pseudo_simple_get,      \
	.put   = snd_pseudo_simple_put,      \
	.private_value = xctrl }

static int snd_pseudo_simple_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	int ctrl = kcontrol->private_value;

	switch (ctrl) {
	case STM_SE_CTRL_AUDIO_MIXER_GRAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = SND_PSEUDO_MIXER_MIN_GRAIN;
		uinfo->value.integer.max = SND_PSEUDO_MIXER_MAX_GRAIN;
		break;

	case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	case STM_SE_CTRL_AUDIO_GAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -96;
		uinfo->value.integer.max = 0;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

static int snd_pseudo_simple_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int value = 0;
	int err;

	mutex_lock(&pseudo->mixer_lock);
	err =
	    stm_se_audio_mixer_get_control(pseudo->backend_mixer, ctrl,
					   (int *)&value);
	mutex_unlock(&pseudo->mixer_lock);

	switch (ctrl) {
	case STM_SE_CTRL_AUDIO_GAIN:
		value = (value / 100);	/* convert mB into dB */
		break;
	default:
		/* nothing to do */
		break;
	}

	if (0 != err)
		printk(KERN_ERR "%s: Could not get mixer parameter %d\n",
		       pseudo->card->shortname, ctrl);
	else
		ucontrol->value.integer.value[0] = value;

	return 0;
}

static int iso_sfreq_value[] = {
	0, 8000, 11025, 12000,
	16000, 22050, 24000, 44100,
	48000, 64000, 88200, 96000,
	176400, 192000
};

static int snd_pseudo_get_sfreq_index(int sfreq)
{
	int i;
	for (i = 0; i < sizeof(iso_sfreq_value) / sizeof(int); i++)
		if (iso_sfreq_value[i] == sfreq)
			return i;

	return 0;
}

static int snd_pseudo_simple_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int newValue, oldValue;
	int err, changed = 0;

	/* retrieve actual value from mixer_mme */
	mutex_lock(&pseudo->mixer_lock);
	err =
	    stm_se_audio_mixer_get_control(pseudo->backend_mixer, ctrl,
					   (int *)&oldValue);
	mutex_unlock(&pseudo->mixer_lock);

	newValue = (int)ucontrol->value.integer.value[0];

	/* retrieve newValue */
	switch (ctrl) {
	case STM_SE_CTRL_AUDIO_MIXER_GRAIN:
		newValue =
		    (newValue <
		     SND_PSEUDO_MIXER_MAX_GRAIN) ?
		    SND_PSEUDO_MIXER_ADJUST_GRAIN(newValue) :
		    SND_PSEUDO_MIXER_MAX_GRAIN;
		break;

	case STM_SE_CTRL_AUDIO_GAIN:
		newValue *= 100;	/* convert in mB */
		/*value = (value / 100) ; *//* convert mB into dB */
		break;

	case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
		/* nothing to do */
		break;

	default:
		BUG();
		return -EINVAL;
	}

	/* update mixer_mme with newValue if needed */
	changed = (newValue != oldValue);
	if (changed) {
		mutex_lock(&pseudo->mixer_lock);
		err =
		    stm_se_audio_mixer_set_control(pseudo->backend_mixer, ctrl,
						   newValue);
		mutex_unlock(&pseudo->mixer_lock);

		if (0 != err)
			printk(KERN_ERR
			       "%s: Could not set mixer parameter %d\n",
			       pseudo->card->shortname, ctrl);
	}

	return changed;
}

/* Correspondence table between Channel_Assignement enum defined in stm_se.h
 * and local value.This is needed to display all possible channel values
 * in ALSA whereas some enum share the same value
 */
static const struct {
	/* sneaky pre-processor trick used to convert
	 * the enumerations to textual equivalents
	 */
	const char			*channel_assign_txt;
	enum stm_se_audio_channel_pair  se_channel_index;
} PseudoMixerChannelAssignment[] = {
#define CA(local_ch_ixd, se_chl_idx) { #local_ch_ixd, se_chl_idx }
CA(L_R, STM_SE_AUDIO_CHANNEL_PAIR_L_R),
CA(CNTR_LFE1, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE1),
CA(LSUR_RSUR, STM_SE_AUDIO_CHANNEL_PAIR_LSUR_RSUR),
CA(LSURREAR_RSURREAR, STM_SE_AUDIO_CHANNEL_PAIR_LSURREAR_RSURREAR),
CA(LT_RT, STM_SE_AUDIO_CHANNEL_PAIR_LT_RT),
CA(LPLII_RPLII, STM_SE_AUDIO_CHANNEL_PAIR_LPLII_RPLII),
CA(CNTRL_CNTRR, STM_SE_AUDIO_CHANNEL_PAIR_CNTRL_CNTRR),
CA(LHIGH_RHIGH, STM_SE_AUDIO_CHANNEL_PAIR_LHIGH_RHIGH),
CA(LWIDE_RWIDE, STM_SE_AUDIO_CHANNEL_PAIR_LWIDE_RWIDE),
CA(LRDUALMONO, STM_SE_AUDIO_CHANNEL_PAIR_LRDUALMONO),
CA(RESERVED1, STM_SE_AUDIO_CHANNEL_PAIR_RESERVED1),
CA(CNTR_0, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_0),
CA(0_LFE1, STM_SE_AUDIO_CHANNEL_PAIR_0_LFE1),
CA(0_LFE2, STM_SE_AUDIO_CHANNEL_PAIR_0_LFE2),
CA(CHIGH_0, STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_0),
CA(CLOWFRONT_0, STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_0),
CA(CNTR_CSURR, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CSURR),
CA(CNTR_CHIGH, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGH),
CA(CNTR_TOPSUR, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_TOPSUR),
CA(CNTR_CHIGHREAR, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGHREAR),
CA(CNTR_CLOWFRONT, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CLOWFRONT),
CA(CHIGH_TOPSUR, STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_TOPSUR),
CA(CHIGH_CHIGHREAR, STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CHIGHREAR),
CA(CHIGH_CLOWFRONT, STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CLOWFRONT),
CA(CNTR_LFE2, STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE2),
CA(CHIGH_LFE1, STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE1),
CA(CHIGH_LFE2, STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE2),
CA(CLOWFRONT_LFE1, STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE1),
CA(CLOWFRONT_LFE2, STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE2),
CA(LSIDESURR_RSIDESURR, STM_SE_AUDIO_CHANNEL_PAIR_LSIDESURR_RSIDESURR),
CA(LHIGHSIDE_RHIGHSIDE, STM_SE_AUDIO_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE),
CA(LDIRSUR_RDIRSUR, STM_SE_AUDIO_CHANNEL_PAIR_LDIRSUR_RDIRSUR),
CA(LHIGHREAR_RHIGHREAR, STM_SE_AUDIO_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR),
CA(CSURR_0, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_0),
CA(TOPSUR_0, STM_SE_AUDIO_CHANNEL_PAIR_TOPSUR_0),
CA(CSURR_TOPSUR, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_TOPSUR),
CA(CSURR_CHIGH, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGH),
CA(CSURR_CHIGHREAR, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGHREAR),
CA(CSURR_CLOWFRONT, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CLOWFRONT),
CA(CSURR_LFE1, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE1),
CA(CSURR_LFE2, STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE2),
CA(CHIGHREAR_0, STM_SE_AUDIO_CHANNEL_PAIR_CHIGHREAR_0),
CA(DSTEREO_LsRs, STM_SE_AUDIO_CHANNEL_PAIR_DSTEREO_LsRs),
CA(PAIR0, STM_SE_AUDIO_CHANNEL_PAIR_PAIR0),
CA(PAIR1, STM_SE_AUDIO_CHANNEL_PAIR_PAIR1),
CA(PAIR2, STM_SE_AUDIO_CHANNEL_PAIR_PAIR2),
CA(PAIR3, STM_SE_AUDIO_CHANNEL_PAIR_PAIR3),
CA(NOT_CONNECTED, STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED),

#undef CA
};

/* channel_assign_offset is trick because the 5 first values
 * of the enum stm_se_audio_channel_pair are identical but
 * but we have to provide them to ALSA user
 */
static int channel_assign_offset = 3;
static int snd_convert_channel_index_to_streaming_engine(int chl_idx)
{
	return (chl_idx <= channel_assign_offset ? 0 :
		(chl_idx - channel_assign_offset));
}

static int snd_snd_convert_text_to_channel_index(const char *channel_text)
{
	int index;

	for (index = 0; STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED !=
	     PseudoMixerChannelAssignment[index].se_channel_index; index++) {
		if (0 == strcmp(channel_text,
		    PseudoMixerChannelAssignment[index].channel_assign_txt))
			return index;
	}

	/* cover the delimiter itself.*/
	return index;
};

static bool snd_get_channel_index(unsigned int se_chl_idx,
					unsigned int *local_chl_idx)
{
	bool need_update = false;

	/* update the local value with the value retrieved from SE if needed */
	if (0 != se_chl_idx) {
		/* systematically updates local value with
		 * Streaming Engine value (even if not needed...)
		 */
		*local_chl_idx = (se_chl_idx + channel_assign_offset);
		need_update = true;
	} else {
		/* updates only if the local value is not synchronized with
		 * Streaming Engine value (may have been modified directly ?)
		 */
		if (*local_chl_idx > channel_assign_offset) {
			/* set default value */
			*local_chl_idx = 0;
			need_update = true;
		}
	}
	return need_update;
};

#define PSEUDO_COMPOUND_CTRL(x)   (x & 0xFFFF)
#define PSEUDO_COMPOUND_ELT(x, y) \
	(((unsigned long) y << 16) | (unsigned long) x)

#define SND_PSEUDO_MIXER_INPUT_GAIN \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT, 0)
#define SND_PSEUDO_MIXER_INPUT_PANNING \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT, 1)

#define SND_PSEUDO_MIXER_DRC_TYPE \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, 0)
#define SND_PSEUDO_MIXER_DRC_BOOST \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, 1)
#define SND_PSEUDO_MIXER_DRC_CUT \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, 2)

#define SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0 \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 0)
#define SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1 \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 1)
#define SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2 \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 2)
#define SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3 \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 3)
#define SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4 \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 4)
#define SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE \
	PSEUDO_COMPOUND_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 5)

#define PSEUDO_COMPOUND(xname, xctrl, xindex) \
{       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name  = xname, .index = xindex,     \
	.info  = snd_pseudo_compound_info,   \
	.get   = snd_pseudo_compound_get,    \
	.put   = snd_pseudo_compound_put,    \
	.private_value = (unsigned long) xctrl }

static int snd_pseudo_compound_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	static char *drc_type[] = {
		"Disabled", "Custom0", "Custom1", "Line Out", "RF Out"
	};
	static char *output_sfreq[] = {
		"Master", "8k", "11k", "12k",
		"16k", "22k", "24k", "44k",
		"48k", "64k", "88k", "96k",
		"176k", "192k"
	};
	char **texts;
	int nb_items;
	int ctrl = kcontrol->private_value;
	int input = kcontrol->id.index;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	switch (ctrl) {
	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
		    sizeof(output_sfreq) / sizeof(*output_sfreq);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			output_sfreq[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));

		break;

	case SND_PSEUDO_MIXER_INPUT_GAIN:
		switch (input) {
		case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY:
		case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY:
			uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
			uinfo->value.integer.min = Q3_13_MIN;
			uinfo->value.integer.max = Q3_13_MAX;
			break;

		default:
		/* STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX# */
		/* STM_SE_CTRL_VALUE_AUDIO_GENERATOR_X */
			uinfo->count = 1;
			uinfo->value.integer.min = -78;          // -78 dB :: 0x0001 in 16bit Q13
			uinfo->value.integer.max = Q3_13_MAX_DB; // +18 dB :: 0xFFFF in 16bit Q13
		}
		break;

	case SND_PSEUDO_MIXER_INPUT_PANNING:
		uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
		uinfo->value.integer.min = Q3_13_MIN;
		uinfo->value.integer.max = Q3_13_UNITY;
		break;

	case STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN:
		uinfo->count = 1;
		uinfo->value.integer.min = Q3_13_MIN;
		uinfo->value.integer.max = Q3_13_MAX;
		break;

	case SND_PSEUDO_MIXER_DRC_TYPE:
		texts = drc_type;
		nb_items = sizeof(drc_type) / sizeof(*drc_type);
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items;
		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);
		strlcpy(uinfo->value.enumerated.name,
			texts[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_MIXER_DRC_BOOST:
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = Q0_8_UNITY;
		break;

	case SND_PSEUDO_MIXER_DRC_CUT:
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = Q0_8_UNITY;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
		 sizeof(PseudoMixerChannelAssignment) /
		 sizeof(*PseudoMixerChannelAssignment);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			PseudoMixerChannelAssignment
			[uinfo->value.enumerated.item].channel_assign_txt,
			 sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE:
		uinfo->count = 1;
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

#define PSEUDO_MIXER_ROLE_AG_ID(x) \
		(x - STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0)
static int snd_pseudo_mixer_get_compound(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol,
					 stm_se_audio_mixer_value_t *value)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err;

	switch (ctrl) {		/* Set up audio mixer object to retrieve info */
	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4:
	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE:
		/* no preset required */
		break;

	case SND_PSEUDO_MIXER_INPUT_PANNING:
	case SND_PSEUDO_MIXER_INPUT_GAIN:
		value->input_gain.input = (stm_object_h) kcontrol->id.index;
		if (((int)value->input_gain.input >=
		     STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0)
		    && ((int)value->input_gain.input <=
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7)) {
			value->input_gain.objecthandle = (stm_object_h)
			    pseudo->audiogeneratorhandle[\
				PSEUDO_MIXER_ROLE_AG_ID(\
					 (int)value->input_gain.input)];
		}
		break;
	case STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN:
		break;

	case SND_PSEUDO_MIXER_DRC_TYPE:
	case SND_PSEUDO_MIXER_DRC_BOOST:
	case SND_PSEUDO_MIXER_DRC_CUT:
		break;

	default:
		BUG();
		return -EINVAL;
	}

	mutex_lock(&pseudo->mixer_lock);
	err =
	    stm_se_audio_mixer_get_compound_control(pseudo->backend_mixer,
						    PSEUDO_COMPOUND_CTRL(ctrl),
						    (void *)value);
	mutex_unlock(&pseudo->mixer_lock);

	if (0 != err)
		printk(KERN_ERR "%s: Could not get mixer parameter %d\n",
		       pseudo->card->shortname, ctrl);

	return 0;
}

static int snd_pseudo_compound_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	stm_se_audio_mixer_value_t value;
	int i;
	int tmp_chl_idx;

	snd_pseudo_mixer_get_compound(kcontrol, ucontrol, &value);

	switch (ctrl) {
	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
		ucontrol->value.integer.value[0] =
		    (long)snd_pseudo_get_sfreq_index(value.output_sfreq.
						     frequency);
		break;

	case SND_PSEUDO_MIXER_INPUT_PANNING:
		for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
			ucontrol->value.integer.value[i] =
			    (long)value.input_gain.panning[i];
		}
		break;

	case SND_PSEUDO_MIXER_INPUT_GAIN:

		switch ((int)value.input_gain.input) {
		case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY:
		case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY:
			for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
				ucontrol->value.integer.value[i] =
				    (long)value.input_gain.gain[i];
			}
			break;

		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_4:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_5:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_6:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7:
			for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
				int dev_idx = PSEUDO_MIXER_ROLE_AG_ID(\
						(int)value.input_gain.input);

				ucontrol->value.integer.value[i] =
					pseudo->restoregain[dev_idx];
			}
			break;
		default:	/* STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX# */
			ucontrol->value.integer.value[0] =
			    (long)value.input_gain.gain[0];
		}
		break;

	case STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN:
		ucontrol->value.integer.value[0] =
		    (long)value.output_gain.gain[0];
		break;

	case SND_PSEUDO_MIXER_DRC_TYPE:
		ucontrol->value.integer.value[0] = (long)value.drc.mode;
		break;

	case SND_PSEUDO_MIXER_DRC_BOOST:
		ucontrol->value.integer.value[0] = (long)value.drc.boost;
		break;

	case SND_PSEUDO_MIXER_DRC_CUT:
		ucontrol->value.integer.value[0] = (long)value.drc.cut;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0:
	    tmp_chl_idx = pseudo->mixer.MixerChannelAssignment.pair0;
	    if (snd_get_channel_index(value.speaker_config.pair0,
					&tmp_chl_idx)) {
		/* update local value if needed */
		pseudo->mixer.MixerChannelAssignment.pair0 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		(long) pseudo->mixer.MixerChannelAssignment.pair0;
	    break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1:
	    tmp_chl_idx = pseudo->mixer.MixerChannelAssignment.pair1;
	    if (snd_get_channel_index(value.speaker_config.pair1,
					&tmp_chl_idx)) {
		/* update local value if needed */
		pseudo->mixer.MixerChannelAssignment.pair1 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		(long) pseudo->mixer.MixerChannelAssignment.pair1;
	    break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2:
	    tmp_chl_idx = pseudo->mixer.MixerChannelAssignment.pair2;
	    if (snd_get_channel_index(value.speaker_config.pair2,
					&tmp_chl_idx)) {
		/* update local value if needed */
		pseudo->mixer.MixerChannelAssignment.pair2 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		(long) pseudo->mixer.MixerChannelAssignment.pair2;
	    break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3:
	    tmp_chl_idx = pseudo->mixer.MixerChannelAssignment.pair3;
	    if (snd_get_channel_index(value.speaker_config.pair3,
					 &tmp_chl_idx)) {
		/* update local value if needed */
		pseudo->mixer.MixerChannelAssignment.pair3 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		(long) pseudo->mixer.MixerChannelAssignment.pair3;
	    break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4:
	    tmp_chl_idx = pseudo->mixer.MixerChannelAssignment.pair4;
	    if (snd_get_channel_index(value.speaker_config.pair4,
					&tmp_chl_idx)) {
		/* update local value if needed */
		pseudo->mixer.MixerChannelAssignment.pair4 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		(long) pseudo->mixer.MixerChannelAssignment.pair4;
	    break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE:
	    ucontrol->value.integer.value[0] =
		!(long)value.speaker_config.malleable;
	    break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

#define SND_PSEUDO_MIXER_SET_INPUT_GAINS(x, in, out, type)  \
	x.input       = (stm_object_h) in;			    \
	x.output      = (stm_object_h) out;

static int snd_pseudo_compound_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int changed = 0;
	long newValue = ucontrol->value.integer.value[0];
	long *newSet = &ucontrol->value.integer.value[0];
	stm_se_audio_mixer_value_t value;
	int err;
	int i;
	int channel_assign_index;

	snd_pseudo_mixer_get_compound(kcontrol, ucontrol, &value);

	/* update mixer datas */
	switch (ctrl) {
	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
		newValue = iso_sfreq_value[(int)newValue];
		changed = newValue != value.output_sfreq.frequency;
		value.output_sfreq.control =
		    (newValue !=
		     0) ? STM_SE_FIXED_OUTPUT_FREQUENCY :
		    STM_SE_MAX_OUTPUT_FREQUENCY;
		value.output_sfreq.frequency = (uint32_t) newValue;
		break;

	case SND_PSEUDO_MIXER_INPUT_PANNING:
		for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
			changed = changed
			    || (value.input_gain.panning[i] !=
				(stm_se_q3dot13_t) newSet[i]);
			value.input_gain.panning[i] =
			    (stm_se_q3dot13_t) newSet[i];
		}
		break;

	case SND_PSEUDO_MIXER_INPUT_GAIN:
		switch ((int)value.input_gain.input) {

		// For Primary and Secondary, the gain is given in Linear Q3_13
		case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY:
		case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY:
			for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
				stm_se_q3dot13_t gain = (stm_se_q3dot13_t) newSet[i];
				changed |= (value.input_gain.gain[i] != gain);
				value.input_gain.gain[i] = gain;
			}
			break;

		// For Application Sound, the gain is given in dB
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_4:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_5:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_6:
		case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7:
			value.input_gain.objecthandle = (stm_object_h)
			    pseudo->audiogeneratorhandle[\
					PSEUDO_MIXER_ROLE_AG_ID(\
						(int)value.input_gain.input)];

			for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
				int  input     = (int)value.input_gain.input;
				int  dev_idx   = PSEUDO_MIXER_ROLE_AG_ID(input);
				long gain_db, gain_lin;
				// Limit the requested gain to the Max gain
				// that can be mapped in Q3_13.
				gain_db  = MAX_Q3_13_DB(newSet[0]);
				gain_lin = convert_mb2linear_q13(newSet[0] * 100);

				changed |= (pseudo->restoregain[dev_idx] != gain_db);

				pseudo->restoregain[dev_idx]         = gain_db;
				pseudo->audiogengainrestore[dev_idx] = gain_lin;
				value.input_gain.gain[i]             = gain_lin;
			}
			break;

		// For Interactive-Audio, the gain is given in Linear Q3_13
		default:	/* STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX# */
			{
				stm_se_q3dot13_t gain = (stm_se_q3dot13_t) newSet[0];
				changed = (value.input_gain.gain[0] != gain);
				value.input_gain.gain[0] = gain;
			}
			break;

		}
		break;

	case STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN:
		changed =
		    (value.output_gain.gain[0] != (stm_se_q3dot13_t) newValue);
		value.output_gain.gain[0] = (stm_se_q3dot13_t) newValue;

		break;

	case SND_PSEUDO_MIXER_DRC_TYPE:
		changed = (value.drc.mode != (int)newValue);
		value.drc.mode = (int)newValue;
		break;

	case SND_PSEUDO_MIXER_DRC_BOOST:
		changed = (value.drc.boost != (uint8_t) newValue);
		value.drc.boost = (uint8_t) newValue;
		break;
	case SND_PSEUDO_MIXER_DRC_CUT:
		changed = (value.drc.cut != (uint8_t) newValue);
		value.drc.cut = (uint8_t) newValue;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0:
		/* store the value locally (full info) */
		pseudo->mixer.MixerChannelAssignment.pair0 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair0 !=
					(int)channel_assign_index);
		value.speaker_config.pair0 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1:
		/* store the value locally (full info) */
		pseudo->mixer.MixerChannelAssignment.pair1 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair1 !=
					(int)channel_assign_index);
		value.speaker_config.pair1 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2:
		/* store the value locally (full info) */
		pseudo->mixer.MixerChannelAssignment.pair2 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair2 !=
					(int)channel_assign_index);
		value.speaker_config.pair2 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3:
		/* store the value locally (full info) */
		pseudo->mixer.MixerChannelAssignment.pair3 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair3 !=
					(int)channel_assign_index);
		value.speaker_config.pair3 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4:
		/* store the value locally (full info) */
		pseudo->mixer.MixerChannelAssignment.pair4 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair4 !=
					(int)channel_assign_index);
		value.speaker_config.pair4 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE:
		changed = (value.speaker_config.malleable != !(int)newValue);
		value.speaker_config.malleable = !(int)newValue;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	if (changed) {
		mutex_lock(&pseudo->mixer_lock);
		err =
		    stm_se_audio_mixer_set_compound_control
		    (pseudo->backend_mixer, PSEUDO_COMPOUND_CTRL(ctrl),
		     (const void *)
		     &value);
		mutex_unlock(&pseudo->mixer_lock);

		if (0 != err)
			printk(KERN_ERR
			       "%s: Could not set mixer parameter %d\n",
			       pseudo->card->shortname, ctrl);
	}

	return changed;
}

#define PSEUDO_SIMPLE_PLAYER(xname, xctrl) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name  = xname, \
	.index = 0,     \
	.info  = snd_pseudo_player_simple_info,   \
	.get   = snd_pseudo_player_simple_get,    \
	.put   = snd_pseudo_player_simple_put,    \
	.private_value = (unsigned long) xctrl }

static int snd_pseudo_player_simple_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	static char *dual_mono[] = {
		"stereo_out", "left_out", "right_out", "mixed_out"
	};
	int nb_items;
	int ctrl = kcontrol->private_value;

	switch (ctrl) {
	case STM_SE_CTRL_AUDIO_GAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -96;
		uinfo->value.integer.max = 32;
		break;

	case STM_SE_CTRL_AUDIO_DELAY:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 500;
		break;

	case STM_SE_CTRL_AUDIO_SOFTMUTE:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	case STM_SE_CTRL_DUALMONO:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
		    sizeof(dual_mono) / sizeof(*dual_mono);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			dual_mono[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;

	case STM_SE_CTRL_STREAM_DRIVEN_DUALMONO:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	case STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -31;
		uinfo->value.integer.max = 0;
		break;

	case STM_SE_CTRL_DCREMOVE:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

static int snd_pseudo_player_simple_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int value = 0;
	int err;

	if (player->mixer)
		mutex_lock(&player->mixer->mixer_lock);
	err = stm_se_audio_player_get_control(player->backend_player, ctrl,
					      (int32_t *) &value);
	if (player->mixer)
		mutex_unlock(&player->mixer->mixer_lock);

	switch (ctrl) {
	case STM_SE_CTRL_AUDIO_GAIN:
		value = (value / 100);	/* convert mB into dB */
		break;
	default:
		/* nothing to do */
		break;
	}

	if (0 != err)
		printk(KERN_ERR "Could not get %s player parameter %d\n",
		       player->name, ctrl);
	else
		ucontrol->value.integer.value[0] = (long)value;
	return 0;
}

static int snd_pseudo_player_simple_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int changed = 0;
	int oldvalue;
	int err;

	/* new value to set */
	long newValue = ucontrol->value.integer.value[0];

	/* retrieve actual value from audio-player */
	if (player->mixer)
		mutex_lock(&player->mixer->mixer_lock);
	err = stm_se_audio_player_get_control(player->backend_player, ctrl,
					      (int32_t *) &oldvalue);
	if (player->mixer)
		mutex_unlock(&player->mixer->mixer_lock);

	switch (ctrl) {
	case STM_SE_CTRL_AUDIO_GAIN:
		newValue *= 100;	/* convert in mB */
		changed = newValue != oldvalue;
		break;

	case STM_SE_CTRL_AUDIO_DELAY:
		changed = newValue != oldvalue;
		break;

	case STM_SE_CTRL_AUDIO_SOFTMUTE:
		changed = newValue != oldvalue;
		break;

	case STM_SE_CTRL_DUALMONO:
		changed = newValue != oldvalue;
		break;

	case STM_SE_CTRL_STREAM_DRIVEN_DUALMONO:
		changed = newValue != oldvalue;
		break;

	case STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL:
		newValue *= 100; /* convert in mB */
		changed = newValue != oldvalue;
		break;

	case STM_SE_CTRL_DCREMOVE:
		changed = newValue != oldvalue;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	if (changed) {
		if (player->mixer)
			mutex_lock(&player->mixer->mixer_lock);
		err = stm_se_audio_player_set_control(player->backend_player,
							ctrl,
							(int32_t) newValue);
		if (player->mixer)
			mutex_unlock(&player->mixer->mixer_lock);

		if (0 != err) {
			printk(KERN_ERR
			       "%s: Could not set mixer parameter %d\n",
			       player->name, ctrl);
			changed = 0;
		}
	}

	return changed;
}

#define PSEUDO_COMPOUND_PLAYER_CTRL(x)   (x & 0xFFFF)
#define PSEUDO_COMPOUND_PLAYER_ELT(x, y)\
		(((unsigned long) y << 16) | (unsigned long) x)

#define SND_PSEUDO_PLAYER_LIMITER_HARD_GAIN \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_LIMITER, 0)
#define SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD_ENABLE \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_LIMITER, 1)
#define SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_LIMITER, 2)

#define SND_PSEUDO_PLAYER_HW_SINK_TYPE \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE, 0)
#define SND_PSEUDO_PLAYER_HW_NUM_CHANNELS \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE, 1)
#define SND_PSEUDO_PLAYER_HW_PLAYBACK_MODE \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE, 2)

#define SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR0 \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 0)
#define SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR1 \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 1)
#define SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR2 \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 2)
#define SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR3 \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 3)
#define SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR4 \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_SPEAKER_CONFIG, 4)

#define SND_PSEUDO_PLAYER_BASSMGT_CONTROL \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_BASSMGT, 0)
#define SND_PSEUDO_PLAYER_BASSMGT_GAIN \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_BASSMGT, 1)
#define SND_PSEUDO_PLAYER_BASSMGT_DELAY \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_BASSMGT, 2)

#define SND_PSEUDO_PLAYER_BTSC_INPUT_GAIN \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_BTSC, 0)
#define SND_PSEUDO_PLAYER_BTSC_TX_GAIN \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_BTSC, 1)
#define SND_PSEUDO_PLAYER_BTSC_DUAL_SIGNAL \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_BTSC, 2)

/* Drc cut boost factor per output based */
#define SND_PSEUDO_PLAYER_DRCTYPE \
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION,\
				   0)
#define SND_PSEUDO_PLAYER_DRCCUT\
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION,\
				   1)
#define SND_PSEUDO_PLAYER_DRCBOOST\
	PSEUDO_COMPOUND_PLAYER_ELT(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION,\
				   2)

#define PSEUDO_COMPOUND_PLAYER(xname, xctrl) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name  = xname, .index = 0,                 \
	.info  = snd_pseudo_player_compound_info,   \
	.get   = snd_pseudo_player_compound_get,    \
	.put   = snd_pseudo_player_compound_put,    \
	.private_value = (unsigned long) xctrl }

static int snd_pseudo_player_compound_info(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_info *uinfo)
{
	static char *drc_type[] = {
		"Disabled", "Custom0", "Custom1", "Line Out", "RF Out"
	};

	static char *player_max_freq[] = {
		"Master", "8k", "11k", "12k",
		"16k", "22k", "24k", "44k",
		"48k", "64k", "88k", "96k",
		"176k", "192k"
	};

	static char *sink_hints[] = {
		"Auto", "TV", "AVR", "Headphone"
	};

	static char *hw_player_mode[] = {
		"PCM", "COMPRESSED", "COMPRESSED_SD", "AC3", "DTS", "BTSC"
	};

	static char *bassmgt_control_mode[] = {
		"OFF", "BALANCE", "DOLBY1", "DOLBY2", "DOLBY3"
	};

	char **texts;
	int nb_items;
	int ctrl = kcontrol->private_value;

	switch (ctrl) {
	case SND_PSEUDO_PLAYER_LIMITER_HARD_GAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD_ENABLE:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 32;
		break;

	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
		    sizeof(player_max_freq) / sizeof(*player_max_freq);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			player_max_freq[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_PLAYER_HW_SINK_TYPE:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
		    sizeof(sink_hints) / sizeof(*sink_hints);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			sink_hints[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_PLAYER_HW_NUM_CHANNELS:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 10;
		break;

	case SND_PSEUDO_PLAYER_HW_PLAYBACK_MODE:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
		    sizeof(hw_player_mode) / sizeof(*hw_player_mode);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			hw_player_mode[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR0:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR1:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR2:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR3:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR4:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
			sizeof(PseudoMixerChannelAssignment) /
				sizeof(*PseudoMixerChannelAssignment);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			PseudoMixerChannelAssignment
			[uinfo->value.enumerated.item].channel_assign_txt,
			sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_CONTROL:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items =
			sizeof(bassmgt_control_mode) /
				sizeof(*bassmgt_control_mode);

		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);

		strlcpy(uinfo->value.enumerated.name,
			bassmgt_control_mode[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_GAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
		uinfo->value.integer.min = -96;	/* dB */
		uinfo->value.integer.max = 0;	/* dB */
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_DELAY:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = SND_PSEUDO_MIXER_CHANNELS;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 30;	/* msec */
		break;

	case SND_PSEUDO_PLAYER_BTSC_INPUT_GAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -9600;
		uinfo->value.integer.max = 0;
		break;

	case SND_PSEUDO_PLAYER_BTSC_TX_GAIN:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = -9600;
		uinfo->value.integer.max = 0;
		break;

	case SND_PSEUDO_PLAYER_BTSC_DUAL_SIGNAL:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
		break;

	case SND_PSEUDO_PLAYER_DRCTYPE:
		texts = drc_type;
		nb_items = sizeof(drc_type) / sizeof(*drc_type);
		uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
		uinfo->count = 1;
		uinfo->value.enumerated.items = nb_items;
		if (uinfo->value.enumerated.item > (nb_items - 1))
			uinfo->value.enumerated.item = (nb_items - 1);
		strlcpy(uinfo->value.enumerated.name,
			texts[uinfo->value.enumerated.item],
			sizeof(uinfo->value.enumerated.name));
		break;
	case SND_PSEUDO_PLAYER_DRCCUT:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = Q0_8_UNITY;
		break;
	case SND_PSEUDO_PLAYER_DRCBOOST:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = Q0_8_UNITY;
		break;


	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

static int snd_pseudo_player_get_compound(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol,
					  stm_se_audio_mixer_value_t *value)
{
	struct snd_pseudo_hw_player_entity *player = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err;

	switch (ctrl) {
	case SND_PSEUDO_PLAYER_LIMITER_HARD_GAIN:
	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD_ENABLE:
	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD:
	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
	case SND_PSEUDO_PLAYER_HW_SINK_TYPE:
	case SND_PSEUDO_PLAYER_HW_NUM_CHANNELS:
	case SND_PSEUDO_PLAYER_HW_PLAYBACK_MODE:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR0:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR1:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR2:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR3:
	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR4:
	case SND_PSEUDO_PLAYER_BASSMGT_CONTROL:
	case SND_PSEUDO_PLAYER_BASSMGT_GAIN:
	case SND_PSEUDO_PLAYER_BASSMGT_DELAY:
	case SND_PSEUDO_PLAYER_BTSC_INPUT_GAIN:
	case SND_PSEUDO_PLAYER_BTSC_TX_GAIN:
	case SND_PSEUDO_PLAYER_BTSC_DUAL_SIGNAL:
	case SND_PSEUDO_PLAYER_DRCTYPE:
	case SND_PSEUDO_PLAYER_DRCCUT:
	case SND_PSEUDO_PLAYER_DRCBOOST:
		/* no preset required */
		break;

	default:
		BUG();
		return -EINVAL;
	}

	if (player->mixer != NULL)
		mutex_lock(&player->mixer->mixer_lock);
	err =
	    stm_se_audio_player_get_compound_control(player->backend_player,
						     PSEUDO_COMPOUND_PLAYER_CTRL
						     (ctrl), (void *)value);
	if (player->mixer != NULL)
		mutex_unlock(&player->mixer->mixer_lock);

	if (0 != err)
		printk(KERN_ERR "Could not get %s player parameter %d\n",
			player->name, ctrl);

	return 0;
}

static int snd_pseudo_player_compound_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	stm_se_audio_mixer_value_t value;
	int tmp_chl_idx;
	int i;
	snd_pseudo_player_get_compound(kcontrol, ucontrol, &value);

	switch (ctrl) {
	case SND_PSEUDO_PLAYER_LIMITER_HARD_GAIN:
		ucontrol->value.integer.value[0] =
		    (long)value.limiter.hard_gain;
		break;

	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD_ENABLE:
		ucontrol->value.integer.value[0] =
		    (long)value.limiter.lookahead_enable;
		break;

	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD:
		ucontrol->value.integer.value[0] =
		    (long)value.limiter.lookahead;
		break;

	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
		ucontrol->value.integer.value[0] =
		    (long)snd_pseudo_get_sfreq_index(value.output_sfreq.
						     frequency);
		break;

	case SND_PSEUDO_PLAYER_HW_SINK_TYPE:
		ucontrol->value.integer.value[0] =
		    (long)value.player_hardware_mode.sink_type;
		break;

	case SND_PSEUDO_PLAYER_HW_NUM_CHANNELS:
		ucontrol->value.integer.value[0] =
		    (long)value.player_hardware_mode.num_channels;
		break;

	case SND_PSEUDO_PLAYER_HW_PLAYBACK_MODE:
		ucontrol->value.integer.value[0] =
		    (long)value.player_hardware_mode.playback_mode;
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR0:
	    tmp_chl_idx = player->card->channel_assignment.pair0;
	    if (snd_get_channel_index(value.speaker_config.pair0,
							 &tmp_chl_idx)) {
		/* update local value if needed */
		player->card->channel_assignment.pair0 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		 (long) player->card->channel_assignment.pair1;
	    break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR1:
	    tmp_chl_idx = player->card->channel_assignment.pair1;
	    if (snd_get_channel_index(value.speaker_config.pair1,
							 &tmp_chl_idx)) {
		/* update local value if needed */
		player->card->channel_assignment.pair1 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		 (long) player->card->channel_assignment.pair1;
	    break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR2:
	    tmp_chl_idx = player->card->channel_assignment.pair2;
	    if (snd_get_channel_index(value.speaker_config.pair2,
							&tmp_chl_idx)) {
		/* update local value if needed */
		player->card->channel_assignment.pair2 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		 (long) player->card->channel_assignment.pair2;
	    break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR3:
	    tmp_chl_idx = player->card->channel_assignment.pair3;
	    if (snd_get_channel_index(value.speaker_config.pair3,
							 &tmp_chl_idx)) {
		/* update local value if needed */
		player->card->channel_assignment.pair3 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		(long) player->card->channel_assignment.pair3;
	    break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR4:
	    tmp_chl_idx = player->card->channel_assignment.pair4;
	    if (snd_get_channel_index(value.speaker_config.pair4,
							&tmp_chl_idx)) {
		/* update local value if needed */
		player->card->channel_assignment.pair4 = tmp_chl_idx;
	    }
	    ucontrol->value.integer.value[0] =
		 (long) player->card->channel_assignment.pair4;
	    break;

	case SND_PSEUDO_PLAYER_BASSMGT_CONTROL:
		ucontrol->value.integer.value[0] = value.bassmgt.apply ?
						  (value.bassmgt.type+1) : 0;
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_GAIN:
		for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
			/*convert mB into dB */
			ucontrol->value.integer.value[i] =\
				((long)value.bassmgt.gain[i]) / 100;
		}
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_DELAY:
		for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
			ucontrol->value.integer.value[i] =
			    (long)value.bassmgt.delay[i];
		}
		break;

	case SND_PSEUDO_PLAYER_BTSC_INPUT_GAIN:
		ucontrol->value.integer.value[0] = (long)value.btsc.input_gain;
		break;

	case SND_PSEUDO_PLAYER_BTSC_TX_GAIN:
		ucontrol->value.integer.value[0] = (long)value.btsc.tx_gain;
		break;

	case SND_PSEUDO_PLAYER_BTSC_DUAL_SIGNAL:
		ucontrol->value.integer.value[0] = (bool)value.btsc.dual_signal;
		break;

	case SND_PSEUDO_PLAYER_DRCTYPE:
		ucontrol->value.integer.value[0] = (long)value.drc.mode;
		break;

	case SND_PSEUDO_PLAYER_DRCCUT:
		ucontrol->value.integer.value[0] = (long)value.drc.cut;
		break;

	case SND_PSEUDO_PLAYER_DRCBOOST:
		ucontrol->value.integer.value[0] = (long)value.drc.boost;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

static int snd_pseudo_player_compound_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int changed = 0;
	long newValue = ucontrol->value.integer.value[0];
	long *newSet = &ucontrol->value.integer.value[0];
	stm_se_audio_mixer_value_t value;
	int channel_assign_index;
	int err, i;

	snd_pseudo_player_get_compound(kcontrol, ucontrol, &value);

	/* update mixer datas */
	switch (ctrl) {

	case SND_PSEUDO_PLAYER_LIMITER_HARD_GAIN:
		changed = (value.limiter.hard_gain != (bool) newValue);
		value.limiter.hard_gain = (bool) newValue;
		break;

	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD_ENABLE:
		changed = 
		  (value.limiter.lookahead_enable != (uint32_t)newValue);
		value.limiter.lookahead_enable = (uint32_t)newValue;
		break;

	case SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD:
		changed = (value.limiter.lookahead != (uint32_t) newValue);
		value.limiter.lookahead = (uint32_t) newValue;
		break;

	case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
		newValue = iso_sfreq_value[(int)newValue];
		changed = newValue != value.output_sfreq.frequency;
		value.output_sfreq.control = STM_SE_MAX_OUTPUT_FREQUENCY;
		value.output_sfreq.frequency = (uint32_t) newValue;
		break;

	case SND_PSEUDO_PLAYER_HW_SINK_TYPE:
		changed =
		    (value.player_hardware_mode.sink_type != (int)newValue);
		value.player_hardware_mode.sink_type = (int)newValue;
		break;

	case SND_PSEUDO_PLAYER_HW_NUM_CHANNELS:
		changed =
		    (value.player_hardware_mode.num_channels != (int)newValue);
		value.player_hardware_mode.num_channels = (int)newValue;
		break;

	case SND_PSEUDO_PLAYER_HW_PLAYBACK_MODE:
		changed =
		    (value.player_hardware_mode.playback_mode != (int)newValue);
		value.player_hardware_mode.playback_mode = (int)newValue;
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR0:
		/* store the value locally (full info) */
		player->card->channel_assignment.pair0 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair0 !=
					(int)channel_assign_index);
		value.speaker_config.pair0 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR1:
		/* store the value locally (full info) */
		player->card->channel_assignment.pair1 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair1 !=
					(int)channel_assign_index);
		value.speaker_config.pair1 =
				(int)channel_assign_index;
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR2:
		/* store the value locally (full info) */
		player->card->channel_assignment.pair2 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair2 !=
					(int)channel_assign_index);
		value.speaker_config.pair2 =
				(int)channel_assign_index;
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR3:
		/* store the value locally (full info) */
		player->card->channel_assignment.pair3 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair3 !=
					(int)channel_assign_index);
		value.speaker_config.pair3 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR4:
		/* store the value locally (full info) */
		player->card->channel_assignment.pair4 = (int)newValue;
		/* convert channel_assign_index to send to StreamingEngine
		 * some part of the info is lost as the 5 first values
		 * correspond to only one enum value (=0)
		 */
		channel_assign_index =
		snd_convert_channel_index_to_streaming_engine((int)newValue);
		changed = (value.speaker_config.pair4 !=
					 (int)channel_assign_index);
		value.speaker_config.pair4 = (int)channel_assign_index;
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_CONTROL:
		if (!newValue && value.bassmgt.apply)
			changed = true;
		else if (newValue && (newValue != value.bassmgt.type+1))
			changed = true;
		else
			changed = false;

		value.bassmgt.apply = (bool) newValue;
		value.bassmgt.type  = newValue-1;
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_GAIN:
		for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
			/* convert dB into mB */
			changed = changed ||\
				  (value.bassmgt.gain[i] != (newSet[i] * 100));
			/* convert dB into mB */
			value.bassmgt.gain[i] = newSet[i] * 100;
		}
		break;

	case SND_PSEUDO_PLAYER_BASSMGT_DELAY:
		for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++) {
			changed = changed
			    || (value.bassmgt.delay[i] != newSet[i]);
			value.bassmgt.delay[i] = newSet[i];
		}
		break;

	case SND_PSEUDO_PLAYER_BTSC_INPUT_GAIN:
		changed = (value.btsc.input_gain != (int32_t)newValue);
		value.btsc.input_gain = (int32_t)newValue;
		break;

	case SND_PSEUDO_PLAYER_BTSC_TX_GAIN:
		changed = (value.btsc.tx_gain != (int32_t)newValue);
		value.btsc.tx_gain = (int32_t)newValue;
		break;

	case SND_PSEUDO_PLAYER_BTSC_DUAL_SIGNAL:
		changed = (value.btsc.dual_signal != (bool)newValue);
		value.btsc.dual_signal = (bool)newValue;
		break;
	case SND_PSEUDO_PLAYER_DRCTYPE:
		changed = (value.drc.mode != (int)newValue);
		value.drc.mode = (int)newValue;
		break;

	case SND_PSEUDO_PLAYER_DRCCUT:
		changed = (value.drc.cut != (uint8_t) newValue);
		value.drc.cut = (uint8_t) newValue;
		break;
	case SND_PSEUDO_PLAYER_DRCBOOST:
		changed = (value.drc.boost != (uint8_t) newValue);
		value.drc.boost = (uint8_t) newValue;
		break;

	default:
		BUG();
		return -EINVAL;
	}

	if (!changed)
		/* Nothing changed -> exit */
		return changed;

	if (player->mixer)
		mutex_lock(&player->mixer->mixer_lock);

	err = stm_se_audio_player_set_compound_control(player->backend_player,
						     PSEUDO_COMPOUND_PLAYER_CTRL
						     (ctrl),
						     (const void *)
						     &value);

	if (player->mixer)
		mutex_unlock(&player->mixer->mixer_lock);

	if (0 != err)
		printk(KERN_ERR
		       "Could not set %s player parameter %d\n",
		       player->name, ctrl);

	return changed;
}
static struct snd_kcontrol_new snd_bcast_controls[] = {
/* pre-mix gain control */
	PSEUDO_COMPOUND("Primary Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY),
	PSEUDO_COMPOUND("Secondary Playback Volume",
			SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY),
	PSEUDO_COMPOUND("Secondary Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY),
	PSEUDO_COMPOUND("PCM0 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0),
	PSEUDO_COMPOUND("PCM1 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1),
	PSEUDO_COMPOUND("PCM2 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2),
	PSEUDO_COMPOUND("PCM3 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3),
	PSEUDO_COMPOUND("PCM4 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0),
	PSEUDO_ROUTE("Metadata Update Playback Route", 0, metadata_update),
	PSEUDO_SIMPLE("Master Grain", STM_SE_CTRL_AUDIO_MIXER_GRAIN, 0),
	PSEUDO_COMPOUND("Master Output Frequency",
			STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY, 0),

/* post-mix gain control */
	PSEUDO_COMPOUND("Post Mix Playback Volume",
			STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_MASTER),
	PSEUDO_SIMPLE("Master Playback Volume", STM_SE_CTRL_AUDIO_GAIN, 0),

/* switches */
	PSEUDO_SWITCH("All Speaker Stereo Playback Switch", 0,
		      all_speaker_stereo_enable),
	PSEUDO_SIMPLE("Downmix Streamdriven Stereo Playback Switch",
		      STM_SE_CTRL_STREAM_DRIVEN_STEREO, 0),

/* routes */
	PSEUDO_ROUTE("Interactive Audio Mode Playback Route", 0,
		     interactive_audio_mode),

/* latency tuning */
	PSEUDO_INTEGER("Master Playback Latency", 0, master_latency),

/* drc settings */
	PSEUDO_COMPOUND("DRC Type", SND_PSEUDO_MIXER_DRC_TYPE, 0),
	PSEUDO_COMPOUND("DRC Boost", SND_PSEUDO_MIXER_DRC_BOOST, 0),
	PSEUDO_COMPOUND("DRC Cut", SND_PSEUDO_MIXER_DRC_CUT, 0),

/* generic spdif meta data */
	PSEUDO_IEC958("IEC958 Playback Default", 0, iec958_metadata),
	PSEUDO_IEC958_READONLY("IEC958 Playback Mask", 0, iec958_mask),

/* Cascaded controls */
	PSEUDO_DUALMONO_OVERRIDE("DualMono Metadata Override Playback Switch",
				 0),
/* Mixer Channel Assignment */
	PSEUDO_COMPOUND("Mixer Channel Pair0",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair1",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair2",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair3",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair4",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4, 0),
	PSEUDO_COMPOUND("Mixer Downmix Promotion",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE, 0),
};

static struct snd_kcontrol_new snd_pseudo_controls[] = {
/* pre-mix gain control */
	PSEUDO_COMPOUND("Primary Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY),
	PSEUDO_COMPOUND("Secondary Playback Volume",
			SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY),
	PSEUDO_COMPOUND("Secondary Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY),
	PSEUDO_COMPOUND("PCM0 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0),
	PSEUDO_COMPOUND("PCM1 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1),
	PSEUDO_COMPOUND("PCM2 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2),
	PSEUDO_COMPOUND("PCM3 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3),
	PSEUDO_COMPOUND("PCM4 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_4),
	PSEUDO_COMPOUND("PCM5 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_5),
	PSEUDO_COMPOUND("PCM6 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_6),
	PSEUDO_COMPOUND("PCM7 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7),
	PSEUDO_COMPOUND("iPCM0 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0),
	PSEUDO_COMPOUND("iPCM0 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0),
	PSEUDO_COMPOUND("iPCM1 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX1),
	PSEUDO_COMPOUND("iPCM1 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX1),
	PSEUDO_COMPOUND("iPCM2 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX2),
	PSEUDO_COMPOUND("iPCM2 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX2),
	PSEUDO_COMPOUND("iPCM3 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX3),
	PSEUDO_COMPOUND("iPCM3 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX3),
	PSEUDO_COMPOUND("iPCM4 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX4),
	PSEUDO_COMPOUND("iPCM4 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX4),
	PSEUDO_COMPOUND("iPCM5 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX5),
	PSEUDO_COMPOUND("iPCM5 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX5),
	PSEUDO_COMPOUND("iPCM6 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX6),
	PSEUDO_COMPOUND("iPCM6 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX6),
	PSEUDO_COMPOUND("iPCM7 Playback Volume", SND_PSEUDO_MIXER_INPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX7),
	PSEUDO_COMPOUND("iPCM7 Pan Playback Volume",
			SND_PSEUDO_MIXER_INPUT_PANNING,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX7),

	PSEUDO_ROUTE("Metadata Update Playback Route", 0, metadata_update),
	PSEUDO_SIMPLE("Master Grain", STM_SE_CTRL_AUDIO_MIXER_GRAIN, 0),
	PSEUDO_COMPOUND("Master Output Frequency",
			STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY, 0),

/* post-mix gain control */
	PSEUDO_COMPOUND("Post Mix Playback Volume",
			STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN,
			STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_MASTER),
	PSEUDO_SIMPLE("Master Playback Volume", STM_SE_CTRL_AUDIO_GAIN, 0),

/* switches */
	PSEUDO_SWITCH("All Speaker Stereo Playback Switch", 0,
		      all_speaker_stereo_enable),
	PSEUDO_SIMPLE("Downmix Streamdriven Stereo Playback Switch",
		      STM_SE_CTRL_STREAM_DRIVEN_STEREO, 0),

/* routes */
	PSEUDO_ROUTE("Interactive Audio Mode Playback Route", 0,
		     interactive_audio_mode),

/* latency tuning */
	PSEUDO_INTEGER("Master Playback Latency", 0, master_latency),

/* drc settings */
	PSEUDO_COMPOUND("DRC Type", SND_PSEUDO_MIXER_DRC_TYPE, 0),
	PSEUDO_COMPOUND("DRC Boost", SND_PSEUDO_MIXER_DRC_BOOST, 0),
	PSEUDO_COMPOUND("DRC Cut", SND_PSEUDO_MIXER_DRC_CUT, 0),

/* generic spdif meta data */
	PSEUDO_IEC958("IEC958 Playback Default", 0, iec958_metadata),
	PSEUDO_IEC958_READONLY("IEC958 Playback Mask", 0, iec958_mask),

/* fatpipe meta data */
	PSEUDO_BLOB("FatPipe Playback Default", 0, fatpipe_metadata),
	PSEUDO_BLOB_READONLY("FatPipe Playback Mask", 0, fatpipe_mask),

/* Cascaded controls */
	PSEUDO_DUALMONO_OVERRIDE("DualMono Metadata Override Playback Switch",
				 0),
/* Mixer Channel Assignment */
	PSEUDO_COMPOUND("Mixer Channel Pair0",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR0, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair1",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR1, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair2",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR2, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair3",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR3, 0),
	PSEUDO_COMPOUND("Mixer Channel Pair4",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_PAIR4, 0),
	PSEUDO_COMPOUND("Mixer Downmix Promotion",
			SND_PSEUDO_MIXER_CHANNEL_ASSIGN_MALLEABLE, 0),
};

static struct snd_kcontrol_new snd_pseudo_dynamic_controls[] = {

	/* 0 */
	PSEUDO_COMPOUND_PLAYER("Limiter Hard Gain",
			       SND_PSEUDO_PLAYER_LIMITER_HARD_GAIN),

	PSEUDO_COMPOUND_PLAYER("Limiter Lookahead Enable",
			       SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD_ENABLE),

	PSEUDO_COMPOUND_PLAYER("Limiter Lookahead Amount",
			       SND_PSEUDO_PLAYER_LIMITER_LOOKAHEAD),

	PSEUDO_SIMPLE_PLAYER("Gain", STM_SE_CTRL_AUDIO_GAIN),

	/*4 */
	PSEUDO_SIMPLE_PLAYER("Soft Mute", STM_SE_CTRL_AUDIO_SOFTMUTE),

	PSEUDO_SIMPLE_PLAYER("Delay", STM_SE_CTRL_AUDIO_DELAY),

	PSEUDO_COMPOUND_PLAYER("BTSC Input Gain",
			       SND_PSEUDO_PLAYER_BTSC_INPUT_GAIN),

	PSEUDO_COMPOUND_PLAYER("BTSC Tx Gain",
			       SND_PSEUDO_PLAYER_BTSC_TX_GAIN),

	/* 8 */
	PSEUDO_COMPOUND_PLAYER("BTSC Dual Signal",
			       SND_PSEUDO_PLAYER_BTSC_DUAL_SIGNAL),

	PSEUDO_COMPOUND_PLAYER("DRC Type", SND_PSEUDO_PLAYER_DRCTYPE),
	PSEUDO_COMPOUND_PLAYER("DRC cut", SND_PSEUDO_PLAYER_DRCCUT),

	PSEUDO_COMPOUND_PLAYER("DRC boost", SND_PSEUDO_PLAYER_DRCBOOST),

	/* 12 */
	PSEUDO_SIMPLE_PLAYER("DualMono Stream Driven",
			     STM_SE_CTRL_STREAM_DRIVEN_DUALMONO),

	PSEUDO_SIMPLE_PLAYER("DualMono channel selected",
			     STM_SE_CTRL_DUALMONO),

	PSEUDO_COMPOUND_PLAYER("Sink Type", SND_PSEUDO_PLAYER_HW_SINK_TYPE),

	PSEUDO_COMPOUND_PLAYER("Maximum Frequency",
			       STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY),

	/* 16 */
	PSEUDO_COMPOUND_PLAYER("Number of Channels",
			       SND_PSEUDO_PLAYER_HW_NUM_CHANNELS),

	PSEUDO_SIMPLE_PLAYER("Target Level",
			     STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL),

	PSEUDO_COMPOUND_PLAYER("PAIR0", SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR0),

	PSEUDO_COMPOUND_PLAYER("PAIR1", SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR1),

	/* 20 */
	PSEUDO_COMPOUND_PLAYER("PAIR2", SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR2),

	PSEUDO_COMPOUND_PLAYER("PAIR3", SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR3),

	PSEUDO_COMPOUND_PLAYER("PAIR4", SND_PSEUDO_PLAYER_CHANNEL_ASSIGN_PAIR4),

	PSEUDO_COMPOUND_PLAYER("Encoding Playback Route",
			       SND_PSEUDO_PLAYER_HW_PLAYBACK_MODE),

	/* 24 */
	PSEUDO_SIMPLE_PLAYER("DC Remove Playback Switch", STM_SE_CTRL_DCREMOVE),

	PSEUDO_COMPOUND_PLAYER("Per-Speaker Playback Gain",
			       SND_PSEUDO_PLAYER_BASSMGT_GAIN),

	PSEUDO_COMPOUND_PLAYER("Per-Speaker Playback Control",
			       SND_PSEUDO_PLAYER_BASSMGT_CONTROL),

	PSEUDO_COMPOUND_PLAYER("Per-Speaker Playback Latency",
			       SND_PSEUDO_PLAYER_BASSMGT_DELAY),
};

int snd_pseudo_mixer_create_backend_player(struct snd_pseudo_hw_player_entity
					   *player, int index)
{
	struct snd_pseudo_mixer_downstream_card *ds_card =
	    &default_topology.card[index];
	int err, idx;

	if (strlen(ds_card->alsaname) == 0) {
		return -ENODEV;
	}

	/* register the backend and acquire a handle on the player instance */
	err = stm_se_audio_player_new(ds_card->name,
				      ds_card->alsaname,
				      &player->backend_player);
	if (err != 0)
		return err;

	player->index = index;
	player->card = ds_card;
	strlcpy(player->name, ds_card->name, sizeof(player->name));


	/* Add controls associated to hw_card for this player */
	for (idx = 0; idx < ARRAY_SIZE(snd_pseudo_dynamic_controls); idx++) {
		struct snd_kcontrol *kctl;

		/* allocate control */
		kctl = snd_ctl_new1(&snd_pseudo_dynamic_controls[idx], player);
		if (!kctl) {
			return -ENOMEM;
		}

		/* generate the name (this is the dynamic bit) */
		snprintf(kctl->id.name, sizeof(kctl->id.name), "%s %s",
			 player->name, snd_pseudo_dynamic_controls[idx].name);

		printk(KERN_DEBUG "%s: Add control:%40s (private_val:0x%08lX)\n",
				player->hw_card->shortname,
				kctl->id.name,
				kctl->private_value);

		/* Register the control within ALSA */
		err = snd_ctl_add(player->hw_card, kctl);
		if (err < 0) {
			printk(KERN_ERR "Failed to add %s control to hw card\n",
				kctl->id.name);
			return err;
		}
		player->hw_card_controls[idx] = kctl;
	}
	return 0;
}

int snd_pseudo_mixer_delete_backend_player(struct snd_pseudo_hw_player_entity
					   *player)
{
	int err, idx;

	for (idx = 0; idx < ARRAY_SIZE(snd_pseudo_dynamic_controls); idx++) {
		if (player->hw_card_controls[idx]) {
			(void)snd_ctl_remove(player->hw_card,
					     player->hw_card_controls[idx]);
			player->hw_card_controls[idx] = NULL;
		}
	}

	/* unregister the player */
	err = stm_se_audio_player_delete(player->backend_player);
	if (err != 0)
		return err;

	player->backend_player = NULL;

	return 0;
}

int snd_pseudo_mixer_attach_backend_player(struct snd_pseudo *pseudo,
					   struct snd_pseudo_hw_player_entity
					   *player)
{
	stm_se_ctrl_audio_player_hardware_mode_t hw_mode;
	stm_se_audio_channel_assignment_t        channel_assignment;
	stm_se_output_frequency_t output_sampling_freq;
	int idx, err;

	mutex_lock(&pseudo->mixer_lock);

	err = stm_se_audio_player_get_compound_control(player->backend_player,
					STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE,
					&hw_mode);

	if (strstr("HDMI", player->card->name) != NULL) {
		hw_mode.player_type = STM_SE_PLAYER_HDMI;
	} else if (strstr("SPDIF", player->card->name) != NULL) {
		hw_mode.player_type =\
			(player->card->flags &\
			SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING) ?\
			STM_SE_PLAYER_SPDIF_HDMI :\
			STM_SE_PLAYER_SPDIF;
	} else {
		hw_mode.player_type = STM_SE_PLAYER_I2S;
	}

	hw_mode.sink_type = STM_SE_PLAYER_SINK_AUTO;
	hw_mode.playback_control = STM_SE_CTRL_VALUE_DISAPPLY;
	hw_mode.num_channels = player->card->num_channels;

	err = stm_se_audio_player_set_compound_control(player->backend_player,
					STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE,
					&hw_mode);

	if (err != 0)
		return err;

	stm_se_audio_player_get_compound_control(player->backend_player,
						STM_SE_CTRL_SPEAKER_CONFIG,
						&channel_assignment);

	if (8 == hw_mode.num_channels) {
		/* 8 channels default init */
		player->card->channel_assignment.pair0 =
			snd_snd_convert_text_to_channel_index("L_R");
		player->card->channel_assignment.pair1 =
			snd_snd_convert_text_to_channel_index("CNTR_LFE1");
		player->card->channel_assignment.pair2 =
			snd_snd_convert_text_to_channel_index("LSUR_RSUR");
		player->card->channel_assignment.pair3 =
		snd_snd_convert_text_to_channel_index("LSURREAR_RSURREAR");
		player->card->channel_assignment.pair4 =
			snd_snd_convert_text_to_channel_index("NOT_CONNECTED");
	} else {
		/* 2 channels default init */
		player->card->channel_assignment.pair0 =
			snd_snd_convert_text_to_channel_index("L_R");
		player->card->channel_assignment.pair1 =
			snd_snd_convert_text_to_channel_index("NOT_CONNECTED");
		player->card->channel_assignment.pair2 =
			snd_snd_convert_text_to_channel_index("NOT_CONNECTED");
		player->card->channel_assignment.pair3 =
			snd_snd_convert_text_to_channel_index("NOT_CONNECTED");
		player->card->channel_assignment.pair4 =
			snd_snd_convert_text_to_channel_index("NOT_CONNECTED");
	}

	channel_assignment.pair0 =
		snd_convert_channel_index_to_streaming_engine(player->
					card->channel_assignment.pair0);
	channel_assignment.pair1 =
		snd_convert_channel_index_to_streaming_engine(player->
					card->channel_assignment.pair1);
	channel_assignment.pair2 =
		snd_convert_channel_index_to_streaming_engine(player->
					card->channel_assignment.pair2);
	channel_assignment.pair3 =
		snd_convert_channel_index_to_streaming_engine(player->
					card->channel_assignment.pair3);
	channel_assignment.pair4 =
		snd_convert_channel_index_to_streaming_engine(player->
					card->channel_assignment.pair4);
	channel_assignment.malleable = 1;

	err = stm_se_audio_player_set_compound_control(player->backend_player,
						STM_SE_CTRL_SPEAKER_CONFIG,
						&channel_assignment);

	if (err != 0)
		return err;

	output_sampling_freq.control   = STM_SE_MAX_OUTPUT_FREQUENCY;
	output_sampling_freq.frequency = player->card->max_freq;

	err = stm_se_audio_player_set_compound_control(player->backend_player,
					STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY,
					&output_sampling_freq);

	if (err != 0)
		return err;

	mutex_unlock(&pseudo->mixer_lock);

	/* Attach the mixer backend to the player backend */
	err =
	    stm_se_audio_mixer_attach(pseudo->backend_mixer,
				      player->backend_player);
	if (err != 0)
		return err;

	mutex_lock(&pseudo->mixer_lock);

	/* Loop over control's array */
	for (idx = 0; idx < ARRAY_SIZE(snd_pseudo_dynamic_controls); idx++) {
		const char *card_name = player->card->name;
		const char *ctl_name = snd_pseudo_dynamic_controls[idx].name;
		struct snd_kcontrol *kctl;

		/* allocate control */
		kctl = snd_ctl_new1(&snd_pseudo_dynamic_controls[idx], player);
		if (!kctl) {
			mutex_unlock(&pseudo->mixer_lock);
			return -ENOMEM;
		}

		/* generate the name (this is the dynamic bit) */
		snprintf(kctl->id.name, sizeof(kctl->id.name),
			 "%s %s", card_name, ctl_name);

		printk(KERN_DEBUG "%s Add control:%40s (private_val:0x%08lX)\n",
				pseudo->card->shortname,
				kctl->id.name,
				kctl->private_value);

		/* Register the control within ALSA */
		err = snd_ctl_add(pseudo->card, kctl);
		if (err < 0) {
			mutex_unlock(&pseudo->mixer_lock);
			return err;
		}

		player->dynamic_controls[idx] = kctl;
	}

	player->mixer = pseudo;
	pseudo->backend_player[player->index] = player;

	add_player_tuning_controls(pseudo, player);

	snd_pseudo_mixer_update(pseudo);

	mutex_unlock(&pseudo->mixer_lock);

	return 0;
}

int snd_pseudo_mixer_detach_backend_player(struct snd_pseudo *pseudo,
					   struct snd_pseudo_hw_player_entity
					   *player)
{
	int idx, err;

	if (!pseudo->backend_player[player->index])
		/* Nothing to do */
		return 0;

	mutex_lock(&pseudo->mixer_lock);

	/* deregister all existing dynamic controls */
	for (idx = 0; idx < ARRAY_SIZE(player->dynamic_controls); idx++) {
		if (player->dynamic_controls[idx]) {
			(void)snd_ctl_remove(pseudo->card,
					     player->
					     dynamic_controls[idx]);
			player->dynamic_controls[idx] = NULL;
		}
	}

	remove_player_tuning_controls(pseudo, player);

	/* Detach the mixer backend from the player backend */
	err = stm_se_audio_mixer_detach(pseudo->backend_mixer,
					player->backend_player);
	if (err != 0) {
		mutex_unlock(&pseudo->mixer_lock);
		return err;
	}

	player->mixer = NULL;
	pseudo->backend_player[player->index] = NULL;

	snd_pseudo_mixer_update(pseudo);

	mutex_unlock(&pseudo->mixer_lock);

	return 0;
}

static int __init snd_card_pseudo_new_mixer(struct snd_pseudo *pseudo)
{
	struct snd_card *card;
	unsigned int idx;
	int err;

	if (pseudo == NULL)
		return -EINVAL;

	card = pseudo->card;

	mutex_init(&pseudo->mixer_lock);
	strlcpy(card->mixername, card->driver, sizeof(card->mixername));

	for (idx = 0; idx < pseudo->sizeof_static_io_ctls; idx++) {
		err = snd_ctl_add(card, snd_ctl_new1(&pseudo->static_io_ctls[idx],
						     pseudo));
		if (err < 0)
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

	/* aggressively zero the structure */
	memset(mixer, 0, sizeof(*mixer));

	mixer->magic = SND_PSEUDO_MIXER_MAGIC;

	/* initialize the media controller entity */
	snd_pseudo_register_mixer_entity(pseudo, dev);

	mixer->metadata_update = SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER;

	/* switches */
	mixer->all_speaker_stereo_enable = 0;	/* Off */
	/* routes */
	mixer->interactive_audio_mode =
	    SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4;

	/* latency tuning */
	mixer->master_latency = 0;

	/* generic spdif meta data */
	/* rely on memset */

	/* generic spdif mask */
	memset(&mixer->iec958_mask, 0xff, sizeof(mixer->iec958_mask));

	/* fatpipe meta data
	 * (see FatPipe 1.1 spec, section 6.0 for interpretation) */
	mixer->fatpipe_metadata.md[0] = 0x40;

	/* fatpipe mask
	 * (see FatPipe 1.1 spec, section 6.0 for interpretation) */
	mixer->fatpipe_mask.md[0] = 0x70;
	mixer->fatpipe_mask.md[2] = 0x1f;
	mixer->fatpipe_mask.md[6] = 0x1f;
	mixer->fatpipe_mask.md[14] = 0xffff;

	mixer->display_device_id = 0;
	mixer->display_output_id = -1;

	/* search for a valid hdmi output in the default display device 0 */
	{
		stm_display_device_h pDev;
		if (stm_display_open_device(mixer->display_device_id, &pDev) !=
		    0) {
			printk(KERN_ERR
			       "Cannot get handle to display device %d\n",
			       mixer->display_device_id);
			mixer->display_device_id = -1;
		} else {
			int i = 0;
			stm_display_output_h out;

			while (stm_display_device_open_output(pDev, i++, &out)
			       == 0) {
				uint32_t caps;
				stm_display_output_get_capabilities(out, &caps);
				if ((caps & OUTPUT_CAPS_HDMI) != 0)
					mixer->display_output_id = (i - 1);
				stm_display_output_close(out);
			}
			stm_display_device_close(pDev);
		}
	}
}

static inline int snd_pseudo_default_backend_get_instance(int StreamId,
							  component_handle_t *
							  Classoid)
{
	return -ENODEV;
}

static inline int
snd_pseudo_default_backend_set_module_parameters(component_handle_t Classoid,
						 void *Data, unsigned int Size)
{
	return 0;
}

static inline int snd_pseudo_default_backend_alloc_substream(component_handle_t
							     Component,
							     int *SubStreamId)
{
	return -ENODEV;
}

int snd_pseudo_create_card(int dev, size_t size, struct snd_card **p_card)
{
	struct snd_card *card;
	int result;

	result = snd_card_create(index[dev], id[dev], THIS_MODULE, size, &card);
	if (result != 0)
		return result;

	*p_card = card;
	return 0;
}

static int snd_card_pseudo_unregister_backend(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	struct snd_pseudo *pseudo = card->private_data;
	component_handle_t mixer = pseudo->backend_mixer;
	int err = 0;

	if (mixer) {
		err = stm_se_audio_mixer_delete(mixer);
		if (err) {
			printk(KERN_ERR
			       "%s: failed to delete backend mixer %s\n",
			       __func__, pseudo->card->shortname);
			return err;
		}
	}

	return err;
}

static int snd_mixer_probe(struct platform_device *devptr, char * driver_name,
			   char * base_name, char * mediactl_name,
			   int suffix, int * agen_devs, int * ia_devs, struct snd_pseudo ** p_pseudo,
			   struct snd_kcontrol_new * static_io_ctls, int sizeof_static_io_ctls)
{
	struct snd_card *card;
	struct snd_pseudo *pseudo;
	int idx, err;
	int dev = devptr->id;
	int result;
	int max_ag_devs;
	int ag_devs, substreams, max_ia_devs;

	*p_pseudo = NULL;

	result = snd_pseudo_create_card(dev, sizeof(struct snd_pseudo), &card);
	if (result != 0)
		return result;

	pseudo    = card->private_data;
	*p_pseudo = pseudo;

	for (idx = 0; idx < AUDIO_GENERATOR_MAX; idx++) {
		pseudo->audiogeneratorhandle[idx] = NULL;
		pseudo->audiogengainrestore[idx] = Q3_13_UNITY;
		pseudo->restoregain[idx] = 0;
	}

	pseudo->card = card;

	pseudo->allocator = bpa2_find_part(bpa2_partition[dev]);
	if (!pseudo->allocator) {
		err = -ENOMEM;
		goto __nodev;
	}

	max_ag_devs =
	    min(agen_devs[dev], MAX_AUDIOGENERATOR_PCM_DEVICES);

	for (idx = 0; idx < max_ag_devs; idx++) {
		err = snd_card_pseudo_pcm(pseudo, idx, AUDIO_GENERATOR_PCM_NAME,
					  MAX_AUDIOGENERATOR_SUBDEVICES);
		if (err < 0)
			goto __nodev;
	}

	ag_devs = idx;
	if (ia_devs)
	{
	    max_ia_devs =   min(ia_devs[dev],
				MAX_INTERACTIVEAUDIO_PCM_DEVICES);
	}
	else
	    max_ia_devs = 0;

	for (idx = 0; idx < max_ia_devs; idx++) {
		substreams = max(ia_devs[idx], 1);
		substreams = min(substreams, MAX_PCM_SUBSTREAMS);

		err = snd_card_pseudo_pcm(pseudo, ag_devs + idx,
					  INTERACTIVE_AUDIO_PCM_NAME,
					  substreams);
		if (err < 0)
			goto __nodev;
	}

	strlcpy(pseudo->mediactl_name, mediactl_name, MAX_MEDIACTL_CARD_NAME);
	strlcpy(card->driver, driver_name, sizeof(card->driver));
	snprintf(card->shortname, sizeof(card->shortname), "%s%d", base_name, dev);
	snprintf(card->longname, sizeof(card->longname), "%s%d", base_name, dev);

	pseudo->room_identifier = dev;
	pseudo->backend_mixer = NULL;

	pseudo->mixer_observer = NULL;
	pseudo->mixer_observer_ctx = NULL;

	request_pcmproc_fw(&devptr->dev,
			   &pcmproc_fw_ctx,
			   tuningFwNumber,
			   tuningFw);

        pseudo->static_io_ctls        = static_io_ctls;
        pseudo->sizeof_static_io_ctls = sizeof_static_io_ctls;
	err = snd_card_pseudo_new_mixer(pseudo);

	if (err < 0)
		goto __nodev;
	snd_pseudo_mixer_init(pseudo, dev);

	snd_card_set_dev(card, &devptr->dev);

	err = snd_card_register(card);
	if (err == 0) {
		platform_set_drvdata(devptr, card);
		return 0;
	}

__nodev:
	snd_card_free(card);
	return err;
}

static int snd_pseudo_probe(struct platform_device *devptr)
{
    struct snd_pseudo *pseudo;
    int status = snd_mixer_probe(devptr, "Pseudo Mixer", "MIXER", "mixer",
				 devptr->id, pcm_audiogenerator_devs, pcm_interactiveaudio_devs, &pseudo,
				 snd_pseudo_controls, ARRAY_SIZE(snd_pseudo_controls));

    if (pseudo != NULL)
    {
        pseudo->spec.type                     = STM_SE_MIXER_SINGLE_STAGE_MIXING;
        pseudo->spec.nb_max_decoded_audio     = STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS;
        pseudo->spec.nb_max_application_audio = STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS;
        pseudo->spec.nb_max_interactive_audio = STM_SE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS;
        pseudo->spec.nb_max_players           = STM_SE_MIXER_NB_MAX_OUTPUTS;
    }

    return status;
}

static int snd_bcast_probe(struct platform_device *devptr)
{
    struct snd_pseudo *pseudo;
    int status = snd_mixer_probe(devptr, "Bcast Mixer", "BCASTMIX", "bcastmixer",
				 0, bcast_audiogenerator_devs, NULL, &pseudo,
				 snd_bcast_controls,ARRAY_SIZE(snd_bcast_controls));

    if (pseudo != NULL)
    {
        pseudo->spec.type                     = STM_SE_MIXER_DUAL_STAGE_MIXING;
        pseudo->spec.nb_max_decoded_audio     = STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS;
        pseudo->spec.nb_max_application_audio = STM_SE_DUAL_STAGE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS;
        pseudo->spec.nb_max_interactive_audio = STM_SE_DUAL_STAGE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS;
        pseudo->spec.nb_max_players           = STM_SE_MIXER_NB_MAX_OUTPUTS;
    }

    return status;
}


static int __exit snd_pseudo_remove(struct platform_device *devptr)
{
	struct snd_card *card = platform_get_drvdata(devptr);
	struct snd_pseudo *pseudo = card->private_data;

	snd_pseudo_unregister_mixer_entity(pseudo);

	snd_card_pseudo_unregister_backend(devptr);

	remove_mixer_tuning_controls(pseudo);
	release_pcmproc_fw(&pcmproc_fw_ctx, pseudo);

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

#define SND_PSEUDO_DRIVER       "snd_pseudo"

static struct platform_driver snd_pseudo_driver = {
	.probe = snd_pseudo_probe,
	.remove = __exit_p(snd_pseudo_remove),
#ifdef CONFIG_PM
	.suspend = snd_pseudo_suspend,
	.resume = snd_pseudo_resume,
#endif
	.driver = {
		   .name = SND_PSEUDO_DRIVER},
};

#define SND_BCAST_MIXER_DRIVER       "snd_bcast"

static struct platform_driver snd_bcast_driver = {
	.probe = snd_bcast_probe,
	.remove = __exit_p(snd_pseudo_remove),
#ifdef CONFIG_PM
	.suspend = snd_pseudo_suspend,
	.resume = snd_pseudo_resume,
#endif
	.driver = {
		   .name = SND_BCAST_MIXER_DRIVER},
};

static int snd_card_pseudo_register_backend(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	struct snd_pseudo *pseudo = card->private_data;
	int err;
	component_handle_t mixer;

	/* register the backend and acquire a handle on the mixer instance */
	err = stm_se_advanced_audio_mixer_new(card->shortname, pseudo->spec, &mixer);
	if (err != 0) {
		printk(KERN_ERR
		       "%s: Can not create backend mixer\n",
		       pseudo->card->shortname);
		return err;
	}
	pseudo->backend_mixer = mixer;

	add_mixer_tuning_controls(pseudo);

	/* update the mixer instance with our parameters */
	mutex_lock(&pseudo->mixer_lock);
	snd_pseudo_mixer_update(pseudo);
	mutex_unlock(&pseudo->mixer_lock);

	printk(KERN_INFO
	       "%s: Backend mixer created\n", pseudo->card->shortname);

	return 0;
}

int register_alsa_backend(char *name)
{
	int i;

	for (i = 0; i < mixer_max_index; i++) {
		if (devices[i])
			snd_card_pseudo_register_backend(devices[i]);
	}

	return 0;
}

/**
 * Register a mixer observer which will receive a callback whenever
 * the mixer settings change.
 *
 * The API (register/deregister) supports multiple clients but at present
 * the implementation supports only one because there is only a single client
 * at the moment.
 */
int snd_pseudo_register_mixer_observer(int mixer_num,
				       snd_pseudo_mixer_observer_t *observer,
				       void *ctx)
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
					 snd_pseudo_mixer_observer_t *observer,
					 void *ctx)
{
	struct snd_card *card;
	struct snd_pseudo *pseudo;

	if (!devices[mixer_num])
		return -ENODEV;

	card = platform_get_drvdata(devices[mixer_num]);
	pseudo = card->private_data;

	mutex_lock(&pseudo->mixer_lock);

	if (pseudo->mixer_observer != observer
	    && pseudo->mixer_observer_ctx != ctx) {
		mutex_unlock(&pseudo->mixer_lock);
		return -EINVAL;
	}

	pseudo->mixer_observer = NULL;
	pseudo->mixer_observer_ctx = NULL;

	mutex_unlock(&pseudo->mixer_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_pseudo_deregister_mixer_observer);

int snd_pseudo_mixer_get(int mixer_num, stm_object_h *sink)
{
	struct snd_card *card;
	struct snd_pseudo *pseudo;

	/* if (!devices[mixer_num]) */
	if (!devices[0])
		return -ENODEV;

	/* card = platform_get_drvdata(devices[mixer_num]); */
	card = platform_get_drvdata(devices[0]);
	pseudo = card->private_data;

	*sink = (stm_object_h) pseudo->backend_mixer;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_pseudo_mixer_get);

stm_object_h snd_pseudo_mixer_get_from_entity(struct media_entity *entity)
{
	struct snd_pseudo *mixer;

	mixer = container_of(entity, struct snd_pseudo, entity);
	return (stm_object_h) (mixer->backend_mixer);
}
EXPORT_SYMBOL_GPL(snd_pseudo_mixer_get_from_entity);

static void __init_or_module snd_pseudo_unregister_all(void)
{
	int i;

	for (i = 0; i < SNDRV_CARDS; ++i)
		if (devices[i])
			platform_device_unregister(devices[i]);
	platform_driver_unregister(&snd_pseudo_driver);
	platform_driver_unregister(&snd_bcast_driver);
	snd_pseudo_unregister_mc_platform_drivers();
}

/**
 * parse_uniplayer_dt_node  - Parse a uniplayer/unireader DeviceTree node
 * in order to retrieve informations necessary to setup the pseudo_mixer card.
 *
 * @ pNode      a valid st,uniplayer or st,unireader compatible DT node to parse
 * @ direction  either SNDRV_PCM_STREAM_PLAYBACK or SNDRV_PCM_STREAM_CAPTURE
 * @ card       pointer to snd_pseudo_mixer_downstream_card struct to fill
 * @ deviceId   Pass a positive value to force the ALSA device number or
 *              pass -1 to use value retrieve from DT node card-device property
 *
 * return       0 in case of success, errno otherwise
 */
static int __init parse_uniplayer_dt_node(struct device_node *pNode,
				int direction,
				struct snd_pseudo_mixer_downstream_card *card,
				int deviceId)
{
	int            ret           = 0;
	int            card_idx      = 0;
	int            dev_idx       = 0;
	int            nb_channels   = 2;
	const char    *dev_name      = NULL;
	const char    *dev_type      = NULL;

	if (deviceId == -1) {
		ret = of_property_read_u32(pNode, "card-device", &dev_idx);
		if (ret) {
			printk(KERN_ERR "Failed to read card-device property\n");
			return ret;
		}
	}
	else
		dev_idx = deviceId;

	ret = of_property_read_string(pNode, "description", &dev_name);
	if (ret) {
		printk(KERN_ERR "Failed to read description property\n");
		return ret;
	}

	if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = of_property_read_string(pNode, "mode", &dev_type);
		if (ret) {
			printk(KERN_ERR "Failed to read mode property\n");
			return ret;
		}
	} else if (direction == SNDRV_PCM_STREAM_CAPTURE) {
		dev_type = "READER";
	} else {
		printk(KERN_ERR "Invalid direction\n");
		return -EINVAL;
	}

	printk(KERN_DEBUG "\nFound ALSA device  : hw:0,%d\n", dev_idx);
	printk(KERN_DEBUG "Name               : %s\n", dev_name);
	printk(KERN_DEBUG "Type               : %s\n", dev_type);
	printk(KERN_DEBUG "Direction          : %s\n\n",
		(direction == SNDRV_PCM_STREAM_PLAYBACK) ?
		"Playback" : "Capture");

	if (direction == SNDRV_PCM_STREAM_CAPTURE)
		/* not implemented for time being just return without error */
		return 0;

	if (card == NULL)
		return -EINVAL;

	if (!strcmp(dev_type, "HDMI") || !strcmp(dev_type, "SPDIF")) {
		strlcpy(card->name, dev_type, sizeof(card->name));
	} else if (!strcmp(dev_type, "PCM")) {
		snprintf(card->name, sizeof(card->name), "Analog%d",
			nb_analog_player++);
	} else {
		printk(KERN_ERR "Unsupported Card type:%s\n", dev_type);
		return -EINVAL;
	}

	/* Today we assume that hw:0,X is the physical card to use.
	 * When card name will be available through DeviceTree,
	 * we should iterate other card indexes and find which
	 * index corresponds to the card name exposed through DeviceTree
	 */
	snprintf(card->alsaname, sizeof(card->alsaname),
		"hw:%d,%d", card_idx, dev_idx);
	card->num_channels = nb_channels;
	card->max_freq     = 48000;

	if (!strcmp(dev_type, "SPDIF"))
		card->flags = SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;

	printk(KERN_DEBUG "STLinuxTV DeviceTree based pseudo_mixer topology\n");
	printk(KERN_DEBUG "card->name         : %s\n"  , card->name);
	printk(KERN_DEBUG "card->alsaname     : %s\n"  , card->alsaname);
	printk(KERN_DEBUG "card->num_channels : %d\n"  , card->num_channels);
	printk(KERN_DEBUG "card->max_freq     : %d\n"  , card->max_freq);
	printk(KERN_DEBUG "card->flags        : 0x%X\n", card->flags);

	return 0;
}

/**
 * get_alsa_topology_from_dt - Retrieve ALSA HW topology from DeviceTree
 *
 * @nb_player Pointer to integer that will be filled with number of player found
 * @nb_reader Pointer to integer that will be filled with number of reader found
 * @topology  Pointer to the topology structure to fill from DeviceTree
 *
 * return     void
 */
static void __init get_alsa_topology_from_dt(int *nb_player, int *nb_reader,
			struct snd_pseudo_mixer_downstream_topology *topology)
{
	int                    ret        = 0;
	struct device_node    *pNode      = NULL;
	struct device_node    *np         = NULL;

	nb_analog_player = 0;
	*nb_player = 0;
	*nb_reader = 0;
	memset(topology, 0, sizeof(*topology));

	/* Retrieve topology from DeviceTree on an ASoC based implementation */
	pNode = of_find_compatible_node(pNode, NULL, "st,snd-soc-sti");
	if (pNode != NULL) {
		int deviceId = 0;
		while ((np = of_parse_phandle(pNode, "st,backend-cpu-dai", deviceId)) != NULL) {
			if (of_device_is_compatible(np, "st,snd_uni_player")
			&&  of_device_is_available(np)) {
				ret = parse_uniplayer_dt_node(np,
						SNDRV_PCM_STREAM_PLAYBACK,
						&topology->card[*nb_player],
						deviceId);
				deviceId++;
				if (ret) {
					printk(KERN_ERR "Cannot parse DT node\n");
					continue;
				}
				(*nb_player)++;
			}
			if (of_device_is_compatible(np, "st,snd_uni_reader")
			&&  of_device_is_available(np)) {
				ret = parse_uniplayer_dt_node(np,
						SNDRV_PCM_STREAM_CAPTURE,
						NULL,
						deviceId);
				deviceId++;
				if (ret) {
					printk(KERN_ERR "Cannot parse DT node\n");
					continue;
				}
				(*nb_reader)++;
			}
		}
		return;
	}
	/* Retrieve topology from DeviceTree on non-ASoC based implementation */
	while ((pNode = of_find_compatible_node(pNode, NULL, "st,snd_uni_player")) != NULL) {
		if (of_device_is_available(pNode)) {
			ret = parse_uniplayer_dt_node(pNode,
					SNDRV_PCM_STREAM_PLAYBACK,
					&topology->card[*nb_player], -1);
			if (ret) {
				printk(KERN_ERR "Cannot parse DT node\n");
				continue;
			}
			(*nb_player)++;
		}
	}

	pNode = NULL;
	while ((pNode = of_find_compatible_node(pNode, NULL, "st,snd_uni_reader")) != NULL) {
		if (of_device_is_available(pNode)) {
			ret = parse_uniplayer_dt_node(pNode,
					SNDRV_PCM_STREAM_CAPTURE,
					NULL, -1);
			if (ret) {
				printk(KERN_ERR "Cannot parse DT node\n");
				continue;
			}
			(*nb_reader)++;
		}
	}
	printk(KERN_DEBUG "ALSA device count: players:%d; readers:%d\n",
		*nb_player, *nb_reader);
}

static int __init alsa_card_pseudo_init(void)
{
	struct platform_device *device;
	int i, cards, err, nb_player, nb_reader;

	/* Retrieve topology from DeviceTree */
	get_alsa_topology_from_dt(&nb_player, &nb_reader, &default_topology);

	/* make the enable module parameter available to other files */
	card_enables = enable;

	/* register the (mixer) card driver */
	err  = platform_driver_register(&snd_pseudo_driver);
	err |= platform_driver_register(&snd_bcast_driver);

	if (err < 0)
		return err;

	/* make a mixer card for each player */
	cards = 0;
	for (i = 0; i < nb_player; i++) {
		if (!enable[i])
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
		printk(KERN_ERR
		       "%s: Pseudo soundcard not found or device busy\n",
		       KBUILD_MODNAME);
#endif
		snd_pseudo_unregister_all();
		return -ENODEV;
	}

    /* make an dual-stage mixer */
    device = platform_device_register_simple(SND_BCAST_MIXER_DRIVER,
							 0, NULL, 0);
    if (IS_ERR(device))
    {
#ifdef MODULE
		printk(KERN_ERR
		       "%s: Pseudo soundcard not found or device busy\n",
		       KBUILD_MODNAME);
#endif
    }
    else
    {
        devices[i] = device;
        i++;
    }
	/* remember the maximum mixer device index */
	mixer_max_index = i;

	snd_pseudo_register_mc_platform_drivers(devices, mixer_max_index);

	err = register_alsa_backend(NULL);
	if (err < 0)
		return err;

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
