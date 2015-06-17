/*
 *  Generic driver for serial MIDI adapters
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
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
/*
 *  The adaptor module parameter allows you to select:
 *	0 - Roland SoundCanvas (use outs paratemer to specify count of output ports)
 */

#include "adriver.h"
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <sound/core.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <linux/delay.h>

#define SNDRV_SERIAL_MAX_OUTS		16 /* min 16 */
#define TX_BUF_SIZE			256

#define SERIAL_ADAPTOR_SOUNDCANVAS 	0  /* Roland Soundcanvas; F5 NN selects part */
#define SERIAL_ADAPTOR_MS124T		1  /* Midiator MS-124T */
#define SERIAL_ADAPTOR_MS124W_SA	2  /* Midiator MS-124W in S/A mode */
#define SERIAL_ADAPTOR_MS124W_MB	3  /* Midiator MS-124W in M/B mode */
#define SERIAL_ADAPTOR_MAX		SERIAL_ADAPTOR_MS124W_MB

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Jaroslav Kysela <perex@perex.cz>");
MODULE_DESCRIPTION("Serial MIDI");
MODULE_LICENSE("GPL");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;		/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;		/* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE;	/* Enable this card */
static char *sdev[SNDRV_CARDS] = {"/dev/ttyS0", [1 ... (SNDRV_CARDS - 1)] = ""}; /* serial device */
static int speed[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 38400}; /* 9600,19200,38400,57600,115200 */
static int adaptor[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = SERIAL_ADAPTOR_SOUNDCANVAS};
static int outs[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};     /* 1 to 16 */
static int devices[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};     /* 1 to 8 */
static int handshake[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};     /* bool */

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for serial device.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for serial device.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable serial device.");
module_param_array(sdev, charp, NULL, 0444);
MODULE_PARM_DESC(sdev, "Device file string for serial device.");
module_param_array(speed, int, NULL, 0444);
MODULE_PARM_DESC(speed, "Speed in bauds.");
module_param_array(adaptor, int, NULL, 0444);
MODULE_PARM_DESC(adaptor, "Type of adaptor.");
module_param_array(outs, int, NULL, 0444);
MODULE_PARM_DESC(outs, "Number of MIDI outputs.");
module_param_array(devices, int, NULL, 0444);
MODULE_PARM_DESC(devices, "Number of devices to attach to the card.");
module_param_array(handshake, int, NULL, 0444);
MODULE_PARM_DESC(handshake, "Do handshaking.");

#define SERIAL_MODE_NOT_OPENED		(0)
#define SERIAL_MODE_BIT_INPUT		(0)
#define SERIAL_MODE_BIT_OUTPUT		(1)
#define SERIAL_MODE_BIT_INPUT_TRIGGERED	(2)
#define SERIAL_MODE_BIT_OUTPUT_TRIGGERED (3)

typedef struct _snd_serialmidi {
	struct snd_card *card;
	char *sdev;			/* serial device name (e.g. /dev/ttyS0) */
	int dev_idx;
	unsigned int speed;		/* speed in bauds */
	unsigned int adaptor;		/* see SERIAL_ADAPTOR_ */
	unsigned long mode;		/* see SERIAL_MODE_* */
	unsigned int outs;		/* count of outputs */
	unsigned char prev_status[SNDRV_SERIAL_MAX_OUTS];
	struct snd_rawmidi *rmidi;		/* rawmidi device */
	struct snd_rawmidi_substream *substream_input;
	struct snd_rawmidi_substream *substream_output;
	struct file *file;
	struct tty_struct *tty;
	struct mutex open_lock;
	void (*old_receive_buf)(struct tty_struct *, const unsigned char *cp, char *fp, int count);
	void (*old_write_wakeup)(struct tty_struct *);
	int old_exclusive;
	int old_low_latency;
	int handshake;
	unsigned char tx_buf[TX_BUF_SIZE];
} serialmidi_t;

static struct platform_device *devptrs[SNDRV_CARDS];

static void ldisc_receive_buf(struct tty_struct *, const unsigned char *cp, char *fp, int count);
static void ldisc_write_wakeup(struct tty_struct *);

