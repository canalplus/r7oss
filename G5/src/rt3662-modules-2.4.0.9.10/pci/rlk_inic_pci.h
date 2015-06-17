#ifndef __RLK_INIC_PCI_H__
#define __RLK_INIC_PCI_H__

#define RLK_INIC_NUM_STATS		14	/* struct rlk_inic_dma_stats, plus one */
#define RLK_INIC_STATS_SIZE		64	/* size in bytes of DMA stats block */
#define RLK_INIC_REGS_SIZE		(0x10000)
#define RLK_INIC_REGS_VER			1	/* version 1 */
#define RLK_INIC_RX_RING_SIZE		64  //64
#define RLK_INIC_TX_RING_SIZE		128 //64									
#define RLK_INIC_RING_BYTES		\
		((sizeof(struct rlk_inic_rx_desc) * RLK_INIC_RX_RING_SIZE) +	\
		 (sizeof(struct rlk_inic_tx_desc) * RLK_INIC_TX_RING_SIZE) +	\
		 RLK_INIC_STATS_SIZE)
#define TX_BUFFS_AVAIL(RT)					\
		((signed int)(((RT)->tx0_done_count+RLK_INIC_TX_RING_SIZE)-(RT)->tx0_send_count))

#define PKT_BUF_SZ		1536	/* Size of each temporary Rx buffer.*/
#define RX_OFFSET		2

/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT		(6*HZ)

#define SWAP32(x) \
    ((UINT32)( \
    (((UINT32)(x) & (UINT32) 0x000000ffUL) << 24) | \
    (((UINT32)(x) & (UINT32) 0x0000ff00UL) <<  8) | \
    (((UINT32)(x) & (UINT32) 0x00ff0000UL) >>  8) | \
    (((UINT32)(x) & (UINT32) 0xff000000UL) >> 24) ))


#if defined(TWINPASS) || defined(IKANOS_VX_1X0)
#define	SWAP_PCI_REG_ACCESS 
#endif 

#ifdef SWAP_PCI_REG_ACCESS
#define RT_REG_READ32(reg)      SWAP32(readl(rt->regs + (reg)))
#else
#define RT_REG_READ32(reg)      readl(rt->regs + (reg))
#endif

#ifdef SWAP_PCI_REG_ACCESS
#define RT_REG_WRITE32(reg,val)	writel(SWAP32(val), rt->regs + (reg))
#else
#define RT_REG_WRITE32(reg,val)	writel((val), rt->regs + (reg))
#endif

#ifdef SWAP_PCI_REG_ACCESS
#define RT_REG_WRITE32_FLUSH(reg,val) do {      \
            writel(SWAP32(val), rt->regs + (reg));    \
            readl(rt->regs + (reg));            \
        } while (0)
#else
#define RT_REG_WRITE32_FLUSH(reg,val) do { \
            writel((val), rt->regs + (reg));  \
            readl(rt->regs + (reg));          \
        } while (0)
#endif

#define RLK_INIC_TX_IDX0_INC(x)	((x) = (((x)+1) % (RLK_INIC_TX_RING_SIZE)))
#define RLK_INIC_RX_IDX0_INC(x)	((x) = (((x)+1) % (RLK_INIC_RX_RING_SIZE)))
#define INT_ENABLE_ALL		    (FE_RX_DONE_INT0 | FE_TX_DONE_INT0)
#define rlk_inic_rx_intr_mask		FE_RX_DONE_INT0

#ifndef SA_SHIRQ
#define SA_SHIRQ IRQF_SHARED
#endif
/*
 *     PDMA RX Descriptor Format define
 */

typedef struct _PDMA_RXD_INFO1_  PDMA_RXD_INFO1_T;

struct _PDMA_RXD_INFO1_
{
	unsigned int    PDP0;
};

typedef struct _PDMA_RXD_INFO2_    PDMA_RXD_INFO2_T;

