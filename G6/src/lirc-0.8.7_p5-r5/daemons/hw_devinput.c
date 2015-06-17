/****************************************************************************
 ** hw_devinput.c ***********************************************************
 ****************************************************************************
 *
 * receive keycodes input via /dev/input/...
 * 
 * Copyright (C) 2002 Oliver Endriss <o.endriss@gmx.de>
 *
 * Distribute under GPL version 2 or later.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>

#include <linux/input.h>
#include <linux/uinput.h>

#ifndef EV_SYN
/* previous name */
#define EV_SYN EV_RST
#endif

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"

/* from evtest.c - Copyright (c) 1999-2000 Vojtech Pavlik */
#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)
/* NBITS was defined in linux/uinput.h */
#undef NBITS
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

static int devinput_init();
static int devinput_init_fwd();
static int devinput_deinit(void);
static int devinput_decode(struct ir_remote *remote,
			   ir_code *prep, ir_code *codep, ir_code *postp,
			   int *repeat_flagp,
			   lirc_t *min_remaining_gapp,
			   lirc_t *max_remaining_gapp);
static char *devinput_rec(struct ir_remote *remotes);

enum locate_type {
	locate_by_name,
	locate_by_phys,
};

struct hardware hw_devinput=
{
	"/dev/input/event0",	/* "device" */
	-1,			/* fd (device) */
	LIRC_CAN_REC_LIRCCODE,	/* features */
	0,			/* send_mode */
	LIRC_MODE_LIRCCODE,	/* rec_mode */
	64,			/* code_length */
	devinput_init_fwd,	/* init_func */
	devinput_deinit,	/* deinit_func */
	NULL,			/* send_func */
	devinput_rec,		/* rec_func */
	devinput_decode,	/* decode_func */
	NULL,                   /* ioctl_func */
	NULL,			/* readdata */
	"devinput"
};

static ir_code code;
static ir_code code_compat;
static int exclusive = 0;
static int uinputfd = -1;
static struct timeval start,end,last;

enum
{
	RPT_UNKNOWN = -1,
	RPT_NO = 0,
	RPT_YES = 1,
};

static int repeat_state = RPT_UNKNOWN;

static int setup_uinputfd(const char *name, int source)
{
	int fd;
	int key;
	struct uinput_user_dev dev;
	long events[NBITS(EV_MAX)];
	long bits[NBITS(KEY_MAX)];
	
	if(ioctl(source, EVIOCGBIT(0, EV_MAX), events) == -1)
	{
		return -1;
	}
	if(!test_bit(EV_REL, events) && !test_bit(EV_ABS, events))
	{
		/* no move events, don't forward anything */
		return -1;
	}
	fd = open("/dev/input/uinput", O_RDWR);
	if(fd == -1)
	{
		fd = open("/dev/uinput", O_RDWR);
		if(fd == -1)
		{
			fd = open("/dev/misc/uinput", O_RDWR);
			if(fd == -1)
			{
				logprintf(LOG_WARNING, "could not open %s\n",
					  "uinput");
				logperror(LOG_WARNING, NULL);
				return -1;
			}
		}
	}
	memset(&dev, 0, sizeof(dev));
	if(ioctl(source, EVIOCGNAME(sizeof(dev.name)), dev.name) >= 0)
	{
		dev.name[sizeof(dev.name)-1] = 0;
		if(strlen(dev.name) > 0)
		{
			strncat(dev.name, " ", sizeof(dev.name) -
				strlen(dev.name));
			dev.name[sizeof(dev.name)-1] = 0;
		}
	}
	strncat(dev.name, name, sizeof(dev.name) - strlen(dev.name));
	dev.name[sizeof(dev.name)-1] = 0;

	if(write(fd, &dev, sizeof(dev)) != sizeof(dev))
	{
		goto setup_error;
	}
	
	if(test_bit(EV_KEY, events))
	{
		if(ioctl(source, EVIOCGBIT(EV_KEY, KEY_MAX), bits) == -1)
		{
			goto setup_error;
		}
		
		if(ioctl(fd, UI_SET_EVBIT, EV_KEY) == -1)
		{
			goto setup_error;
		}
		
		/* only forward mouse button events */
		for(key = BTN_MISC; key <= BTN_GEAR_UP; key++)
		{
			if(test_bit(key, bits))
			{
				if(ioctl(fd, UI_SET_KEYBIT, key) == -1)
				{
					goto setup_error;
				}
			}
		}
	}
	if(test_bit(EV_REL, events))
	{
		if(ioctl(source, EVIOCGBIT(EV_REL, REL_MAX), bits) == -1)
		{
			goto setup_error;
		}
		if(ioctl(fd, UI_SET_EVBIT, EV_REL) == -1)
		{
			goto setup_error;
		}
		for(key = 0; key <= REL_MAX; key++)
		{
			if(test_bit(key, bits))
			{
				if(ioctl(fd, UI_SET_RELBIT, key) == -1)
				{
					goto setup_error;
				}
			}
		}
	}
	if(test_bit(EV_ABS, events))
	{
		if(ioctl(source, EVIOCGBIT(EV_ABS, ABS_MAX), bits) == -1)
		{
			goto setup_error;
		}
		if(ioctl(fd, UI_SET_EVBIT, EV_ABS) == -1)
		{
			goto setup_error;
		}
		for(key = 0; key <= ABS_MAX; key++)
		{
			if(test_bit(key, bits))
			{
				if(ioctl(fd, UI_SET_ABSBIT, key) == -1)
				{
					goto setup_error;
				}
			}
		}
	}
	

	if(ioctl(fd, UI_DEV_CREATE) == -1)
	{
		goto setup_error;
	}
	return fd;
	
 setup_error:
	logprintf(LOG_ERR, "could not setup %s\n", "uinput");
	logperror(LOG_ERR, NULL);
	close(fd);
	return -1;
}

