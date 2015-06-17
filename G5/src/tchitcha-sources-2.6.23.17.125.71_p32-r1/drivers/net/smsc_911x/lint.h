/***************************************************************************
 *
 * Copyright (C) 2004-2005  SMSC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 ***************************************************************************
 * Additionally it deserves noting that the everything in this file is
 *   for the purpose of making the lint tool happy. Therefore everything
 *   declared or defined in this file has been thoroughly customized. It
 *   is not suitable to be use as a reference nor compatible for any
 *   other purpose.
 *   For proper declarations and definitions see the linux source code.
 ***************************************************************************
 * File: lint.h
 */

#ifndef LINT_H
#define LINT_H

/********************************************************
******************SHARED*********************************
********************************************************/
#define IFNAMSIZ	(10)
#define SIOCDEVPRIVATE (10000UL)
struct ifreq
{
//#define IFHWADDRLEN	6
//#define	IFNAMSIZ	16
#ifdef CMD9118
	union
	{
		char	ifrn_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	} ifr_ifrn;
#endif

	union {
//		struct	sockaddr ifru_addr;
//		struct	sockaddr ifru_dstaddr;
//		struct	sockaddr ifru_broadaddr;
//		struct	sockaddr ifru_netmask;
//		struct  sockaddr ifru_hwaddr;
//		short	ifru_flags;
//		int	ifru_ivalue;
//		int	ifru_mtu;
//		struct  ifmap ifru_map;
//		char	ifru_slave[IFNAMSIZ];	/* Just fits the size */
//		char	ifru_newname[IFNAMSIZ];
		char *	ifru_data;
//		struct	if_settings ifru_settings;
	} ifr_ifru;
};
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface	*/
#define ifr_name	ifr_ifrn.ifrn_name	/* interface name 	*/

#define NULL ((void *)(0))
extern void memset(void *__s, int __c, unsigned int __count);
extern void memcpy(void *a,const void * b,unsigned int c);
extern void sprintf(char * a,const char * b,...);
extern void strcpy(signed char * s,const signed char * s);

/***********************************************************
************DRIVER ONLY*************************************
************************************************************/
#ifndef CMD9118

#define LINUX_VERSION_CODE 132113
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

//typedef void irqreturn_t;
//#define IRQ_NONE
//#define IRQ_HANDLED
//#define IRQ_RETVAL(x)


//typedef long TIME_SPAN;
//#define MAX_TIME_SPAN	((TIME_SPAN)(0x7FFFFFFFUL))
//typedef unsigned long DWORD;
//typedef unsigned short WORD;
//typedef unsigned char BYTE;
//typedef unsigned char BOOLEAN;
//#define TRUE	((BOOLEAN)1)
//#define FALSE	((BOOLEAN)0)
//
//#define HIBYTE(word)  ((BYTE)(((WORD)(word))>>8))
//#define LOBYTE(word)  ((BYTE)(((WORD)(word))&0x00FFU))
//#define HIWORD(dWord) ((WORD)(((DWORD)(dWord))>>16))
//#define LOWORD(dWord) ((WORD)(((DWORD)(dWord))&0x0000FFFFUL))
typedef unsigned long spinlock_t;

typedef unsigned long ULONG;
typedef unsigned char UCHAR;

//extern void SMSC_TRACE(const char * a,...);
//extern void SMSC_WARNING(const char * a,...);
//extern void SMSC_ASSERT(BOOLEAN condition);

//#define TRANSFER_PIO			((DWORD)256)
//#define TRANSFER_REQUEST_DMA	((DWORD)255)

//DMA Transfer structure
//typedef struct _DMA_XFER
//{
//	DWORD dwLanReg;
//	DWORD *pdwBuf;
//	DWORD dwDmaCh;
//	DWORD dwDwCnt;
//	BOOLEAN fMemWr;
//} DMA_XFER, *PDMA_XFER;
//
//typedef struct _FLOW_CONTROL_PARAMETERS
//{
//	DWORD MaxThroughput;
//	DWORD MaxPacketCount;
//	DWORD PacketCost;
//	DWORD BurstPeriod;
//	DWORD IntDeas;
//} FLOW_CONTROL_PARAMETERS, *PFLOW_CONTROL_PARAMETERS;

struct net_device_stats
{
	ULONG	rx_packets;
	ULONG	tx_packets;
	ULONG	rx_bytes;
	ULONG	tx_bytes;
	ULONG	rx_errors;
	ULONG	tx_errors;
	ULONG	rx_dropped;
	ULONG	multicast;
	ULONG	collisions;
	ULONG	rx_length_errors;
	ULONG	rx_crc_errors;
	ULONG	tx_aborted_errors;
	ULONG	tx_carrier_errors;
};

struct timer_list {
	ULONG expires;
	ULONG data;
	void (*function)(ULONG param);
};