struct _PDMA_RXD_INFO2_
{
#ifdef __BIG_ENDIAN
	unsigned int    DDONE_bit             : 1;
	unsigned int    LS0                   : 1;
	unsigned int    PLEN0                 : 14;
	unsigned int    UN_USED               : 1;
	unsigned int    LS1                   : 1;
	unsigned int    PLEN1                 : 14;
#else
	unsigned int    PLEN1                 : 14;
	unsigned int    LS1                   : 1;
	unsigned int    UN_USED               : 1;
	unsigned int    PLEN0                 : 14;
	unsigned int    LS0                   : 1;
	unsigned int    DDONE_bit             : 1;
#endif
};

typedef struct _PDMA_RXD_INFO3_  PDMA_RXD_INFO3_T;

struct _PDMA_RXD_INFO3_
{
	unsigned int    PDP1;
};

typedef struct _PDMA_RXD_INFO4_    PDMA_RXD_INFO4_T;

struct _PDMA_RXD_INFO4_
{
#ifdef __BIG_ENDIAN
	unsigned int    IPFVLD_bit          : 1;
	unsigned int    L4FVLD_bit          : 1;
	unsigned int    IPF                 : 1;
	unsigned int    L4F                 : 1;
	unsigned int    AIS                 : 1;
	unsigned int        SP                  : 3;
	unsigned int    AI                  : 8;
	unsigned int    UN_USE1             : 1;
	unsigned int    FVLD                : 1;
	unsigned int    FOE_Entry           : 14;
#else
	unsigned int    FOE_Entry           : 14;
	unsigned int    FVLD                : 1;
	unsigned int    UN_USE1             : 1;
	unsigned int    AI                  : 8;
	unsigned int        SP                  : 3;
	unsigned int    AIS                 : 1;
	unsigned int    L4F                 : 1;
	unsigned int    IPF                 : 1;
	unsigned int    L4FVLD_bit          : 1;
	unsigned int    IPFVLD_bit          : 1;
#endif

};


struct rlk_inic_rx_desc
{
	PDMA_RXD_INFO1_T rxd_info1;
	PDMA_RXD_INFO2_T rxd_info2;
	PDMA_RXD_INFO3_T rxd_info3;
	PDMA_RXD_INFO4_T rxd_info4;
};


/*
 *    PDMA TX Descriptor Format define
 */

typedef struct _PDMA_TXD_INFO1_  PDMA_TXD_INFO1_T;

struct _PDMA_TXD_INFO1_
{
	unsigned int    SDP0;
};

typedef struct _PDMA_TXD_INFO2_    PDMA_TXD_INFO2_T;

struct _PDMA_TXD_INFO2_
{
#ifdef __BIG_ENDIAN
	unsigned int    DDONE_bit             : 1;
	unsigned int    LS0_bit               : 1;
	unsigned int    SDL0                  : 14;
	unsigned int    BURST_bit             : 1;
	unsigned int    LS1_bit               : 1;
	unsigned int    SDL1                  : 14;
#else
	unsigned int    SDL1                  : 14;
	unsigned int    LS1_bit               : 1;
	unsigned int    BURST_bit             : 1;
	unsigned int    SDL0                  : 14;
	unsigned int    LS0_bit               : 1;
	unsigned int    DDONE_bit             : 1;
#endif

};

typedef struct _PDMA_TXD_INFO3_  PDMA_TXD_INFO3_T;

struct _PDMA_TXD_INFO3_
{
	unsigned int    SDP1;
};

typedef struct _PDMA_TXD_INFO4_    PDMA_TXD_INFO4_T;

