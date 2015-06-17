/*
 *  Driver for HAL2 soundcard
 *  Copyright (c) by Ulf Carlsson <ulfc@thepuffingroup.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define SNDRV_MAIN_OBJECT_FILE
#include "driver.h"
#include "hal2.h"
#include "initval.h"

int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
int dma1_size[SNDRV_CARDS] = SNDRV_DEFAULT_DMA_SIZE;	/* 8,16,32,64,128 */
int dma2_size[SNDRV_CARDS] = SNDRV_DEFAULT_DMA_SIZE;	/* 8,16,32,64,128 */

EXPORT_NO_SYMBOLS;
MODULE_PARM(index, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(index, "Index value for HAL2 soundcard.");
MODULE_PARM(id, "1-" __MODULE_STRING(SNDRV_CARDS) "s");
MODULE_PARM_DESC(id, "ID string for HAL2 soundcard.");
MODULE_PARM(dma1_size, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(dma1_size, "DMA1 size in kB for HAL2 driver.");
MODULE_PARM(dma2_size, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(dma2_size, "DMA2 size in kB for HAL2 driver.");

struct snd_card_hal2 {
	snd_irq_t *irqptr;
	snd_dma_t *dma1ptr;
	snd_dma_t *dma2ptr;
	struct snd_card *card;
	snd_hal2_card_t *hal2;
	struct snd_pcm *pcm;
	snd_kmixer_t *mixer;
	struct snd_rawmidi *midi_uart;
	snd_hal2_ctl_regs_t *ctl_regs;
	snd_hal2_aes_regs_t *aes_regs;
	snd_hal2_vol_regs_t *vol_regs;
	snd_hal2_syn_regs_t *syn_regs;
};

static struct snd_card_hal2 *snd_hal2_cards[SNDRV_CARDS] = SNDRV_DEFAULT_PTR;

static void snd_card_hal2_use_inc(struct snd_card *card)
{
	MOD_INC_USE_COUNT;
}

static void snd_card_hal2_use_dec(struct snd_card *card)
{
	MOD_DEC_USE_COUNT;
}

static int snd_card_hal2_detect(snd_hal2_card_t * hal2)
{
	unsigned short board, major, minor;
	unsigned short rev;

	/* reset HAL2 */
	snd_hal2_isr_write(hal2, 0);

	/* release reset */
	snd_hal2_isr_write(hal2, H2_ISR_GLOBAL_RESET_N | H2_ISR_CODEC_RESET_N | H2_ISR_QUAD_MODE);

	snd_hal2_i_write16(hal2, H2I_RELAY_C, H2I_RELAY_C_STATE); 

#define CONFIG_SND_DEBUG_DETECT

#ifdef CONFIG_SND_DEBUG_DETECT
	if ((rev = snd_hal2_rev_look(hal2)) & H2_REV_AUDIO_PRESENT) {
		snd_printk("HAL2 wasn't there? rev: 0x%04hx\n", rev);
		return -ENODEV;
	}
#else
	if ((rev = snd_hal2_rev_look(hal2)) & H2_REV_AUDIO_PRESENT)
		return -ENODEV;
#endif

	board = (rev & H2_REV_BOARD_M) >> 12;
	major = (rev & H2_REV_MAJOR_CHIP_M) >> 4;
	minor = (rev & H2_REV_MINOR_CHIP_M);

	snd_printk("SGI HAL2 Processor, Revision %i.%i.%i\n",
	       board, major, minor);

	if (board != 4 || major != 1 || minor != 0) {
		snd_printk("Other revision than 4.1.0 detected\n");
		snd_printk("Your card is probably not supported\n");
	}

	return 0;
}

static void snd_card_hal2_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct snd_card_hal2 *hal2card = (struct snd_card_hal2 *) dev_id;

	if (hal2card == NULL || hal2card->hal2 == NULL || hal2card->pcm == NULL)
		return;

	snd_hal2_interrupt(hal2card->hal2);
}

static int snd_card_hal2_resources(int dev, struct snd_card_hal2 *hal2card,
				   struct snd_card *card)
{
	int err;

	/* This is not really a PCI interrupt, but PCI interrupts are shareable
	 * so this will probably work
	 */
	if ((err = snd_register_interrupt(card, "HAL2", 12, SNDRV_IRQ_TYPE_PCI,
					  snd_card_hal2_interrupt, hal2card,
					  NULL, &hal2card->irqptr)) < 0) {
		snd_printk("Couldn't get irq\n");
		return err;
	}

	/* It seems like we can use a the PCI dma type here. It's not entirely
	 * true, but it seems to work.
	 */
	if ((err = snd_register_dma_channel(card, "HAL2 record", 0,
					    SNDRV_DMA_TYPE_PCI,
					    dma1_size[dev], NULL,
					    &hal2card->dma1ptr)) < 0) {
		snd_printk("Couldn't get dma1\n");
		return err;
	}
	if ((err = snd_register_dma_channel(card, "HAL2 playback", 0,
					    SNDRV_DMA_TYPE_PCI,
					    dma2_size[dev], NULL,
					    &hal2card->dma2ptr)) < 0) {
		snd_printk("Couldn't get dma2\n");
		return err;
	}
	return 0;
}

static int snd_card_hal2_probe(int dev, struct snd_card_hal2 *hal2card)
{
	struct snd_card *card;
	snd_hal2_card_t *hal2 = NULL;
	struct snd_pcm *pcm = NULL;

	card = snd_card_new(index[dev], id[dev],
			    snd_card_hal2_use_inc, snd_card_hal2_use_dec);
	if (card == NULL)
		return -ENOMEM;
	if (snd_card_hal2_resources(dev, hal2card, card) < 0) {
		snd_card_free(card);
		return -EBUSY;
	}
	pcm = snd_hal2_new_device(card, hal2card->irqptr, hal2card->dma1ptr,
				  hal2card->dma2ptr, hal2card->ctl_regs,
				  hal2card->aes_regs, hal2card->vol_regs,
				  hal2card->syn_regs);
	if (pcm == NULL) {
		snd_card_free(card);
		return -ENOMEM;
	}

	hal2 = (snd_hal2_card_t *) card->private_data;

	if (snd_card_hal2_detect(hal2)) {
		snd_card_free(card);
		return -ENODEV;
	}

	if (snd_pcm_register(pcm, 0) < 0) {
		goto __nodev;
	}
	strcpy(card->driver, "HAL2");
	strcpy(card->shortname, "HAL2");
	sprintf(card->longname, "HAL2 at irq %i", hal2card->irqptr->irq);

	if (!snd_card_register(card)) {
		hal2card->card = card;
		hal2card->hal2 = hal2;
		hal2card->pcm = pcm;
		return 0;
	}

	snd_pcm_unregister(pcm);
	pcm = NULL;

      __nodev:
	if (pcm)
		snd_pcm_free(pcm);
	if (card)
		snd_card_free(card);
	return -ENXIO;
}

static int __init alsa_card_hal2_init(void)
{
	int dev = 0;
	struct snd_card_hal2 *hal2card;

	/* I doubt anyone has a machine with two HAL2 cards. It's possible to
	 * have two HPC's, so it is probably possible to have two HAL2 cards.
	 * Show me such a machine and I'll happily implement support for your
	 * multiple HAL2 cards!
	 */
	hal2card = (struct snd_card_hal2 *)
				snd_kmalloc(sizeof(struct snd_card_hal2), GFP_KERNEL);
	if (hal2card == NULL)
		return -ENOMEM;
	memset(hal2card, 0, sizeof(struct snd_card_hal2));

	hal2card->ctl_regs = (snd_hal2_ctl_regs_t *) KSEG1ADDR(H2_CTL_PIO);
	hal2card->aes_regs = (snd_hal2_aes_regs_t *) KSEG1ADDR(H2_AES_PIO);
	hal2card->vol_regs = (snd_hal2_vol_regs_t *) KSEG1ADDR(H2_VOL_PIO);
	hal2card->syn_regs = (snd_hal2_syn_regs_t *) KSEG1ADDR(H2_SYN_PIO);

	if (snd_card_hal2_probe(dev, hal2card) < 0) {
		snd_kfree(hal2card);
		snd_printk("HAL2 soundcard #%i not found or device busy\n",
			   dev + 1);
		return -ENODEV;
	}
	snd_hal2_cards[dev] = hal2card;

	return 0;
}

static void __exit alsa_card_hal2_exit(void)
{
	int dev = 0;
	struct snd_card_hal2 *hal2card;
	struct snd_pcm *pcm;

	/* We don't have to check all the indexes since we don't have suppor for
	 * more than one card, but let's do it anyway
	 */
	hal2card = snd_hal2_cards[dev];
	if (hal2card) {
		snd_card_unregister(hal2card->card);
		if (hal2card->pcm) {
			pcm = hal2card->pcm;
			hal2card->pcm = NULL;	/* turn off interrupts */
			snd_pcm_unregister(pcm);
		}
		snd_card_free(hal2card->card);
		snd_kfree(hal2card);
	}
}

module_init(alsa_card_hal2_init)
module_exit(alsa_card_hal2_exit)
