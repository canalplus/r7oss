/*
 * hdmi-info.c
 *
 * Copyright (C) STMicroelectronics Limited 2006. All rights reserved.
 *
 * Demonstration program showing how to use the sysfs attributes
 * exported by the framebuffer.
 */

#include <sys/types.h> // undefines __USE_POSIX
#include <sys/poll.h>
#include <sys/stat.h>

#define __USE_POSIX
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>

#define lengthof(a) (sizeof(a) / sizeof((a)[0]))

#define DISPLAY "/sys/class/stmcoredisplay/display%d/"
#define HDMI "hdmi%d.%d/"

struct videomode {
	char standard;
	int x;
	int y;
	char scan;
	int refresh;
};

enum {
	ATTR_HDMI_CEA861,
	ATTR_HDMI_NAME,
	ATTR_HDMI_DISPLAY,
	ATTR_HDMI_HOTPLUG,
	ATTR_HDMI_TMDS_STATUS,
	ATTR_HDMI_MODES,
	ATTR_MAX
};

const char *attributes[ATTR_MAX] = {
	[ATTR_HDMI_CEA861]  = DISPLAY HDMI "cea861_codes",
	[ATTR_HDMI_NAME]    = DISPLAY HDMI "name",
	[ATTR_HDMI_DISPLAY] = DISPLAY HDMI "type",
	[ATTR_HDMI_HOTPLUG] = DISPLAY HDMI "hotplug",
	[ATTR_HDMI_TMDS_STATUS] = DISPLAY HDMI "tmds_status",
	[ATTR_HDMI_MODES]   = DISPLAY HDMI "modes"
};

static int device_number = 0;
static int subdevice_number = 0;

char *get_attribute(int attr)
{
	char attrname[256];

	snprintf(attrname, 256, attributes[attr], device_number, device_number, subdevice_number);

	int fd = open(attrname, O_RDONLY);
	if (fd < 0)
		return NULL;

	long page_size = sysconf(_SC_PAGESIZE);

	char *p = malloc(page_size + 1);
	if (!p) {
		return NULL;
	}

	ssize_t sz = read(fd, p, page_size);
	close(fd);
	if (sz < 0 || sz > page_size) {
		free(p);
		return NULL;
	}

	// ensure string functions can safely operate on the attribute
	p[sz] = '\0';

	return p;
}

int get_cea861_modes(int *modes, int len)
{
	char *attr = get_attribute(ATTR_HDMI_CEA861);
	if (!attr)
		return -1;

	char *mode, *r;
	int num_modes = 0;
	for (mode = strtok_r(attr, "\n", &r); mode; mode = strtok_r(NULL, "\n", &r)) {
		if (num_modes >= len) {
			free(attr);
			return -1;
		}

		modes[num_modes++] = atoi(mode);
	}

	free(attr);
	return num_modes;
}

int get_display_name(char *name, int len)
{
	char *attr = get_attribute(ATTR_HDMI_NAME);
	if (!attr)
		return -1;

	int res = -1;
	int attr_len = strlen(attr);
	if (attr_len <= len && attr[attr_len - 1] == '\n') {
		// copy but swallow the trailing '\n'
		memcpy(name, attr, attr_len - 1);
		name[attr_len - 1] = '\0';

		res = 0;
	}

	free(attr);
	return res;
}

int get_display_type(char *type, int len)
{
	char *attr = get_attribute(ATTR_HDMI_DISPLAY);
	if (!attr)
		return -1;

	int res = -1;
	int attr_len = strlen(attr);
	if (attr_len <= len && attr[attr_len - 1] == '\n') {
		// copy but swallow the trailing '\n'
		memcpy(type, attr, attr_len - 1);
		type[attr_len - 1] = '\0';

		res = 0;
	}

	free(attr);
	return res;
}

int get_hotplug(void)
{
	char *attr = get_attribute(ATTR_HDMI_HOTPLUG);
	if (!attr)
		return -1;

	int res = -1;
	if (0 == strcmp(attr, "y\n")) {
		res = 1;
	} else if (0 == strcmp(attr, "n\n")) {
		res = 0;
	}

	free(attr);
	return res;
}

int get_modes(struct videomode *modes, int len)
{
	char *attr = get_attribute(ATTR_HDMI_MODES);
	if (!attr)
		return -1;

	char *mode, *r;
	int num_modes = 0;
	for (mode = strtok_r(attr, "\n", &r); mode; mode = strtok_r(NULL, "\n", &r)) {
		if (num_modes >= len) {
			free(attr);
			return -1;
		}

		if (5 != sscanf(mode, "%c:%dx%d%c-%d",
		                &modes[num_modes].standard, &modes[num_modes].x, &modes[num_modes].y,
		                &modes[num_modes].scan, &modes[num_modes].refresh)) {
			free(attr);
			return -1;
		}

		num_modes++;
	}

	free(attr);
	return num_modes;
}

