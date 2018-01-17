/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2005-6 Aidan Thornton <makomk@lycos.co.uk>
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

#include <stdlib.h>
#include "ngcs_common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

typedef struct ngcs_incoming_s
{
	int chan;
	int type;
	int len;
	char *data;
	struct list_head list;
} ngcs_incoming;

static void ngcs_chan_closed(ngcs_chan * chan);

#if 0
int ngcs_sendmsg(int sock, int chan, int type, int len, const char *data)
{
	int head[3];

	if (len > 0)
		assert(data);
	head[0] = chan;
	head[1] = type;
	head[2] = len;
	if (ngcs_sendall(sock, head, 3 * sizeof(int)))
		return 1;
	if (len <= 0)
		return 0;
	if (ngcs_sendall(sock, data, len))
		return 1;
	return 0;
}
#endif

int ngcs_recvmsg(int sock, int *chan, int *type, int *len, char **data)
{
	int head[3];

	assert(chan);
	assert(type);
	assert(len);
	assert(data);

	if (ngcs_recvall(sock, head, 3 * sizeof(int)))
		return 1;
	*chan = head[0];
	*type = head[1];
	*len = head[2];
	if (*len < 0)
	{
		*data = NULL;
		return 0;
	}
	*data = malloc(*len);
	if (*data == NULL)
		return 1;
	if (ngcs_recvall(sock, *data, *len))
		return 1;
	return 0;
}

int ngcs_pack(ngcs_data * data, int cnt, char *buf)
{
	int n;
	int outcnt = 0;

	for (n = 0; n < cnt; n++)
	{
		if (buf)
		{
			*(int *) buf = data[n].type;
			buf += sizeof(int);
		}
		switch (data[n].type)
		{
			case NGCS_TYPE_INT:
			case NGCS_TYPE_BOOL:
				outcnt += 3 * sizeof(int);
				if (buf)
				{
					*(int *) buf = sizeof(int);
					buf += sizeof(int);
					*(int *) buf = data[n].d.i;
					buf += sizeof(int);
				}
				break;
			case NGCS_TYPE_LONG:
				outcnt += 2 * sizeof(int) + sizeof(long);
				if (buf)
				{
					*(int *) buf = sizeof(long);
					buf += sizeof(int);
					*(int *) buf = data[n].d.l;
					buf += sizeof(long);
				}
				break;
			case NGCS_TYPE_ERROR:
			case NGCS_TYPE_STRING:
				if (data[n].len < 0)
					data[n].len = strlen(data[n].d.s);
				outcnt += 2 * sizeof(int) + data[n].len;
				if (buf)
				{
					*(int *) buf = data[n].len;
					buf += sizeof(int);
					if (data[n].len)
						memcpy(buf, data[n].d.s, data[n].len);
					buf += data[n].len;
				}
				break;
			default:
				if (data[n].len < 0)
					return 1;
				outcnt += 2 * sizeof(int) + data[n].len;
				if (buf)
				{
					*(int *) buf = data[n].len;
					buf += sizeof(int);
					if (data[n].len)
						memcpy(buf, data[n].d.p, data[n].len);
					buf += data[n].len;
				}
				break;
		}
	}
	return outcnt;
}

int ngcs_unpack_one(int type, int len, const char *data, ngcs_data * res)
{
	res->type = type;
	res->len = len;
	if (len < 0)
	{
		res->d.p = NULL;
		return 0;
	}

	switch (type)
	{
		case NGCS_TYPE_INT:
		case NGCS_TYPE_BOOL:
			if (len != sizeof(int))
				return 1;
			res->d.i = *(const int *) data;
			break;
		case NGCS_TYPE_LONG:
			if (len != sizeof(long))
				return 1;
			res->d.i = *(const long *) data;
			break;
		case NGCS_TYPE_ERROR:
		case NGCS_TYPE_STRING:
			res->d.s = malloc(len + 1);
			if (!res->d.s)
				return 1;
			memcpy(res->d.s, data, len);
			res->d.s[len] = 0;
			break;
		default:
			res->d.p = malloc(len);
			if (!res->d.p)
				return 1;
			memcpy(res->d.p, data, len);
			break;
	}
	return 0;
}


