/*
 * hdmi-control.c
 *
 * Copyright (C) STMicroelectronics Limited 2007. All rights reserved.
 *
 * Demonstration program showing how to use the hdmi device controls
 */

#include <sys/types.h> // undefines __USE_POSIX
#include <sys/ioctl.h>

#define __USE_POSIX
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

#include <linux/drivers/stm/coredisplay/stmhdmi.h>

#define lengthof(a) (sizeof(a) / sizeof((a)[0]))

#define HDMIDEV "/dev/hdmi%d.%d"

static int device_number = 0;
static int subdevice_number = 0;

int open_device()
{
	char devname[256];

	snprintf(devname, 256, HDMIDEV, device_number, subdevice_number);

	return open(devname, O_RDWR);
}


static void issue_help_message(void)
{
	printf("Usage: hdmi-control [OPTIONS]\n");
	printf("Change the state of the connected HDMI device.\n");
	printf("\n");
	printf("Mandatory arguments to long options are mandatory for short options too.\n");
	printf("String arguments only require the first letter and are case insensitive.\n\n");
	printf("  -a, --audio-source=[spdif|2ch_i2s|8ch_i2s|pcm|disabled] \n");
	printf("     Select the HDMI hardware audio source. pcm == 2ch_i2s for compatibility\n\n");
	printf("  -c, --content-type=[graphics|photo|cinema|game|video]\n");
	printf("     Select the IT content type sent in the AVI info frame,\n");
	printf("     video == CE, i.e. IT bit = 0.\n\n");
	printf("  -C, --cea-mode-select=[edid|picar|4x3|16x9]\n");
	printf("     Select how the CEA mode number is chosen for SD/ED modes\n");
	printf("     using the EDID aspect ratio, the picture aspect ratio, or explicitly\n");
	printf("  -d, --display=NUM\n");
	printf("     Specify the display device to use (default = 0).\n\n");
	printf("  -e, --edid-mode=[strict|nonstrict]\n");
	printf("     Specify how display modes not specified in the sink EDID are\n");
	printf("     handled (default = strict).\n\n");
	printf("  -f, --safe-mode=[dvi|hdmi]\n");
	printf("     Specify the protocol to use in safe mode (default = dvi)\n");
	printf("  -h, --help\n");
	printf("     Show this message\n\n");
	printf("  -m, --mute\n");
	printf("     Set A/V mute\n\n");
	printf("  -o, --hotplug-mode=[restart|disable|lazydisable]\n");
	printf("     Specify how a hotplug de-assert or pulse should be\n");
	printf("     treated (default = restart)\n\n");
	printf("  -s, --scan-type=[notknown|overscan|underscan]\n");
	printf("     Select the scan type sent in the AVI info frame.\n\n");
	printf("  -t, --audio-type=[normal|1-bit|DST|2xDST|HBR]\n");
	printf("     Select the audio packet type.\n\n");
	printf("  -u, --unmute\n");
	printf("     Clear A/V mute\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	bool update_audio_input = false;
	int  audio_input        = 0;
	bool update_audio_type  = false;
	int  audio_type         = 0;
	bool update_scan_mode   = false;
	int  scan_mode          = STMHDMIIO_SCAN_UNKNOWN;
	bool update_content_type= false;
	int  content_type       = STMHDMIIO_CE_CONTENT;
	bool set_avmute         = false;
	bool clear_avmute       = false;
	bool update_edid_mode   = false;
	int  edid_mode          = STMHDMIIO_EDID_STRICT_MODE_HANDLING;
	bool update_safe_mode   = false;
	int  safe_mode          = STMHDMIIO_SAFE_MODE_DVI;
	bool update_cea_mode    = false;
	int  cea_mode           = STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO;
	bool update_hotplug_mode= false;
	stmhdmiio_hpd_mode_t  hotplug_mode = STMHDMIIO_HPD_STOP_AND_RESTART;
	int fd;

	static struct option long_options[] = {
		{ "audio-source", 1, 0, 'a' },
		{ "content-type", 1, 0, 'c' },
		{ "cea-mode-select", 1, 0, 'C' },
		{ "display", 1, 0, 'd' },
		{ "edid-mode", 1, 0, 'e' },
		{ "safe-mode", 1, 0, 'f' },
		{ "help", 0, 0, 'h' },
		{ "mute", 0, 0, 'm' },
		{ "hotplug-mode", 1, 0, 'o' },
		{ "scan-type", 1, 0, 's' },
		{ "audio-type", 1, 0, 't' },
		{ "unmute", 0, 0, 'u' },
		{ 0, 0, 0, 0 }
	};

	if(argc == 1) {
		issue_help_message();
		return 0;
	}

	int option;
	while ((option = getopt_long (argc, argv, "a:c:C:d:e:f:hmo:s:t:u", long_options, NULL)) != -1) {
		switch (option) {
		case 'a':
			if(optarg[0] == '2' || optarg[0] == 'p' || optarg[0] == 'P') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_2CH_I2S;
			} else if(optarg[0] == '8') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_8CH_I2S;
			} else if(optarg[0] == 's' || optarg[0] == 's') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_SPDIF;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_NONE;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'c':
			if(!strncmp(optarg,"gr",2)) {
				update_content_type = true;
				content_type = STMHDMIIO_IT_GRAPHICS;
			} else if(optarg[0] == 'p' || optarg[0] == 'P') {
				update_content_type = true;
				content_type = STMHDMIIO_IT_PHOTO;
			} else if(optarg[0] == 'c' || optarg[0] == 'C') {
				update_content_type = true;
				content_type = STMHDMIIO_IT_CINEMA;
			} else if(!strncmp(optarg,"ga",2)) {
				update_content_type = true;
				content_type = STMHDMIIO_IT_GAME;
			} else if(optarg[0] == 'v' || optarg[0] == 'V') {
				update_content_type = true;
				content_type = STMHDMIIO_CE_CONTENT;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'C':
			if(optarg[0] == 'e' || optarg[0] == 'E') {
				update_cea_mode = true;
				cea_mode = STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO;
			} else if(optarg[0] == 'p' || optarg[0] == 'P') {
				update_cea_mode = true;
				cea_mode = STMHDMIIO_CEA_MODE_FOLLOW_PICTURE_ASPECT_RATIO;
			} else if(optarg[0] == '4') {
				update_cea_mode = true;
				cea_mode = STMHDMIIO_CEA_MODE_4_3;
			} else if(optarg[0] == '1') {
				update_cea_mode = true;
				cea_mode = STMHDMIIO_CEA_MODE_16_9;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'd':
			device_number = atoi(optarg);
			break;
		case 'e':
			if(optarg[0] == 's' || optarg[0] == 'S') {
				update_edid_mode = true;
				edid_mode = STMHDMIIO_EDID_STRICT_MODE_HANDLING;
			} else if(optarg[0] == 'n' || optarg[0] == 'N') {
				update_edid_mode = true;
				edid_mode = STMHDMIIO_EDID_NON_STRICT_MODE_HANDLING;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'f':
			if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_safe_mode = true;
				safe_mode = STMHDMIIO_SAFE_MODE_DVI;
			} else if(optarg[0] == 'h' || optarg[0] == 'H') {
				update_safe_mode = true;
				safe_mode = STMHDMIIO_SAFE_MODE_HDMI;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'h':
			issue_help_message();
			break;
		case 'm':
			set_avmute = true;
			clear_avmute = false;
			break;
		case 'o':
			if(optarg[0] == 'r' || optarg[0] == 'R') {
				update_hotplug_mode = true;
				hotplug_mode = STMHDMIIO_HPD_STOP_AND_RESTART;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_hotplug_mode = true;
				hotplug_mode = STMHDMIIO_HPD_DISABLE_OUTPUT;
			} else if(optarg[0] == 'l' || optarg[0] == 'L') {
				update_hotplug_mode = true;
				hotplug_mode = STMHDMIIO_HPD_STOP_IF_NECESSARY;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 's':
			if(optarg[0] == 'o' || optarg[0] == 'O') {
				update_scan_mode = true;
				scan_mode = STMHDMIIO_SCAN_OVERSCANNED;
			} else if(optarg[0] == 'u' || optarg[0] == 'U') {
				update_scan_mode = true;
				scan_mode = STMHDMIIO_SCAN_UNDERSCANNED;
			} else if(optarg[0] == 'n' || optarg[0] == 'N') {
				update_scan_mode = true;
				scan_mode = STMHDMIIO_SCAN_UNKNOWN;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 't':
			if(optarg[0] == '1') {
				update_audio_type = true;
				audio_type = STMHDMIIO_AUDIO_TYPE_ONEBIT;
			} else if(optarg[0] == '2') {
				update_audio_type = true;
				audio_type = STMHDMIIO_AUDIO_TYPE_DST_DOUBLE;
			} else if(optarg[0] == 'n' || optarg[0] == 'N') {
				update_audio_type = true;
				audio_type = STMHDMIIO_AUDIO_TYPE_NORMAL;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_audio_type = true;
				audio_type = STMHDMIIO_AUDIO_TYPE_DST;
			} else if(optarg[0] == 'h' || optarg[0] == 'H') {
				update_audio_type = true;
				audio_type = STMHDMIIO_AUDIO_TYPE_HBR;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'u':
			set_avmute = false;
			clear_avmute = true;
			break;
		default:
			printf("Try `hdmi-control --help' for more information.\n");
			exit(1);
		}
	}

	if (optind != argc) {
		printf ("hdmi-control: too many arguments\n");
		printf ("Try `hdmi-control --help' for more information.\n");
		exit(2);
	}

	if((fd = open_device())<0) {
		perror("Unable to open hdmi device");
		return 1;
	}

	if(update_audio_input) {
		if(ioctl(fd, STMHDMIIO_SET_AUDIO_SOURCE, audio_input)<0) {
			perror("Unable to change audio source");
			return 1;
		}
	}

	if(update_audio_type) {
		if(ioctl(fd, STMHDMIIO_SET_AUDIO_TYPE, audio_type)<0) {
			perror("Unable to change audio type");
			return 1;
		}
	}

	if(update_scan_mode) {
		if(ioctl(fd, STMHDMIIO_SET_OVERSCAN_MODE, scan_mode)<0) {
			perror("Unable to change overscan information");
			return 1;
		}
	}

	if(update_edid_mode) {
		if(ioctl(fd, STMHDMIIO_SET_EDID_MODE_HANDLING, edid_mode)<0) {
			perror("Unable to change edid mode");
			return 1;
		}
	}

	if(update_safe_mode) {
		if(ioctl(fd, STMHDMIIO_SET_SAFE_MODE_PROTOCOL, safe_mode)<0) {
			perror("Unable to change safe mode protocol");
			return 1;
		}
	}

	if(update_cea_mode) {
		if(ioctl(fd, STMHDMIIO_SET_CEA_MODE_SELECTION, cea_mode)<0) {
			perror("Unable to change CEA mode selection");
			return 1;
		}
	}

	if(update_hotplug_mode) {
		if(ioctl(fd, STMHDMIIO_SET_HOTPLUG_MODE, hotplug_mode)<0) {
			perror("Unable to change hotplug mode");
			return 1;
		}
	}

	if(update_content_type) {
		if(ioctl(fd, STMHDMIIO_SET_CONTENT_TYPE, content_type)<0) {
			perror("Unable to change content information");
			return 1;
		}
	}

	if(set_avmute || clear_avmute) {
		unsigned long val = set_avmute?1:0;
		if(ioctl(fd,STMHDMIIO_SET_AVMUTE,val)<0) {
			perror("Unable to set AV Mute state");
			return 1;
		}
	}

	return 0;
}