static int tty_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct inode * inode = file->f_dentry->d_inode;
	mm_segment_t fs;
	int retval;

	if (file->f_op == NULL)
		return -ENXIO;
	fs = get_fs();
	set_fs(get_ds());
	retval = file->f_op->ioctl(inode, file, cmd, arg);
	set_fs(fs);
	return retval;
}

static int open_tty(serialmidi_t *serial, unsigned int mode)
{
	int retval = 0;
	struct tty_struct *tty;
	struct termios old_termios, *ntermios;
	struct tty_driver *driver;
	int ldisc, speed, cflag;

	mutex_lock(&serial->open_lock);
	if (serial->tty) {
		set_bit(mode, &serial->mode);
		goto __end;
	}
	if (IS_ERR(serial->file = filp_open(serial->sdev, O_RDWR|O_NONBLOCK, 0))) {
		retval = PTR_ERR(serial->file);
		serial->file = NULL;
		goto __end;
	}
	tty = (struct tty_struct *)serial->file->private_data;
	if (tty == NULL || tty->magic != TTY_MAGIC) {
		snd_printk(KERN_ERR "device %s has not valid tty", serial->sdev);
		retval = -EIO;
		goto __end;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) /* correct version? */
	driver = tty->driver;
#else
	driver = &tty->driver;
#endif
	if (driver == NULL || driver->set_termios == NULL) {
		snd_printk(KERN_ERR "tty %s has not set_termios", serial->sdev);
		retval = -EIO;
		goto __end;
	}
#ifdef CONFIG_HAVE_TTY_COUNT_ATOMIC
	if (atomic_read(&tty->count) > 1) {
#else
	if (tty->count > 1) {
#endif
		snd_printk(KERN_ERR "tty %s is already used", serial->sdev);
		retval = -EBUSY;
		goto __end;
	}
	
	/* select N_TTY line discipline (for sure) */
	ldisc = N_TTY;
	if ((retval = tty_ioctl(serial->file, TIOCSETD, (unsigned long)&ldisc)) < 0) {
		snd_printk(KERN_ERR "TIOCSETD (N_TTY) failed for tty %s", serial->sdev);
		goto __end;
	}

	/* sanity check, we use disc_data for own purposes */
	if (tty->disc_data != NULL) {
		snd_printk(KERN_ERR "disc_data are used for tty %s", serial->sdev);
		goto __end;
	}
	
	switch (serial->speed) {
	case 9600:
		speed = B9600;
		break;
	case 19200:
		speed = B19200;
		break;
	case 38400:
		speed = B38400;
		break;
	case 57600:
		speed = B57600;
		break;
	case 115200:
	default:
		speed = B115200;
		break;
	}

	if (serial->handshake)
		cflag = speed | CREAD | CSIZE | CS8 | CRTSCTS | HUPCL;
	else
		cflag = speed | CREAD | CSIZE | CS8 | HUPCL;

	switch (serial->adaptor) {
	case SERIAL_ADAPTOR_MS124W_SA:
	case SERIAL_ADAPTOR_MS124W_MB:
	case SERIAL_ADAPTOR_MS124T:
		/* TODO */
		break;
	}
	
	old_termios = *tty->termios;
	ntermios = tty->termios;
	ntermios->c_lflag = NOFLSH;
	ntermios->c_iflag = IGNBRK | IGNPAR;
	ntermios->c_oflag = 0;
	ntermios->c_cflag = cflag;
	ntermios->c_cc[VEOL] = 0; /* '\r'; */
	ntermios->c_cc[VERASE] = 0;
	ntermios->c_cc[VKILL] = 0;
	ntermios->c_cc[VMIN] = 0;
	ntermios->c_cc[VTIME] = 0;
	(*driver->set_termios)(tty, &old_termios);
	serial->tty = tty;

	/* some magic here, we need own receive_buf */
	/* it would be probably better to create own line discipline */
	/* but this solution is sufficient at the time */
	tty->disc_data = serial;
	serial->old_receive_buf = tty->ldisc.receive_buf;
	tty->ldisc.receive_buf = ldisc_receive_buf;
	serial->old_write_wakeup = tty->ldisc.write_wakeup;
	tty->ldisc.write_wakeup = ldisc_write_wakeup;
	serial->old_low_latency = tty->low_latency;
	tty->low_latency = 1;
	serial->old_exclusive = test_bit(TTY_EXCLUSIVE, &tty->flags);
	set_bit(TTY_EXCLUSIVE, &tty->flags);

	set_bit(mode, &serial->mode);
	retval = 0;

      __end:
      	if (retval < 0) {
      		if (serial->file) {
      			filp_close(serial->file, NULL);
      			serial->file = NULL;
      		}
      	}
	mutex_unlock(&serial->open_lock);
	return retval;
}

static int close_tty(serialmidi_t *serial, unsigned int mode)
{
	unsigned int imode = mode == SERIAL_MODE_BIT_INPUT ?
			SERIAL_MODE_BIT_OUTPUT : SERIAL_MODE_BIT_INPUT;
	struct tty_struct *tty;

	mutex_lock(&serial->open_lock);
	clear_bit(mode, &serial->mode);
	if (test_bit(imode, &serial->mode))
		goto __end;
	tty = serial->tty;
	if (tty->disc_data == serial)
		tty->disc_data = NULL;
	if (tty && tty->ldisc.receive_buf == ldisc_receive_buf) {
		tty->low_latency = serial->old_low_latency;
		tty->ldisc.receive_buf = serial->old_receive_buf;
		if (serial->old_exclusive)
			set_bit(TTY_EXCLUSIVE, &tty->flags);
		else
			clear_bit(TTY_EXCLUSIVE, &tty->flags);
	}
	filp_close(serial->file, NULL);
	serial->tty = NULL;
	serial->file = NULL;
      __end:
	mutex_unlock(&serial->open_lock);
	return 0;
}

static void ldisc_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	serialmidi_t *serial = tty->disc_data;

	if (serial == NULL)
		return;
	if (!test_bit(SERIAL_MODE_BIT_INPUT_TRIGGERED, &serial->mode))
		return;
	snd_rawmidi_receive(serial->substream_input, cp, count);
}