int ngcs_unpack(const char *data, int len, ngcs_data ** res)
{
	const char *d = data;
	int l = len;
	int cnt = 0;
	int rtype, rlen;
	while (l >= 2 * (int) sizeof(int))
	{
		int head[2];
		memcpy(head, d, 2 * sizeof(int));
		if (head[1] < 0)
			return -1;
		d += 2 * sizeof(int) + head[1];
		l -= 2 * sizeof(int) + head[1];
		cnt++;
	}
	d = data;
	l = len;
	*res = malloc(cnt * sizeof(ngcs_data));
	cnt = 0;
	while (l >= 2 * (int) sizeof(int))
	{
		int head[2];
		memcpy(head, d, 2 * sizeof(int));
		rtype = head[0];
		rlen = head[1];
		if (head[1] < 0)
		{
			free(*res);
			*res = NULL;
			return -1;
		}
		d += 2 * sizeof(int);
		l -= 2 * sizeof(int);
		if (l < rlen)
		{
			free(*res);
			*res = NULL;
			return -1;
		}

		if (ngcs_unpack_one(rtype, rlen, d, (*res) + cnt))
		{
			free(*res);
			*res = NULL;
			return -1;
		}

		d += rlen;
		l -= rlen;
		cnt++;
	}
	return cnt;
}

void ngcs_free_unpack_one(ngcs_data * res)
{
	switch (res->type)
	{
		case NGCS_TYPE_INT:
		case NGCS_TYPE_BOOL:
		case NGCS_TYPE_LONG:
			break;
		case NGCS_TYPE_ERROR:
		case NGCS_TYPE_STRING:
			if (res->d.s)
				free(res->d.s);
			break;
		default:
			if (res->d.p)
				free(res->d.p);
			break;
	}

}

void ngcs_free_unpack(int len, ngcs_data * res)
{
	int n;

	for (n = 0; n < len; n++)
	{
		ngcs_free_unpack_one(res + n);
	}
	free(res);
}

ngcs_conn *ngcs_conn_from_fd(int fd, void *userdata,
							 void (*close_hook) (ngcs_conn * conn),
							 void (*pollmode_hook) (ngcs_conn * conn,
													int have_pending_writes))
{
	ngcs_conn *conn;
	struct timeval tv;

	conn = malloc(sizeof(ngcs_conn));
	if (conn == NULL)
		return NULL;

	/* FIXME - should set non-blocking and use buffering */
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) ||
		setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)))
	{
		/* FIXME - shoupd print error? */
		free(conn);
		return NULL;
	}


	INIT_LIST_HEAD(&conn->chans.list);
	INIT_LIST_HEAD(&conn->inqueue);
	conn->fd = fd;
	conn->userdata = userdata;
	conn->close_hook = close_hook;
	conn->pollmode_hook = pollmode_hook;
	conn->towrite = 0;

	conn->wrbuflen = 1024;
	conn->wrbuf = malloc(conn->wrbuflen);
	if (conn->wrbuf == NULL)
	{
		free(conn);
		return NULL;
	}
	return conn;
}


static int ngcs_conn_send(ngcs_conn * conn, int chan, int type, int len,
						  const char *data)
{
	int total_len = 3 * sizeof(int) + (len < 0 ? 0 : len);
	int head[3];

	if (len > 0)
		assert(data);
	head[0] = chan;
	head[1] = type;
	head[2] = len;

	if (conn->fd < 0)
		return 1;

	/* Resize buffer if neccesary */
	if (conn->wrbuflen - conn->towrite < total_len)
	{
		/* FIXME - there are better allocation strategies */
		char *newbuf = realloc(conn->wrbuf, conn->towrite + total_len);

		if (newbuf == NULL)
			return 1;

		conn->wrbuf = newbuf;
		conn->wrbuflen = conn->towrite + total_len;
	}

	/* Copy message to write buffer */
	memcpy(conn->wrbuf + conn->towrite, head, 3 * sizeof(int));
	if (len > 0)
		memcpy(conn->wrbuf + conn->towrite + 3 * sizeof(int), data, len);
	conn->towrite += total_len;

	/* Try to send as much of the data as we can */
	ngcs_conn_write_ready(conn);

	/* FIXME - only need to call this if it's changed */
	if (conn->towrite > 0 && conn->pollmode_hook)
		conn->pollmode_hook(conn, 1);

	return (conn->fd < 0);
}

