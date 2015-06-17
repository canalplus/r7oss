/*
 * st-asc.h: ST Asynchronous serial controller (ASC) driver
 *
 * Copyright (C) 2003-2013 STMicroelectronics (R&D) Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _STASC_H
#define _STASC_H



/*---- UART Register definitions ------------------------------*/

/* Register offsets */

#define ASC_BAUDRATE			0x00
#define ASC_TXBUF			0x04
#define ASC_RXBUF			0x08
#define ASC_CTL				0x0C
#define ASC_INTEN			0x10
#define ASC_STA				0x14
#define ASC_GUARDTIME			0x18
#define ASC_TIMEOUT			0x1C
#define ASC_TXRESET			0x20
#define ASC_RXRESET			0x24
#define ASC_RETRIES			0x28

/* ASC_RXBUF */
#define ASC_RXBUF_PE			0x100
#define ASC_RXBUF_FE			0x200
/**
 * Some of status comes from higher bits of the character and some come from
 * the status register. Combining both of them in to single status using dummy
 * bits.
 */
#define ASC_RXBUF_DUMMY_RX		0x10000
#define ASC_RXBUF_DUMMY_BE		0x20000
#define ASC_RXBUF_DUMMY_OE		0x40000

/* ASC_CTL */

#define ASC_CTL_MODE_MSK		0x0007
#define  ASC_CTL_MODE_8BIT		0x0001
#define  ASC_CTL_MODE_7BIT_PAR		0x0003
#define  ASC_CTL_MODE_9BIT		0x0004
#define  ASC_CTL_MODE_8BIT_WKUP		0x0005
#define  ASC_CTL_MODE_8BIT_PAR		0x0007
#define ASC_CTL_STOP_MSK		0x0018
#define  ASC_CTL_STOP_HALFBIT		0x0000
#define  ASC_CTL_STOP_1BIT		0x0008
#define  ASC_CTL_STOP_1_HALFBIT		0x0010
#define  ASC_CTL_STOP_2BIT		0x0018
#define ASC_CTL_PARITYODD		0x0020
#define ASC_CTL_LOOPBACK		0x0040
#define ASC_CTL_RUN			0x0080
#define ASC_CTL_RXENABLE		0x0100
#define ASC_CTL_SCENABLE		0x0200
#define ASC_CTL_FIFOENABLE		0x0400
#define ASC_CTL_CTSENABLE		0x0800
#define ASC_CTL_BAUDMODE		0x1000

/* ASC_GUARDTIME */

#define ASC_GUARDTIME_MSK		0x00FF

/* ASC_INTEN */

#define ASC_INTEN_RBE			0x0001
#define ASC_INTEN_TE			0x0002
#define ASC_INTEN_THE			0x0004
#define ASC_INTEN_PE			0x0008
#define ASC_INTEN_FE			0x0010
#define ASC_INTEN_OE			0x0020
#define ASC_INTEN_TNE			0x0040
#define ASC_INTEN_TOI			0x0080
#define ASC_INTEN_RHF			0x0100

/* ASC_RETRIES */

#define ASC_RETRIES_MSK			0x00FF

/* ASC_RXBUF */

#define ASC_RXBUF_DATA_MSK		0x00FF
#define ASC_RXBUF_MSK			0x03FF

/* ASC_STA */

#define ASC_STA_RBF			0x0001
#define ASC_STA_TE			0x0002
#define ASC_STA_THE			0x0004
#define ASC_STA_PE			0x0008
#define ASC_STA_FE			0x0010
#define ASC_STA_OE			0x0020
#define ASC_STA_TNE			0x0040
#define ASC_STA_TOI			0x0080
#define ASC_STA_RHF			0x0100
#define ASC_STA_TF			0x0200
#define ASC_STA_NKD			0x0400

/* ASC_TIMEOUT */

#define ASC_TIMEOUT_MSK			0x00FF

/* ASC_TXBUF */

#define ASC_TXBUF_MSK			0x01FF

#endif /* _STASC_H */
