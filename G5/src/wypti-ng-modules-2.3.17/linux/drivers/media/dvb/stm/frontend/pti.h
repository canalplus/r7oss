#ifndef _PTI_H_
#define _PTI_H_

#ifdef CONFIG_CPU_SUBTYPE_STB5301
#define PTI_BASE_ADDRESS 0x20E00000
#endif

#ifdef CONFIG_CPU_SUBTYPE_STB7100
#define PTI_BASE_ADDRESS 0x19230000
#endif

#define PTI_BASE_SIZE    0x10000

#define PTI_IRAM_BASE 0xC000
#define PTI_IRAM_SIZE (0x10000 - 0xC000)
#define PTI_DRAM_BASE 0x8000
#define PTI_DRAM_SIZE (0xC000 - 0x8000)

/* Interrupt registers */
#define PTI_STATUS(x)    (x * 0x4)
#define PTI_ENABLE(x)    (((x * 0x4) + 0x10))
#define PTI_ACK(x)       (((x * 0x4) + 0x20))

/* DMA Registers */
#define PTI_DMAEMPTY_STAT       (0x0034)
#define PTI_DMAEMPTY_EN         (0x0038)
#define PTI_DMA_0_BASE          (0x1000)
#define PTI_DMA_0_TOP           (0x1004)
#define PTI_DMA_0_WRITE         (0x1008)
#define PTI_DMA_0_READ          (0x100C)
#define PTI_DMA_0_SETUP         (0x1010)
#define PTI_DMA_0_HOLDOFF       (0x1014)
#define PTI_DMA_0_STATUS        (0x1018)
#define PTI_DMA_ENABLE          (0x101C)
#define PTI_DMA_1_BASE          (0x1020)
#define PTI_DMA_1_TOP           (0x1024)
#define PTI_DMA_1_WRITE         (0x1028)
#define PTI_DMA_1_READ          (0x102C)
#define PTI_DMA_1_SETUP         (0x1030)
#define PTI_DMA_1_HOLDOFF       (0x1034)
#define PTI_DMA_1_CD_ADDR       (0x1038)
#define PTI_DMA_SECT_START      (0x103C)
#define PTI_DMA_2_BASE          (0x1040)
#define PTI_DMA_2_TOP           (0x1044)
#define PTI_DMA_2_WRITE         (0x1048)
#define PTI_DMA_2_READ          (0x104C)
#define PTI_DMA_2_SETUP         (0x1050)
#define PTI_DMA_2_HOLDOFF       (0x1054)
#define PTI_DMA_2_CD_ADDR       (0x1058)
#define PTI_DMA_FLUSH           (0x105C)
#define PTI_DMA_3_BASE          (0x1060)
#define PTI_DMA_3_TOP           (0x1064)
#define PTI_DMA_3_WRITE         (0x1068)
#define PTI_DMA_3_READ          (0x106C)
#define PTI_DMA_3_SETUP         (0x1070)
#define PTI_DMA_3_HOLDOFF       (0x1074)
#define PTI_DMA_3_CD_ADDR       (0x1078)

#define PTI_DMA_PTI3_PROG       (0x107C)

/* DMA Settings */
#define PTI_DMA_DONE            (1<<0)


/* Mode Register */
#define PTI_MODE         (0x0030)

#define PTI_MODE_ENABLE      (1 << 0)

/* IIF Registers */
#define PTI_IIF_FIFO_COUNT      (0x2000)
#define PTI_IIF_FIFO_COUNT_BYTES(value) (value & 0x7f)
#define PTI_IIF_FIFO_FULL       (1<<7)

#define PTI_IIF_ALT_FIFO_COUNT  (0x2004)
#define PTI_IIF_ALT_FIFO_COUNT_BYTES(value) (value & 0xff)

#define PTI_IIF_FIFO_ENABLE     (0x2008)
#define PTI_IIF_ALT_LATENCY     (0x2010)
#define PTI_IIF_SYNC_LOCK       (0x2014)
#define PTI_IIF_SYNC_DROP       (0x2018)
#define PTI_IIF_SYNC_CONFIG     (0x201C)
#define PTI_IIF_SYNC_PERIOD     (0x2020)
#define PTI_IIF_HFIFO_COUNT     (0x2024)


/* REGISTERS BELOW NOT TO BE ALLOWED INTO PUBLIC DOMAIN, DEBUG PURPOSE ONLY,
 * (c) STMicroelectronics 2005, not for redistribution
 */

#define PTI_MODE_RESET_IPTR  (1 << 1)
#define PTI_MODE_SINGLE_STEP (1 << 2)
#define PTI_MODE_SOFT_RESET  (1 << 3)


#define PTI_REGISTERS(x) (((((x) & 0xf) < 8 ? (x) : (x)+1) * 0x4) + 0x7000)
#define PTI_IPTR         (0x7020)

#endif //_PTI_H_