bool wait_for_hotplug(bool connected, bool quiet)
{
	struct pollfd pollfd;
	char attrname[256];

	snprintf(attrname, 256, attributes[ATTR_HDMI_HOTPLUG],  device_number, device_number, subdevice_number);

	pollfd.fd = open(attrname, O_RDONLY);
	if (pollfd.fd < 0) {
		fprintf(stderr, "ERROR: Unable to wait for HDMI hotplug\n");
		return false;
	}

	pollfd.events = POLLPRI | POLLERR;
	pollfd.revents = 0;


	if (!quiet) {
		printf("Waiting for HDMI %s ... ", connected ? "connection" : "disconnection");
		fflush(stdout);
	}

	char attr[4];
	ssize_t sz = read(pollfd.fd, attr, sizeof(attr));
	assert(sz < sizeof(attr));

	int res;
	if (sz < 0 || attr[1] != '\n')
		res = -1;
	else if ((attr[0] == 'y') == connected)
		res = 1;
	else
		res = poll(&pollfd, 1, -1);

	close(pollfd.fd);

	if (!quiet)
		printf("%s\n", res > 0 ? "OK" : "FAILED");
	if (res < 0)
		fprintf(stderr, "ERROR: Cannot wait for HDMI hotplug\n");

	return 1 == res;
}

bool show_cea861_modes(void)
{
	int modes[64];
	int res = get_cea861_modes(modes, lengthof(modes));
	if (res < 0) {
		fprintf(stderr, "ERROR: Cannot determine available CEA-861 display modes\n");
		return false;
	}

	printf("Available CEA-861 display modes: ");
	for (int i=0; i<res; i++)
		printf("%d%s", modes[i], i<(res-1) ? ", " : "\n");

	return true;
}

bool show_display_name(void)
{
	char name[32];
	int res = get_display_name(name, sizeof(name));
	if (res < 0) {
		fprintf(stderr, "ERROR: Cannot determine display name\n");
		return false;
	}

	printf("Display name: %s\n", name);
	return true;
}

bool show_display_type(void)
{
	char type[32];
	int res = get_display_type(type, sizeof(type));
	if (res < 0) {
		fprintf(stderr, "ERROR: Cannot determine display type\n");
		return false;
	}


	if (0 == strcmp("INVALID", type)) {
		printf("Display device is not responding (turned off?)\n");
		return false;
	}

	printf("Display device uses %s connector\n", type);
	return true;
}

bool show_hotplug(void)
{
	int plugged_in = get_hotplug();
	if (plugged_in < 0) {
		fprintf(stderr, "ERROR: Cannot determine connection status of display device\n");
		return false;
	}

	if (plugged_in) {
		printf("Display device is connected\n");
	} else {
		printf("Display device is NOT connected\n");
	}

	return plugged_in;
}


bool show_tmds_status(void)
{
	char *s = get_attribute(ATTR_HDMI_TMDS_STATUS);
	if(!s) {
		return false;
	}
	printf("TMDS Status: %s",s);
	return true;
}


bool show_modes(void)
{
	struct videomode modes[64];
	int res = get_modes(modes, lengthof(modes));
	if (res < 0) {
		fprintf(stderr, "ERROR: Cannot determine available display modes\n");
		return false;
	}

	printf("Standard modes: ");
	for (int i=0, j=0; i<res; i++) {
		if (modes[i].standard == 'S') {
			if (j != 0)
				printf(0 == j%4 ? "\n                " : ", ");
			j++;
			printf("%dx%d%c@%d", modes[i].x, modes[i].y, modes[i].scan, modes[i].refresh);
		}
	}
	printf("\n");

	printf("Other modes:    ");
	for (int i=0, j=0; i<res; i++) {
		if (modes[i].standard != 'S')  {
			if (j != 0)
				printf(0 == j%4 ? "\n                " : ", ");
			j++;
			printf("%dx%d%c@%d", modes[i].x, modes[i].y, modes[i].scan, modes[i].refresh);
		}
	}
	printf("\n");

	return true;
}

