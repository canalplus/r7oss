/*
 *  Multi-Target Trace solution
 *
 *  KPTRACE SUB-PROTOCOL DEFINITION

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */
#ifndef _MTT_PCTL_KPTRACE_H_
#define _MTT_PCTL_KPTRACE_H_

/**** kptrace payload (sub-protocol) definitions **
 **/
#define KPTRACE_BUF_SIZE 1024

/* This is, within the MTT payload, the coding used by
 * kptrace. the "key" field is built to encode the
 * event type. As kptrace need format encoding for the
 * parser, and to avoid to rewrite too much of the front end
 * we encode display type in the key field, wich overrides the
 * type_info field (format part).
 **/
struct mtt_ptcl_kptrace {
	uint32_t key;/*trace keying to not change existing parsers for now*/
	union {
		struct {
			uint32_t r[4];
		} regs;
		char buf[KPTRACE_BUF_SIZE];
	};
};

/* kptrace trace type keying, values
 * are preset to the right field.
 * the parser/decoder will rely on these
 * values, do not change without changing the kptrace
 * subprotocol variant "KPT_VER".
 * */

/* The parser/decoder relies on those values.
 * Do not change existing values if you add new keys.
 **/
#define MTT_KPT_VER_MAJOR 4
#define MTT_KPT_VER_MINOR 0
#define MTT_KPT_VER_BUILD 0

enum kpt_key_indexes {
	KEY_E = 0x00010000,	/*entering system call */
	KEY_I = 0x00020000,	/*interrupt */
	KEY_i = 0x00030000,	/*return from interrupt */
	KEY_IS = 0x00040000,	/*do_setitimer */
	KEY_Is = 0x00050000,	/*do_setitimer returned */
	KEY_IE = 0x00060000,	/*it_real_fn */
	KEY_Ie = 0x00070000,	/*it_real_fn returned */
	KEY_KC = 0x00080000,	/*kthread_create */
	KEY_Kc = 0x00090000,	/*kthread_create returned */
	/*KEY_KD = 0x000a0000,	(removed : daemonize) */
	KEY_KT = 0x000b0000,	/*kernel_thread */
	KEY_KX = 0x000c0000,	/*exit_thread */
	KEY_Ix = 0x000d0000,	/*irq_exit */
	KEY_HF = 0x000e0000,	/*hash_futex */
	KEY_K = 0x000f0000,	/*kptrace_write_record */
	KEY_KM = 0x00100000,	/*kptrace_mark */
	KEY_KP = 0x00110000,	/*kptrace_pause */
	KEY_KR = 0x00120000,	/*kptrace_restart */
	KEY_MM = 0x00130000,	/*kmalloc entry */
	KEY_Mm = 0x00140000,	/*kmalloc return */
	KEY_MF = 0x00150000,	/*kfree */
	KEY_MD = 0x00160000,	/*do_page_fault */
	KEY_MV = 0x00170000,	/*vmalloc */
	KEY_Mv = 0x00180000,	/*vmalloc returns */
	KEY_MQ = 0x00190000,	/*vfree */
	KEY_MG = 0x001a0000, /*__get_free_pages*/
	KEY_Mg = 0x001b0000, /*__get_free_pages returned*/
	KEY_MA = 0x001c0000,	/*alloc_pages */
	KEY_Ma = 0x001d0000,	/*alloc_pages returned */
	KEY_MP = 0x001e0000,	/*free_page */
	KEY_MZ = 0x001f0000,	/*free_pages */
	KEY_MS = 0x00200000,	/*kmem_cache_alloc */
	KEY_Ms = 0x00210000,	/*kmem_cache_alloc returned */
	KEY_MX = 0x00220000,	/*kmem_cache_free */
	KEY_MB = 0x00230000,	/*bigphysarea_alloc_pages */
	KEY_Mb = 0x00240000,	/*bigphysarea_alloc_pages returned */
	KEY_MC = 0x00250000,	/*bigphysarea_free_pages */
	KEY_NR = 0x00260000,	/*netif_receive_skb */
	KEY_Nr = 0x00270000,	/*netif_receive_skb returned */
	KEY_NX = 0x00280000,	/*dev_queue_xmit */
	KEY_Nx = 0x00290000,	/*dev_queue_xmit returned */
	KEY_P = 0x002a0000,	/*Oprofile kernel mode sample */
	KEY_Pu = 0x002b0000,	/*Oprofile usermode mode sample */
	KEY_S = 0x002c0000,	/*softirq */
	KEY_s = 0x002d0000,	/*softirq returned */
	KEY_SS = 0x002e0000,	/*sock_sendmsg */
	KEY_Ss = 0x002f0000,	/*sock_sendmsg returned */
	KEY_SR = 0x00300000,	/*sock_recvmsg */
	KEY_Sr = 0x00310000,	/*sock_recvmsg returned */
	KEY_U = 0x00320000,	/*Begin user event */
	KEY_u = 0x00330000,	/*End user event */
	KEY_US = 0x00340000,	/*Userspace event */
	KEY_W = 0x00350000,	/*waking process */
	KEY_X = 0x00360000,	/*exiting system call */
	/*KEY_ZL = 0x00370000	(removed) */
	/*KEY_Zl = 0x00380000	(removed) */
	KEY_ZM = 0x00390000,	/*mutex_lock */
	KEY_Zm = 0x003a0000,	/*mutex_unlock */
	KEY_ZD = 0x003b0000,	/*down */
	KEY_ZI = 0x003c0000,	/*down_interruptible */
	KEY_ZT = 0x003d0000,	/*down_trylock */
	KEY_Zt = 0x003e0000,	/*down_trylock returns */
	KEY_ZU = 0x003f0000,	/*up */
	KEY_Zu = 0x00400000, /*__up*/
	KEY_ZR = 0x00410000,	/*down_read */
	KEY_ZA = 0x00420000,	/*down_read_trylock */
	KEY_Za = 0x00430000,	/*down_read_trylock returns */
	KEY_Zr = 0x00440000,	/*up_read */
	KEY_ZW = 0x00450000,	/*down_write */
	KEY_ZB = 0x00460000,	/*down_write_trylock */
	KEY_Zb = 0x00470000,	/*down_write_trylock returns */
	KEY_Zw = 0x00480000,	/*up_write */
	KEY_MH = 0x00490000,
	KEY_Mh = 0x004a0000,
	KEY_MI = 0x004b0000,
	KEY_C = 0x004c0000,
	KEY_IP = 0x004d0000,	/* IPI entry */
	KEY_ip = 0x004e0000	/* IPI exit */
};

