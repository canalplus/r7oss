#ifndef __RLK_INIC_REG_H__
#define __RLK_INIC_REG_H__

#define RLK_INIC_BIT(x)              ((1 << x))

#if 0					   
#undef PCI_DMA_FROMDEVICE
#undef PCI_DMA_TODEVICE
#define PCI_DMA_FROMDEVICE	PCI_DMA_BIDIRECTIONAL
#define PCI_DMA_TODEVICE    PCI_DMA_BIDIRECTIONAL
#define PCI_DMA_64BIT   0xffffffffffffffffULL
#define PCI_DMA_32BIT   0x00000000ffffffffULL
#endif


#define RLK_INIC_PSE_RESET			RLK_INIC_BIT(0)

#define RLK_INIC_PST_DRX_IDX0		RLK_INIC_BIT(16)
#define RLK_PST_DTX_IDX1			RLK_INIC_BIT(1)
#define RLK_INIC_PST_DTX_IDX0		RLK_INIC_BIT(0)

#define TX_WB_DDONE       RLK_INIC_BIT(6)
#define WPDMA_BT_SIZE	  RLK_INIC_BIT(4)	
#define RX_DMA_BUSY       RLK_INIC_BIT(3)
#define TX_DMA_BUSY       RLK_INIC_BIT(1)
#define RX_DMA_EN         RLK_INIC_BIT(2)
#define TX_DMA_EN         RLK_INIC_BIT(0)

/*
	 FE_INT_STATUS
*/                                        
#define FE_TX_COHERENT      RLK_INIC_BIT(17)
#define FE_RX_COHERENT      RLK_INIC_BIT(16)

#define FE_TX_DONE_INT1     RLK_INIC_BIT(9)
#define FE_TX_DONE_INT0     RLK_INIC_BIT(1) // RLK_INIC_BIT(8)
#define FE_RX_DONE_INT0     RLK_INIC_BIT(2)
#define FE_TX_DLY_INT       RLK_INIC_BIT(1)
#define FE_RX_DLY_INT       RLK_INIC_BIT(0)


#define FE_RST_GLO          0x0C
#define FE_INT_STATUS       0x10
#define FE_INT_ENABLE       0x14


#define PDMA_RELATED        0x0100
#define PDMA_GLO_CFG        (PDMA_RELATED + 0x00)
#define PDMA_RST_IDX        (PDMA_RELATED + 0x04)
#define PDMA_SCH_CFG        (PDMA_RELATED + 0x08)
#define DELAY_INT_CFG       (PDMA_RELATED + 0x0C)
#define TX_BASE_PTR0        (PDMA_RELATED + 0x10)
#define TX_MAX_CNT0         (PDMA_RELATED + 0x14)
#define TX_CTX_IDX0         (PDMA_RELATED + 0x18)
#define TX_DTX_IDX0         (PDMA_RELATED + 0x1C)
#define TX_BASE_PTR1        (PDMA_RELATED + 0x20)
#define TX_MAX_CNT1         (PDMA_RELATED + 0x24)
#define TX_CTX_IDX1         (PDMA_RELATED + 0x28)
#define TX_DTX_IDX1         (PDMA_RELATED + 0x2C)
#define RX_BASE_PTR0        (PDMA_RELATED + 0x30)
#define RX_MAX_CNT0         (PDMA_RELATED + 0x34)
#define RX_CALC_IDX0        (PDMA_RELATED + 0x38)
#define RX_DRX_IDX0         (PDMA_RELATED + 0x3C)
#define RESET_CARD          (0xFFFC)
#define RESET_FLR           (0x78)
							    
#endif /* __RLK_INIC_REG_H__ */
