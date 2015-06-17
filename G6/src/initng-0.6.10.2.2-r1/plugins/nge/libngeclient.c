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

const char *ngeclient_error = NULL;
char err_msg_buffer[1024];

/* open socket, specified path */
nge_connection *ngeclient_connect(const char *path)
{
	int len;
	struct sockaddr_un sockname;
	nge_connection *c;

	assert(path);

	/* allocate the connection */
	c = calloc(1, sizeof(nge_connection));
	assert(c);

	c->sock = -1;
	c->read_buffer = NULL;
	c->read_buffer_len = 0;

	/* Create the socket. */
	c->sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (c->sock < 0)
	{
		ngeclient_error = "Failed to init socket.";
		free(c);
		return (NULL);
	}

	/* Bind a name to the socket. */
	sockname.sun_family = AF_UNIX;
	strcpy(sockname.sun_path, path);
	len = strlen(path) + sizeof(sockname.sun_family);

	if (connect(c->sock, (struct sockaddr *) &sockname, len) < 0)
	{
		close(c->sock);
		ngeclient_error = "Error connecting to socket";
		free(c);
		return (NULL);
	}


	/* Put it not to block, waiting for more data on rscv */
	{
		int cur = fcntl(c->sock, F_GETFL, 0);

		fcntl(c->sock, F_SETFL, cur | O_NONBLOCK);
	}

	/* return happily */
	return (c);
}

/* close pipes */
void ngeclient_close(nge_connection * c)
{
	assert(c);

	/* if socket is open, close it */
	if (c->sock > 0)
		close(c->sock);

	/* free read_buffer */
	if (c->read_buffer)
		free(c->read_buffer);

	/* free connection info */
	free(c);
}




/*
 * read_and_fill_buffer()
 * This function fetches data on the input fd sock (globally defined)
 * it fills read_buffer, that is reallocated to fill data.
 */
static int ngeclient_read_and_fill_buffer(nge_connection * c)
{
	int totally_got = 0;
	int chars_recv = 1;			/* must be one so the while loop will be run */

	assert(c);
	assert(c->sock > 0);

	/* while we got data from recv, (or the initziating time ) */
	while (chars_recv > 0)
	{

		/*printf("reallocating to %i\n", c->read_buffer_len + 101 ); */
		/* Allocate more room for input */
		c->read_buffer = realloc(c->read_buffer,
								 (c->read_buffer_len + 101) * sizeof(char));

		/* Make it null terminated, specially if c->read_buffer is created from NULL */
		c->read_buffer[c->read_buffer_len] = '\0';

		/* get 100 chars */
		chars_recv = recv(c->sock,
						  &c->read_buffer[c->read_buffer_len], 100, 0);

		/* null the end */
		if (chars_recv > 0)
		{
			c->read_buffer_len += chars_recv;
			c->read_buffer[c->read_buffer_len] = '\0';
			totally_got += chars_recv;
		}
	}

	/* return chars got */
	return (totally_got);
}

/*
 * poll_for_input(seconds)
 * Will wait for (seconds) and if there is data incoming.
 * event->read_buffer is filled, and this will return the chars read.
 */
int ngeclient_poll_for_input(nge_connection * c, int seconds)
{
	int ret_poll;
	struct pollfd fds[1];

	assert(c);

	/* fille in the poll */
	fds[0].fd = c->sock;
	fds[0].events = POLLIN;

	/* now wait for data to apeare */
	ret_poll = poll(fds, 1, seconds);

	/* if it was any data */
	if (ret_poll > 0)
		/* go fetch it */
		return (ngeclient_read_and_fill_buffer(c));

	/* return the 0 or -1 poll generates */
	return (ret_poll);
}

/*
 * This function cuts event->read_buffer from
 * beginning and then reallocates to save memory.
 */
static void ngeclient_cut_buffert(nge_connection * c, int chars)
{
	assert(c);

	/* dont cut any then */
	if (chars == 0 || !c->read_buffer || c->read_buffer_len < 1)
		return;

	/* make sure we are not trying to cut more then we have */
	if (chars > c->read_buffer_len)
		chars = c->read_buffer_len;

	/* get length of buffer that is left, after chars is cut */
	c->read_buffer_len -= chars;

	/* move the content in front, to the beginning */
	memmove(c->read_buffer, &c->read_buffer[chars],
			c->read_buffer_len * sizeof(char));

	/* reallocate now, when theres not that mutch data anymore */
	/*printf("reallocating to %i\n", c->read_buffer_len); */
	c->read_buffer = realloc(c->read_buffer,
							 (c->read_buffer_len + 1) * sizeof(char));

	/* make sure its nulled at end */
	c->read_buffer[c->read_buffer_len] = '\0';
}