ngcs_chan *ngcs_chan_reg(ngcs_conn * conn, int chanid,
						 void (*gotdata) (ngcs_chan *, int, int, char *),
						 void (*chanclose) (ngcs_chan *),
						 void (*chanfree) (ngcs_chan *))
{
	ngcs_chan *chan = malloc(sizeof(ngcs_chan));

	if (!chan)
		return NULL;

	chan->id = chanid;
	chan->conn = conn;
	chan->userdata = NULL;
	chan->gotdata = gotdata;
	chan->onclose = chanclose;
	chan->onfree = chanfree;
	chan->close_state = NGCS_CHAN_OPEN;
	chan->is_freed = 0;
	chan->is_freeing = 0;
	chan->list.next = 0;
	chan->list.prev = 0;
	list_add(&chan->list, &conn->chans.list);
	return chan;
}

void ngcs_chan_close(ngcs_chan * chan)
{
	if (chan->close_state != NGCS_CHAN_OPEN)
		return;
	chan->close_state = NGCS_CHAN_CLOSING;	/* do this early, safer */

	if (chan->conn->fd >= 0)
	{
		ngcs_conn_send(chan->conn, chan->id, NGCS_TYPE_NULL, -1, NULL);
	}
	else
	{
		chan->close_state = NGCS_CHAN_CLOSED;
		if (chan->onclose)
			chan->onclose(chan);
	}
}

/* Called when the other side closes a channel */
static void ngcs_chan_closed(ngcs_chan * chan)
{
	assert(chan->close_state != NGCS_CHAN_CLOSED);

	chan->close_state = NGCS_CHAN_CLOSED;

	/* chan->onclose may free the channel. It should be NULL anyway if
	   chan->is_freed is true, but best to be safe */
	if (chan->is_freed)
	{
		list_del(&chan->list);
		free(chan);
		return;
	}

	if (chan->onclose)
		chan->onclose(chan);
}

void ngcs_chan_close_now(ngcs_chan * chan)
{
	void (*onclose) (ngcs_chan *);

	if (chan->close_state != NGCS_CHAN_OPEN)
		return;

	ngcs_chan_close(chan);

	chan->gotdata = NULL;
	onclose = chan->onclose;
	chan->onclose = NULL;

	if (chan->close_state == NGCS_CHAN_CLOSING && onclose)
		onclose(chan);


}

void ngcs_chan_del(ngcs_chan * chan)
{
	if (chan->is_freeing)
		return;
	chan->is_freeing = 1;
	if (!chan->is_freed)
	{
		ngcs_chan_close_now(chan);
		chan->is_freed = 1;
		if (chan->onfree)
			chan->onfree(chan);
	}

	if (chan->close_state == NGCS_CHAN_CLOSED)
	{
		list_del(&chan->list);
		free(chan);
		return;
	}
	chan->is_freeing = 1;
}

/* called to dispatch a single message in various places */
static void ngcs_conn_dispatch_one(ngcs_conn * conn, ngcs_incoming * it)
{
	int done = 0;
	ngcs_chan *chan;

	/* Dispatch to handler for this channel */
	while_ngcs_chans(chan, conn) if (chan->id == it->chan &&
									 chan->close_state != NGCS_CHAN_CLOSED)
	{
		if (it->len < 0)
			ngcs_chan_closed(chan);
		else if (chan->gotdata)
			chan->gotdata(chan, it->type, it->len, it->data);
		done = 1;
		break;
	}

	if (!done && it->len >= 0)
	{
		/* Unknown channel - close it */
		ngcs_conn_send(conn, it->chan, NGCS_TYPE_NULL, -1, NULL);
	}
}

void ngcs_conn_dispatch(ngcs_conn * conn)
{
	ngcs_incoming *it, *next;

	list_for_each_entry_prev_safe(it, next, &conn->inqueue, list)
	{
		ngcs_conn_dispatch_one(conn, it);

		if (it->len >= 0)
			free(it->data);
		list_del(&it->list);
		free(it);
	}
}

void ngcs_conn_data_ready(ngcs_conn * conn)
{
	ngcs_incoming in;

	ngcs_conn_dispatch(conn);

	/* Get message */
	if (ngcs_recvmsg(conn->fd, &in.chan, &in.type, &in.len, &in.data))
	{
		ngcs_conn_close(conn);
		return;
	}

	/* Dispatch to handler for this channel */
	ngcs_conn_dispatch_one(conn, &in);

	if (in.len >= 0)
		free(in.data);
}

