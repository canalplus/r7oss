#ifndef _FEI_H_
#define _FEI_H_

#define STFE_IB_IP_FMT_CFG         (0x00)
#define STFE_IGNORE_ERR_AT_SOP     (1<<7)
#define STFE_IGNORE_ERR_IN_PKT     (1<<6)
#define STFE_IGNORE_ERR_IN_BYTE    (1<<5)
#define STFE_INVERT_TSCLK          (1<<4)
#define STFE_ALIGN_BYTE_SOP        (1<<3)
#define STFE_AYSNC_NOT_SYNC        (1<<2)
#define STFE_BYTE_ENDIANESS_MSB    (1<<1)
#define STFE_SERIAL_NOT_PARALLEL   (1<<0)

#define STFE_IB_SYNCLCKDRP_CFG      (0x04)
#define STFE_SYNC(x)                (x & 0xf)
#define STFE_DROP(x)                ((x<<4) & 0xf)
#define STFE_TOKEN(x)               ((x<<8) & 0xff)
#define STFE_SLDENDIANNESS          (1<<16)

#define STFE_IB_TAGBYTES_CFG        (0x08)
#define STFE_TAG_HEADER(x)          (x << 16)
#define STFE_TAG_COUNTER(x)         ((x<<1) & 0x7fff)
#define STFE_TAG_ENABLE             (1<<0)

#define STFE_IB_PID_SET             (0x0C)
#define STFE_PID_OFFSET(x)          (x & 0x3f)
#define STFE_PID_NUMBITS(x)         ((x << 6) & 0xfff)
#define STFE_PID_ENABLE             (1<<31)

#define STFE_IB_PKT_LEN             (0x10)
#define STFE_IB_BUFF_STRT            (0x14)
#define STFE_IB_BUFF_END            (0x18)
#define STFE_IB_READ_PNT            (0x1C)
#define STFE_IB_WRT_PNT             (0x20)

#define STFE_IB_PRI_THRLD           (0x24)
#define STFE_PRI_VALUE(x)           (x & 0x7fffff)
#define STFE_PRI_LOWPRI(x)          ((x & 0xf) << 24)
#define STFE_PRI_HIGHPRI(x)         ((x & 0xf) << 28)

#define STFE_IB_STAT                (0x28)
#define STFE_STAT_FIFO_OVERFLOW(x)  (x & 0x1)
#define STFE_STAT_BUFFER_OVERFLOW(x) (x & 0x2)
#define STFE_STAT_OUTOFORDERRP(x)   (x & 0x4)
#define STFE_STAT_PID_OVERFLOW(x)   (x & 0x8)
#define STFE_STAT_PKT_OVERFLOW(x)   (x & 0x10)
#define STFE_STAT_ERROR_PACKETS(x)  ((x >> 8) & 0xf)
#define STFE_STAT_SHORT_PACKETS(x)  ((x >> 12) & 0xf)

#define STFE_IB_MASK                (0x2C)
#define STFE_MASK_FIFO_OVERFLOW     (1<<0)
#define STFE_MASK_BUFFER_OVERFLOW   (1<<1)
#define STFE_MASK_OUTOFORDERRP(x)   (1<<2)
#define STFE_MASK_PID_OVERFLOW(x)   (1<<3)
#define STFE_MASK_PKT_OVERFLOW(x)   (1<<4)
#define STFE_MASK_ERROR_PACKETS(x)  ((x & 0xf) << 8)
#define STFE_MASK_SHORT_PACKETS(x)  ((x & 0xf)>> 12)

#define STFE_IB_SYS                 (0x30)
#define STFE_SYS_RESET              (1<<1)
#define STFE_SYS_ENABLE             (1<<0)

struct stfe_input_record {
	unsigned long dma_busbase;
	unsigned long dma_bustop;
	unsigned long dma_buswp;
	unsigned long dma_pkt_size;
	unsigned long dma_busnextbase;
	unsigned long dma_busnexttop;
	unsigned long dma_membase;
	unsigned long dma_memtop;
};

#define STFE_DMA_DATINT_STAT   (0xc00)
#define STFE_DMA_DATINT_MASK   (0xc04)
#define STFE_DMA_ERRINT_STAT   (0xc08)
#define STFE_DMA_ERRINT_MASK   (0xc0c)
#define STFE_DMA_ERR_OVER      (0xc10)
#define STFE_DMA_ABORT         (0xc14)
#define STFE_DMA_MSSG_BNDRY    (0xc18)
#define STFE_DMA_IDLINT_STAT   (0xc1c)
#define STFE_DMA_IDLINT_MASK   (0xc20)
#define STFE_DMA_MODE          (0xc24)
#define STFE_DMA_PTR_BASE      (0xc28)

#define STFE_DMA_BUSBASE   (0xc80)
#define STFE_DMA_BUSSTOP   (0xc84)
#define STFE_DMA_BUSWP     (0xc88)

#define STFE_DMA_MEMBASE (0xc98)
#define STFE_DMA_MEMTOP  (0xc9c)
#define STFE_DMA_MEMRP   (0xca0)


#if 0
STFE_TC_COUNT_LSW           (0x400)
STFE_TC_COUNT_MSW           (0x4004)

STFE_PID_BASE
STFE_SYS_STAT
STFE_SYS_MASK
STFE_DMA_BUSSTOP
STFE_DMA_BUSWP
STFE_DMA_PKT_SIZE
STFE_DMA_BUSNEXTBASE
STFE_DMA_BUSNEXTSTOP
STFE_DMA_PKT_FIFO_LVL
STFE_DMA_PKT_FIFO_PTRS
STFE_DMA_IJCT_PKT(n)
STFE_DMA_PKT_FIFO(n)
STFE_DMA_BUS_SIZE(n)
STFE_DMA_BUS_LVL(n)
STFE_DMA_BUS_THRHLD(n)

#endif





#endif //_FEI_H_