static void tx_loop(serialmidi_t *serial)
{
	struct tty_struct *tty;
	char *buf = serial->tx_buf;
	int count;
	struct tty_driver *driver;

	tty = serial->tty;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) /* correct version? */
	driver = tty->driver;
#else
	driver = &tty->driver;
#endif
	if (driver == NULL)
		return;

	if (down_trylock(&tty->atomic_write))
		return;
	while (1) {
		count = driver->write_room(tty);
		if (count <= 0) {
			up(&tty->atomic_write);
			return;
		}
		count = count > TX_BUF_SIZE ? TX_BUF_SIZE : count;
		count = snd_rawmidi_transmit_peek(serial->substream_output, buf, count);
		if (count > 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
			driver->write(tty, buf, count);
#else
			count = driver->write(tty, 0, buf, count);
#endif
			snd_rawmidi_transmit_ack(serial->substream_output, count);
		} else {
			clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
			break;
		}
	}
	up(&tty->atomic_write);
}

static void ldisc_write_wakeup(struct tty_struct *tty)
{
	serialmidi_t *serial = tty->disc_data;

	tx_loop(serial);
}

static void snd_serialmidi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	serialmidi_t *serial = substream->rmidi->private_data;
	
	if (up) {
		set_bit(SERIAL_MODE_BIT_OUTPUT_TRIGGERED, &serial->mode);
		set_bit(TTY_DO_WRITE_WAKEUP, &serial->tty->flags);
		tx_loop(serial);
	} else {
		clear_bit(TTY_DO_WRITE_WAKEUP, &serial->tty->flags);
		clear_bit(SERIAL_MODE_BIT_OUTPUT_TRIGGERED, &serial->mode);
	}
}

static void snd_serialmidi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	serialmidi_t *serial = substream->rmidi->private_data;

	if (up) {
		set_bit(SERIAL_MODE_BIT_INPUT_TRIGGERED, &serial->mode);
	} else {
		clear_bit(SERIAL_MODE_BIT_INPUT_TRIGGERED, &serial->mode);
	}
}

static int snd_serialmidi_output_open(struct snd_rawmidi_substream *substream)
{
	serialmidi_t *serial = substream->rmidi->private_data;
	int err;

	if ((err = open_tty(serial, SERIAL_MODE_BIT_OUTPUT)) < 0)
		return err;
	serial->substream_output = substream;
	return 0;
}

