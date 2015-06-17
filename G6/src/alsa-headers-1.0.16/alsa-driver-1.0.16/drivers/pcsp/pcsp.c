/*
 * PC-Speaker driver for Linux
 *
 * Copyright (C) 1997-2001  David Woodhouse
 * Copyright (C) 2001-2007  Stas Sergeev
 */

#include "adriver.h"
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>

#include <linux/input.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include "pcsp_input.h"
#include "pcsp.h"

MODULE_AUTHOR("Stas Sergeev <stsp@users.sourceforge.net>");
MODULE_DESCRIPTION("PC-Speaker driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{PC-Speaker, pcsp}}");

static int index = SNDRV_DEFAULT_IDX1;	/* Index 0-MAX */
static char *id = SNDRV_DEFAULT_STR1;	/* ID for this card */
static int enable = SNDRV_DEFAULT_ENABLE1;	/* Enable this card */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for pcsp soundcard.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for pcsp soundcard.");
module_param(enable, bool, 0444);
MODULE_PARM_DESC(enable, "dummy");

struct snd_pcsp pcsp_chip;


static int __init snd_pcsp_create(struct snd_card *card)
{
	static struct snd_device_ops ops = { };
	struct timespec tp;
	int err;
	int div, min_div, order;

	hrtimer_get_res(CLOCK_MONOTONIC, &tp);
	if (tp.tv_sec || tp.tv_nsec > PCSP_MAX_PERIOD_NS) {
		printk(KERN_ERR "PCSP: Timer resolution is not sufficient "
			"(%linS)\n", tp.tv_nsec);
		printk(KERN_ERR "PCSP: Make sure you have HPET and ACPI "
			"enabled.\n");
		return -EIO;
	}

	if (loops_per_jiffy >= PCSP_MIN_LPJ && tp.tv_nsec <= PCSP_MIN_PERIOD_NS)
		min_div = MIN_DIV;
	else
		min_div = MAX_DIV;
#if PCSP_DEBUG
	printk("PCSP: lpj=%li, min_div=%i, res=%li\n",
		loops_per_jiffy, min_div, tp.tv_nsec);
#endif

	div = MAX_DIV / min_div;
	order = fls(div) - 1;

	pcsp_chip.max_treble = min(order, PCSP_MAX_TREBLE);
	pcsp_chip.treble = min(pcsp_chip.max_treble, PCSP_DEFAULT_TREBLE);
	pcsp_chip.playback_ptr = 0;
	pcsp_chip.period_ptr = 0;
	pcsp_chip.timer_active = 0;
	pcsp_chip.enable = 1;
	pcsp_chip.pcspkr = 1;

	spin_lock_init(&pcsp_chip.substream_lock);

	pcsp_chip.card = card;
	pcsp_chip.port = 0x61;
	pcsp_chip.irq = -1;
	pcsp_chip.dma = -1;

	/* Register device */
	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, &pcsp_chip, &ops);
	if (err < 0)
		return err;

	return 0;
}

static int __init snd_card_pcsp_probe(int dev)
{
	struct snd_card *card;
	int err;

	if (dev != 0)
		return -EINVAL;

	hrtimer_init(&pcsp_chip.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pcsp_chip.timer.cb_mode = HRTIMER_CB_IRQSAFE;
	pcsp_chip.timer.function = pcsp_do_timer;

	card = snd_card_new(index, id, THIS_MODULE, 0);
	if (!card)
		return -ENOMEM;

	err = snd_pcsp_create(card);
	if (err < 0) {
		snd_card_free(card);
		return err;
	}
	err = snd_pcsp_new_pcm(&pcsp_chip);
	if (err < 0) {
		snd_card_free(card);
		return err;
	}
	err = snd_pcsp_new_mixer(&pcsp_chip);
	if (err < 0) {
		snd_card_free(card);
		return err;
	}

	strcpy(card->driver, "PC-Speaker");
	strcpy(card->shortname, "pcsp");
	sprintf(card->longname, "Internal PC-Speaker at port 0x%x",
		pcsp_chip.port);

	err = snd_card_register(card);
	if (err < 0) {
		snd_card_free(card);
		return err;
	}

	pcspkr_input_init(&pcsp_chip.input_dev);

	return 0;
}

static int __init alsa_card_pcsp_init(void)
{
	int dev = 0, cards = 0;

#ifdef CONFIG_DEBUG_PAGEALLOC
	/* Well, CONFIG_DEBUG_PAGEALLOC makes the sound horrible. Lets alert */
	printk(KERN_WARNING
	       "PCSP: Warning, CONFIG_DEBUG_PAGEALLOC is enabled!\n"
	       "You have to disable it if you want to use the PC-Speaker driver.\n"
	       "Unless it is disabled, enjoy the horrible, distorted and crackling "
	       "noise.\n");
#endif

	if (enable) {
		if (snd_card_pcsp_probe(dev) >= 0)
			cards++;
	}
	if (!cards) {
		printk(KERN_ERR "PC-Speaker initialization failed.\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit alsa_card_pcsp_exit(void)
{
	pcspkr_input_remove(pcsp_chip.input_dev);
	snd_card_free(pcsp_chip.card);
}

module_init(alsa_card_pcsp_init);
module_exit(alsa_card_pcsp_exit);