/* argument types, in the case of syscall
 * they take precedence over the type info!
 **/
#define KEY_ARGX(pos) (0x0 << (4*pos))	/*unknown, look into mtt:type_info */
#define KEY_ARGH(pos) (0xa << (4*pos))	/*hex value reg */
#define KEY_ARGI(pos) (0xb << (4*pos))	/*int value reg */
#define KEY_ARGS(pos) (0xc << (4*pos))	/*string */

#define  KEY_ARGS_HHHH (KEY_ARGH(0)|KEY_ARGH(1)|KEY_ARGH(2)|KEY_ARGH(3))
#define  KEY_ARGS_HIHH (KEY_ARGH(0)|KEY_ARGI(1)|KEY_ARGH(2)|KEY_ARGH(3))
#define  KEY_ARGS_HIIH (KEY_ARGH(0)|KEY_ARGI(1)|KEY_ARGI(2)|KEY_ARGH(3))
#define  KEY_ARGS_HIII (KEY_ARGH(0)|KEY_ARGI(1)|KEY_ARGI(2)|KEY_ARGI(3))
#define  KEY_ARGS_IHHH (KEY_ARGI(0)|KEY_ARGH(1)|KEY_ARGH(2)|KEY_ARGH(3))
#define  KEY_ARGS_IHIH (KEY_ARGI(0)|KEY_ARGH(1)|KEY_ARGI(2)|KEY_ARGH(3))
#define  KEY_ARGS_IIHH (KEY_ARGI(0)|KEY_ARGI(1)|KEY_ARGH(2)|KEY_ARGH(3))
#define  KEY_ARGS_HHHS (KEY_ARGH(0)|KEY_ARGH(1)|KEY_ARGH(2)|KEY_ARGS(3))
#define  KEY_ARGS_IHHS (KEY_ARGI(0)|KEY_ARGH(1)|KEY_ARGH(2)|KEY_ARGS(3))

#endif /*_MTT_PCTL_KPTRACE_H_*/