/*
 * ngeclient_get_content(tag)
 * If tag is "<specialtag>This is some data.</specialtag>"
 * This returns a new allocated strng "This is some data."
 */

static char *ngeclient_get_content(const char *tag)
{
	int len = 0;
	char *point = (char *) tag;

	assert(tag);

	/*
	 * Maks sure we stand here:
	 * "<specialtag>This is some data.</specialtag>"
	 *  |
	 */
	if (point[0] != '<')
	{
		ngeclient_error = "ngeclient_get_content failed. Bad xml.";
		return (NULL);
	}

	/* step to the end of tag */
	while (point[0] && point[0] != '>')
		point++;

	/* make sure we got there */
	if (point[0] != '>')
	{
		ngeclient_error = "ngeclient_get_content failed. Bad xml.";
		return (NULL);
	}
	point++;

	/*
	 * Maks sure we stand here:
	 * "<specialtag>This is some data.</specialtag>"
	 *              |
	 */

	while (point[len] && point[len] != '<')
		len++;

	/* make sure we are standing on start on next tag. */
	if (point[len] != '<')
	{
		ngeclient_error = "ngeclient_get_content failed. Bad xml.";
		return (NULL);
	}

	/*
	 * Maks sure we stand here:
	 * "<specialtag>This is some data.</specialtag>"
	 *              |                 ^
	 */
	if (len > 0)
		return (strndup(point, len));

	/* else return ZERO */
	return (NULL);
}

/*
 * ngeclient_get_option(tag, name);
 * Say we have a tag "<connect initng_version="initng-123">"
 * Then calling ngeclient_get_option(tag, "initng_version")
 * will return a string "initng-123"
 */
static char *ngeclient_get_option(const char *tag, const char *name)
{
	assert(tag);
	assert(name);
	char *point = (char *) tag;
	int name_len = strlen(name);

	/* search for the name in tag */
	while ((point = strstr(point, name)))
	{
		int len = 0;

		/* Standing here:
		 * "<connect initng_version="initng-123">"
		 *           |
		 */

		point += name_len;
		/* Standing here:
		 * "<connect initng_version="initng-123">"
		 *                         |
		 */

		/* skip spaces */
		while (point[0] && point[0] == ' ' && point[0] == '\t')
			point++;

		/* must be syntax */
		if (point[0] != '=')
			continue;
		point++;

		/* skip spaces */
		while (point[0] && point[0] == ' ' && point[0] == '\t')
			point++;

		/* Standing here:
		 * "<connect initng_version="initng-123">"
		 *                          |
		 */

		/* must be syntax */
		if (point[0] != '"')
			continue;
		point++;

		/* Standing here:
		 * "<connect initng_version="initng-123">"
		 *                           |
		 */

		while (point[len] && point[len] != '"')
			len++;

		/* Standing here: | To here: ^
		 * "<connect initng_version="initng-123">"
		 *                           |         ^
		 */

		/* return that string */
		return (strndup(point, len));
	}

	sprintf(err_msg_buffer, "Could not find option \"%s\" in tag", name);
	ngeclient_error = (const char *) err_msg_buffer;
	return (NULL);
}

/*
 * this is a quick hack, for getting an int, outpuf an
 * ngeclient_get_option
 */
static int ngeclient_get_int(const char *tag, const char *name)
{
	int value;
	char *string;

	assert(tag);
	assert(name);
	string = ngeclient_get_option(tag, name);
	if (!string)
		return (-1);
	value = atoi(string);
	free(string);
	return (value);
}


