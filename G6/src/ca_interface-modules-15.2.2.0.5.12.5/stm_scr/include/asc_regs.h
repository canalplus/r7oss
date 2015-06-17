/*
 *  drivers/char/stasc.h
 *
 *  ST40 Asynchronous serial controller (ASC) driver
 *  Derived from sh-sci.h
 *  Copyright (c) STMicroelectronics Limited
 *  Author: Andrea Cisternino (March 2003)
 *
 *  Documentation for the Asynchronous Serial Controller in the STm8000 SoC
 *  can be found in the following documents:
 *
 *    1) DVD Platform Architecture Volume 4: I/O Devices (ADCS: 7402381)
 *    2) STm8000 Datasheet (ADCS: 7323276)
 */
/* Exported Types --------------------------------------------------------- */
/*****************************************************************************/
#ifndef __ASCREGS_H
#define __ASCREGS_H

#include "ca_os_wrapper.h"



/* ASC register layout.  Each register is 32-bits wide */
typedef struct {
	uint32_t    asc_baudrate;
	uint32_t    asc_txbuffer;
	uint32_t    asc_rxbuffer;
	uint32_t    asc_control;
	uint32_t    asc_intenable;
	uint32_t    asc_status;
	uint32_t    asc_guardtime;
	uint32_t    asc_timeout;
	uint32_t    asc_txreset;
	uint32_t    asc_rxreset;
	uint32_t    asc_retries;
	uint32_t    asc_threshold;
} asc_regs_t;

/* Exported Constants ----------------------------------------------------- */
#define ASC_MAX_PORTS		2

/* asc_control register */

/* -- 2:0 Mode
000     RESERVED
001     8-bit data
010     RESERVED
011     7-bit data
100     9-bit data
101     8-bit data + wake up bit
110     RESERVED
111     8-bit data + parity
*/
#define CONTROL_MODE            0x7     /* Bitmask for mode bits */
#define CONTROL_8BITSPARITY_BUTLAST 0x0 /* 8 data bits + parity + but last(last byte will have guart time of 2 ETU)*/
#define CONTROL_8BITS           0x1     /* 8 data bits */
#define CONTROL_7BITSPARITY     0x3     /* 7 data bits + parity */
#define CONTROL_9BITS           0x4     /* 9 data bits */
#define CONTROL_8BITSWAKEUP     0x5     /* 8 data bits + wakeup bit */
#define CONTROL_8BITSPARITY     0x7     /* 8 data bits + parity */

/* -- 4:3 StopBits
00      0.5 stop bits
01      1 stop bit
10      1.5 stop bits
11      2 stop bits
*/
#define CONTROL_STOPBITS        0x18    /* Bitmask for stopbits bits */
#define CONTROL_1_0_STOPBITS    0x8     /* 1 stop bit */
#define CONTROL_1_5_STOPBITS    0x10    /* 1.5 stop bits */
#define CONTROL_2_0_STOPBITS    0x18    /* 2 stop bits */

/* -- 5 ParityOdd
0       even parity
1       odd parity
*/
#define CONTROL_PARITY          0x20

/* -- 6 LoopBack
0       standard tx/rx
1       loop back enabled
*/

#define CONTROL_LOOPBACK        0x40

/* -- 7 Run
0       baudrate generator disabled
1       baudrate generator enabled
*/

#define CONTROL_RUN             0x80

/* -- 8 RxEnable
0       receiver disabled
1       receiver enabled
*/

#define CONTROL_RXENABLE        0x100

/* -- 9 SCEnable
0       smartcard mode disabled
1       smartcard mode enabled
*/

#define CONTROL_SMARTCARD       0x200

/* -- 10 FifoEnable
0       FIFO disabled
1       FIFO enabled
*/

#define CONTROL_FIFO            0x400

/* -- 11 CtsEnable
0       CTS ignored
1       CTS enabled
*/

#define CONTROL_CTSENABLE       0x800