void ngcs_conn_write_ready(ngcs_conn * conn)
{
	int retval;

	if (conn->fd < 0 || conn->towrite <= 0)
		return;

	retval = send(conn->fd, conn->wrbuf, conn->towrite, 0);

	if (retval < 0)
	{
		if (errno != EAGAIN && errno != EINTR)
		{
			ngcs_conn_close(conn);
		}

		return;
	}

	memmove(conn->wrbuf, conn->wrbuf + retval, conn->towrite - retval);
	conn->towrite -= retval;

	if (conn->towrite <= 0 && conn->pollmode_hook)
		conn->pollmode_hook(conn, 0);
}

int ngcs_conn_has_pending_writes(ngcs_conn * conn)
{
	return (conn->fd >= 0 && conn->towrite > 0);
}

#if 0										/* FIXME - needs complete rewrite/rethink */
int ngcs_chan_read_msg(ngcs_chan * chan, int *type, int *len, char **data)
{
	fd_set fds;
	int chanid;
	ngcs_incoming *it, *next;

	list_for_each_entry_prev_safe(it, next, &chan->conn->inqueue, list)
	{
		if (it->chan == chan->id)
		{
			*type = it->type;
			*len = it->len;
			*data = it->data;
			list_del(&it->list);
			free(it);
			return 0;
		}
	}

	if (chan->close_state == NGCS_CHAN_CLOSED)
	{
		*type = NGCS_TYPE_NULL;
		*len = -1;
		*data = NULL;
		return 0;
	}

	if (chan->conn->fd < 0)
		return 1;

	while (1)
	{
		while (1)
		{
			FD_ZERO(&fds);
			FD_SET(chan->conn->fd, &fds);
			if (select(chan->conn->fd + 1, &fds, NULL, NULL, NULL) > 0)
				break;
		}

		/* Get message */
		if (ngcs_recvmsg(chan->conn->fd, &chanid, type, len, data))
		{
			ngcs_conn_close(chan->conn);
			return 1;
		}

		if (chanid == chan->id)
		{
			/* got the message */

			if (*len < 0)
				ngcs_chan_close(chan);

			return 0;
		}

		it = malloc(sizeof(ngcs_incoming));
		if (it == NULL)
		{
			ngcs_conn_close(chan->conn);
			return 1;
		}

		it->chan = chanid;
		it->type = *type;
		it->len = *len;
		it->data = *data;
		it->list.prev = 0;
		it->list.next = 0;
		list_add(&it->list, &chan->conn->inqueue);
	}
}
#endif

/* FIXME - below is a bit iffy if we're doing polled stuff */
void ngcs_conn_close(ngcs_conn * conn)
{
	ngcs_chan *chan, *tmp;
	ngcs_incoming *it, *next;

	if (conn->fd < 0)
		return;

	close(conn->fd);
	conn->fd = -1;

	if (conn->towrite > 0 && conn->pollmode_hook)
		conn->pollmode_hook(conn, 0);

	while_ngcs_chans_safe(chan, tmp, conn)
	{
		ngcs_chan_close(chan);
	}

	list_for_each_entry_prev_safe(it, next, &conn->inqueue, list)
	{
		if (it->len >= 0)
			free(it->data);
		list_del(&it->list);
		free(it);
	}

	if (conn->close_hook)
		conn->close_hook(conn);
}

void ngcs_conn_free(ngcs_conn * conn)
{
	ngcs_chan *chan, *tmp;

	ngcs_conn_close(conn);
	while_ngcs_chans_safe(chan, tmp, conn)
	{
		list_del(&chan->list);
		free(chan);
	}
	free(conn->wrbuf);
	free(conn);
	/* TODO */
}

int ngcs_sendall(int sock, const void *buf, int len)
{
	int ret;

	while (len > 0)
	{
		ret = send(sock, buf, len, 0);
		if (ret <= 0)
		{
			return 1;
		}
		else
		{
			buf += ret;
			len -= ret;
		}
	}
	return 0;
}

int ngcs_recvall(int sock, void *buf, int len)
{
	int ret;

	while (len > 0)
	{
		ret = recv(sock, buf, len, 0);
		if (ret <= 0)
		{
			return 1;
		}
		else
		{
			buf += ret;
			len -= ret;
		}
	}
	return 0;
}

int ngcs_chan_send(ngcs_chan * chan, int type, int len, const char *data)
{
	return ngcs_conn_send(chan->conn, chan->id, type, len, data);
}
