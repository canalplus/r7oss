/******************************************************************************
 *
 * File name   : clock-regs-stx7141.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
******************************************************************************/


#ifndef __CLOCK_LLA_7141REGS_H
#define __CLOCK_LLA_7141REGS_H


#define CKGA_BASE_ADDRESS	       0xfe213000
#define CKGB_BASE_ADDRESS	       0xfe000000
#define CKGC_BASE_ADDRESS	       0xfe210000
#define SYSCFG_BASE_ADDRESS	     0xfe001000
#define PIO5_BASE_ADDRESS	       0xfd025000

#define PIO_BASE_ADDRESS(bank)	  0xfd025000


#define DEVICE_ID	       0x000
#define EXTRA_DEVICE_ID	 0x004
#define SYSTEM_STATUS0	  0x008
#define SYSTEM_STATUS(x)	(SYSTEM_STATUS0 + (x) * 4)

#define SYSTEM_CONFIG0	  0x100
#define SYSTEM_CONFIG(x)	(SYSTEM_CONFIG0 + (x) * 4)

/*
    INTPRI00 - Interrupt Priority Level (Read/Write)
    ==============================================================
    bit [J*4, (J*4)+3] = IP[J] with 0<=J<=7: Specifies the priority level for
					interrupts beloiging
					     to priority group J.
					     (0 => Interrupt are masked,
					      1..15 => Set priority level)

    INTREQ00 - Interrupt Request Level (Read-only)
    ============================================================
    bit J = IR[J] with 0<=J<=31: Indicates the request level of the interrupt
				associated with bit J.
				 (1 => Interrupt is being asserted,
				  0 => Interrupt is not being asserted)

    INTMSK00 - Interrupt Mask (Read/Write)
    ====================================================
    bit J = IM[J] with 0<=J<=31: Specifies wether the interrupt associated with
				bit J is masked.
				 (1 => Interrupt is masked, 0 => Ignored)


    INTMSKCLR00  - Interrupt Mask Clear (Write-only)
    =============================================================
    bit J = IMC[J] with 0<=J<=31: Specifies wether the interrupt mask
				associated with bit J is cleared.
				  (1 => Unmask the interrupt. I.e. clear the
				corrispondending bit in INTMSK00,
				   0 => Ignored)

    INTC2MODE  - INTC2 Mode (Read/Write)
    =============================================================
    bit 0 = INTC2 MODE: Specifies wether the most significant bit of the INTEVT
			code is masked out.
			(1 = The INTEVT code are AND-ed with 0x7FF before being
			passes to the CPU,
			 0 = The INTEVT code is unmodified)
    bit 31:1 = Reserved
*/

#define INTC2_PRIORITY00	0x300
#define INTC2_PRIORITY04	0x304
#define INTC2_PRIORITY08	0x308

#define INTC2_REQUEST00	 0x320
#define INTC2_REQUEST04	 0x324
#define INTC2_REQUEST08	 0x328

#define INTC2_MASK00	    0x340
#define INTC2_MASK04	    0x344
#define INTC2_MASK08	    0x348

#define INTC2_MASK_CLEAR00      0x360
#define INTC2_MASK_CLEAR04      0x364
#define INTC2_MASK_CLEAR08      0x368

#define INTC2_MODE	      0x380

/* --- CKGA registers ( hardware specific ) --------------------------------- */
#define CKGA_PLL0_CFG		 0x000
#define CKGA_PLL1_CFG		 0x004
#define CKGA_POWER_CFG		0x010
#define CKGA_CLKOPSRC_SWITCH_CFG      0x014
#define CKGA_OSC_ENABLE_FB	    0x018
#define CKGA_PLL0_ENABLE_FB	   0x01c
#define CKGA_PLL1_ENABLE_FB	   0x020
#define CKGA_CLKOPSRC_SWITCH_CFG2     0x024

#define CKGA_CLKOBS_MUX1_CFG	  0x030
#define CKGA_CLKOBS_MASTER_MAXCOUNT   0x034
#define CKGA_CLKOBS_CMD	       0x038
#define CKGA_CLKOBS_STATUS	    0x03c
#define CKGA_CLKOBS_SLAVE0_COUNT      0x040
#define CKGA_OSCMUX_DEBUG	     0x044
/*#define CKGA_CLKOBS_MUX2_CFG	  0x048*/
#define CKGA_LOW_POWER_CTRL	   0x04C

#define CKGA_OSC_DIV0_CFG	     0x800
#define CKGA_OSC_DIV_CFG(x)		(CKGA_OSC_DIV0_CFG + (x) * 4)

#define CKGA_PLL0HS_DIV0_CFG	  0x900
#define CKGA_PLL0HS_DIV_CFG(x)		(CKGA_PLL0HS_DIV0_CFG + (x) * 4)

