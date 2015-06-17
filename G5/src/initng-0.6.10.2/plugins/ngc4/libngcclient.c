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
#include <ctype.h>


/* #include "ngc.h" */
#include <initng.h>
#include <initng_global.h>
#include <initng_control_command.h>
#include <initng_active_state.h>
#include <initng_toolbox.h>
#include <initng-paths.h>

#include "initng_ngc4.h"
#include "libngcclient.h"
#include "ansi_colors.h"

/* maximum size of string received, warning ngc -p can contain a lot */
#define MAX_RESCIVE_SIZE 100000

/* An error message is set to this global, if ngc fails. */
const char *ngcclient_error = NULL;

/* open socket to communicate with */
static int ngcclient_open_socket(const char *path)
{
	int len;
	struct sockaddr_un sockname;

	assert(path);

	int sock = -1;

	/* Create the socket. */
	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
	{
		ngcclient_error = "Failed to init socket";
		return (FALSE);
	}

	/* Bind a name to the socket. */
	sockname.sun_family = AF_UNIX;
	strcpy(sockname.sun_path, path);

	/* calculate length of sockname */
	len = strlen(sockname.sun_path) + sizeof(sockname.sun_family);

	if (connect(sock, (struct sockaddr *) &sockname, len) < 0)
	{
		close(sock);
		ngcclient_error = "Error connecting to initng socket";
		return (FALSE);
	}


	/* Put it not to block, waiting for more data on rscv */
	/*{
	   int cur = fcntl(sock, F_GETFL, 0);
	   fcntl(sock, F_SETFL, cur | O_NONBLOCK);
	   } */


	/* return happily */
	ngcclient_error = NULL;
	return (sock);
}


/* close pipes */
static void ngcclient_close_socket(int sock)
{
	if (sock != -1)
	{
		close(sock);
		sock = -1;
	}
}

#define ngcclient_send_short_command(c, o) ngcclient_send_command(c, NULL, o)
#define ngcclient_send_long_command(l, o)  ngcclient_send_command('\0', l, o)

/* send a command */
reply *ngcclient_send_command(const char *path, const char c, const char *l,
							  const char *o)
{
	read_header header;
	reply *rep;
	int sock = -1;

	/* clear structure just in case */
	memset(&header, 0, sizeof(read_header));
	header.p_ver = PROTOCOL_4_VERSION;

	/* allocate the rep */
	rep = calloc(1, sizeof(reply));

	/* fill the header with data */
	header.c = c;
	if (l)
		strncpy(header.l, l, 100);
	else
		header.l[0] = '\0';

	/* if there is an option string, we have to know length */
	if (o)
		header.body_len = strlen(o) * sizeof(char);

	/*print_out("Sending: %c, %s, %s\n", c, l ,o); */

	/* open the socket two way to initng */
	if ((sock = ngcclient_open_socket(path)) < 1)
	{
		/*
		 * Set in ngcclient_open_socket()
		 * ngcclient_error="Socket is not open, or failed to open!";
		 */
		return (NULL);
	}

	/* send the header */
	if (send(sock, &header, sizeof(read_header), 0) <
		(signed) sizeof(read_header))
	{
		ngcclient_error = "Could not send header!";
		return (NULL);
	}

	/* Also send the content of body (usually a string) */
	if (header.body_len &&
		send(sock, o, header.body_len, 0) < (signed) header.body_len)
	{
		ngcclient_error = "Could not send body!";
		return (NULL);
	}

	/* no idea to stress initng */
	usleep(50000);

	/*
	 * SPECIAL CASE, when using ngc -c, (RELOAD INITNG)
	 * initng starts reloading directly we wont ever get an reply
	 * so youst return happily here
	 */
	if (header.c == 'c')
	{
		ngcclient_error = NULL;
		ngcclient_close_socket(sock);

		/* fabricate an reply, and return that */
		rep->result.s = TRUE;
		rep->result.c = 'c';
		rep->result.t = STRING_COMMAND;
		strcpy(rep->result.version, "Fake reply, not from initng\n");
		rep->result.p_ver = PROTOCOL_4_VERSION;
		rep->payload = (char *) strdup("On ngc -c, initng reloads itself. By that it closes the connection to ngc and so can not return if this command succeds or not.");
		rep->result.payload = strlen(rep->payload) * sizeof(char);

		return (rep);
	}

	/* FETCH THE RESULT */

	int res = 0;

	/* read header data */
	res = TEMP_FAILURE_RETRY(recv
							 (sock, &rep->result, sizeof(result_desc), 0));
	if (res < (signed) sizeof(result_desc))
	{
		ngcclient_error = "failed to fetch the result.";
		return (NULL);
	}


	/*printf("Res: %i should be %i # errno:%i\n", res, sizeof(result_desc), errno);
	   printf("result: s: %i, c: '%c', t: %i, version: \"%.10s...\", p_ver: %i, payload: %i\n",
	   rep->result.s, rep->result.c, rep->result.t, rep->result.version,
	   rep->result.p_ver, rep->result.payload); */


	/* check that protocol matches */
	if (rep->result.p_ver != PROTOCOL_4_VERSION)
	{
		printf("protocol missmatch %i:%i\n", rep->result.p_ver,
			   PROTOCOL_4_VERSION);
		ngcclient_error = "PROTOCOL_4_VERSION missmatch!";
		free(rep);
		return (NULL);
	}

	/* ok, parse how inting thinks this request succeds */
	switch (rep->result.s)
	{
		case S_FALSE:
			ngcclient_error = "Request returns negative.";
			free(rep);
			return (NULL);

		case S_REQUIRES_OPT:
			ngcclient_error = "The command requires an option!";
			free(rep);
			return (NULL);

		case S_NOT_REQUIRES_OPT:
			ngcclient_error = "The command cant have an option!";
			free(rep);
			return (NULL);

		case S_INVALID_TYPE:
			ngcclient_error = "The data returning of this command is an unknown type.";
			free(rep);
			return (NULL);

		case S_COMMAND_NOT_FOUND:
			ngcclient_error = "Command not found.";
			free(rep);
			return (NULL);

			/* This is a good one */
		case S_TRUE:
			break;

		default:
			ngcclient_error = "Unknown error.";
			free(rep);
			return (NULL);
	}

	/* if there was a payload, download that too */
	if (rep->result.payload > 0)
	{
		ssize_t got = 0;

		/*printf("There is a payload.\n"); */

		/* i allocate 1 byte extra, to be sure a null on the end */
		rep->payload = calloc(1, rep->result.payload + 1);
		if (!rep->payload)
		{
			ngcclient_error = "Unable to allocate space for payload.";
			return (NULL);
		}

		/* sleep a bit, to make sure initng filled the buffer */
		usleep(50000);

		got = TEMP_FAILURE_RETRY(recv
								 (sock, rep->payload, rep->result.payload,
								  0));
		/* printf("got an payload of: %i bytes, suposed to be %i. #errno %i\n", got, rep->result.payload, errno); */
	}


	/* close the socket */
	ngcclient_close_socket(sock);

	return (rep);
}