#define MODULE_PARM(var,type)		 	\
const char __module_parm_##var[] = type

#define MODULE_PARM_DESC(var,desc)		\
const char __module_parm_desc_##var[] = desc

#define MODULE_LICENSE(license) 	\
static const char __module_license[] =license

#define MOD_INC_USE_COUNT	do { } while (0)
#define MOD_DEC_USE_COUNT	do { } while (0)

#define MAX_ADDR_LEN (6)

struct dev_mc_list
{
	struct dev_mc_list	*next;
	UCHAR dmi_addr[MAX_ADDR_LEN];
	UCHAR dmi_addrlen;
};

struct net_device
{
	char name[IFNAMSIZ];
	struct net_device_stats* (*get_stats)(struct net_device *dev);
	unsigned short flags;
//	spinlock_t xmit_lock;
	void * priv;
	UCHAR dev_addr[MAX_ADDR_LEN];
	struct dev_mc_list * mc_list;
	int mc_count;
//	int (*init)(struct net_device *dev);
	int	(*open)(struct net_device *dev);
	int	(*stop)(struct net_device *dev);
	int	(*hard_start_xmit) (struct sk_buff *skb,struct net_device *dev);
	void (*set_multicast_list)(struct net_device *dev);
	int (*do_ioctl)(struct net_device *dev, struct ifreq *ifr,int cmd);
};

struct mii_ioctl_data {
	ULONG	phy_id;
	ULONG	reg_num;
	ULONG	val_in;
	ULONG	val_out;
};

#define SIOCGMIIPHY	(1000UL)
#define SIOCGMIIREG	(1001UL)
#define SIOCSMIIREG	(1002UL)


struct sk_buff {
	struct net_device	*dev;
	unsigned int 	len;
	unsigned char	ip_summed;
	unsigned short	protocol;
	unsigned char	*head;
	unsigned char	*data;
	unsigned char	*tail;
};

struct tasklet_struct
{
	struct tasklet_struct *next;
	unsigned long state;
	unsigned long count;
	void (*func)(unsigned long param);
	unsigned long data;
};

#define DECLARE_TASKLET(name, func, data) \
struct tasklet_struct name = { NULL, 0, 0, func, data }

extern struct sk_buff *dev_alloc_skb(unsigned int length);
extern unsigned short eth_type_trans(struct sk_buff *skb, struct net_device *dev);
extern void netif_carrier_on(struct net_device *dev);
extern void netif_carrier_off(struct net_device *dev);
extern int netif_rx(struct sk_buff *skb);
extern void netif_start_queue(struct net_device *dev);
extern void netif_wake_queue(struct net_device *dev);
extern void netif_stop_queue(struct net_device *dev);
extern void free_irq(unsigned int Irq, void *isr);
extern int check_mem_region(unsigned long a, unsigned long b);
extern void release_mem_region(unsigned long a, unsigned long b);
extern void tasklet_schedule(struct tasklet_struct *t);
extern void del_timer_sync(struct timer_list * timer);
extern void init_timer(struct timer_list * timer);
extern void ether_setup(struct net_device *dev);
extern int register_netdev(struct net_device *dev);
extern void add_timer(struct timer_list * timer);
extern void spin_lock(spinlock_t *s);
extern void spin_unlock(spinlock_t *s);
extern void spin_lock_init(spinlock_t *s);
extern void spin_lock_irqsave(spinlock_t *s,unsigned long f);
extern void spin_unlock_irqrestore(spinlock_t *s,unsigned long f);
extern void unregister_netdev(struct net_device *dev);
extern void udelay(unsigned long usecs);
#define SET_MODULE_OWNER(some_struct) do { } while (0)
extern void request_mem_region(unsigned long start, unsigned long n, const signed char *name);
extern int request_irq(unsigned int a,
		       void (*handler)(int a, void * b, struct pt_regs * c),
		       unsigned long c, const signed char * d, void * e);
extern void dev_kfree_skb(struct sk_buff *skb);
extern void skb_reserve(struct sk_buff *skb, unsigned int len);
extern void skb_put(struct sk_buff *skb, unsigned int len);


#define NET_RX_SUCCESS		0
#define NET_RX_DROP			1
#define NET_RX_CN_LOW		2
#define NET_RX_CN_MOD		3
#define NET_RX_CN_HIGH		4

#define CHECKSUM_NONE 0

#define	ENOMEM		12
#define	EFAULT		14
#define	ENODEV		19

#define GFP_KERNEL	(1)

#define IFF_MULTICAST   (1U)
#define IFF_PROMISC		(2U)
#define IFF_ALLMULTI	(4U)

#define SA_INTERRUPT	0x20000000

extern void *kmalloc(unsigned int a, int b);
extern void kfree(const void *a);

extern unsigned long volatile jiffies;
#define HZ 100