struct _PDMA_TXD_INFO4_
{
#ifdef __BIG_ENDIAN
	unsigned int    IC0_bit             : 1;
	unsigned int    UC0_bit             : 1;
	unsigned int    TC0                 : 1;
	unsigned int    UN_USE1             : 2;
	unsigned int    PN                  : 3;
	unsigned int    UN_USE2             : 5;
	unsigned int    QN                  : 3;
	unsigned int    UN_USE3             : 3;
	unsigned int    INSP                : 1;
	unsigned int    SIDX                : 4;
	unsigned int    INSV                : 1;
	unsigned int    VPRI                : 3;
	unsigned int    VIDX                : 4;
#else
	unsigned int    VIDX                : 4;
	unsigned int    VPRI                : 3;
	unsigned int    INSV                : 1;
	unsigned int    SIDX                : 4;
	unsigned int    INSP                : 1;
	unsigned int    UN_USE3             : 3;
	unsigned int    QN                  : 3;
	unsigned int    UN_USE2             : 5;
	unsigned int    PN                  : 3;
	unsigned int    UN_USE1             : 2;
	unsigned int    TC0                 : 1;
	unsigned int    UC0_bit             : 1;
	unsigned int    IC0_bit             : 1;
#endif
};

struct rlk_inic_tx_desc
{
	PDMA_TXD_INFO1_T txd_info1;
	PDMA_TXD_INFO2_T txd_info2;
	PDMA_TXD_INFO3_T txd_info3;
	PDMA_TXD_INFO4_T txd_info4;
};

struct ring_info
{
	struct sk_buff  *skb;
	dma_addr_t      mapping;
	unsigned        frag;
};

struct rlk_inic_dma_stats
{
	u64         tx_ok;
	u64         rx_ok;
	u64         tx_err;
	u32         rx_err;
	u16         rx_fifo;
	u16         frame_align;
	u32         tx_ok_1col;
	u32         tx_ok_mcol;
	u64         rx_ok_phys;
	u64         rx_ok_bcast;
	u32         rx_ok_mcast;
	u16         tx_abort;
	u16         tx_underrun;
} __attribute__((packed));

struct rlk_inic_extra_stats
{
	unsigned long       rx_frags;
};


typedef struct inic_private
{
	void                __iomem *regs;
	struct net_device   *dev;
	spinlock_t          lock;
	u32                 msg_enable;

	struct pci_dev      *pdev;
	u32                 rx_config;
	u16                 cpcmd;

	struct net_device_stats     net_stats;
	struct rlk_inic_extra_stats   rlk_inic_stats;
	struct rlk_inic_dma_stats     *nic_stats;
	dma_addr_t                  nic_stats_dma;

	struct rlk_inic_rx_desc       *rx_ring;
	struct ring_info            rx_skb[RLK_INIC_RX_RING_SIZE];
	unsigned                    rx_buf_sz;

	int                                 rx_dma_owner_idx0;	/* Point to the next RXD DMA wants to use in RXD Ring#0.  */
	int                                 rx_wants_alloc_idx0;/* Point to the next RXD CPU wants to allocate to RXD Ring #0. */
	int                                 tx_cpu_owner_idx0;	/* Point to the next TXD in TXD_Ring0 CPU wants to use */
	int                                 tx_cpu_owner_idx1;
	/* 
	 * software counter 
	 * use two counter to maintain tx ring                                          .
	 * @ tx_send_counter : counting the tx frames tramsitting to TX DMA 
	 * @ tx_done_counter : counting the tx frames had tramsitted by TX DMA 
	 * @ (tx_send_counter - tx_done_counter) <= NUM_TXQ0_DESC 
	 */
	volatile unsigned int               tx0_send_count;		/* transmited frames counter */
	volatile unsigned int               tx0_done_count;	/* counting the tx frames had dma done */
	unsigned int                        int_enable_reg;	/* interrupts we want */ 
	unsigned int                        int_disable_mask; /* run-time disabled interrupts */


	struct rlk_inic_tx_desc       *tx_ring0;
	struct ring_info            tx_skb[RLK_INIC_TX_RING_SIZE];
	dma_addr_t                  ring_dma;

	RACFG_OBJECT				RaCfgObj;
	struct iw_statistics iw_stats;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    struct napi_struct   napi;
#endif
} iNIC_PRIVATE;

void rlk_inic_init_hw (iNIC_PRIVATE *rt);
void rlk_inic_stop_hw (iNIC_PRIVATE *rt);

#endif /* __RLK_INIC_PCI_H__ */