static int snd_serialmidi_output_close(struct snd_rawmidi_substream *substream)
{
	serialmidi_t *serial = substream->rmidi->private_data;

	serial->substream_output = NULL;
	return close_tty(serial, SERIAL_MODE_BIT_OUTPUT);
}

static int snd_serialmidi_input_open(struct snd_rawmidi_substream *substream)
{
	serialmidi_t *serial = substream->rmidi->private_data;
	int err;

	if ((err = open_tty(serial, SERIAL_MODE_BIT_INPUT)) < 0)
		return err;
	serial->substream_input = substream;
	return 0;
}

static int snd_serialmidi_input_close(struct snd_rawmidi_substream *substream)
{
	serialmidi_t *serial = substream->rmidi->private_data;

	serial->substream_input = NULL;
	return close_tty(serial, SERIAL_MODE_BIT_INPUT);
}

static struct snd_rawmidi_ops snd_serialmidi_output =
{
	.open =		snd_serialmidi_output_open,
	.close =	snd_serialmidi_output_close,
	.trigger =	snd_serialmidi_output_trigger,
};

static struct snd_rawmidi_ops snd_serialmidi_input =
{
	.open =		snd_serialmidi_input_open,
	.close =	snd_serialmidi_input_close,
	.trigger =	snd_serialmidi_input_trigger,
};

static int snd_serialmidi_free(serialmidi_t *serial)
{
	if (serial->sdev);
		kfree(serial->sdev);
	kfree(serial);
	return 0;
}

static int snd_serialmidi_dev_free(struct snd_device *device)
{
	serialmidi_t *serial = device->device_data;
	return snd_serialmidi_free(serial);
}

static int __init snd_serialmidi_rmidi(serialmidi_t *serial)
{
        struct snd_rawmidi *rrawmidi;
        int err;

        if ((err = snd_rawmidi_new(serial->card, "UART Serial MIDI", serial->dev_idx, serial->outs, 1, &rrawmidi)) < 0)
                return err;
        snd_rawmidi_set_ops(rrawmidi, SNDRV_RAWMIDI_STREAM_INPUT, &snd_serialmidi_input);
        snd_rawmidi_set_ops(rrawmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &snd_serialmidi_output);
	snprintf(rrawmidi->name, sizeof(rrawmidi->name), "%s %d", serial->card->shortname, serial->dev_idx);
        rrawmidi->info_flags = SNDRV_RAWMIDI_INFO_OUTPUT |
                               SNDRV_RAWMIDI_INFO_INPUT |
                               SNDRV_RAWMIDI_INFO_DUPLEX;
        rrawmidi->private_data = serial;
	serial->rmidi = rrawmidi;
        return 0;
}

static int __init snd_serialmidi_create(struct snd_card *card, const char *sdev,
					unsigned int speed, unsigned int adaptor,
					unsigned int outs, int idx, int hshake)
{
	static struct snd_device_ops ops = {
		.dev_free =	snd_serialmidi_dev_free,
	};
	serialmidi_t *serial;
	int err;

	switch (adaptor) {
	case SERIAL_ADAPTOR_SOUNDCANVAS:
		break;
	case SERIAL_ADAPTOR_MS124T:
	case SERIAL_ADAPTOR_MS124W_SA:
		outs = 1;
		break;
	case SERIAL_ADAPTOR_MS124W_MB:
		outs = 16;
		break;
	default:
		snd_printk(KERN_ERR "Adaptor type is out of range 0-%d (%d)\n",
				SERIAL_ADAPTOR_MAX, adaptor);
		return -ENODEV;
	}
	if (outs < 1)
		outs = 1;
	else if (outs > 16)
		outs = 16;

	if ((serial = kzalloc(sizeof(*serial), GFP_KERNEL)) == NULL)
		return -ENOMEM;

	mutex_init(&serial->open_lock);
	serial->card = card;
	serial->dev_idx = idx;
	serial->sdev = kstrdup(sdev, GFP_KERNEL);
	if (serial->sdev == NULL) {
		snd_serialmidi_free(serial);
		return -ENOMEM;
	}
	serial->adaptor = adaptor;
	serial->speed = speed;
	serial->outs = outs;
	serial->handshake = hshake;
	memset(serial->prev_status, 0x80, sizeof(unsigned char) * SNDRV_SERIAL_MAX_OUTS);
	
	/* Register device */
	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, serial, &ops)) < 0) {
		snd_serialmidi_free(serial);
		return err;
	}
	
	if ((err = snd_serialmidi_rmidi(serial)) < 0) {
		snd_device_free(card, serial);
		return err;
	}
	
	return 0;
}