/* -- 12 BaudMode
0       Baud counter decrements, tick when it reaches 0.
1       Baud counter added to itself, ticks on carry.
*/

#define CONTROL_BAUDMODE        0x1000

/* -- 13 NACKDisable
0       Nacking behaviour enabled (NACK signal generated)
1       Nacking behviour is disabled (No NACK signal)
*/

#define CONTROL_NACKDISABLE     0x2000

/* -- 14 AUTO_PARITY_REJECTION  -- Available from IP v2.0
0       Words with parity error will be stored into FIFO
1       Words with parity error will not be stored into FIFO
*/
#define CONTROL_PARITYREJECT    0x4000

/* -- 15 FLOW_CONTROL_DISABLE  -- Available from IP v2.1
0       Flow Control is Enabled
1       Flow Control is Disabled
*/
#define CONTROL_FLOWCRTLDISABLE 0x8000


/* ASCnStatus register */
#define STATUS_RXBUFFULL        0x1
#define STATUS_TXEMPTY          0x2
#define STATUS_TXHALFEMPTY      0x4
#define STATUS_PARITYERROR      0x8
#define STATUS_FRAMEERROR       0x10
#define STATUS_OVERRUNERROR     0x20
#define STATUS_TIMEOUTNOTEMPTY  0x40
#define STATUS_TIMEOUTIDLE      0x80
#define STATUS_RXHALFFULL       0x100
#define STATUS_TXFULL           0x200
#define STATUS_NACKED           0x400
#define STATUS_THRESHOLD        0x800       /* Available from UART Ip v2.0  */
#define STATUS_RESIDUALBYTES    0x3FF000    /* Available from UART Ip v2.0  */

/* Definition for all ASC errors */
#define STATUS_ASC_ERRORS     (STATUS_NACKED|STATUS_OVERRUNERROR|\
				STATUS_PARITYERROR|STATUS_FRAMEERROR)

/* RX BYTE AVAILABILIY CHECK */
#define STATUS_RX_THR_AVAILABLE (STATUS_RXBUFFULL|STATUS_RXHALFFULL|\
				STATUS_TIMEOUTNOTEMPTY|STATUS_THRESHOLD)
#define STATUS_RX_AVAILABLE   (STATUS_RXBUFFULL|STATUS_RXHALFFULL|\
				STATUS_TIMEOUTNOTEMPTY)

/* asc_intenable register */
#define IE_RXBUFFULL            0x1
#define IE_TXEMPTY              0x2
#define IE_TXHALFEMPTY          0x4
#define IE_PARITYERROR          0x8
#define IE_FRAMEERROR           0x10
#define IE_OVERRUNERROR         0x20
#define IE_TIMEOUTNOTEMPTY      0x40
#define IE_TIMEOUTIDLE          0x80
#define IE_RXHALFFULL           0x100
#define IE_THRESHOLD            0x200   /* Available from UART Ip v2.0  */


#define IE_RX_INTERRUPT	   (IE_RXHALFFULL|IE_TIMEOUTNOTEMPTY)
#define IE_TX_INTERRUPT	   (IE_TXEMPTY | IE_TXHALFEMPTY)


/* Exported Variables ----------------------------------------------------- */
/* Exported Macros -------------------------------------------------------- */

#define ASC_CLEARMODE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_MODE)), (&(base->asc_control)))
#define ASC_8BITS(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_MODE)), (&(base->asc_control)));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_8BITS)), (&(base->asc_control)));\
	} while (0);
#define ASC_7BITS_PARITY(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_MODE)), (&(base->asc_control)));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_7BITSPARITY)),\
			&(base->asc_control));\
	} while (0);

#define ASC_9BITS(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_MODE)), (&(base->asc_control)));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_9BITS)), (&(base->asc_control)));\
	} while (0);

#define ASC_8BITS_WAKEUP(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_MODE)), (&(base->asc_control)));\
		writel((readl((void *)(&(base->asc_control)))|\
		((uint32_t)CONTROL_8BITSWAKEUP)), (&(base->asc_control)));\
	} while (0);