/*
 * mprintf, a sprintf clone that automaticly mallocs the string
 * and new content to same string applys after that content.
 */
static int mprintf(char **p, const char *format, ...)
{
	va_list arg;				/* used for the variable lists */

	int len = 0;				/* will contain lent for current string */
	int add_len = 0;			/* This mutch more strings are we gonna alloc for */

	assert(p);

	/*printf("\n\nmprintf(%s);\n", format); */

	/* count old chars */
	if (*p)
		len = strlen(*p);

	/*
	 * format contains the minimum needed chars that
	 * are gonna be used, so we set that value and try
	 * with that.
	 */
	add_len = strlen(format) + 1;

	/*
	 * Now realloc the memoryspace containing the
	 * string so that the new apending string will
	 * have room.
	 * Also have a check that it succeds.
	 */
	/*printf("Changing size to %i\n", len + add_len); */
	if (!(*p = realloc(*p, ((len + add_len) * sizeof(char)))))
		return (-1);

	/* Ok, have a try until vsnprintf succeds */
	while (1)
	{
		/* start filling the newly allocaded area */
		va_start(arg, format);
		int done = vsnprintf((*p) + len, add_len, format, arg);

		va_end(arg);

		/* check if that was enouth */
		if (done > -1 && done < add_len)
		{
			/*printf("GOOD: done : %i, len: %i\n", done, add_len); */

			/* Ok return happily */
			return (done);
		}
		/*printf("BAD: done : %i, len: %i\n", done, add_len); */

		/* try increase it a bit. */
		add_len = (done < 0 ? add_len * 2 : done + 1);
		/*printf("Changing size to %i\n", len + add_len); */
		if (!(*p = realloc(*p, ((len + add_len) * sizeof(char)))))
			return (-1);

	}

	/* will never go here */
	return (-1);
}


