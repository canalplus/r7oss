/*
 * This header provides constants clk index STMicroelectronics
 * STiD127 SoC.
 */
#ifndef _DT_BINDINGS_CLK_STID127
#define _DT_BINDINGS_CLK_STID127

/* CLOCKGEN A0 HS0 */
#define CLK_CT_DIAG		0
#define CLK_FDMA_0		1
#define CLK_FDMA_1		2

/* CLOCKGEN A0 LS */
#define CLK_IC_CM_ST40		0
#define CLK_IC_SPI		1
#define CLK_IC_CPU		2
#define CLK_IC_MAIN		3
#define CLK_IC_ROUTER		4
#define CLK_IC_PCIE		5
#define CLK_IC_LP		6
#define CLK_IC_LP_CPU		7
#define CLK_IC_STFE		8
#define CLK_IC_DMA		9
#define CLK_IC_GLOBAL_ROUTER	10
#define CLK_IC_GLOBAL_PCI	11
#define CLK_IC_GLOBAL_PCI_TARG	12
#define CLK_IC_GLOBAL_NETWORK	13
#define CLK_A9_TRACE_INT	14
#define CLK_A9_EXT2F		15
#define CLK_IC_LP_D3		16
#define CLK_IC_LP_DQAM		17
#define CLK_IC_LP_ETH		18
#define CLK_IC_LP_HD		19
#define CLK_IC_SECURE		20

/* CLOCKGEN A1 HS0 */
#define CLK_IC_DDR		5

/* CLOCKGEN A1 LS */

/* CLOCKGEN TEL */
#define CLK_FDMA_TEL		0
#define CLK_ZSI			1
#define CLK_ETH0		2
#define CLK_PAD_OUT		3
#define CLK_USB_SRC		4

/* CLOCKGEN DOC */
#define CLK_FP			0
#define CLK_D3_XP70		1
#define CLK_IFE			2
#define CLK_TSOUT_SRC		3
#define CLK_DOC_VCO		4

/* CLOCKGEN CCM TEL */
#define CLK_ZSI_TEL		0
#define CLK_ZSI_APPL		3

/* CLOCKGEN CCM USB */
#define CLK_USB_REF		3

/* CLOCKGEN CCM LPC */
#define CLK_THERMAL_SENSE	2
#define CLK_LPC_COMMS		3

#endif