static int __init snd_serial_probe(struct platform_device *devptr)
{
	struct snd_card *card;
	int err;
	int dev = devptr->id;

	card = snd_card_new(index[dev], id[dev], THIS_MODULE, 0);
	if (card == NULL)
		return -ENOMEM;
	strcpy(card->driver, "Serial MIDI");
	if (id[dev] && *id[dev])
		snprintf(card->shortname, sizeof(card->shortname), "%s", id[dev]);
	else
		strcpy(card->shortname, card->driver);

	if (devices[dev] > 1) {
		int i, start_dev;
		char devname[32];
		/* assign multiple devices to a single card */
		if (devices[dev] > 8) {
			printk(KERN_ERR "serialmidi: invalid devices %d\n", devices[dev]);
			snd_card_free(card);
			return -EINVAL;
		}
		/* device name mangling */
		strncpy(devname, sdev[dev], sizeof(devname));
		devname[31] = 0;
		i = strlen(devname);
		if (i > 0) i--;
		if (devname[i] >= '0' && devname[i] <= '9') {
			for (; i > 0; i--)
				if (devname[i] < '0' || devname[i] > '9') {
					i++;
					break;
				}
			start_dev = simple_strtoul(devname + i, NULL, 0);
			devname[i] = 0;
		} else
			start_dev = 0;
		for (i = 0; i < devices[dev]; i++) {
			char devname2[33];
			sprintf(devname2, "%s%d", devname, start_dev + i);
			if ((err = snd_serialmidi_create(card, devname2, speed[dev],
							 adaptor[dev], outs[dev], i,
							 handshake[dev])) < 0) {
				snd_card_free(card);
				return err;
			}
		}
	} else {
		if ((err = snd_serialmidi_create(card, sdev[dev], speed[dev],
						 adaptor[dev], outs[dev], 0,
						 handshake[dev])) < 0) {
			snd_card_free(card);
			return err;
		}
	}

	sprintf(card->longname, "%s at %s", card->shortname, sdev[dev]);

	snd_card_set_dev(card, &devptr->dev);

	if ((err = snd_card_register(card)) < 0) {
		snd_card_free(card);
		return err;
	}

	platform_set_drvdata(devptr, card);
	return 0;
}

static int snd_serial_remove(struct platform_device *devptr)
{
	snd_card_free(platform_get_drvdata(devptr));
	platform_set_drvdata(devptr, NULL);
	return 0;
}

#define SND_SERIAL_DRIVER	"snd_serial"

static struct platform_driver snd_serial_driver = {
	.probe		= snd_serial_probe,
	.remove		= snd_serial_remove,
	.driver		= {
		.name	= SND_SERIAL_DRIVER
	},
};

static void __init_or_module snd_serial_unregister_all(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devices); ++i)
		platform_device_unregister(devptrs[i]);
	platform_driver_unregister(&snd_serial_driver);
}

static int __init alsa_card_serialmidi_init(void)
{
	int i, cards, err;

	if ((err = platform_driver_register(&snd_serial_driver)) < 0)
		return err;

	cards = 0;
	for (i = 0; i < SNDRV_CARDS; i++) {
		struct platform_device *device;
		if (! enable[i])
			continue;
		device = platform_device_register_simple(SND_SERIAL_DRIVER,
							 i, NULL, 0);
		if (IS_ERR(device)) {
			err = PTR_ERR(device);
			goto errout;
		}
		devptrs[i] = device;
		cards++;
	}
	if (!cards) {
#ifdef MODULE
		printk(KERN_ERR "serial MIDI device not found or device busy\n");
#endif
		err = -ENODEV;
		goto errout;
	}
	return 0;

 errout:
	snd_serial_unregister_all();
	return err;
}

static void __exit alsa_card_serialmidi_exit(void)
{
	snd_serial_unregister_all();
}

module_init(alsa_card_serialmidi_init)
module_exit(alsa_card_serialmidi_exit)
