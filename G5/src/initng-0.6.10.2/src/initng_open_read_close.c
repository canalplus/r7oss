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

#include "initng.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "initng_global.h"
#include "initng_toolbox.h"
#include "initng_open_read_close.h"

static void bailout(int *, char **buffer);


int open_read_close(const char *filename, char **buffer)
{
	int conf_file;				/* File descriptor for config file */
	struct stat stat_buf;
	int res;					/* Result of read */

	/* Mark *buffer and conf_file as not set, for cleanup if bailing out... */

	*buffer = NULL;
	conf_file = -1;

	/* */

	conf_file = open(filename, O_RDONLY);	/* Open config file. */

	if (conf_file == -1)
	{
		D_("open_read_close(%s) error %d opening file; %s\n",
		   filename, errno, strerror(errno));

		bailout(&conf_file, buffer);
		return (FALSE);
	}

	if (fstat(conf_file, &stat_buf) == -1)
	{
		D_("open_read_close(%s) error %s getting file size; %s\n",
		   filename, errno, strerror(errno));

		bailout(&conf_file, buffer);
		return (FALSE);
	}

	/* Allocate a file buffer */

	*buffer = (char *) i_calloc((stat_buf.st_size + 1), sizeof(char));

	/* Read whole file */

	res = read(conf_file, *buffer, stat_buf.st_size);

	if (res == -1)
	{
		F_("open_read_close(%s): Error %d reading file; %s\n",
		   filename, errno, strerror(errno));

		bailout(&conf_file, buffer);
		return (FALSE);
	}

	if (res != stat_buf.st_size)
	{
		F_("open_read_close(%s): read %d instead of %d bytes\n",
		   filename, (int) res, (int) stat_buf.st_size);

		bailout(&conf_file, buffer);
		return (FALSE);
	}

	/* Normally we wouldn't care about this, but as this is init(ng)? */

	if (close(conf_file) < 0)
	{
		F_("open_read_close(%s): Error %d closing file; %s\n",
		   filename, errno, strerror(errno));

		bailout(&conf_file, buffer);
		return (FALSE);
	}

	(*buffer)[stat_buf.st_size] = '\0';		/* Null terminate *buffer */

	return (TRUE);
}

/* Avoid using go to sending a pointer to conf_file */
static void bailout(int *p_conf_file, char **buffer)
{
	/* if conf_file != -1 it is open */

	if (*p_conf_file != -1)
		(void) close(*p_conf_file);			/* Ignore result this time */

	*p_conf_file = -1;

	/* *buffer != NULL => we have called calloc, so free it */

	if (*buffer)
		free(*buffer);

	*buffer = NULL;
}