#define ASC_8BITS_PARITY(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_MODE)), &(base->asc_control));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_8BITSPARITY_BUTLAST)),\
			&(base->asc_control));\
	} while (0);

/*changing the mode from CONTROL_8BITSPARITY to CONTROL_8BITSPARITY_BUTLAST*/
/*In this mode guard time of last byte will be set to 2*/

/* Macros for setting stop bits */
#define ASC_0_5_STOPBITS(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_STOPBITS)), (&(base->asc_control)))
#define ASC_1_0_STOPBITS(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_STOPBITS)), &(base->asc_control));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_1_0_STOPBITS)),\
			&(base->asc_control));\
	} while (0);
#define ASC_1_5_STOPBITS(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_STOPBITS)), &(base->asc_control));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_1_5_STOPBITS)),\
			&(base->asc_control));\
	} while (0);

#define ASC_2_0_STOPBITS(base)\
	do {\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_STOPBITS)),\
			(void *)(&(base->asc_control)));\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_2_0_STOPBITS)),\
			(void *)(&(base->asc_control)));\
	} while (0);

/* Macros for setting parity */
#define ASC_PARITY_ODD(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_PARITY)), (&(base->asc_control)))
#define ASC_PARITY_EVEN(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_PARITY)), (&(base->asc_control)))


/* Macros for setting loopback mode */
#define ASC_LOOPBACK_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_LOOPBACK)), (&(base->asc_control)))
#define ASC_LOOPBACK_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_LOOPBACK)), (&(base->asc_control)))
/* Macros for setting run mode */
#define ASC_RUN_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_RUN)), (&(base->asc_control)))
#define ASC_RUN_DISABLE(base) \
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_RUN)), (&(base->asc_control)))

/* Macros for setting receiver enable */
#define ASC_RECEIVER_ENABLE(base) \
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_RXENABLE)), (&(base->asc_control)))
#define ASC_RECEIVER_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_RXENABLE)), (&(base->asc_control)))

/* Macros for setting smartcard mode */
#define ASC_SMARTCARD_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_SMARTCARD)), (&(base->asc_control)))
#define ASC_SMARTCARD_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_SMARTCARD)), (&(base->asc_control)))

/* Macros for setting FIFO enable */
#define ASC_FIFO_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_FIFO)), (&(base->asc_control)))
#define ASC_FIFO_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_FIFO)), (&(base->asc_control)))

/* Macros for setting CTS enable */
#define ASC_CTS_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_CTSENABLE)), (&(base->asc_control)))
#define ASC_CTS_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_CTSENABLE)), (&(base->asc_control)))

/* Macros for setting BaudMode */
#define ASC_BAUD_TICKONZERO(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_BAUDMODE)), (&(base->asc_control)))
#define ASC_BAUD_TICKONCARRY(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			((uint32_t)CONTROL_BAUDMODE)), (&(base->asc_control)))

/* FIFO reset */
#define ASC_RESET_TXFIFO(base)  writel(0xFFFFFFFF, (&(base->asc_txreset)))
#define ASC_RESET_RXFIFO(base)  writel(0xFFFFFFFF, (&(base->asc_rxreset)))

/* Macros for setting retries */
#define ASC_SET_RETRIES(base, v)    writel((v), (&(base->asc_retries)))
#define ASC_GET_RETRIES(base)   (readl((void *)(&(base->asc_retries))))

/* Macros for enabling/disabling interrupts */
#define ASC_ENABLE_INTS(base, v)\
		writel((readl((void *)(&(base->asc_intenable)))|(v)),\
			(&(base->asc_intenable)))
#define ASC_DISABLE_INTS(base, v)\
		writel((readl((void *)(&(base->asc_intenable)))&\
			(~(uint32_t)(v))), (&(base->asc_intenable)))
#define ASC_GLOBAL_DISABLEINTS(base)    writel(0, (&(base->asc_intenable)))

