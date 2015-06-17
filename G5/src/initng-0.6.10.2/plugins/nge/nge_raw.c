/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <initng.h>
#include <initng-paths.h>

#include "libngeclient.h"
int main(int argc, char *argv[]);


/* THIS IS MAIN */
int main(int argc, char *argv[])
{
	nge_connection *c = NULL;
	int got;

	/* open correct socket */
	if (strstr(argv[0], "ngde"))
		c = ngeclient_connect(NGE_TEST);
	else
		c = ngeclient_connect(NGE_REAL);

	/* if open_socket errors, ngeclient_error is set */
	if (ngeclient_error)
	{
		fprintf(stderr, "%s\n", ngeclient_error);
		exit(1);
	}

	if (!c)
	{
		fprintf(stderr, "No connection");
		exit(1);
	}



	/* poll_for_input will return -1 if socket closes */
	while ((got = ngeclient_poll_for_input(c, 10000) >= 0))
	{
		if (ngeclient_error)
		{
			fprintf(stderr, "%s\n", ngeclient_error);
			ngeclient_error = NULL;
		}

		/* if we got something */
		if (got > 0 && c->read_buffer && c->read_buffer[0])
		{
			fprintf(stdout, "%s", c->read_buffer);
		}

		free(c->read_buffer);
		c->read_buffer = 0;
	}

	/* check for error messages from ngeclinet.so */
	if (ngeclient_error)
	{
		fprintf(stderr, "%s\n", ngeclient_error);
		ngeclient_error = NULL;
	}


	/* clean up */
	ngeclient_close(c);
	exit(0);
}
