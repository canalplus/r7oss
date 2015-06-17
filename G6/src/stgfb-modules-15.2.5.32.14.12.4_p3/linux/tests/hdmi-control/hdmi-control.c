/*
 * hdmi-control.c
 *
 * Copyright (C) STMicroelectronics Limited 2007-2012. All rights reserved.
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

#include <linux/kernel/drivers/stm/hdmi/stmhdmi.h>

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
	printf("  -a, --audio-source=[spdif|2ch_i2s|4ch_i2s|6ch_i2s|8ch_i2s|pcm|disabled] \n");
	printf("     Select the HDMI hardware audio source. pcm == 2ch_i2s for compatibility\n\n");
	printf("  -A, --channel_count=[2 to 8 | s: Refer to Stream Header]\n");
	printf("     Set HDMI audio config according CEA-861-E\n\n");
	printf("  -c, --content-type=[graphics|photo|cinema|game|video]\n");
	printf("     Select the IT content type sent in the AVI info frame,\n");
	printf("     video == CE, i.e. IT bit = 0.\n\n");
	printf("  -C, --cea-mode-select=[edid|picar|4x3|16x9]\n");
	printf("     Select how the CEA mode number is chosen for SD/ED modes\n");
	printf("     using the EDID aspect ratio, the picture aspect ratio, or explicitly\n");
	printf("  -d, --display=NUM\n");
	printf("     Specify the display device to use (default = 0).\n\n");
	printf("  -D, --disable\n");
	printf("     Disable HDMI output.\n\n");
	printf("  -e, --edid-mode=[strict|nonstrict|connection event|disconnection event] \n");
	printf("     Specify how display modes not specified in the sink EDID are\n");
	printf("     handled (default = strict).\n\n");
	printf("  -E, --enable\n");
	printf("     Enable HDMI output.\n\n");
	printf("  -g, --extended-colorimetry= [enable|disable]\n");
	printf("     Set extended colorimetry information validity\n\n");
	printf("  -f, --safe-mode=[dvi|hdmi]\n");
	printf("     Specify the protocol to use in safe mode (default = dvi)\n\n");
	printf("  -F, --force-output-color=[enable|disable]\n");
	printf("     Force HDMI output to a single color\n\n");
	printf("  -S, --set-forced-color=0xRRGGBB\n");
	printf("     Forced HDMI output color\n\n");
	printf("  -h, --help\n");
	printf("     Show this message\n\n");
	printf("  -m, --mute\n");
	printf("     Set A/V mute\n\n");
	printf("  -M, --speaker_mapping=[0 to 0x31]\n");
	printf("     Set HDMI audio config according CEA-861-E\n\n");
	printf("  -o, --hotplug-mode=[restart|disable|lazydisable]\n");
	printf("     Specify how a hotplug de-assert or pulse should be\n");
	printf("     treated (default = restart)\n\n");
	printf("  -q, --quantization-mode=[auto|default|limited|full]\n");
	printf("     Specify how the AVI RGB and YCC quantization bits will be set\n");
	printf("     treated (default = restart)\n\n");
	printf("  -R, --force-restart\n");
	printf("     Force an attempt to restart the HDMI output if connected to a display but currently stopped.\n\n");
	printf("  -r, --pixel-repeat\n");
	printf("     Set pixel repetition[1|2|4].\n\n");
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
	bool update_audio_cfg   = false;
	struct stmhdmiio_audio info = { };
	int  channel_count      = 8;
	int  speaker_mapping    = 0x1f;
	bool update_audio_type  = false;
	int  audio_type         = 0;
	bool update_scan_mode   = false;
	int  scan_mode          = STMHDMIIO_SCAN_UNKNOWN;
	bool update_content_type= false;
	int  content_type       = STMHDMIIO_CE_CONTENT;
	bool set_avmute         = false;
	bool clear_avmute       = false;
	bool update_edid_mode   = false;
	bool display_connection_status = false;
	int  edid_mode          = STMHDMIIO_EDID_STRICT_MODE_HANDLING;
	bool update_safe_mode   = false;
	int  safe_mode          = STMHDMIIO_SAFE_MODE_DVI;
	bool update_force_output_color = false;
	unsigned long force_output_color = 0;
	bool update_forced_rgb_color = false;
	unsigned long  forced_rgb_color = 0;
	bool update_cea_mode    = false;
	int  cea_mode           = STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO;
	bool update_hotplug_mode= false;
	int  quantization_mode  = STMHDMIIO_QUANTIZATION_AUTO;
	bool update_quantization= false;
	stmhdmiio_hpd_mode_t  hotplug_mode = STMHDMIIO_HPD_STOP_AND_RESTART;
	int  disabled           = 0;
	bool update_disabled    = false;
	bool update_ec          = false;
	unsigned long force_ec  = 0;
	bool force_restart      = false;
	bool update_pixel_repetition = false;
	struct hdmi_event read_event = {0};
	struct hdmi_event_subscription evt_subscription = {0};
	unsigned long count     = 0;
	char ret                = 1;
	char buffer             = 0;

	int  pixel_repeat = 1;
	int fd;

	static struct option long_options[] = {
		{ "audio-source", 1, 0, 'a' },
		{ "channel_count", 1, 0, 'A' },
		{ "content-type", 1, 0, 'c' },
		{ "cea-mode-select", 1, 0, 'C' },
		{ "display", 1, 0, 'd' },
		{ "disable", 0, 0, 'D' },
		{ "edid-mode", 1, 0, 'e' },
		{ "enable", 0, 0, 'E' },
		{ "safe-mode", 1, 0, 'f' },
		{ "force-output-color", 1, 0, 'F' },
		{ "extended-colorimetry", 1, 0, 'g' },
		{ "set-forced-color", 1, 0, 'S' },
		{ "help", 0, 0, 'h' },
		{ "mute", 0, 0, 'm' },
		{ "speaker_mapping", 1, 0, 'M' },
		{ "hotplug-mode", 1, 0, 'o' },
		{ "quantization-mode", 1, 0, 'q' },
		{ "force-restart", 0, 0, 'R' },
		{ "pixel-repetition", 1, 0, 'r' },
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
	while ((option = getopt_long (argc, argv, "a:A:c:C:d:De:Ef:F:g:hmo:M:q:Rr:s:S:t:u", long_options, NULL)) != -1) {
		switch (option) {
		case 'a':
			if(optarg[0] == '2' || optarg[0] == 'p' || optarg[0] == 'P') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_2CH_I2S;
			} else if(optarg[0] == '4') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_4CH_I2S;
			} else if(optarg[0] == '6') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_6CH_I2S;
			} else if(optarg[0] == '8') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_8CH_I2S;
			} else if(optarg[0] == 's' || optarg[0] == 'S') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_SPDIF;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_audio_input = true;
				audio_input = STMHDMIIO_AUDIO_SOURCE_NONE;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'A':
			if(optarg[0] == 's' || optarg[0] == 'S') {
				channel_count = 1;
				printf("channel_count = Refer to Stream Header\n");
			} else {
				channel_count = (int)strtol(optarg, NULL, 0);
				if (channel_count < 2 || channel_count > 8 ) {
					fprintf(stderr, "invalid number channel %d\n", channel_count);
					printf("Unknown argument, try 'hdmi-control --help' for more information\n");
					break;
				}
				printf("channel_count = %d \n", channel_count);
			}
			info.channel_count = channel_count;
			update_audio_cfg = true;
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
		case 'D':
			disabled = 1;
			update_disabled = true;
			break;
		case 'e':
			if(optarg[0] == 's' || optarg[0] == 'S') {
				update_edid_mode = true;
				edid_mode = STMHDMIIO_EDID_STRICT_MODE_HANDLING;
			} else if(optarg[0] == 'n' || optarg[0] == 'N') {
				update_edid_mode = true;
				edid_mode = STMHDMIIO_EDID_NON_STRICT_MODE_HANDLING;
			}
			 else if(optarg[0] == 'c' || optarg[0] == 'C') {
				display_connection_status = true;
				evt_subscription.type = HDMI_EVENT_DISPLAY_CONNECTED;
			}
			else if(optarg[0] == 'd' || optarg[0] == 'D') {
				display_connection_status = true;
				evt_subscription.type = HDMI_EVENT_DISPLAY_DISCONNECTED;
			}
			else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'E':
			disabled = 0;
			update_disabled = true;
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
		case 'F':
			if(optarg[0] == 'e' || optarg[0] == 'E') {
				update_force_output_color = true;
				force_output_color = 1;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_force_output_color = true;
				force_output_color = 0;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'g':
			if(optarg[0] == 'e' || optarg[0] == 'E') {
				update_ec = true;
				force_ec = 1;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_ec = true;
				force_ec = 0;
			} else {
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			break;
		case 'S':
			update_forced_rgb_color = true;
			forced_rgb_color = strtoul(optarg,NULL,16);
			break;
		case 'h':
			issue_help_message();
			break;
		case 'm':
			set_avmute = true;
			clear_avmute = false;
			break;
		case 'M':
			speaker_mapping = (int)strtol(optarg, NULL, 0);
			if (speaker_mapping < 0 || speaker_mapping > 0x31 ) {
				fprintf(stderr, "invalid speaker mapping %d\n", speaker_mapping);
				printf("Unknown argument, try 'hdmi-control --help' for more information\n");
			}
			else {
				info.speaker_mapping = speaker_mapping;
				update_audio_cfg = true;
				printf("speaker_mapping = %d (%#x)\n", speaker_mapping, speaker_mapping);
			}
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
		case 'q':
			if(optarg[0] == 'a' || optarg[0] == 'A') {
				update_quantization = true;
				quantization_mode = STMHDMIIO_QUANTIZATION_AUTO;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				update_quantization = true;
				quantization_mode = STMHDMIIO_QUANTIZATION_DEFAULT;
			} else if(optarg[0] == 'l' || optarg[0] == 'L') {
				update_quantization = true;
				quantization_mode = STMHDMIIO_QUANTIZATION_LIMITED;
			} else if(optarg[0] == 'f' || optarg[0] == 'F') {
				update_quantization = true;
				quantization_mode = STMHDMIIO_QUANTIZATION_FULL;
			}
			break;
		case 'R':
			force_restart = true;
			break;
		case 'r':
			pixel_repeat = (int)strtol(optarg, NULL, 0);
			if (pixel_repeat <= 0 || pixel_repeat > 4 || pixel_repeat == 3) {
				fprintf(stderr, "Invalid pixel repetition %d, try 'hdmi-control --help' for more information\n", pixel_repeat);
			}
			else {
				update_pixel_repetition= true;
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
		exit(3);
	}

	if(update_disabled) {
		if(ioctl(fd, STMHDMIIO_SET_DISABLED, disabled)<0) {
			perror("Unable to change disabled state");
			goto exit_failed;
		}
	}

	if(update_audio_input) {
		if(ioctl(fd, STMHDMIIO_SET_AUDIO_SOURCE, audio_input)<0) {
			perror("Unable to change audio source");
			goto exit_failed;
		}
	}
	if(update_audio_cfg) {
		if(ioctl(fd, STMHDMIIO_SET_AUDIO_DATA, &info)<0) {
			perror("Unable to change audio cfg");
			goto exit_failed;
		}
	}

	if(update_audio_type) {
		if(ioctl(fd, STMHDMIIO_SET_AUDIO_TYPE, audio_type)<0) {
			perror("Unable to change audio type");
			goto exit_failed;
		}
	}

	if(update_scan_mode) {
		if(ioctl(fd, STMHDMIIO_SET_OVERSCAN_MODE, scan_mode)<0) {
			perror("Unable to change overscan information");
			goto exit_failed;
		}
	}

	if(update_edid_mode) {
		if(ioctl(fd, STMHDMIIO_SET_EDID_MODE_HANDLING, edid_mode)<0) {
			perror("Unable to change edid mode");
			goto exit_failed;
		}
	}

	if(update_safe_mode) {
		if(ioctl(fd, STMHDMIIO_SET_SAFE_MODE_PROTOCOL, safe_mode)<0) {
			perror("Unable to change safe mode protocol");
			goto exit_failed;
		}
	}

	if(update_force_output_color) {
		if(ioctl(fd, STMHDMIIO_SET_FORCE_OUTPUT, force_output_color)<0) {
			perror("Unable to force a single color");
			goto exit_failed;
		}
	}

	if(update_ec) {
		if(ioctl(fd, STMHDMIIO_SET_EXTENDED_COLOR, force_ec)<0) {
			perror("Unable to set extended colorimtery");
			goto exit_failed;
		}
	}

	if(update_forced_rgb_color) {
		if(ioctl(fd, STMHDMIIO_SET_FORCED_RGB_VALUE, forced_rgb_color)<0) {
			perror("Unable to force RGB value");
			goto exit_failed;
		}
	}

	if(update_cea_mode) {
		if(ioctl(fd, STMHDMIIO_SET_CEA_MODE_SELECTION, cea_mode)<0) {
			perror("Unable to change CEA mode selection");
			goto exit_failed;
		}
	}

	if(update_hotplug_mode) {
		if(ioctl(fd, STMHDMIIO_SET_HOTPLUG_MODE, hotplug_mode)<0) {
			perror("Unable to change hotplug mode");
			goto exit_failed;
		}
	}

	if(update_content_type) {
		if(ioctl(fd, STMHDMIIO_SET_CONTENT_TYPE, content_type)<0) {
			perror("Unable to change content information");
			goto exit_failed;
		}
	}

	if(update_quantization) {
		if(ioctl(fd, STMHDMIIO_SET_QUANTIZATION_MODE, quantization_mode)<0) {
			perror("Unable to change quantization mode");
			goto exit_failed;
		}
	}

	if(set_avmute || clear_avmute) {
		unsigned long val = set_avmute?1:0;
		if(ioctl(fd,STMHDMIIO_SET_AVMUTE,val)<0) {
			perror("Unable to set AV Mute state");
			goto exit_failed;
		}
	}

	if(force_restart) {
		if(ioctl(fd,STMHDMIIO_FORCE_RESTART)<0) {
			perror("Restart not available in current state");
			goto exit_failed;
		}
	}
	if(update_pixel_repetition) {
		if(ioctl(fd,STMHDMIIO_SET_PIXEL_REPETITION, pixel_repeat)<0) {
			perror("Unable to change the pixel repetition");
			goto exit_failed;
		}
	}
	if(display_connection_status) {
		if(ioctl(fd,STMHDMIIO_GET_DISPLAY_CONNECTION_STATE, &count)<0) {
			perror("Unable to get dispay connection sate");
			goto exit_failed;
		}
		switch(count)
		{
			case (HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_DVI):
				printf("CONNECTION STATUS ====> DVI is connected\n");
				break;
			case (HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_HDMI):
				printf("CONNECTION STATUS ====> HDMI is connected\n");
				break;
			case (HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_SAFEMODE_DVI):
			    printf("CONNECTION STATUS ====> SAFEMOE DVI is connected\n");
			    break;
			case (HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_SAFEMODE_HDMI):
			    printf("CONNECTION STATUS ====> SAFEMOE HDMI is connected\n");
			    break;
			default:
			    printf("CONNECTION STATUS ====> Display is disconnected\n");
				break;
        }
		if(ioctl(fd,STMHDMIIO_SUBSCRIBE_EVENT, &evt_subscription)<0) {
			perror("Unable to subscribe to dispay connection event");
			goto exit_failed;
		}
		if(ioctl(fd,STMHDMIIO_DQEVENT, &read_event)<0) {
			perror("Unable to set DQ edid event\n");
		}
		switch (read_event.type) {
			case HDMI_EVENT_DISPLAY_CONNECTED:
				printf("CONNECTION EVENT  ====> Display is connected\n");
				printf("CONNECTION INFO   ====> Sequence   = %d \n", read_event.sequence);
				printf("CONNECTION INFO   ====> Time stamp = %lu \n", (unsigned long)read_event.timestamp.tv_sec);
				switch(read_event.u.data[0])
				{
					case HDMI_SINK_IS_DVI:
						printf("CONNECTION INFO   ====> Display is DVI\n");
					break;
					case HDMI_SINK_IS_HDMI:
						printf("CONNECTION INFO   ====> Display is HDMI\n");
					break;
					case HDMI_SINK_IS_SAFEMODE_DVI:
						printf("CONNECTION INFO   ====> Display is SAFEMODE DVI\n");
					break;
					case HDMI_SINK_IS_SAFEMODE_HDMI:
						printf("CONNECTION INFO   ====> Display is SAFEMODE HDMI\n");
					break;
				}
				break;
			case HDMI_EVENT_DISPLAY_DISCONNECTED:
				printf("CONNECTION EVENT  ====> Display is disconnected\n");
			break;
			default:
				perror("Unknown event\n");
				break;
		}
		if(((read_event.u.data[0] == HDMI_SINK_IS_HDMI)||(HDMI_SINK_IS_HDMI == HDMI_SINK_IS_DVI))
		&&(read_event.type == HDMI_EVENT_DISPLAY_CONNECTED))
		{
			printf("=========\nDump EDID\n=========\n");
			while (ret!=0)
			{
				ret = read(fd, &buffer, sizeof(char));
				if(ret !=0)
					printf("%02x ", buffer);
				count = count%16;
				if(count == 0)
					printf("\n");
				count++;
			}
		}
		if(ioctl(fd,STMHDMIIO_UNSUBSCRIBE_EVENT)<0) {
			perror("Unable to unsubscribe from dispay connection event\n");
		}
	}
	close(fd);
	return 0;

exit_failed:
	close(fd);
	return 1;
}