/* Macro for checking TxEmpty status */
#define ASC_ISTXEMPTY(base)\
		(((readl((void *)(&(base->asc_status)))&STATUS_TXEMPTY) != 0))

/* Macro for checking RxFull status */
#define ASC_ISRXFULL(base)\
	(((readl((void *)(&(base->asc_status)))&STATUS_RXBUFFULL) != 0))

/* Macro for setting guardtime */
#define ASC_SETGUARDTIME(base, v)  writel((v), (&(base->asc_guardtime)))

/* Macro for setting timeout */
#define ASC_SET_TIMEOUT(base, v)  writel((v), (&(base->asc_timeout)))

/* Macro for setting NACKDisable */
#define ASC_NACK_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			(uint32_t)CONTROL_NACKDISABLE), (&(base->asc_control)))
#define ASC_NACK_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_NACKDISABLE)),\
			(&(base->asc_control)))

/* Macro for Reading ASC Status Register*/
#define ASC_STATUS(base)  readl((void *)(&(base->asc_status)))

/* Macro to Read ASC BaudRate Register*/
#define ASC_READ_BAUDRATE(base)  readl((void *)(&(base->asc_baudrate)))

/* Macro to write ASC Baudrate Register*/
#define ASC_SET_BAUDRATE(base, v)  writel((v), (&(base->asc_baudrate)))

/* Macro To read ASC Tx Buffer*/
#define ASC_READ_TXBUF(base)  readl((void *)(&(base->asc_txbuffer)))

/* Macro to Write to Tx Buffer*/
#define ASC_WRITE_TXBUF(base, v)  writel(v, (void *)(&(base->asc_txbuffer)))

/* Macro to Read RX Buffer*/
#define ASC_READ_RXBUF(base)  readl((void *)(&(base->asc_rxbuffer)))

/* Macro to read Control register*/
#define ASC_READ_CONTROL(base)  readl((void *)(&(base->asc_control)))

/* Macro to Set Control Register */
#define ASC_SET_CONTROL(base, v)  writel((v), (void *)(&(base->asc_control)))

/* Macro to Read Interrupt Enable Register*/
#define ASC_READ_INTENABLE(base)  readl((void *)(&(base->asc_intenable)))

/* Macro to read Guard Time Register*/
#define ASC_READ_GUARDTIME(base)  readl((void *)(&(base->asc_guardtime)))

/* Macro to read Timeout Register */
#define ASC_READ_TIMEOUT(base)  readl((void *)(&(base->asc_timeout)))

/* ASC IP Version 2.0 Specific */
#define ASC_READ_THRESHOLD(base)\
		(readl((void *)(&(base->asc_threshold)))&0x1FF)

/* Macro to Read Residual Bytes present in RX FIFO */
#define ASC_GET_RESIDUALBYTE(base)\
		(readl((void *)(&(base->asc_status)))&STATUS_RESIDUALBYTES)

/* Macro to Set Threshold Register */
#define ASC_SET_THRESHOLD(base, v)\
	writel((v), (void *)(&(base->asc_threshold)))

/* Macro to Control Auto parity Rejection */
#define ASC_PARITYREJECT_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			(uint32_t)CONTROL_PARITYREJECT), (&(base->asc_control)))
#define ASC_PARITYREJECT_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_PARITYREJECT)),\
			(&(base->asc_control)))

/* Macro to Control Auto parity Rejection */
#define ASC_SCFLOWCTRL_DISABLE(base)\
		writel((readl((void *)(&(base->asc_control)))|\
			(uint32_t)CONTROL_FLOWCRTLDISABLE),\
			(&(base->asc_control)))
#define ASC_SCFLOWCTRL_ENABLE(base)\
		writel((readl((void *)(&(base->asc_control)))&\
			(~(uint32_t)CONTROL_FLOWCRTLDISABLE)),\
			(&(base->asc_control)))

#endif /*__ASCREGS_H */
/* End of ascregs.h */
/* Macros for setting the control mode */
