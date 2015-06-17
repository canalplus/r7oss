/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : lnbh24.h
Author :           LLA

API dedinitions for LNB device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _LNBH24_H
#define _LNBH24_H

/* constants --------------------------------------------------------------- */

/* map lnb register controls of lnb21 */
#define LNBH24_NBREGS   1
#define LNBH24_NBFIELDS 8

/* IOCFG register */
#define  RLNBH24_REGS         0x00

/* IOCFG fields */
/* Over current protection Indicator */
#define  FLNBH24_OLF        0x01
/* To force 22V on the bus for minimum output current diagnostic */
#define  FLNBH24_AUX        0x01
/* Over temperature protection Indicator */
#define  FLNBH24_OTF        0x02
/* To adjust minimum output current diagnostic threshold current 6mA/12mA */
#define  FLNBH24_ITEST        0x02

#define  FLNBH24_EN          0x04	/* Port Enable */
#define  FLNBH24_VSEL      0x08	/* Voltage Select 13/18V */
#define  FLNBH24_LLC        0x10	/* Coax Cable Loss Compensation */

#define  FLNBH24_TEN        0x20	/* Tone Enable/Disable */
#define  FLNBH24_TTX        0x40	/*  LNB mode selection (Rx or Tx) */
/* Short Circuit Protection, Dynamic/Static Mode */
#define  FLNBH24_PCL        0x80

int stm_fe_lnbh24_attach(struct stm_fe_lnb_s *priv);
int stm_fe_lnbh24_detach(struct stm_fe_lnb_s *priv);

#endif /* _LNBH24_H */
