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

#ifndef NGCS_COMMON_H
#define NGCS_COMMON_H
#include <initng_list.h>

/*! \brief Null type
 *
 *  Describes a lack of data. The associated data is ignored and should be empty.
 */
#define NGCS_TYPE_NULL 0

/*! \brief Native integer
 *
 *  This represents an int. The associated data is an int in native byte order.
 */
#define NGCS_TYPE_INT 1

/*! \brief Counted string
 *
 *  Represents a string (not neccesarily null-terminated). The actual data is 
 *  the contents of the string.
 */
#define NGCS_TYPE_STRING 2


/*! \brief Blob
 *
 * Just some chunk of data - no interpretation is specified.
 */
#define NGCS_TYPE_BLOB 3


/*! \brief Native long
 *
 *  The associated data is a long in native byte order.
 */
#define NGCS_TYPE_LONG 4


/*! \brief Compound data structure
 *
 *  The associated data is a data structure. It consists of a sequence of 
 *  entries, each made up of two ints (native byte order) corresponding to the
 *  data type and length of data, followed by the actual data.
 */
#define NGCS_TYPE_STRUCT 6

/*! \brief Boolean var
 *
 * Like an integer, but zero is interpreted as true and non-zero as false 
 */
#define NGCS_TYPE_BOOL 7

/*! \brief Error
 *
 * Error - a string, basically
 */
#define NGCS_TYPE_ERROR 8


typedef enum
{
	NGCS_CHAN_OPEN = 0,

	NGCS_CHAN_CLOSING = 1,

	NGCS_CHAN_CLOSED = 2
} e_ngcs_chan_state;

typedef struct ngcs_conn_s ngcs_conn;
typedef struct ngcs_chan_s ngcs_chan;

/*! \brief Data associated with a channel on a connection */
struct ngcs_chan_s
{
	/*! \brief The channel ID (uniquely identifies this channel
	   from the channels on this connection) */
	int id;

	/*! \brief The connection the channel is on */
	ngcs_conn *conn;

	/*! \brief Can be used by your handler code to store state relating to this
	   channel */
	void *userdata;

	/*! \brief A function called when data is received on the channel */
	void (*gotdata) (ngcs_chan *, int /* type */ , int /* len */ ,
					 char * /* data */ );

	/*! \brief A function called when the channel is closed (possibly at 
	   connection termination) */
	void (*onclose) (ngcs_chan *);
	/*! \brief A function called before the channel is freed */
	void (*onfree) (ngcs_chan *);

	e_ngcs_chan_state close_state;
	int is_freed;
	int is_freeing;

	struct list_head list;
};



typedef struct ngcs_data_s
{
	/*! \brief The data type of this datum (see NGCS_TYPE constants) */
	int type;

	/*! \brief The length in bytes of the data */
	int len;
	union
	{
		/*! \brief The data as an integer, if type == NGCS_TYPE_INT */
		int i;

		/*! \brief The data as a long, if type == NGCS_TYPE_LONG */
		long l;

		/*! \brief The data as a null-terminated string, if type == NGCS_TYPE_STRING */
		char *s;

		/*! \brief The raw data, in cases not specifically covered */
		void *p;
	} d;
} ngcs_data;

struct ngcs_conn_s
{
	/*! \brief The file descriptor associated with the connection */
	int fd;

	/*! \brief A list of all channels registered on this connection */
	ngcs_chan chans;

	/*! \brief Can be used by your code to store state relating to this connection */
	void *userdata;

	/*! \brief A function called when the connection is closed */
	void (*close_hook) (ngcs_conn * conn);

	void (*pollmode_hook) (ngcs_conn * conn, int have_pending_writes);

	char *wrbuf;				/* write buffer */
	int wrbuflen;				/* write buffer length */
	int towrite;				/* length of data in write buffer */

	struct list_head inqueue;
};

int ngcs_sendall(int sock, const void *buf, int len);
int ngcs_recvall(int sock, void *buf, int len);

