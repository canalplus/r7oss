/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : lnbh26.h
Author :           LLA

API dedinitions for LNB device

Date        Modification                                    Name
----        ------------                                --------
18-Apr-12   Created                                         Rahul

************************************************************************/
#ifndef _LNBH26_H
#define _LNBH26_H

/* constants --------------------------------------------------------------- */

/* map lnb register controls of lnb21 */
#define LNBH26_NBREGS   6
#define LNBH26_NBFIELDS 38

/* STATUS 1 */
#define  RLNBH26_STATUS1         0x0000
#define  FLNBH26_OLF_A           0x00000001
#define  FLNBH26_OLF_B           0x00000002
#define  FLNBH26_VMON_A          0x00000004
#define  FLNBH26_VMON_B          0x00000008
#define  FLNBH26_PDO_A           0x00000010
#define  FLNBH26_PDO_B           0x00000020
#define  FLNBH26_OTF             0x00000040
#define  FLNBH26_PNG             0x00000080

/* STATUS 2 */
#define  RLNBH26_STATUS2         0x0001
#define  FLNBH26_TDET_A          0x00010001
#define  FLNBH26_TDET_B          0x00010002
#define  FLNBH26_TMON_A          0x00010004
#define  FLNBH26_TMON_B          0x00010008
#define  FLNBH26_IMON_A          0x00010010
#define  FLNBH26_IMON_B          0x00010020
/*
#define  FLNBH26_RESERVE                0x00000040
#define  FLNBH26_RESERVE                0x00000080
*/
/* DATA 1 */
#define  RLNBH26_DATA1           0x0002			/* Voltage Select  */
#define  FLNBH26_VSEL1_A         0x00020001
#define  FLNBH26_VSEL2_A         0x00020002
#define  FLNBH26_VSEL3_A         0x00020004
#define  FLNBH26_VSEL4_A         0x00020008
#define  FLNBH26_VSEL1_B         0x00020010
#define  FLNBH26_VSEL2_B         0x00020020
#define  FLNBH26_VSEL3_B         0x00020040
#define  FLNBH26_VSEL4_B         0x00020080

/* DATA 2 */
#define  RLNBH26_DATA2           0x0003
#define  FLNBH26_TEN_A           0x00030001	/* Tone Enable/Disable */
/* Low Power Mode Activate/Di-actvated */
#define  FLNBH26_LPM_A           0x00030002
#define  FLNBH26_EXTM_A          0x00030004
/*
#define  FLNBH26_RESERVE                0x00030008
*/
#define  FLNBH26_TEN_B           0x00030010	/* Tone Enable/Disable */
/* Low Power Mode Activate/Di-actvated */
#define  FLNBH26_LPM_B           0x00030020
#define  FLNBH26_EXTM_B          0x00030040
/*
#define  FLNBH26_RESERVE                 0x00030080
*/
/* DATA 3 */
#define  RLNBH26_DATA3           0x0004
#define  FLNBH26_ISET_A          0x00040001	/*Current Limit of LNB output*/
#define  FLNBH26_ISW_A           0x00040002
#define  FLNBH26_PCL_A           0x00040004
#define  FLNBH26_TIMER_A         0x00040008
#define  FLNBH26_ISET_B          0x00040010	/*Current Limit of LNB output*/
#define  FLNBH26_ISW_B           0x00040020
#define  FLNBH26_PCL_B           0x00040040
#define  FLNBH26_TIMER_B         0x00040080

/* DATA 3 */
#define  RLNBH26_DATA4           0x0005
/*
#define  FLNBH26_RESERVE                0x00050001
#define  FLNBH26_RESERVE                0x00050002
#define  FLNBH26_RESERVE                0x00050004
#define  FLNBH26_RESERVE                0x00050008
#define  FLNBH26_RESERVE                0x00050010
#define  FLNBH26_RESERVE                0x00050020
*/
#define  FLNBH26_THERM           0x00050040
#define  FLNBH26_COMP            0x00050080


#define FLNBH26_VSEL1(path) (path == 1 ? FLNBH26_VSEL1_A : FLNBH26_VSEL1_B)
#define FLNBH26_VSEL2(path) (path == 1 ? FLNBH26_VSEL2_A : FLNBH26_VSEL2_B)
#define FLNBH26_VSEL3(path) (path == 1 ? FLNBH26_VSEL3_A : FLNBH26_VSEL3_B)
#define FLNBH26_VSEL4(path) (path == 1 ? FLNBH26_VSEL4_A : FLNBH26_VSEL4_B)
#define FLNBH26_TEN(path) (path == 1 ? FLNBH26_TEN_A : FLNBH26_TEN_B)
#define FLNBH26_LPM(path) (path == 1 ? FLNBH26_LPM_A : FLNBH26_LPM_B)
#define FLNBH26_OLF(path) (path == 1 ? FLNBH26_OLF_A : FLNBH26_OLF_B)

int stm_fe_lnbh26_attach(struct stm_fe_lnb_s *priv);
int stm_fe_lnbh26_detach(struct stm_fe_lnb_s *priv);

#endif /* _LNBH26_H */
