/*
 * hdmi-control.c
 *
 * Copyright (C) STMicroelectronics Limited 2007. All rights reserved.
 *
 * Demonstration program showing how to use the hdmi device controls
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <string.h>
#include <linux/stmhdmi_v4l2.h>
#include <linux/v4l2/hdmi/stmhdmi_v4l2_ctrls.h>

char HDMIDEV[13];

static void hdmi_show_controls(struct v4l2_ext_control *g_ctrl, int count)
{
	int i = 0;

	for (i = 0; i < count; i++) {
		switch (g_ctrl[i].id) {
		case V4L2_CID_DV_STM_TX_AUDIO_SOURCE:
		{
			switch(g_ctrl[i].value) {
			/*
			 * For compatibility PCM is used
			 */
			case V4L2_DV_STM_AUDIO_SOURCE_2CH_I2S:
			case V4L2_DV_STM_AUDIO_SOURCE_PCM:
				printf("Audio source is 2 channel\n");
				break;

			case V4L2_DV_STM_AUDIO_SOURCE_8CH_I2S:
				printf("Audio source is 8 channel\n");
				break;

			case V4L2_DV_STM_AUDIO_SOURCE_SPDIF:
				printf("Audio source is spdif\n");
				break;

			case V4L2_DV_STM_AUDIO_SOURCE_NONE:
				printf("Audio source is none\n");
				break;
			
			default:
				printf("Invalid audio source\n");

			}
			break;
		}

		case V4L2_CID_DV_STM_TX_CONTENT_TYPE:
		{
			switch(g_ctrl[i].value) {
			case V4L2_DV_STM_IT_GRAPHICS:
				printf("Content type is graphics\n");
				break;

			case V4L2_DV_STM_IT_PHOTO:
				printf("Content type is photo\n");
				break;

			case V4L2_DV_STM_IT_CINEMA:
				printf("Content type is cinema\n");
				break;

			case V4L2_DV_STM_IT_GAME:
				printf("Content type is game \n");
				break;

			case V4L2_DV_STM_CE_CONTENT:
				printf("Content type is ce\n");
				break;

			default:
				printf("Unknown conten type\n");

			}
			break;	
		}

		case V4L2_CID_DV_STM_TX_CONTROL:
		{
			if (g_ctrl[i].value)
				printf("HDMI-Tx is enabled\n");
			else
				printf("HDMI-Tx is disabled\n");
			break;
		}

		case V4L2_CID_DV_STM_TX_AVMUTE:
		{
			if (g_ctrl[i].value)
				printf("AV is mute\n");
			else
				printf("AV is unmute\n");
			break;
		}

		case V4L2_CID_DV_TX_RGB_RANGE:
		{
			switch(g_ctrl[i].value) {
			case V4L2_DV_RGB_RANGE_AUTO:
				printf("RGB range currently active is AUTO\n");
				break;

			case V4L2_DV_RGB_RANGE_LIMITED:
				printf("RGB range currently active is LIMITED\n");
				break;

			case V4L2_DV_RGB_RANGE_FULL:
				printf("RGB range currently active is FULL\n");
				break;

			default:
				printf("Unknown RGB range active \n");
			}
			break;
		}

		case V4L2_DV_STM_TX_OVERSCAN_MODE:
		{
			switch(g_ctrl[i].value) {
			case V4L2_DV_STM_SCAN_OVERSCANNED:
				printf("Scanning mode currently active is OVERSCANNED\n");
				break;
		
			case V4L2_DV_STM_SCAN_UNDERSCANNED:
				printf("Scanning mode currently active is UNDERSCANNED\n");
				break;

			case V4L2_DV_STM_SCAN_UNKNOWN:
				printf("Scanning mode currently active is UNKNOWN\n");
				break;

			default:
				printf("Unknown scan mode active\n");
			}
			break;
		}

		case V4L2_CID_DV_TX_MODE:
		{
			switch(g_ctrl[i].value) {
			case V4L2_DV_TX_MODE_DVI_D:
				printf("Tx Mode is DVI\n");
				break;

			case V4L2_DV_TX_MODE_HDMI:
				printf("Tx Mode is HDMI\n");
				break;

			default:
				printf("Tx Mode is unknown\n");
			}
			break;
		}

		case V4L2_CID_DV_STM_SPD_IDENTIFIER_DATA:
		{
			printf("SPD identifier is: %d\n", g_ctrl[i].value);
			break;
		}

		case V4L2_CID_DV_STM_SPD_VENDOR_DATA:
		{
			printf("SPD vendor info is: %s\n", g_ctrl[i].string);
			break;
		}

		case V4L2_CID_DV_STM_SPD_PRODUCT_DATA:
		{
			printf("SPD product info is: %s\n", g_ctrl[i].string);
			break;
		}

			break;

		}
	}
}