/* called on a <event type="service_state_change"  */
static void ngeclient_handle_service_state_change(nge_event * event,
												  char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = SERVICE_STATE_CHANGE;
	event->payload.service_state_change.service = ngeclient_get_option(tag,
																	   "service");
	event->payload.service_state_change.is = (e_is) ngeclient_get_int(tag,
																	  "is");
	event->payload.service_state_change.state_name = ngeclient_get_option(tag,
																		  "state");
	event->payload.service_state_change.percent_started = ngeclient_get_int(tag, "percent_started");
	event->payload.service_state_change.percent_stopped = ngeclient_get_int(tag, "percent_stopped");
	event->payload.service_state_change.service_type = ngeclient_get_option(tag, "service_type");
	event->payload.service_state_change.hidden = ngeclient_get_int(tag,
																   "hidden");
}

/* called on a <event type="system_state_change"  */
static void ngeclient_handle_system_state_change(nge_event * event, char *tag,
												 int chars)
{
	assert(event);
	assert(tag);
	event->state_type = SYSTEM_STATE_CHANGE;
	event->payload.system_state_change.system_state = (h_sys_state) ngeclient_get_int(tag, "system_state");
	event->payload.system_state_change.runlevel = ngeclient_get_option(tag,
																	   "runlevel");
}

/* called on a <event type="initial_service_state"  */
static void ngeclient_handle_initial_service_state(nge_event * event,
												   char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = INITIAL_SERVICE_STATE_CHANGE;
	event->payload.service_state_change.service = ngeclient_get_option(tag,
																	   "service");
	event->payload.service_state_change.is = (e_is) ngeclient_get_int(tag,
																	  "is");
	event->payload.service_state_change.state_name = ngeclient_get_option(tag,
																		  "state");

	event->payload.service_state_change.service_type = ngeclient_get_option(tag, "service_type");
	event->payload.service_state_change.hidden = ngeclient_get_int(tag,
																   "hidden");
}

/* called on a <event type="process_killed"  */
static void ngeclient_handle_process_killed(nge_event * event,
											char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = PROCESS_KILLED;
	event->payload.process_killed.service = ngeclient_get_option(tag,
																 "service");
	event->payload.process_killed.is = (e_is) ngeclient_get_int(tag, "is");
	event->payload.process_killed.state_name = ngeclient_get_option(tag,
																	"state");
	event->payload.process_killed.process = ngeclient_get_option(tag,
																 "process");
	event->payload.process_killed.exit_status = ngeclient_get_int(tag,
																  "exit_status");
	event->payload.process_killed.term_sig = ngeclient_get_int(tag,
															   "term_sig");

}

/* called on a <event type="initial_system_state"  */
static void ngeclient_handle_initial_system_state(nge_event * event,
												  char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = INITIAL_SYSTEM_STATE_CHANGE;
	event->payload.system_state_change.system_state = (h_sys_state) ngeclient_get_int(tag, "system_state");
	event->payload.system_state_change.runlevel = ngeclient_get_option(tag,
																	   "runlevel");

}

/* called on a <event type="initial_state_finished"  */
static void ngeclient_handle_initial_state_finished(nge_event * event,
													char *tag, int chars)
{
	assert(event);
	event->state_type = INITIAL_STATE_FINISHED;
}

/* called on a <event type="service_output"  */
static void ngeclient_handle_service_output(nge_event * event, char *tag,
											int chars)
{
	assert(event);
	assert(tag);
	event->state_type = SERVICE_OUTPUT;

	event->payload.service_output.service = ngeclient_get_option(tag,
																 "service");
	event->payload.service_output.process = ngeclient_get_option(tag,
																 "process");
	event->payload.service_output.output = ngeclient_get_content(tag);
}

/* called on a <event type="err_msg"  */
static void ngeclient_handle_err_msg(nge_event * event, char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = ERR_MSG;

	event->payload.err_msg.mt = (e_mt) ngeclient_get_int(tag, "mt");
	event->payload.err_msg.file = ngeclient_get_option(tag, "file");
	event->payload.err_msg.func = ngeclient_get_option(tag, "func");
	event->payload.err_msg.line = ngeclient_get_int(tag, "line");
	event->payload.err_msg.message = ngeclient_get_content(tag);
}

/* called on a <event type="ping"  */
static void ngeclient_handle_ping(nge_event * event, char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = PING;
}