#if 0
/* using fnmatch */
static int do_match (const char *text, const char *wild)
{
	while (*wild)
	{
		if (*wild == '*')
		{
			const char *next = text;
			wild++;
			while(*next)
			{
				if(do_match (next, wild))
				{
					return 1;
				}
				next++;
			}
			return *wild ? 0:1;
		}
		else if (*wild == '?')
		{
			wild++;
			if (!*text++) return 0;
		}
		else if (*wild == '\\')
		{
			if (!wild[1])
			{
				return 0;
			}
			if (wild[1] != *text++)
			{
				return 0;
			}
			wild += 2;
		}
		else if (*wild++ != *text++)
		{
			return 0;
		}
	}
	return *text ? 0:1;
}
#endif

static int locate_dev (const char *pattern, enum locate_type type)
{
	static char devname[FILENAME_MAX];
	char ioname[255];
	DIR *dir;
	struct dirent *obj;
	int request;

	dir = opendir ("/dev/input");
	if (!dir)
	{
		return 1;
	}

	devname[0] = 0;
	switch (type)
	{
		case locate_by_name:
			request = EVIOCGNAME (sizeof (ioname));
			break;
#ifdef EVIOCGPHYS			
		case locate_by_phys:
			request = EVIOCGPHYS (sizeof (ioname));
			break;
#endif
		default:
			closedir (dir);
			return 1;
	}

	while ((obj = readdir (dir)))
	{
		int fd;
		if (obj->d_name[0] == '.' &&
		    (obj->d_name[1] == 0 ||
		     (obj->d_name[1] == '.' && obj->d_name[2] == 0)))
		{
			continue; /* skip "." and ".." */
		}
		sprintf (devname, "/dev/input/%s", obj->d_name);
		fd = open (devname, O_RDONLY);
		if (!fd)
		{
			continue;
		}
		if (ioctl (fd, request, ioname) >= 0)
		{
			int ret;
			close (fd);
			
			ioname[sizeof(ioname)-1] = 0;
			//ret = !do_match (ioname, pattern);
			ret = fnmatch(pattern, ioname, 0);
			if (ret == 0)
			{
				hw.device = devname;
				closedir (dir);
				return 0;
			}
		}
		close (fd);
	}

	closedir (dir);
	return 1;
}

int devinput_init()
{
	logprintf(LOG_INFO, "initializing '%s'", hw.device);

	if (!strncmp (hw.device, "name=", 5)) {
		if (locate_dev (hw.device + 5, locate_by_name)) {
			logprintf(LOG_ERR, "unable to find '%s'", hw.device);
			return 0;
		}
	}
	else if (!strncmp (hw.device, "phys=", 5)) {
		if (locate_dev (hw.device + 5, locate_by_phys)) {
			logprintf(LOG_ERR, "unable to find '%s'", hw.device);
			return 0;
		}
	}
	
	if ((hw.fd = open(hw.device, O_RDONLY)) < 0) {
		logprintf(LOG_ERR, "unable to open '%s'", hw.device);
		return 0;
	}
	
#if 0
	exclusive = 1;
	if (ioctl(hw.fd, EVIOCGRAB, 1) == -1)
	{
		exclusive = 0;
		logprintf(LOG_WARNING, "can't get exclusive access to events "
			  "coming from `%s' interface",
			  hw.device);
	}
#endif
	return 1;
}