#define CKGA_PLL0LS_DIV0_CFG	  0xA00
#define CKGA_PLL0LS_DIV_CFG(x)		(CKGA_PLL0LS_DIV0_CFG + (x) * 4)

#define CKGA_PLL1_DIV0_CFG	    0xB00
#define CKGA_PLL1_DIV_CFG(x)		(CKGA_PLL1_DIV0_CFG + (x) * 4)

/* Clockgen B registers */
#define CKGB_LOCK	       0x010
#define CKGB_FS0_CTRL	   0x014
#define CKGB_FS0_MD1	    0x018
#define CKGB_FS0_PE1	    0x01c
#define CKGB_FS0_EN_PRG1	0x020
#define CKGB_FS0_SDIV1	  0x024
#define CKGB_FS0_MD2	    0x028
#define CKGB_FS0_PE2	    0x02c
#define CKGB_FS0_EN_PRG2	0x030
#define CKGB_FS0_SDIV2	  0x034
#define CKGB_FS0_MD3	    0x038
#define CKGB_FS0_PE3	    0x03c
#define CKGB_FS0_EN_PRG3	0x040
#define CKGB_FS0_SDIV3	  0x044
#define CKGB_FS0_MD4	    0x048
#define CKGB_FS0_PE4	    0x04c
#define CKGB_FS0_EN_PRG4	0x050
#define CKGB_FS0_SDIV4	  0x054
#define CKGB_FS0_CLKOUT_CTRL    0x058
#define CKGB_FS1_CTRL	   0x05c
#define CKGB_FS1_MD1	    0x060
#define CKGB_FS1_PE1	    0x064
#define CKGB_FS1_EN_PRG1	0x068
#define CKGB_FS1_SDIV1	  0x06c
#define CKGB_FS1_MD2	    0x070
#define CKGB_FS1_PE2	    0x074
#define CKGB_FS1_EN_PRG2	0x078
#define CKGB_FS1_SDIV2	  0x07c
#define CKGB_FS1_MD3	    0x080
#define CKGB_FS1_PE3	    0x084
#define CKGB_FS1_EN_PRG3	0x088
#define CKGB_FS1_SDIV3	  0x08c
#define CKGB_FS1_MD4	    0x090
#define CKGB_FS1_PE4	    0x094
#define CKGB_FS1_EN_PRG4	0x098
#define CKGB_FS1_SDIV4	  0x09c
#define CKGB_FS1_CLKOUT_CTRL    0x0a0
#define CKGB_DISPLAY_CFG	0x0a4
#define CKGB_FS_SELECT	  0x0a8
#define CKGB_POWER_DOWN	 0x0ac
#define CKGB_POWER_ENABLE       0x0b0
#define CKGB_OUT_CTRL	   0x0b4
#define CKGB_CRISTAL_SEL	0x0b8

/* Clock recovery registers */
#define CKGB_RECOV_REF_MAX      0x000
#define CKGB_RECOV_CMD	  0x004
#define CKGB_RECOV_CPT_PCM      0x008
#define CKGB_RECOV_CPT_HD       0x00c



#define CKGC_FS0_CFG	0x000

#define CKGC_FS0_MD1	0x010
#define CKGC_FS0_PE1	0x014
#define CKGC_FS0_SDIV1      0x018
#define CKGC_FS0_EN_PRG1    0x01c

#define CKGC_FS0_MD2	0x020
#define CKGC_FS0_PE2	0x024
#define CKGC_FS0_SDIV2      0x028
#define CKGC_FS0_EN_PRG2    0x02c

#define CKGC_FS0_MD3	0x030
#define CKGC_FS0_PE3	0x034
#define CKGC_FS0_SDIV3      0x038
#define CKGC_FS0_EN_PRG3    0x03c

#define CKGC_FS0_MD4	0x040
#define CKGC_FS0_PE4	0x044
#define CKGC_FS0_SDIV4      0x048
#define CKGC_FS0_EN_PRG4    0x04c

#define DAC_CFG_ADD_SIG     0x100
#define IOCTL_CFG_ADD       0x200

#define HDMI_AUDIO_CFG 0x204



/* --- PIO registers (  ) ------------------------------- */
#define	 PIO_CLEAR_PnC0		0x28
#define	 PIO_CLEAR_PnC1		0x38
#define	 PIO_CLEAR_PnC2		0x48
#define	 PIO_PnC0		      0x20
#define	 PIO_PnC1		      0x30
#define	 PIO_PnC2		      0x40
#define	 PIO_SET_PnC0		  0x24
#define	 PIO_SET_PnC1		  0x34
#define	 PIO_SET_PnC2		  0x44

#endif  /* End __CLOCK_LLA_7141REGS_H */