/* called on a <event type= */
static void ngeclient_handle_event(nge_event * event, char *tag, int chars)
{
	char *type = ngeclient_get_option(tag, "type");

	if (strcmp(type, "service_state_change") == 0)
	{
		ngeclient_handle_service_state_change(event, tag, chars);
	}
	else if (strcmp(type, "system_state_change") == 0)
	{
		ngeclient_handle_system_state_change(event, tag, chars);
	}
	else if (strcmp(type, "service_output") == 0)
	{
		ngeclient_handle_service_output(event, tag, chars);
	}
	else if (strcmp(type, "process_killed") == 0)
	{
		ngeclient_handle_process_killed(event, tag, chars);
	}
	else if (strcmp(type, "err_msg") == 0)
	{
		ngeclient_handle_err_msg(event, tag, chars);
	}
	if (strcmp(type, "initial_service_state") == 0)
	{
		ngeclient_handle_initial_service_state(event, tag, chars);
	}
	else if (strcmp(type, "initial_system_state") == 0)
	{
		ngeclient_handle_initial_system_state(event, tag, chars);
	}
	else if (strcmp(type, "initial_state_finished") == 0)
	{
		ngeclient_handle_initial_state_finished(event, tag, chars);
	}
	else if (strcmp(type, "ping") == 0)
	{
		ngeclient_handle_ping(event, tag, chars);
	}
	else
	{
		/*
		   sprintf(err_msg_buffer, "Unknown event tag: \"%s\"", type);
		   ngeclient_error = (const char *)err_msg_buffer;
		 */
	}

	free(type);
}

/* called on a <connect protocol_version="1" initng_version="initng-123"/> */
static void ngeclient_handle_connect(nge_event * event, char *tag, int chars)
{
	assert(event);
	assert(tag);
	event->state_type = CONNECT;

	event->payload.connect.initng_version = ngeclient_get_option(tag,
																 "initng_version");
	event->payload.connect.pver = ngeclient_get_int(tag, "protocol_version");

	if (event->payload.connect.pver != NGE_VERSION)
	{
		sprintf(err_msg_buffer,
				"NGE protocol version missmatch!\n LOCAL:%i\n REMOTE: %i",
				NGE_VERSION, event->payload.connect.pver);
		ngeclient_error = (const char *) err_msg_buffer;
	}
}

/* called when a <disconnect> hook apeares */
static void ngeclient_handle_disconnect(nge_event * event, char *tag,
										int chars)
{
	assert(event);
	assert(tag);
	event->state_type = DISSCONNECT;
}

/* called for every tag nge fetches */
static void ngeclient_handle_tag(nge_event * event, char *tag, int chars)
{
	/* if this is a <? comment > tag */
	if (tag[1] == '?')
		return;

	/* if this is a "<event " tag */
	if (strncmp(&tag[1], "event ", 6) == 0)
	{
		ngeclient_handle_event(event, tag, chars);
		return;
	}

	/* if this is a "<connect " tag */
	if (strncmp(&tag[1], "connect ", 8) == 0)
	{
		ngeclient_handle_connect(event, tag, chars);
		return;
	}

	/* if this is a "<disconnect>" tag */
	if (strncmp(&tag[1], "disconnect>", 12) == 0)
	{
		ngeclient_handle_disconnect(event, tag, chars);
		return;
	}

	ngeclient_error = "Unknown tag to handle.";
	/*fprintf(stderr, "Dont know how to handle tag \"%s\", %i chars.\n", tag, chars); */
}

/*
 * get_next_event, returns an nge_event when got from initng.
 * block is the seconds that may be left.
 */