static void issue_help_message(void)
{
    printf("Usage: hdmi-info [OPTIONS]\n");
    printf("Query the state of the connected HDMI device.\n");
    printf("\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");
    printf("  -c, --cea861-modelist    Show the available CEA-861 display modes.\n");
    printf("  -C, --no-cea861-modelist Don't show the available CEA-861 display modes.\n");
    printf("  -d, --display=NUM        Specify the display device to use (default = 0).\n");
    printf("  -h, --help               Display this help, then exit.\n");
    printf("  -l, --tmds-link-status   Show the TMDS link status.\n");
    printf("  -L, --no-tmds-link-status\n"
           "                           Don't show the TMDS link status.\n");
    printf("  -m, --modelist           Show the available display modes.\n");
    printf("  -M, --no-modelist        Don't show the available display modes.\n");
    printf("  -n, --display-name       Show the display name.\n");
    printf("  -N, --no-display-name    Don't show the display name.\n");
    printf("  -q, --quiet              Don't show anything.\n");
    printf("  -s, --connection-status  Show the connection status.\n");
    printf("  -S, --no-connection-status\n"
           "                           Don't show the connection status.\n");
    printf("  -t, --display-type       Show the display type.\n");
    printf("  -T, --no-display-type    Don't show the display type.\n");
    printf("  -v, --verbose            Show all available information.\n");
    printf("  -w, --wait-for-connected=BOOL\n"
           "                           Block until HDMI device is connected or unplugged.\n"
	   "                           Choose from: plugged, unplugged, true, false.\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
	bool show[ATTR_MAX] = {
		[ATTR_HDMI_HOTPLUG] = true
	};
	bool wait_on_entry = false;
	bool wait_for_plug_in;
	bool keep_going = true;
	bool quiet = false;

	static struct option long_options[] = {
		{ "cea861-modelist", 0, 0, 'c' },
		{ "no-cea861-modelist", 0, 0, 'C' },
		{ "display", 1, 0, 'd' },
		{ "tmds-link-status", 0, 0, 'l' },
		{ "no-tmds-link-status", 0, 0, 'L' },
		{ "connection-status", 0, 0, 's' },
		{ "no-connection-status", 0, 0, 'S' },
		{ "display-type", 0, 0, 't' },
		{ "no-display-type", 0, 0, 'T' },
		{ "help", 0, 0, 'h' },
		{ "modelist", 0, 0, 'm' },
		{ "no-modelist", 0, 0, 'M' },
		{ "display-name", 0, 0, 'n' },
		{ "no-display-name", 0, 0, 'N' },
		{ "quiet", 0, 0, 'q' },
		{ "verbose", 0, 0, 'v' },
		{ "wait-for-connected", 1, 0, 'w' },
		{ 0, 0, 0, 0 }
	};

	int option;
	while ((option = getopt_long (argc, argv, "cCd:lLsStThmMnNqvw:", long_options, NULL)) != -1) {
		switch (option) {
		case 'c':
			show[ATTR_HDMI_CEA861] = true;
			break;
		case 'C':
			show[ATTR_HDMI_CEA861] = false;
			break;
		case 'd':
			device_number = atoi(optarg);
			break;
		case 'l':
			show[ATTR_HDMI_TMDS_STATUS] = true;
			break;
		case 'L':
			show[ATTR_HDMI_TMDS_STATUS] = false;
			break;
		case 's':
			show[ATTR_HDMI_HOTPLUG] = true;
			break;
		case 'S':
			show[ATTR_HDMI_HOTPLUG] = false;
			break;
		case 't':
			show[ATTR_HDMI_DISPLAY] = true;
			break;
		case 'T':
			show[ATTR_HDMI_DISPLAY] = false;
			break;
		case 'h':
			issue_help_message();
			exit(0);
		case 'm':
			show[ATTR_HDMI_MODES] = true;
			break;
		case 'M':
			show[ATTR_HDMI_MODES] = false;
			break;
		case 'n':
			show[ATTR_HDMI_NAME] = true;
			break;
		case 'N':
			show[ATTR_HDMI_NAME] = false;
			break;
		case 'q':
			for (int i=0; i<ATTR_MAX; i++)
				show[i] = false;
			quiet = true;
			break;
		case 'v':
			for (int i=0; i<ATTR_MAX; i++)
				show[i] = true;
			break;

		case 'w':
			wait_on_entry = true;
			wait_for_plug_in = 0 != strcmp("unplugged", optarg) &&
			                   0 != strcmp("false", optarg) &&
				 	   0 != strcmp("0", optarg);
			break;
		default:
			printf("Try `hdmi-info --help' for more information.\n");
			exit(1);
		}
	}

	if (optind != argc) {
		printf ("hdmi-info: too many arguments\n");
		printf ("Try `hdmi-info --help' for more information.\n");
		exit(2);
	}

	if (wait_on_entry)
		keep_going = wait_for_hotplug(wait_for_plug_in, quiet);

	if (keep_going && show[ATTR_HDMI_HOTPLUG])
		keep_going = show_hotplug();
	if (keep_going && show[ATTR_HDMI_TMDS_STATUS])
		keep_going = show_tmds_status();
	if (keep_going && show[ATTR_HDMI_DISPLAY])
		keep_going = show_display_type();
	if (keep_going && show[ATTR_HDMI_NAME])
		keep_going = show_display_name();
	if (keep_going && show[ATTR_HDMI_CEA861])
		keep_going = show_cea861_modes();
	if (keep_going && show[ATTR_HDMI_MODES])
		keep_going = show_modes();

	return keep_going ? 0 : 42;
}
