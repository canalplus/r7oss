/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV utilities

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV utilities may alternatively be licensed under a proprietary
license from ST.

Source file name : v4lEncodeTestApp.c

V4L2 audio / video encoder test application main file

************************************************************************/

#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <signal.h>
#include "video.h"
#include "audio.h"

#define V4LENCODETESTAPP_VERSION "2.0"
/*	Revision history
 *	1.2 AC3 AAC Audio Codec
 *	1.3 MP3 Audio Codec
 *	1.4 Audio encode Tunneling Mode
 *		Change input command interface
 *		Video encode Tunneling Mode
 *	1.5 Fix Command line Argument interface
 *		support CRC Validation
 *	1.6 Update Channel_Map
 *		Add exit mode (q or x)
 *	1.7 HVA dynamic parameters updates
 *		Put HVAEnc_SequenceParams_s in struct drv_context_s
 *	1.8 Source code cleanup
 *	1.9 v4l2-ctrl framework porting
 *	2.0 STKPI memory profile support for memory optimization
 */

#define SLEEP_WAIT_KEYBOARD 10000
char kbd[200];

void usage(int argc, char *argv[])
{
	encode_print
	    ("Usage: %s [-t] [-c <param>] [-i <input>]"\
			" [-o <filename>] [-n <index>]\n", argv[0]);
	encode_print("\n");
	encode_print("-c defines the parameter (configuration) file\n");
	encode_print("<param> is the param filename\n");
	encode_print("\n");
	encode_print("-i defines the raw input Audio or Video file\n");
	encode_print("<input> is the input filename\n");
	encode_print("\n");
	encode_print("-t enables Tunneling encoding mode\n");
	encode_print("\n");
	encode_print("-o defines the encoded output file\n");
	encode_print("<filename> is the output filename\n");
	encode_print("\n");
	encode_print
	    ("-v Requests verification of output Frames by CRC check\n");
	encode_print("-l <n> Loops input <n> times\n");
	encode_print("\n");
	encode_print
	    ("-n defines the adapter index to use for parallel encode\n");
	encode_print("<index> is the index to encode on\n");
	encode_print("\n");
	encode_print("ex: %s -c -i -o\n", argv[0]);
	encode_print("\n");
	encode_print("Whilst the application is running:\n");
	encode_print("q or x: To quit/exit\n");
	encode_print("\n");
	exit(EXIT_FAILURE);
}

void *scan_keyboard(void *ContextPt)
{
	drv_context_t *Context = (drv_context_t *) ContextPt;

	while (Context->global_exit == 0) {
		usleep(SLEEP_WAIT_KEYBOARD);
		fgets(kbd, 200, stdin);
		if ((strcmp(kbd, "q\n") == 0) || (strcmp(kbd, "x\n") == 0)) {
			encode_print("Exit\n");
			fflush(stdout);
			Context->global_exit = 1;
		}
	}
	pthread_exit(NULL);
	return NULL;
}

int main(int argc, char *argv[])
{
	char *param_file_name = NULL;
	char *input_file_name = NULL;
	char *output_file_name = NULL;
	int opt;

	drv_context_t Context;

	pthread_t thread_keyboard;

	memset(&Context, 0, sizeof(Context));

	encode_print("V4lEncodeTestApp : Version %s\n",
		     V4LENCODETESTAPP_VERSION);

	if (argc < 4) {
		usage(argc, argv);
		return 0;
	}

	/* Default to use the */
	Context.io_index = 0;

	while ((opt = getopt(argc, argv, "htc:i:o:n:vl:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argc, argv);
			return 0;
		case 't':
			Context.tunneling_mode = 1;
			break;
		case 'c':
			param_file_name = optarg;
			break;
		case 'i':
			Context.input_file_name = input_file_name = optarg;
			break;
		case 'o':
			Context.output_file_name = output_file_name = optarg;
			break;
		case 'n':
			Context.io_index = atoi(optarg);
			break;
		case 'v':
			Context.validation++;
			break;
		case 'l':
			Context.loop = atoi(optarg);
			break;
		case '?':
		default:
			usage(argc, argv);
			break;
		}
	}

	if (Context.tunneling_mode == 0) {
		Context.InputFp = fopen(input_file_name, "rb");
		if (Context.InputFp == NULL) {
			encode_error("can't open YUV or PCMfile (%s)\n",
				     input_file_name);
			return 1;
		}
	}
	Context.OutputFp = fopen(output_file_name, "wb");
	if (Context.OutputFp == NULL) {
		encode_error("can't open output file\n");
		return 1;
	}

	encode_print("Starting keyboard thread:\n");
	pthread_create(&thread_keyboard, NULL, scan_keyboard, &Context);

	if (parse_audio_ini(&Context, param_file_name)) {
		encode_error("Error in handling config file\n");
		return 1;
	}

	if (Context.Params.audio_init == 1) {
		encode_audio(&Context);
		encode_print("\nFinish\n");
	}
	/* check if Video config file */
	else {
		if (parse_video_ini(&Context, param_file_name)) {
			encode_error("Error in handling config file\n");
			return 1;
		}

		encode_video(&Context);
		encode_print("Finish\n");
	}

	fclose(Context.OutputFp);
	pthread_kill(thread_keyboard, SIGKILL);
	return 0;
}