int devinput_init_fwd()
{
	if(!devinput_init()) return 0;
	
	if(exclusive)
	{
		uinputfd = setup_uinputfd("(lircd bypass)", hw.fd);
	}
	
	return 1;
}


int devinput_deinit(void)
{
	logprintf(LOG_INFO, "closing '%s'", hw.device);
	if(uinputfd != -1)
	{
		ioctl(uinputfd, UI_DEV_DESTROY);
		close(uinputfd);
		uinputfd = -1;
	}
	close(hw.fd);
	hw.fd=-1;
	return 1;
}


int devinput_decode(struct ir_remote *remote,
		    ir_code *prep, ir_code *codep, ir_code *postp,
		    int *repeat_flagp,
		    lirc_t *min_remaining_gapp,
		    lirc_t *max_remaining_gapp)
{
	LOGPRINTF(1, "devinput_decode");

        if(!map_code(remote,prep,codep,postp,
                     0,0,hw_devinput.code_length,code,0,0))
        {
		static int print_warning = 1;
		
		if(!map_code(remote,prep,codep,postp,
			     0,0,32,code_compat,0,0))
		{
			return(0);
		}
                if(print_warning)
		{
			print_warning = 0;
			logperror(LOG_WARNING, "you are using an obsolete devinput config file");
			logperror(LOG_WARNING, "get the new version at http://lirc.sourceforge.net/remotes/devinput/lircd.conf.devinput");
		}
        }
	
	map_gap(remote, &start, &last, 0, repeat_flagp,
		min_remaining_gapp, max_remaining_gapp);
	/* override repeat */
	switch( repeat_state )
	{
	case RPT_NO:
		*repeat_flagp = 0;
		break;
	case RPT_YES:
		*repeat_flagp = 1;
		break;
	default:
		break;
	}
	
	return 1;
}


char *devinput_rec(struct ir_remote *remotes)
{
	struct input_event event;
	int rd;
	ir_code value;

	LOGPRINTF(1, "devinput_rec");
	
	last=end;
	gettimeofday(&start,NULL);
	
	rd = read(hw.fd, &event, sizeof event);
	if (rd != sizeof event) {
		logprintf(LOG_ERR, "error reading '%s'", hw.device);
		if(rd <= 0 && errno != EINTR)
		{
			devinput_deinit();
		}
		return 0;
	}

	LOGPRINTF(1, "time %ld.%06ld  type %d  code %d  value %d",
		  event.time.tv_sec, event.time.tv_usec,
		  event.type, event.code, event.value);
	
	value = (unsigned) event.value;
#ifdef EV_SW
	if(value == 2 && (event.type == EV_KEY || event.type == EV_SW))
	{
		value = 1;
	}
	code_compat = ((event.type == EV_KEY || event.type == EV_SW) &&
		       event.value != 0) ? 0x80000000 : 0;
#else
	if(value == 2 && event.type == EV_KEY)
	{
		value = 1;
	}
	code_compat = ((event.type == EV_KEY) &&
		       event.value != 0) ? 0x80000000 : 0;
#endif
	code_compat |= ((event.type & 0x7fff) << 16);
	code_compat |= event.code;

	if(event.type == EV_KEY)
	{
		if(event.value == 2)
		{
			repeat_state = RPT_YES;
		}
		else
		{
			repeat_state = RPT_NO;
		}
	}
	else
	{
		repeat_state = RPT_UNKNOWN;
	}
	
	code = ((ir_code) (unsigned) event.type) << 48 |
		((ir_code) (unsigned) event.code) << 32 |
		value;
	
	LOGPRINTF(1, "code %.8llx", code);

	if(uinputfd != -1)
	{
		if(event.type == EV_REL ||
		   event.type == EV_ABS ||
		   (event.type == EV_KEY &&
		    event.code >= BTN_MISC &&
		    event.code <= BTN_GEAR_UP) ||
		   event.type == EV_SYN)
		{
			LOGPRINTF(1, "forwarding: %04x %04x", event.type, event.code);
			if(write(uinputfd, &event, sizeof(event)) != sizeof(event))
			{
				logprintf(LOG_ERR, "writing to uinput failed");
				logperror(LOG_ERR, NULL);
			}
			return NULL;
		}
	}
	
	/* ignore EV_SYN */
	if(event.type == EV_SYN) return NULL;

	gettimeofday(&end,NULL);
	return decode_all(remotes);
}