nge_event *get_next_event(nge_connection * c, int block)
{
	assert(c);

	/* quick fill for waiting input */
	ngeclient_poll_for_input(c, 0);

	/* If there was no in buffer from the qick or old polls, do the long poll. */
	if (c->read_buffer == NULL || c->read_buffer_len < 1)
	{
		ngeclient_poll_for_input(c, block);
	}

	/* check that its anything there after the long poll */
	if (c->read_buffer == NULL || c->read_buffer_len < 1)
	{
		return (NULL);
	}


	/* while there is data to read */
	while (c->read_buffer && c->read_buffer[0] != '\0'
		   && c->read_buffer_len > 0)
	{
		/* skip newlines */
		while (c->read_buffer[0] == '\n')
		{
			/* cut first char from c->read_buffer */
			ngeclient_cut_buffert(c, 1);
			continue;
		}

		/* if end of line */
		if (c->read_buffer[0] == '\0')
		{
			/* no need to save an empty string */
			free(c->read_buffer);
			c->read_buffer = NULL;

			/* rerun own function */
			return (get_next_event(c, block));
		}

		/* First char is not an tag starter */
		if (c->read_buffer[0] != '<')
		{
			printf("buffer: %s\n", c->read_buffer);
			ngeclient_error = "Expected an < char, as the first char in an xml protocol.";
			return (NULL);
		}


		/* count the lengt of this tag */
		int chars = 0;
		int tagend_found = 0;

		while (c->read_buffer[chars])
		{
			/* if found "/>" This meens tag-out */
			if (c->read_buffer[chars] == '/'
				&& c->read_buffer[chars + 1] == '>')
			{
				chars += 2;
				tagend_found = 1;
				break;
			}

			/* if found "</" This is also tag-out */
			if (c->read_buffer[chars] == '<'
				&& c->read_buffer[chars + 1] == '/')
			{
				/*printf("tag_stop: %s\n", &c->read_buffer[chars]); */
				chars += 2;

				/* skip to the end char of tag */
				while (c->read_buffer[chars] && c->read_buffer[chars] != '>')
					chars++;
				if (c->read_buffer[chars] == '>')
					chars++;

				tagend_found = 1;
				break;
			}

			/* increase chars of tags counter */
			chars++;
		}

		/* if tagend not found, return to mainloop and fetch more data */
		if (tagend_found == 0)
		{
			ngeclient_error = "Onlt got a segment of a tag.";
			return (NULL);
		}

		/* make sure there is chars in this tag */
		if (chars < 0)
		{
			ngeclient_error = "No chars in this tag.";
			return (NULL);
		}

		/* maker sure we stand on a '<' */
		if (c->read_buffer[0] != '<')
		{
			ngeclient_error = "Did not stand on a tagstarter.";
			return (NULL);
		}

		/* if standing on new char */

		{
			/* allocate space for this new event */
			nge_event *event = calloc(1, sizeof(nge_event));

			/* copy the full tag */
			char *tmp = strndup(c->read_buffer, chars);

			/* Strip chars - that we copied abow. */
			ngeclient_cut_buffert(c, chars);
			/* must be malloced */
			assert(event);
			/* This will fill event with concent from tmp */
			ngeclient_handle_tag(event, tmp, chars);
			/* free tmp */
			free(tmp);
			/* if we got anything to return */
			if (event->state_type != 0)
				return (event);
			/* else free */
			free(event);
		}

	}
	return (NULL);
}

void ngeclient_event_free(nge_event * e)
{
	switch (e->state_type)
	{
		case PING:
		case DISSCONNECT:
		case INITIAL_STATE_FINISHED:
			break;
		case SYSTEM_STATE_CHANGE:
		case INITIAL_SYSTEM_STATE_CHANGE:
			if (e->payload.system_state_change.runlevel)
				free(e->payload.system_state_change.runlevel);
			break;
		case SERVICE_STATE_CHANGE:
		case INITIAL_SERVICE_STATE_CHANGE:
			if (e->payload.service_state_change.service)
				free(e->payload.service_state_change.service);
			if (e->payload.service_state_change.state_name)
				free(e->payload.service_state_change.state_name);
			if (e->payload.service_state_change.service_type)
				free(e->payload.service_state_change.service_type);
			break;

		case ERR_MSG:
			if (e->payload.err_msg.file)
				free(e->payload.err_msg.file);
			if (e->payload.err_msg.func)
				free(e->payload.err_msg.func);
			if (e->payload.err_msg.message)
				free(e->payload.err_msg.message);
			break;

		case CONNECT:
			if (e->payload.connect.initng_version)
				free(e->payload.connect.initng_version);
			break;

		case SERVICE_OUTPUT:
			if (e->payload.service_output.service)
				free(e->payload.service_output.service);
			if (e->payload.service_output.service)
				free(e->payload.service_output.process);
			if (e->payload.service_output.service)
				free(e->payload.service_output.output);
			break;
		case PROCESS_KILLED:
			if (e->payload.process_killed.service)
				free(e->payload.process_killed.service);
			if (e->payload.process_killed.state_name)
				free(e->payload.process_killed.state_name);
			if (e->payload.process_killed.process)
				free(e->payload.process_killed.process);
			break;
	}
	free(e);
}
