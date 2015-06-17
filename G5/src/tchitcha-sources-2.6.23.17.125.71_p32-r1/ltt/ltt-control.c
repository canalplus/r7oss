/*
 * LTT control module over a netlink socket.
 *
 * Inspired from Relay Apps, by Tom Zanussi and iptables
 *
 * Copyright 2005 -
 * 	Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ltt-tracer.h>
#include <linux/netlink.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/security.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <net/sock.h>
#include "ltt-control.h"

#define LTTCTLM_BASE	0x10
#define LTTCTLM_CONTROL	(LTTCTLM_BASE + 1)	/* LTT control message */

static struct sock *socket;

void ltt_control_input(struct sock *sk, int len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh = NULL;
	u8 *payload = NULL;
	lttctl_peer_msg_t *msg;
	int err;

	printk(KERN_DEBUG "ltt-control ltt_control_input\n");

	while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {

		nlh = (struct nlmsghdr *)skb->data;
		if (security_netlink_recv(skb, CAP_SYS_PTRACE)) {
			netlink_ack(skb, nlh, EPERM);
			kfree_skb(skb);
			continue;
		}
		/* process netlink message pointed by skb->data */
		err = EINVAL;
		payload = NLMSG_DATA(nlh);
		/*
		 * process netlink message with header pointed by
		 * nlh and payload pointed by payload
		 */
		if (nlh->nlmsg_len !=
				sizeof(lttctl_peer_msg_t) +
				sizeof(struct nlmsghdr)) {
			printk(KERN_ALERT
				"ltt-control bad message length %d vs. %zu\n",
				nlh->nlmsg_len, sizeof(lttctl_peer_msg_t) +
				sizeof(struct nlmsghdr));
			netlink_ack(skb, nlh, EINVAL);
			kfree_skb(skb);
			continue;
		}
		msg = (lttctl_peer_msg_t *)payload;

		switch (msg->op) {
		case OP_CREATE:
			err = ltt_control(LTT_CONTROL_CREATE_TRACE,
					msg->trace_name,
					msg->trace_type, msg->args);
			break;
		case OP_DESTROY:
			err = ltt_control(LTT_CONTROL_DESTROY_TRACE,
					msg->trace_name,
					msg->trace_type, msg->args);
			break;
		case OP_START:
			err = ltt_control(LTT_CONTROL_START,
					msg->trace_name,
					msg->trace_type, msg->args);
			break;
		case OP_STOP:
			err = ltt_control(LTT_CONTROL_STOP,
					msg->trace_name,
					msg->trace_type, msg->args);
			break;
		default:
			err = EBADRQC;
			printk(KERN_INFO "ltt-control invalid operation\n");
		}
		netlink_ack(skb, nlh, err);
		kfree_skb(skb);
	}
}


static int ltt_control_init(void)
{
	printk(KERN_INFO "ltt-control init\n");
	socket = netlink_kernel_create(NETLINK_LTT, 1,
			ltt_control_input, NULL, THIS_MODULE);
	if (socket == NULL)
		return -EPERM;
	else
		return 0;
}

static void ltt_control_exit(void)
{
	printk(KERN_INFO "ltt-control exit\n");
	sock_release(socket->sk_socket);
}


module_init(ltt_control_init)
module_exit(ltt_control_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("Linux Trace Toolkit Controller");