/* little tool for sending command, and get a nice string return */
char *ngcclient_reply_to_string(reply * rep, int ansi)
{
	char *string = NULL;

	/*
	 * Make sure ngcclient_error is not set,
	 * this should be checkked before calling this function
	 */
	if (ngcclient_error)
	{
		string = strdup(ngcclient_error);
		return (string);
	}

	/* Make sure we got the result */
	if (!rep)
	{
		string = strdup("Unable to get reply.");
		return (string);
	}

	/* Have a look what the result is */
	switch (rep->result.t)
	{
			/* if it returned a string, print it */
		case STRING_COMMAND:
			if (!rep->payload || rep->result.payload == 0)
			{
				string = strdup("No payload.\n");
				return (string);
			}
			string = (char *) rep->payload;
			break;
			/* if it returned some data, call data handlers */
		case PAYLOAD_COMMAND:
			if (!rep->payload || rep->result.payload == 0)
			{
				string = strdup("No payload.\n");
				return (string);
			}

			/* look att the data type, first char */
			switch ((data_type) ((int *) rep->payload)[0])
			{
				case HELP_ROW:
					string = ngc_hlp(rep, ansi);
					break;
				case ACTIVE_ROW:
					string = ngc_active_db(rep, ansi);
					break;
				case OPTION_ROW:
					string = ngc_option_db(rep, ansi);
					break;
				case STATE_ROW:
					string = ngc_state_entry(rep, ansi);
					break;
				default:
					printf("UNKWNOWN PAYLOAD: %i\n",
						   (int) ((int *) rep->payload)[0]);
					break;
			}
			break;
		case INT_COMMAND:
			if (!rep->payload || rep->result.payload == 0)
			{
				string = strdup("No payload.\n");
				return (string);
			}
			{
				/* simplest create an int pointer */
				int *p = rep->payload;

				string = malloc(20);

				sprintf(string, "%i", *p);
				break;
			}
		case VOID_COMMAND:
			string = strdup("Command succeded without reply.");
			break;
		case COMMAND_FAIL:
			string = strdup("Command replied a failure.");
			break;
		case TRUE_OR_FALSE_COMMAND:
			if (!rep->payload || rep->result.payload == 0)
			{
				string = strdup("No payload.\n");
				return (string);
			}
			{
				int *p = rep->payload;

				if (*p > 0)
				{
					string = strdup("Command POSITIVE.");
					break;
				}

				string = strdup("Command NEGATIVE.");
				break;
			}
	}

	/* free */
	return (string);
}


/* This are utilities to nice out data structures */



/* get help */
char *ngc_hlp(reply * rep, int ansi)
{
	char *string = NULL;

	assert(rep);
	/* data got */
	help_row *row = rep->payload;

	/* print head */
	mprintf(&string, " ngc understand this commands:\n");
	mprintf(&string,
			" short Option                          : description\n");
	mprintf(&string,
			" ----------------------------------------------------------\n");

	while (row->dt == HELP_ROW)
	{
		char lname[256];

		/* copy name to the new static array */
		strncpy(lname, row->l, 200);

		if (ansi)
		{
			switch (row->o)
			{
				case USES_OPT:
					strcat(lname, C_FG_YELLOW " <opt>" C_OFF);
					break;
				case REQUIRES_OPT:
					strcat(lname, C_FG_CYAN " opt" C_OFF);
					break;
				default:
					strcat(lname, C_FG_CYAN C_OFF);
					break;
			}
		}
		else
		{
			switch (row->o)
			{
				case USES_OPT:
					strcat(lname, " <opt>");
					break;
				case REQUIRES_OPT:
					strcat(lname, " opt");
					break;
				default:
					break;
			}
		}

		/* to prevent an possible overflow */
		lname[255] = '\0';

		if (row->c != '\0')
		{
			if (ansi)
			{
				mprintf(&string,
						" [" C_FG_LIGHT_RED "-%c" C_OFF "] --%-40s: %s\n",
						row->c, lname, row->d);
			}
			else
			{
				mprintf(&string, " [-%c] --%-40s: %s\n",
						row->c, lname, row->d);
			}
		}
		else
		{
			mprintf(&string, "       --%-40s: %s\n", lname, row->d);
		}

		row++;

	}
	return (string);
}

char *ngc_state_entry(reply * rep, int ansi)
{
	state_row *row = rep->payload;
	char *string = NULL;

	mprintf(&string,
			" I State    State name                    Description\n");
	mprintf(&string,
			" ----------------------------------------------------------\n");

	while (row->dt == STATE_ROW)
	{
		if (ansi)
			mprintf(&string, " %i %s%-8s %-29s %s\n" C_OFF, row->is,
					is_to_ansi(row->is), is_to_string(row->is), row->name,
					row->desc);
		else
			mprintf(&string, " %i %-8s %-29s %s\n", row->is,
					is_to_string(row->is), row->name, row->desc);

		row++;
	}

	return (string);
}

