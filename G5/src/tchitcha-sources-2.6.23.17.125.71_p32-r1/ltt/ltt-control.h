/*
 * LTT control module over a netlink socket.
 *
 * Inspired from Relay Apps, by Tom Zanussi and iptables
 *
 * Copyright 2005 -
 * 	Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 */

#ifndef _LTT_CONTROL_H
#define _LTT_CONTROL_H

enum trace_op {
	OP_CREATE,
	OP_DESTROY,
	OP_START,
	OP_STOP,
	OP_ALIGN,
	OP_NONE
};

typedef struct lttctl_peer_msg {
	char trace_name[NAME_MAX];
	char trace_type[NAME_MAX];
	enum trace_op op;
	union ltt_control_args args;
} lttctl_peer_msg_t;

#endif /* _LTT_CONTROL_H */