#endif //not CMD9118


/*******************************************************
*************XSCALE DRIVER******************************
*******************************************************/
#ifdef USE_XSCALE
#define MST_EXP_BASE (0x80000000UL)
#define MST_EXP_PHYS (0x00000005UL)
#define MSC2 (*((volatile unsigned long *)(0x48000000UL)))
#define MAINSTONE_nExBRD_IRQ (50UL)
#define DCSR_RUN (1UL)
#define DCSR_STOPSTATE (2UL)
#define DCSR_NODESC (4UL)
#define DCMD_INCTRGADDR (1UL)
#define DCMD_INCSRCADDR (2UL)
#define DCMD_BURST32 (4UL)
#define DCMD_LENGTH (8UL)
#define DCSR(dmaCh) (*((volatile unsigned long *)(0x40000000UL+dmaCh)))
#define DTADR(dmaCh) (*((volatile unsigned long *)(0x40000000UL+dmaCh)))
#define DSADR(dmaCh) (*((volatile unsigned long *)(0x40000000UL+dmaCh)))
#define DCMD(dmaCh) (*((volatile unsigned long *)(0x40000000UL+dmaCh)))
extern unsigned long virt_to_bus(unsigned long arg);
#endif //USE_XSCALE


/********************************************************
**************PEAKS DRIVER*******************************
********************************************************/
#if (defined(USE_PEAKS)||defined(USE_PEAKS_LITE))
struct hw_interrupt_type {
	const char * typename;
	unsigned int (*startup)(unsigned int irq);
	void (*shutdown)(unsigned int irq);
	void (*enable)(unsigned int irq);
	void (*disable)(unsigned int irq);
	void (*ack)(unsigned int irq);
	void (*end)(unsigned int irq);
	void (*set_affinity)(unsigned int irq, unsigned long mask);
};
#define Gicr(arg) (*(volatile WORD *)(arg))
typedef struct _IRQ_TYPE {
	struct hw_interrupt_type *handler;
} IRQ_TYPE, * PIRQ_TYPE;
PIRQ_TYPE irq_desc=NULL;
extern void purge_cache(unsigned long arg1,unsigned long arg2,unsigned long arg3);
#endif //USE_PEAKS or USE_PEAKS_LITE


/**********************************************************
***************COMMAND APPLICATION*************************
**********************************************************/
#ifdef CMD9118
#define AF_INET		(0)
#define SOCK_DGRAM	(0)
struct in_addr {
	unsigned long s_addr;
};
#define INADDR_ANY (0xFFFFFFFFUL)
struct sockaddr_in {
  unsigned short int	sin_family;	/* Address family		*/
  unsigned short int	sin_port;	/* Port number			*/
  struct in_addr	sin_addr;	/* Internet address		*/

  /* Pad to size of `struct sockaddr'. */
//  unsigned char		__pad[__SOCK_SIZE__ - sizeof(short int) -
//			sizeof(unsigned short int) - sizeof(struct in_addr)];
};
struct sockaddr {
  unsigned short int	sa_family;	/* Address family		*/
//  unsigned short int	sin_port;	/* Port number			*/
//  struct in_addr	sin_addr;	/* Internet address		*/

  /* Pad to size of `struct sockaddr'. */
//  unsigned char		__pad[__SOCK_SIZE__ - sizeof(short int) -
//			sizeof(unsigned short int) - sizeof(struct in_addr)];
};

#define SOCK_STREAM		(1)
#define SOL_SOCKET		(2)
#define SO_REUSEADDR	(3)

#define FILE void

extern void * fopen(const char * fileName,const char * mode);

extern void * stdin;
extern void * stderr;
extern const char * optarg;
extern void printf(const char * b,...);
extern int recv(int sock,unsigned char *ch,int a,int b);
extern int send(int sock,unsigned char *ch,int a,int b);
extern unsigned int inet_addr(const char * hostname);
extern int socket(int a,int b,int c);
extern void close(int a);
extern int fork(void);
extern int system(const char * command);
extern void fclose(void *);
extern void strncpy(char * a, const char *b, int c);
extern void perror(char * c);
extern unsigned short htons(unsigned short a);
extern void sleep(int s);
extern void exit(int x);
extern int sscanf(const char *a,char *b,...);
extern int accept(int a,struct sockaddr *b,int *c);
extern int listen(int a,int b);
extern int strcmp(const char *a,const char *b);
extern int bind(int a,struct sockaddr *b,int c);
extern int connect(int a,struct sockaddr *b,int c);
extern int fread(char *a,int b,int c,void *d);
extern void ioctl(int a,int b,struct ifreq *c);
extern int getopt(int a,char **b,char *c);
extern int setsockopt(int a,int b,int c,char *d,int e);
extern void setsid(void);


#endif //CMD9118

#endif //LINT_H