char *ngc_active_db(reply * rep, int ansi)
{
	active_row *row = rep->payload;
	char *string = NULL;

	assert(rep);

	/* print head */
	if (ansi)
	{
		mprintf(&string, C_FG_LIGHT_RED " hh:mm:ss" C_OFF
				C_FG_CYAN " T " C_OFF
				"service                             : " C_FG_NEON_GREEN
				"status\n" C_OFF);
	}
	else
	{
		mprintf(&string,
				" hh:mm:ss T service                             : status\n");
	}

	/* don't make it weighter! only 80chars, not weighter. */
	mprintf(&string,
			" ----------------------------------------------------------------\n");

	while (row->dt == ACTIVE_ROW)
	{
		struct tm *ts = localtime(&row->time_set.tv_sec);

		/* don't make it weighter! only 80chars, not weighter. */
		if (ansi)
		{
			mprintf(&string, " "
					C_FG_LIGHT_RED "%.2i:%.2i:%.2i" C_OFF C_FG_CYAN " %c"
					C_OFF " %-35s : ", ts->tm_hour, ts->tm_min, ts->tm_sec,
					row->type[0] ? (char) toupper((int) row->type[0]) : ' ',
					row->name);
		}
		else
		{
			mprintf(&string, " %.2i:%.2i:%.2i %c %-35s : ",
					ts->tm_hour, ts->tm_min, ts->tm_sec,
					row->type[0] ? (char) toupper((int) row->type[0]) : ' ',
					row->name);
		}

		if (ansi)
		{
			mprintf(&string, "%s%s" C_OFF "\n", is_to_ansi(row->is),
					row->state);
		}
		else
		{
			mprintf(&string, "%s\n", row->state);
		}

		row++;
	}
	return (string);
}

const char *is_to_ansi(e_is is)
{
	switch (is)
	{
		case IS_UP:
			return (C_FG_NEON_GREEN);
		case IS_DOWN:
			return (C_FG_LIGHT_BLUE);
		case IS_FAILED:
			return (C_FG_LIGHT_RED);
		case IS_STARTING:
			return (C_FG_YELLOW);
		case IS_STOPPING:
			return (C_FG_CYAN);
		default:
			return (C_FG_MAGENTA);
	}
}

const char *is_to_string(e_is is)
{
	switch (is)
	{
		case IS_UP:
			return ("UP");
		case IS_DOWN:
			return ("DOWN");
		case IS_FAILED:
			return ("FAILED");
		case IS_STARTING:
			return ("STARTING");
		case IS_STOPPING:
			return ("STOPPING");
		case IS_WAITING:
			return ("WAITING");
		default:
			return ("UNKNOWN");
	}
}

char *ngc_option_db(reply * rep, int ansi)
{
	char *string = NULL;

	assert(rep);
	option_row *row = rep->payload;
	char ct[20];

	/* print head */
	if (ansi)
	{
		mprintf(&string,
				" " C_FG_LIGHT_RED "%-10s" C_OFF C_FG_CYAN "%-8s" C_OFF
				" %-24s %s\n", "Where", "Type", "Name", "Description");
	}
	else
	{
		mprintf(&string, " %-10s%-8s %-24s %s\n", "Where", "Type", "Name",
				"Description");
	}

	mprintf(&string,
			" ----------------------------------------------------------------\n");

	while (row->dt == OPTION_ROW)
	{
		switch (row->t)
		{
			case STRING:
				strcpy(ct, "STRING");
				break;
			case VARIABLE_STRING:
				strcpy(ct, "V_STRING");
				break;
			case STRINGS:
				strcpy(ct, "STRINGS");
				break;
			case VARIABLE_STRINGS:
				strcpy(ct, "V_STRINGS");
				break;
			case SET:
				strcpy(ct, "SET");
				break;
			case VARIABLE_SET:
				strcpy(ct, "V_SET");
				break;
			case INT:
				strcpy(ct, "INT");
				break;
			case VARIABLE_INT:
				strcpy(ct, "V_INT");
				break;
			case ALIAS:
				strcpy(ct, "ALIAS");
				break;
			case U_D_T:
				strcpy(ct, "U_D_T");
				break;
			case TIME_T:
				strcpy(ct, "TIME_T");
				break;
			case VARIABLE_TIME_T:
				strcpy(ct, "V_TIME_T");
				break;
		}

		if (ansi)
		{
			mprintf(&string,
					" " C_FG_LIGHT_RED "%-10s" C_OFF C_FG_CYAN "%-8s" C_OFF
					" %-24s %s\n", row->o, ct, row->n, row->d);
		}
		else
		{
			mprintf(&string, " %-10s%-8s %-24s %s\n", row->o, ct, row->n,
					row->d);
		}
		row++;
	}

	/* return the string */
	return (string);
}
