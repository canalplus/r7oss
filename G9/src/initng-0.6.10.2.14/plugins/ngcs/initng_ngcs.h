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

#ifndef INITNG_NGCS_H
#define INITNG_NGCS_H

#include <initng_active_db.h>
#include <initng_system_states.h>
#include <initng_list.h>
#include "ngcs_common.h"
#include "ngcs_paths.h"

#include <stddef.h>

typedef struct ngcs_cmd_s ngcs_cmd;
typedef struct ngcs_request_s ngcs_request;
typedef struct ngcs_svr_conn_s ngcs_svr_conn;

/* int module_init(const char *version);
   void module_unload(void); */

int service_status(s_event * event);
void is_system_halt(h_sys_state state);

/*! \brief Close the specified channel
 *  Called to close a channel. Sends a message with length -1 on the channel to
 *  notify the client (if the connection is still open) and frees the ngcs_chan
 *  structure, calling the ngcs_chan.free callback first if there is one
 *
 *  \param chan the channel to be closed
 */
void ngcs_close_channel(ngcs_chan * chan);

/* \brief Register an ngcs command
 * Registers an ngcs command. ngcs clients can then call it by sending a
 *  message on channel 0, consisting of a structure (see ngcs_pack() and
 *  ngcs_unpack()) containing the command name followed by any arguments. If
 * more than one registered command has the same name, the results are undefined.
 *
 * \param cmd structure describing the command - must not be freed, modified
 *        or passed to ngcs_reg_cmd again until ngcs_unreg_cmd(cmd) has been called
 * \sa ngcs_unreg_cmd() ngcs_cmd
 */
void ngcs_reg_cmd(ngcs_cmd * cmd);

/* \brief Unregister an ngcs command
 * Unregisters a command preivously registered with ngcs_reg_cmd.
 *
 * \param cmd the ngcs_cmd structure passed to ngcs_reg_cmd
 * \sa ngcs_reg_cmd()
 */
void ngcs_unreg_cmd(ngcs_cmd * cmd);

/* \brief Open a new channel on the specified connection
 *
 * Opens a new channel on the specified connection by assigning a channel number
 * and calling ngcs_chan_reg(). The caller is responsible for communicating the
 * channel number of the new channel to the client.
 *
 * \param conn the connection to create/open a channel on
 * \param gotdata  Function called when data is received on the channel.
 * \param chanfree Function called when the channel is freed.
 * \return the ngcs_chan structure for the new channel, as returned by
 *         ngcs_chan_reg, or null on failure.
 * \sa ngcs_chan_reg
 */
ngcs_chan *ngcs_open_channel(ngcs_conn * conn,
							 void (*gotdata) (ngcs_chan *, int, int,
											  char *),
							 void (*chanfree) (ngcs_chan *));

/* \brief Send a response to a request
 *
 * This command sends a response to a request. It should be called exactly
 * once in the handler for a request. (See ngcs_sendmsg() for documentation
 * of the arguments)
 *
 * \param req  the ngcs_request* passed to the handler
 * \param type the data type of the message
 * \param len  the length (in bytes) of the data to send
 * \param data the actual data to send
 * \return Zero on success, non-zero on failure
 * \sa ngcs_sendmsg() ngcs_cmd
 */
int ngcs_send_response(ngcs_request * req, int type, int len,
					   const char *data);

/*! \brief Ngcs command handler
 *
 *  Represents an ngcs command handler. Ngcs commands are sent on channel 0 of
 *  a connection and are identified by a (case-sensitive) string, their name
 *
 *  \sa ngcs_reg_cmd() ngcs_unreg_cmd()
 */
struct ngcs_cmd_s
{
	/*! \brief Name of the command */
	const char *name;

	/*! \brief Callback function to handle the request
	 *
	 *  The callback function to handle the request. It should call  ngcs_send_response
	 *  exactly once to send a response to the request.
	 *  \sa ngcs_send_response()
	 */
	void (*func) (ngcs_request * req);

	/* \brief List head - internal use */
	struct list_head list;
};

struct ngcs_request_s
{
	int argc;
	ngcs_data *argv;
	ngcs_chan *chan;
	ngcs_conn *conn;
	int sent_resp_flag;
};

struct ngcs_svr_conn_s
{
	f_module_h fdw;
	int nextid;
	ngcs_conn *conn;
	struct list_head list;
};

extern ngcs_svr_conn ngcs_conns;
extern ngcs_svr_conn ngcs_dead_conns;
extern ngcs_cmd ngcs_cmds;

#define while_ngcs_conns(current) list_for_each_entry_prev(current, &ngcs_conns.list, list)
#define while_ngcs_conns_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &ngcs_conns.list, list)

#define while_ngcs_dead_conns(current) list_for_each_entry_prev(current, &ngcs_dead_conns.list, list)
#define while_ngcs_dead_conns_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &ngcs_dead_conns.list, list)

#define while_ngcs_cmds(current) list_for_each_entry_prev(current, &ngcs_cmds.list, list)
#define while_ngcs_cmds_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &ngcs_cmds.list, list)

#endif /* !INITNG_NGCS_H */