#if 0
/*! \brief Sends a ngcs message on the specified socket
 *
 *  This function is used to send a raw ngcs message on a socket. The message
 *  is expected to conform to certain conventions, but these are not checked.
 *  (See the definitions of the NGCS_TYPE constants for details of exactly what
 *  is expected with each one). Normally, you will use one of the wrappers
 *  around this rather than using it directly.
 * 
 *  \param sock the socket on which the message is sent
 *  \param chan the channel number the message is marked with
 *  \param type the data type of the message (typically one of the NGCS_TYPE
 *         constants)
 *  \param len the length of the message body (in bytes)
 *  \param data the message body
 *  \return Zero on success, non-zero on failure
 *  \sa ngcs_recvmsg(), ngcs_pack(), ngcs_chan_send()
 */
int ngcs_sendmsg(int sock, int chan, int type, int len, const char *data);
#endif

/*! \brief Receives a raw ngcs message from the specified socket
 *
 * This function reads one raw ngcs message from the specified socket,
 * allocating the neccesary memory to hold its contents (which must be 
 * free()d by the caller). Normally, you won't use this.
 *
 * \param sock the socket from which the message should be read
 * \param chan a pointer to a location where the channel number of the message
 *        should be stored
 * \param type a pointer to a location where the data type of the message
 *        should be stored
 * \param len a pointer to a location where the length of the message body (in bytes)
 *        should be stored. On return, *len may be less than zero, in which case 
 *        *data is set to NULL
 * \param data a location where a pointer to the message body is stored. *data is
 *        valid on return if ngcs_recvmsg() succeeded with *len>=0, and in this case
 *        must be free()d by the caller after use.
 * \return Zero on success, non-zero on failure
 */
int ngcs_recvmsg(int sock, int *chan, int *type, int *len, char **data);


/*! \brief Pack an ngcs stucture for transmission
 * 
 * This function packs a sequence of items into a buffer, which can be transmitted
 * down a socket or otherwise transferred, then unpacked by ngcs_unpack()
 * The resulting packed data strcuture should be transmitted marked as type
 * NGCS_TYPE_STRUCT.
 *
 * Since the caller must ensure that the buffer is large enough, your code should
 * call ngcs_pack with buf=NULL to get the required size, then allocate a buffer
 * and call it again with the same data an the newly-allocated buffer
 *
 * \param data an array of structures describing the items of data to pack
 * \param cnt the number of items to pack (the length of data[])
 * \param buf the buffer into which to pack the data. May be null, in which case
 *            the required buffer size is returned
 *
 * \return the total packed length of the data, in bytes
 */
int ngcs_pack(ngcs_data * data, int cnt, char *buf);

int ngcs_unpack_one(int type, int len, const char *data, ngcs_data * res);

/*! \brief Unpack a received structure
 *
 *  This is the reverse of ngcs_pack() - given packed data representing a
 *  structure, it creates an array of ngcs_data structures representing the
 *  contents of the structure. If you receive data marked as type 
 *  NGCS_TYPE_STRUCT, you should be able to unpack it using this function.
 *
 *  Note that this function allocates memory, which you must call
 *  ngcs_free_unpack(*res) to free once you're done. However, you must *not*
 *  do so if ngcs_unpack() fails (ie. if return value is < 0).
 *
 *  \param data the packed data representing the structure
 *  \param len the length of data, in bytes
 *  \param res a location where a pointer to an array of ngcs_data structures,
 *             representing the result of unpacking the data, is returned.
 *  \return the number of items unpacked, or -1 on error
 */
int ngcs_unpack(const char *data, int len, ngcs_data ** res);

void ngcs_free_unpack_one(ngcs_data * res);

/*! \brief Frees the memory allocated by ngcs_unpack()
 *  
 *  Once you're done with the results of ngcs_unpack(), you should
 *  call this method to free the memory allocated by it.
 *
 * \param len the number of data items unpacked (that is, the return value of ngcs_unpack())
 * \param res the array of ngcs_data structures returned by ngcs_unpack in its *res parameter
 */
void ngcs_free_unpack(int len, ngcs_data * res);