static void issue_help_message(void)
{
	printf("hdmi-control version-1.0\n");
	printf("Last modified (8 Jan 2013)\n");
	printf("Compile info:- DATE(%s), TIME(%s)\n", __DATE__, __TIME__); 
	printf("Usage: hdmi-control [OPTIONS]\n");
	printf("Set/Get the state of the connected HDMI device.\n");
	printf("\n");
	printf("Any option without arguments will return the status of the control. Look\n");
	printf("for the keywords (R/W) infront of the options:\n");
	printf("R only means READ, W only means WRITE, RW means both READ and WRITE\n");
	printf("String arguments only require the first letter and are case insensitive.\n\n");
	printf("  -a, --audio-source=[spdif|2ch_i2s|8ch_i2s|pcm|disabled] (RW)\n");
	printf("     Select the HDMI hardware audio source. pcm == 2ch_i2s for compatibility\n\n");
	printf("  --audio-channels=[2 to 8] <NOT SUPPORTED WITH V4L2>\n");
	printf("     Set HDMI audio config according CEA-861-E\n\n");
	printf("  --audio-type=[normal|1-bit|DST|2xDST|HBR] <NOT SUPPORTED WITH V4L2>\n");
	printf("     Select the audio packet type.\n\n");
	printf("  -c, --content-type=[graphics|photo|cinema|game|video] (RW)\n");
	printf("     Select the IT content type sent in the AVI info frame,\n");
	printf("     video == CE, i.e. IT bit = 0.\n\n");
	printf("  --cea-mode-select=[edid|picar|4x3|16x9] <NOT SUPPORTED WITH V4L2>\n");
	printf("     Select how the CEA mode number is chosen for SD/ED modes\n");
	printf("     using the EDID aspect ratio, the picture aspect ratio, or explicitly\n\n");
	printf("  -e<value>, --enable=<value> (RW)\n");
	printf("     0 disables HDMI output, any other value enables it\n\n");
	printf("  --edid (R)\n");
	printf("      Display EDID information from the device\n\n");
	printf("  -E, --edid-mode=[strict|nonstrict] <NOT SUPPORTED WITH V4L2>\n");
	printf("     Specify how display modes not specified in the sink EDID are\n");
	printf("     handled (default = strict).\n\n");
	printf("  -f<value>, --flush-queue=<0,1,2,3,4> (W)\n");
	printf("     Flush data queues, [1 = ACP, 2 = ISRC, 3 = VENDOR, 4 = GAMUT, 5 = NTSC-IFRAME]\n");
	printf("     Multiple queue options can be given by comma separated list\n\n");
	printf("  --force-restart <NOT SUPPORTED WITH V4L2>\n");
	printf("     Force an attempt to restart the HDMI output if connected to a display but currently stopped.\n\n");
	printf("  -F, --force-output-color=[enable|disable] <NOT AVAILABLE WITH V4L2>\n");
	printf("     Force HDMI output to a single color\n\n");
	printf("  -h, --help\n");
	printf("     Show this message\n\n");
	printf("  -m<value>, --mute (RW)\n");
	printf("     Enable/diable A/V mute; 0 disables mute, any other value enables mute\n\n");
	printf("  -M, --speaker_mapping=[0 to 0x31] <NOT AVAILABLE WITH V4L2>\n");
	printf("     Set HDMI audio config according CEA-861-E\n\n");
	printf("  -o, --hotplug-mode=[restart|disable|lazydisable] <NOT AVAILABLE WITH V4L2>\n");
	printf("     Specify how a hotplug de-assert or pulse should be\n");
	printf("     treated (default = restart)\n\n");
	printf("  -r, --pixel-repeat <NOT AVAILABLE WITH V4L2>\n");
	printf("     Set pixel repetition[1|2|4].\n\n");
	printf("  -R<value>, --rgb-range=<(A)uto, (L)imited, (F)ull> (RW)\n");
	printf("     Specify how the AVI RGB and YCC quantization bits will be set\n\n");
	printf("  -s, --scan-type=[notknown|overscan|underscan] (RW)\n");
	printf("     Select the scan type sent in the AVI info frame.\n\n");
	printf("  --spd=<Yxxxx/value> (RW)\n");
	printf("     <Vstring>, V represents the following string is vendor information\n");
	printf("     <Pstring>, P represents the following string is product information\n");
	printf("     <value>, value is the product identifier (numerical value)\n\n");
	printf("  -S, --set-forced-color=0xRRGGBB <NOT AVAILABLE WITH V4L2> \n");
	printf("     Forced HDMI output color\n\n");
	printf("  -T<val>, --tx-mode=<value> (RW)\n");
	printf("     Set the Tx mode for the device\n");
	printf("     D selects the DVI mode, H selects the HDMI mode\n\n");

	printf("\n");
}