/*! \brief Use a file descriptor as a ngcs connection
 *
 * Sets up the specified file descriptor to be used as an ngcs connection,
 * and sets up a connection using it.
 *
 * \param fd the file descriptor to use (should be a socket)
 * \param userdata a pointer that will be stored in the ngcs_conn structure
 *           describing the connection. The code using the ngcs library can
 *           use this to store its own connection-related state information
 * \param close_hook a callback called when the connection is closed for any
 *        reason. May be called from within any call to ngcs_chan_send, so be
 *        careful what you put in it.
 */
ngcs_conn *ngcs_conn_from_fd(int fd, void *userdata,
							 void (*close_hook) (ngcs_conn * conn),
							 void (*pollmode_hook) (ngcs_conn * conn,
													int have_pending_writes));

/*! \brief Registers a channel handler on the specified connection
 * 
 * Registers a channel handler for the specified channel number on the
 * connection. If more than one channel handler is registered on the same
 * connection for th same channel number, the results are undefined.
 *
 * \param conn the connection to register a channel handler on
 * \param chanid the channel number the handler is for
 * \param gotdata  A function called when data is received on the channel.
 *                 May be NULL, in which case received data is ignored 
 * \param chanclose A function called when the channel is closed.
 *                  May call ngcs_chan_free() on the channel, but may not send messages on it.
 *                  If you want to free the channel as soon as it closes, you may set this to 
 *                  ngcs_chan_free.
 * \param chanfree A function called when the channel is freed. Should clean
 *                 up any associated data. May be NULL if you don't care
 * \return the ngcs_chan structure associated with the new channel (don't 
 *         free it), or NULL on failure
 */
ngcs_chan *ngcs_chan_reg(ngcs_conn * conn, int chanid,
						 void (*gotdata) (ngcs_chan *, int, int, char *),
						 void (*chanclose) (ngcs_chan *),
						 void (*chanfree) (ngcs_chan *));

/*! \brief Closes a channel
 *
 *  Closes the specified channel, sending a message to signal this to the other end,
 *  but does not free it. Calls the chanfree function registered for the channel */
void ngcs_chan_close(ngcs_chan * chan);

void ngcs_chan_close_now(ngcs_chan * chan);

/*! \brief Closes a channel and frees associated data structures
 *
 * This function closes the channel with ngcs_chan_close, then calls the registered
 * chanfree function and frees all associated data structures (including the ngcs_chan
 * structure itself).
 */
void ngcs_chan_del(ngcs_chan * chan);

/*! \brief Handles incoming data
 *
 * Reads a message from the connection and dispatches it to the handler.
 * Should be called when select() indicates data is waiting to be read.
 */
void ngcs_conn_data_ready(ngcs_conn * conn);

void ngcs_conn_write_ready(ngcs_conn * conn);

int ngcs_conn_has_pending_writes(ngcs_conn * conn);

/*! \brief Closes a connection
 *
 * Closes a connection, but does not free any associated data structures. 
 * The close_hook for the connection is called, if there is one,
 * as are any ngcs_chan.free hooks associated with channels on it.
 *
 * \sa ngcs_conn_free()
 */
void ngcs_conn_close(ngcs_conn * conn);


/*! \brief Closes and frees a connection
 *
 * Closes a connection by calling ngcs_conn_close() and then frees all the
 * associated data structures (including the ngcs_conn structure itself)
 *
 * \sa ngcs_conn_close()
 */
void ngcs_conn_free(ngcs_conn * conn);


/*! \brief Send a message on the specified channel
 *
 * This command sends a raw ngcs message on the specified channel. It's a 
 * wrapper around ngcs_sendmsg() which maintains internal state.
 *
 * \param chan the channel on which to send the data
 * \param type the data type of the message
 * \param len  the length (in bytes) of the data to send
 * \param data the actual data to send 
 * \return Zero on success, non-zero on failure
 * \sa ngcs_sendmsg()
 */
int ngcs_chan_send(ngcs_chan * chan, int type, int len, const char *data);


int ngcs_chan_read_msg(ngcs_chan * chan, int *type, int *len, char **data);

void ngcs_conn_dispatch(ngcs_conn * conn);

#define while_ngcs_chans(current, conn) list_for_each_entry_prev(current, &(conn)->chans.list, list)
#define while_ngcs_chans_safe(current, safe, conn) list_for_each_entry_prev_safe(current, safe, &(conn)->chans.list, list)

#endif