int main(int argc, char *argv[])
{
	struct v4l2_ext_controls s_ext_ctrls;
	struct v4l2_ext_controls g_ext_ctrls;
	struct v4l2_ext_control s_ctrl[argc];
	struct v4l2_ext_control g_ctrl[argc];
	int s_count = 0, g_count = 0;
	int fd, i;
	char spd_vendor[8];
	char spd_product[16];

	memset(&s_ext_ctrls, 0, sizeof(struct v4l2_ext_controls));
	memset(&g_ext_ctrls, 0, sizeof(struct v4l2_ext_controls));
	memset(s_ctrl, 0, sizeof(s_ctrl));
	memset(g_ctrl, 0, sizeof(g_ctrl));

	/*
	 * Initializing the external controls structure for HDMI 
	 * controls. These values will remain consistent for all 
	 * the external controls. We will be sending one control
	 * in a single command.
	 */
	s_ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_DV;
	s_ext_ctrls.controls = s_ctrl;

	g_ext_ctrls.ctrl_class = V4L2_CTRL_CLASS_DV;
	g_ext_ctrls.controls = g_ctrl;


	/*
	 * Open HDMI device
	 */
	for (i = 0; ; i++) {
		sprintf(HDMIDEV, "/dev/video%d", i);
		fd = open(HDMIDEV, O_RDONLY);
		if (fd > 0) {
			struct v4l2_capability caps;
			if(ioctl(fd, VIDIOC_QUERYCAP, &caps)) {
				close(fd);
				continue;
			} else if (caps.capabilities == V4L2_CAP_VIDEO_OUTPUT) {
				close(fd);
				break;
			} else {
				close(fd);
			}
		} else {
			break;
		}
	}
	if (fd < 0) {
		printf("No output control device found\n");
		return 1;
	} else {
		fd = open(HDMIDEV, O_RDWR);
		if (fd < 0) {
			printf("Failed to open HDMI device: %s\n", HDMIDEV);
			return 1;
		}
	}

	enum {
		HDMI_AUDIO_CHANNELS,
		HDMI_AUDIO_TYPE,
		HDMI_CEA_MODE,
		HDMI_EDID_INFO,
		HDMI_FORCE_RESTART,
		HDMI_SPD_DATA,
	};

	static struct option long_options[] = {
		{ "audio-source", 2, 0, 'a' },
		{ "audio-channels", 1, 0, HDMI_AUDIO_CHANNELS },
		{ "audio-type", 1, 0, HDMI_AUDIO_TYPE },
		{ "content-type", 2, 0, 'c' },
		{ "cea-mode-select", 1, 0, HDMI_CEA_MODE },
		{ "enable", 2, 0, 'e' },
		{ "edid", 1, 0, HDMI_EDID_INFO },
		{ "edid-mode", 1, 0, 'E' },
		{ "flush-queue", 1, 0, 'f' },
		{ "force-restart", 0, 0, HDMI_FORCE_RESTART },
		{ "force-output-color", 1, 0, 'F' },
		{ "help", 0, 0, 'h' },
		{ "mute", 2, 0, 'm' },
		{ "speaker_mapping", 1, 0, 'M' },
		{ "hotplug-mode", 1, 0, 'o' },
		{ "pixel-repeat", 1, 0, 'r' },
		{ "rgb-range", 2, 0, 'R' },
		{ "scan-type", 2, 0, 's' },
		{ "spd", 2, 0, HDMI_SPD_DATA },
		{ "set-forced-color", 1, 0, 'S' },
		{ "tx-mode", 2, 0, 'T' },
		{ 0, 0, 0, 0 }
	};

	if(argc == 1) {
		issue_help_message();
		return 0;
	}

	int option;
	while ((option = getopt_long (argc, argv, "a::c::d::e::E::f:F::hm::M::o::r::R::s::S::T::", long_options, NULL)) != -1) {
		switch (option) {
		case 'a':
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_STM_TX_AUDIO_SOURCE;
				g_count++;
				break;
			}

			/*
			 * Set audio source
			 */
			if(optarg[0] == '2' || optarg[0] == 'p' || optarg[0] == 'P') {
				s_ctrl[s_count].value = V4L2_DV_STM_AUDIO_SOURCE_2CH_I2S;
			} else if(optarg[0] == '8') {
				s_ctrl[s_count].value = V4L2_DV_STM_AUDIO_SOURCE_8CH_I2S;
			} else if(optarg[0] == 's' || optarg[0] == 'S') {
				s_ctrl[s_count].value = V4L2_DV_STM_AUDIO_SOURCE_SPDIF;
			} else if(optarg[0] == 'd' || optarg[0] == 'D') {
				s_ctrl[s_count].value = V4L2_DV_STM_AUDIO_SOURCE_NONE;
			} else {
				printf("Cannot determine audio source, see --help\n");
				break;
			}
				
			s_ctrl[s_count].id = V4L2_CID_DV_STM_TX_AUDIO_SOURCE;
			s_count++;
			break;
		}

		case 'c':
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_STM_TX_CONTENT_TYPE;
				g_count++;
				break;
			}

			/*
			 * Update content type
			 */
			if(!strncmp(optarg,"gr",2)) {
				s_ctrl[s_count].value = V4L2_DV_STM_IT_GRAPHICS;
			} else if(optarg[0] == 'p' || optarg[0] == 'P') {
				s_ctrl[s_count].value = V4L2_DV_STM_IT_PHOTO;
			} else if(optarg[0] == 'c' || optarg[0] == 'C') {
				s_ctrl[s_count].value = V4L2_DV_STM_IT_CINEMA;
			} else if(!strncmp(optarg,"ga",2)) {
				s_ctrl[s_count].value = V4L2_DV_STM_IT_GAME;
			} else if(optarg[0] == 'v' || optarg[0] == 'V') {
				s_ctrl[s_count].value = V4L2_DV_STM_CE_CONTENT;
			} else {
				printf("Cannot determine content type, see --help\n");
				break;
			}

			s_ctrl[s_count].id = V4L2_CID_DV_STM_TX_CONTENT_TYPE;
			s_count++;
			break;
		}

		case 'e':
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_STM_TX_CONTROL;
				g_count++;
				break;
			}

			/*
			 * Enable/Disable HDMI-Tx
			 */
			s_ctrl[s_count].id = V4L2_CID_DV_STM_TX_CONTROL;
			s_ctrl[s_count].value = atoi(&optarg[0]);
			s_count++;
			break;

		}

		case 'E':
		{
			printf("EDID modes not supported with V4L2\n");
			break;
		}

		case 'f':
		{
			char *str = optarg;

			do {
				switch(str[0]) {
				case '1':
					s_ctrl[s_count].value |= V4L2_DV_STM_FLUSH_ACP_QUEUE;
					break;

				case '2':
					s_ctrl[s_count].value |= V4L2_DV_STM_FLUSH_ISRC_QUEUE;
					break;

				case '3':
					s_ctrl[s_count].value |= V4L2_DV_STM_FLUSH_VENDOR_QUEUE;
					break;

				case '4':
					s_ctrl[s_count].value |= V4L2_DV_STM_FLUSH_GAMUT_QUEUE;
					break;

				case '5':
					s_ctrl[s_count].value |= V4L2_DV_STM_FLUSH_NTSC_QUEUE;
					break;

				default:
					printf("Invalid queue id for flushing\n");
					break;
				}

				str = strchr(str, ',');
				if (str != NULL)
					str += 1;
				else
					break;

			} while(str);
			
			if (s_ctrl[s_count].value) {
				s_ctrl[s_count].id = V4L2_CID_DV_STM_FLUSH_DATA_PACKET_QUEUE;
				s_count++;
			} else 
				printf("Invalid flush queue ID provided, see --help\n");
			break;
		}
	
		case 'F':
		{
			printf("Force output control not available\n");
			break;
		}

		case 'h':
		{
			issue_help_message();
			break;
		}

		case 'm':
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_STM_TX_AVMUTE;
				g_count++;
				break;
			}

			/*
			 * AV mute 
			 */
			s_ctrl[s_count].id = V4L2_CID_DV_STM_TX_AVMUTE;
			s_ctrl[s_count].value = atoi(&optarg[0]);
			s_count++;
			break;
		}

		case 'M':
		{
			printf("Speaker mapping not available\n");
			break;
		}

		case 'o':
		{
			printf("Hotplug control not available\n");
			break;
		}

		case 'r':
		{
			printf("Pixel repetition not available\n");
			break;
		}

		case 'R':
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_TX_RGB_RANGE;
				g_count++;
				break;
			}

			/*
			 * Set RGB range
			 */
			if(optarg[0] == 'a' || optarg[0] == 'A') {
				s_ctrl[s_count].value = V4L2_DV_RGB_RANGE_AUTO;
			} else if(optarg[0] == 'l' || optarg[0] == 'L') {
				s_ctrl[s_count].value = V4L2_DV_RGB_RANGE_LIMITED;
			} else if(optarg[0] == 'f' || optarg[0] == 'F') {
				s_ctrl[s_count].value = V4L2_DV_RGB_RANGE_FULL;
			} else {
				printf("Cannot determine Quantization mode, see --help\n");
				break;
			}

			s_ctrl[s_count].id = V4L2_CID_DV_TX_RGB_RANGE;
			s_count++;
			break;
		}

		case 's':
		{ 
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_DV_STM_TX_OVERSCAN_MODE;
				g_count++;
				break;
			}

			/*
			 * Set scanning mode
			 */
			if(optarg[0] == 'o' || optarg[0] == 'O') {
				s_ctrl[s_count].value = V4L2_DV_STM_SCAN_OVERSCANNED;
			} else if(optarg[0] == 'u' || optarg[0] == 'U') {
				s_ctrl[s_count].value = V4L2_DV_STM_SCAN_UNDERSCANNED;
			} else if(optarg[0] == 'n' || optarg[0] == 'N') {
				s_ctrl[s_count].value = V4L2_DV_STM_SCAN_UNKNOWN;
			} else {
				printf("Cannot determine scan type, see --help\n");
				break;
			}
		
			s_ctrl[s_count].id = V4L2_DV_STM_TX_OVERSCAN_MODE;
			s_count++;
			break;
		}

		case 'S':

		{
			printf("Forced HDMI output color not available\n");
			break;
		}

		case 'T':
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_TX_MODE;
				g_count++;
				break;
			}

			if(optarg[0] == 'd' || optarg[0] == 'D') {
				s_ctrl[s_count].value = V4L2_DV_TX_MODE_DVI_D;
			} else if(optarg[0] == 'h' || optarg[0] == 'H') {
				s_ctrl[s_count].value = V4L2_DV_TX_MODE_HDMI;
			} else {
				printf("Cannot determine Tx Mode, see --help\n");
				break;
			}
			
			s_ctrl[s_count].id = V4L2_CID_DV_TX_MODE;
			s_count++;
			break;
		}

		case HDMI_AUDIO_CHANNELS:
		{
			printf("HDMI Audio config not available\n");
			break;
		}

		case HDMI_AUDIO_TYPE:
		{
			printf("Audio packet type control not available\n");
			break;
		}

		case HDMI_CEA_MODE:
		{
			printf("CEA mode control not available\n");
			break;
		}

		case HDMI_EDID_INFO:
		{
			char *str_fwd = optarg, *str_prev = optarg;
			struct v4l2_subdev_edid edid;
			unsigned char *data;
			int i;

			if(*optarg == '\0') {
				printf("Argument list cannot be empty\n");
				break;
			}

			/*
			 * Get the pad number
			 */
			str_fwd = strchr(str_fwd, ',');
			if (!str_fwd){
				printf("Invalid argument list, see --help\n");
				break;
			}
			*str_fwd = '\0';
			edid.pad = strtoul(str_prev, NULL, 0);
			if (edid.pad < 0) {
				printf("Invalid pad number passed\n");
				break;
			}

			/*
			 * Get the start block
			 */
			str_prev = str_fwd + 1;
			str_fwd += 1;
			str_fwd = strchr(str_fwd, ',');
			if (!str_fwd) {
				printf("End of input, expected blocks for EDID\n");
				break;
			}
			*str_fwd = '\0';
			edid.start_block = strtoul(str_prev, NULL, 0);
			if (edid.start_block < 0) {
				printf("Invalid start block passed\n");
				break;
			}

			/*
			 * Get the number of blocks
			 */
			str_prev = str_fwd + 1;
			if (!str_prev || (*str_prev == '\0')) {
				printf("End of input, expected blocks argument\n");
				break;
			}
			edid.blocks = strtoul(str_prev, NULL, 0);
			if (edid.blocks < 0) {
				printf("Invalid number of blocks\n");
				break;
			}

			edid.edid = (unsigned char *)malloc(128);
			if (!edid.edid) {
				printf("Out of memory for EDID data\n");
				break;
			}
		
			if (ioctl(fd, VIDIOC_SUBDEV_G_EDID, &edid)) {
				perror("Unable to get EDID info\n");
				return 1;
			}

			/*
			 * Display EDID info
			 */
			data = edid.edid;
			while(edid.blocks) {
				printf("EDID Block 1\n");
				printf("%4s %8s %8s %8s %8s\n", "Byte", "0", "1", "2", "3");
				printf("%4s %8s %8s %8s %8s\n", "----", "----", "-----", "----", "----");
				for (i = 0; i < 128; i+=4)
					printf("%4d %8d %8d %8d %8d\n", i, data[i], data[i + 1], data[i + 2], data[i + 3]);
				edid.blocks--;
				if (edid.blocks)
					data += 128;
			}

			free(edid.edid);
			break;
		}
				
		case HDMI_FORCE_RESTART:
		{
			printf("HDMI force restart not available\n");
			break;
		}
		
		case HDMI_SPD_DATA:
		{
			if (!optarg) {
				g_ctrl[g_count].id = V4L2_CID_DV_STM_SPD_IDENTIFIER_DATA;
				g_count++;
				break;
			} else if (strlen(optarg) == 1) {
				if (strncasecmp(optarg, "v", 1) == 0) {
					g_ctrl[g_count].id = V4L2_CID_DV_STM_SPD_VENDOR_DATA;
					g_ctrl[g_count].size = 8;
					g_ctrl[g_count].string = spd_vendor;
				} else if (strncasecmp(optarg, "p", 1) == 0) {
					g_ctrl[g_count].id = V4L2_CID_DV_STM_SPD_PRODUCT_DATA;
					g_ctrl[g_count].size = 16;
					g_ctrl[g_count].string = spd_product;
				}

				g_count++;
				break;
			}

			/*
			 * Set the Source Product Data (SPD) for the device
			 */
			if (strncasecmp(optarg, "v", 1) && strncasecmp(optarg, "p", 1)) {
				/*
				 * The data is product identifier
				 */
				s_ctrl[s_count].value = strtoul(optarg, NULL, 0);
				if (!s_ctrl[s_count].value || (s_ctrl[s_count].value < 0)) {
					printf("SPD data has product identifier is invalid, see --help\n");
					break;
				}
				s_ctrl[s_count].id = V4L2_CID_DV_STM_SPD_IDENTIFIER_DATA;
			} else {
				if (strncasecmp(optarg, "v", 1) == 0)
					s_ctrl[s_count].id = V4L2_CID_DV_STM_SPD_VENDOR_DATA;
				else if (strncasecmp(optarg, "p", 1) == 0)
					s_ctrl[s_count].id = V4L2_CID_DV_STM_SPD_PRODUCT_DATA;
				
				s_ctrl[s_count].string = &optarg[1];
				s_ctrl[s_count].size = sizeof(optarg) - 1;
			}

			s_count++;
			break;
		}

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

	s_ext_ctrls.count = s_count;
	if (s_ext_ctrls.count) {
		if(ioctl(fd, VIDIOC_S_EXT_CTRLS, &s_ext_ctrls)) {
			perror("Unable to set ext controls\n");
			printf("Failed to set control at index %d\n", s_ext_ctrls.error_idx);
			return 1;
		}
	}

	g_ext_ctrls.count = g_count;
	if (g_ext_ctrls.count) {
		if(ioctl(fd, VIDIOC_G_EXT_CTRLS, &g_ext_ctrls)) {
			perror("Unable to get ext controls\n");
			printf("Failed to get control at index %d\n", s_ext_ctrls.error_idx);
			return 1;
		}

		hdmi_show_controls(g_ctrl, g_count);
	}

	return 0;
}
