/* -------------------------------------------------------------------
 * Project component : Streamer                                      *
 * -------------------------------------------------------------------
 * COPYRIGHT :                                                       *
 *   (C) STMicroelectronics 2001, 2002 ALL RIGHTS RESERVED           *
 *   This program contains proprietary information and               *
 *   is not to be used, copied, nor disclosed without                *
 *   the written consent of ST Microelectronics.                     *
 * -------------------------------------------------------------------
*/

#ifndef _STRREGMAPPING_H_
#define _STRREGMAPPING_H_

/*  base register number of all stbusRD channels */
#define STR_BRDBASENB			0	/*  0x0000 >> 2 */

/*  base register number of  STBUSRD channel i */
#define STR_BRDBASECHNB(i)		(STR_BRDBASENB + i * 8)

/*  base register number of all stbusWR channels */
#define STR_BWRBASENB			512  /*  0x0800 >> 2 */

/*  base register number of STBUSWR channel i */
#define STR_BWRBASECHNB(i)		(STR_BWRBASENB + i * 8)

/*  base register number of all QRD channels */
#define STR_QRDBASENB			1024 /*  0x1000 >> 2 */

/*  base register number of the physical QRD pCh, logical channel lCh */
#define STR_QRDBASECHNB(pCh,lCh)	(STR_QRDBASENB + pCh * 64 + lCh * 8)

#define STR_QRIA(pCh,lCh)    (STR_QRDBASECHNB(pCh,lCh)+0)
#define STR_QRRA(pCh,lCh)    (STR_QRDBASECHNB(pCh,lCh)+1)
#define STR_QRPM(pCh,lCh)    (STR_QRDBASECHNB(pCh,lCh)+2)

/*  base register number of all QWR channels */
#define STR_QWRBASENB			2048 /*  0x2000 >> 2 */

#define STR_QWIA(pCh,lCh)    (STR_QWRBASECHNB(pCh,lCh)+0)
#define STR_QWRA(pCh,lCh)    (STR_QWRBASECHNB(pCh,lCh)+1)
#define STR_QWPM(pCh,lCh)    (STR_QWRBASECHNB(pCh,lCh)+2)

/*  base register number of the physical QWR pCh, logical channel lCh */
#define STR_QWRBASECHNB(pCh,lCh)	(STR_QWRBASENB + pCh * 64 + lCh * 8)

#define STR_ITBASENB		4096 /*  0x4000 >> 2 */
#define STR_BIM(s)			(STR_ITBASENB + 2 * s)
#define STR_QIM(s)			(STR_ITBASENB + 2 * s + 1)

/*  Interrupt Source Enable */
#define STR_ISE				4352         /*  0x4400 >> 2 */

/*  Shadow registers mapping for ISE */
#define STR_SHISE			4353         /*  0x4404 >> 2 */

/*  STBUS synchronization Flag register number */
#define STR_BSF				5120         /*  0x5000 >> 2 */

#define STR_RQSF(i)			(5152 + i)   /*  0x5080 >> 2 */
#define STR_RQSF0			STR_RQSF(0)
#define STR_RQSF1			STR_RQSF(1)
#define STR_RQSF2			STR_RQSF(2)
#define STR_RQSF3			STR_RQSF(3)
#define STR_WQSF(i)			(5156 + i)   /*  0x5090 >> 2 */
#define STR_WQSF0			STR_WQSF(0)
#define STR_WQSF1			STR_WQSF(1)
#define STR_WQSF2			STR_WQSF(2)
#define STR_WQSF3			STR_WQSF(3)

#define STR_RQID(i)			(5160 + i)   /*  0x50A0 >> 2 */
#define STR_RQID0			STR_RQID(0)
#define STR_RQID1			STR_RQID(1)
#define STR_RQID2			STR_RQID(2)
#define STR_RQID3			STR_RQID(3)
#define STR_WQID(i)			(5164 + i)   /*  0x50B0 >> 2 */
#define STR_WQID0			STR_WQID(0)
#define STR_WQID1			STR_WQID(1)
#define STR_WQID2			STR_WQID(2)
#define STR_WQID3			STR_WQID(3)

/*  Shadow registers mapping for QSF, QID */
#define STR_SHBSF			5184         /*  0x5100 >> 2 */

#define STR_SHRQSF(i)		(5216 + i)   /*  0x5180 >> 2 */
#define STR_SHRQSF0			STR_SHRQSF(0)
#define STR_SHRQSF1			STR_SHRQSF(1)
#define STR_SHRQSF2			STR_SHRQSF(2)
#define STR_SHRQSF3			STR_SHRQSF(3)
#define STR_SHWQSF(i)		(5220 + i)   /*  0x5190 >> 2 */
#define STR_SHWQSF0			STR_SHWQSF(0)
#define STR_SHWQSF1			STR_SHWQSF(1)
#define STR_SHWQSF2			STR_SHWQSF(2)
#define STR_SHWQSF3			STR_SHWQSF(3)

#define STR_SHRQID(i)		(5224 + i)   /*  0x51A0 >> 2 */
#define STR_SHRQID0			STR_SHRQID(0)
#define STR_SHRQID1			STR_SHRQID(1)
#define STR_SHRQID2			STR_SHRQID(2)
#define STR_SHRQID3			STR_SHRQID(3)
#define STR_SHWQID(i)		(5228 + i)   /*  0x51B0 >> 2 */
#define STR_SHWQID0			STR_SHWQID(0)
#define STR_SHWQID1			STR_SHWQID(1)
#define STR_SHWQID2			STR_SHWQID(2)
#define STR_SHWQID3			STR_SHWQID(3)
/*  End of Shadow registers mapping */

/*  Aliasing registers */
#define STR_AQSF(i)			(5376 + i)   /*  0x5400 >> 2 */
#define STR_AQSF0			STR_AQSF(0)
#define STR_AQSF1			STR_AQSF(1)
#define STR_AQSF2			STR_AQSF(2)
#define STR_AQSF3			STR_AQSF(3)
#define STR_AQSF4			STR_AQSF(4)
#define STR_AQSF5			STR_AQSF(5)
#define STR_AQSF6			STR_AQSF(6)
#define STR_AQSF7			STR_AQSF(7)
#define STR_AQID(i)			(5384 + i)   /*  0x5420 >> 2 */
#define STR_AQID0			STR_AQID(0)
#define STR_AQID1			STR_AQID(1)
#define STR_AQID2			STR_AQID(2)
#define STR_AQID3			STR_AQID(3)
#define STR_AQID4			STR_AQID(4)
#define STR_AQID5			STR_AQID(5)
#define STR_AQID6			STR_AQID(6)
#define STR_AQID7			STR_AQID(7)
/*  End of Aliasing registers mapping */

#define STR_CRQSF(i)		(5408 + i)
#define STR_CRQSF0			STR_CRQSF(0)
#define STR_CRQSF1			STR_CRQSF(1)
#define STR_CRQSF2			STR_CRQSF(2)
#define STR_CRQSF3			STR_CRQSF(3)
#define STR_CWQSF(i)		(5412 + i)
#define STR_CWQSF0			STR_CWQSF(0)
#define STR_CWQSF1			STR_CWQSF(1)
#define STR_CWQSF2			STR_CWQSF(2)
#define STR_CWQSF3			STR_CWQSF(3)

/*  Aliasing on Shadow registers */
#define STR_ASHQSF(i)		(5440 + i)     /*  0x5500 >> 2 */
#define STR_ASHQSF0			STR_ASHQSF(0)
#define STR_ASHQSF1			STR_ASHQSF(1)
#define STR_ASHQSF2			STR_ASHQSF(2)
#define STR_ASHQSF3			STR_ASHQSF(3)
#define STR_ASHQSF4			STR_ASHQSF(4)
#define STR_ASHQSF5			STR_ASHQSF(5)
#define STR_ASHQSF6			STR_ASHQSF(6)
#define STR_ASHQSF7			STR_ASHQSF(7)
#define STR_ASHQID(i)		(5448 + i)     /*  0x5520 >> 2 */
#define STR_ASHQID0			STR_ASHQID(0)
#define STR_ASHQID1			STR_ASHQID(1)
#define STR_ASHQID2			STR_ASHQID(2)
#define STR_ASHQID3			STR_ASHQID(3)
#define STR_ASHQID4			STR_ASHQID(4)
#define STR_ASHQID5			STR_ASHQID(5)
#define STR_ASHQID6			STR_ASHQID(6)
#define STR_ASHQID7			STR_ASHQID(7)
/*  End of Aliasing on Shadow registers mapping */

/*  Priority register status register */
#define STR_PRI				5632

/*  MFU Configuration register */
#define STR_MC				5633

/*  STBUS half-channel status register */
#define STR_BHCS			5634

/*  STBUS Wr Configuration register */
#define STR_BWC				5635

/*  Customization */
#define STR_CG(i)			(5636 + i)
#define STR_CG0				STR_CG(0)
#define STR_CG3				STR_CG(3)
#define STR_CG4				STR_CG(4)
#define STR_CG5				STR_CG(5)
#define STR_CG6				STR_CG(6)

/*  Streamer Identification */
#define STR_SID				5644

/*  Customization (ct'd) */
#define STR_CG7				5648
#define STR_CG8				5649
#define STR_CG9				5650
#define STR_CG10			5651

/*  Shadow copy */
#define STR_SHCPY			5696         /*  0x5900 >> 2 */

/*  Register count */
#define STR_REGCOUNT		(STR_SHCPY + 1)

/*  Compute absolute address of a register */
#define STR_ABS(BASE, REG) (BASE + (REG << 2))

/*  Register bitfields description */
/*  Queues */
/*************************************************************************
* 31 .. 28 | 27 .. 24  |  23   | 22 ..0 |
* ---------------------------------------
*  INTRC   |  INTDC    | INTIM | INTSA  |
*************************************************************************/ 
#define STR_Q_INTSA_BIT  0
#define STR_Q_INTSA_SIZE 23
#define STR_Q_INTSA_MASK ((1<<STR_Q_INTSA_SIZE+1)-1)
#define STR_Q_INTSA(v)   ((v & STR_Q_INTSA_MASK) << STR_Q_INTSA_BIT)

#define STR_Q_INTIM_BIT  (STR_Q_INTSA_BIT + STR_Q_INTSA_SIZE)
#define STR_Q_INTIM_SIZE 1
#define STR_Q_INTIM_MASK ((1<<STR_Q_INTIM_SIZE+1)-1)
#define STR_Q_INTIM(v)   ((v & STR_Q_INTIM_MASK) << STR_Q_INTIM_BIT)

#define STR_Q_INTDC_BIT  (STR_Q_INTIM_BIT + STR_Q_INTIM_SIZE)
#define STR_Q_INTDC_SIZE 4
#define STR_Q_INTDC_MASK ((1<<STR_Q_INTDC_SIZE+1)-1)
#define STR_Q_INTDC(v)   ((v & STR_Q_INTDC_MASK) << STR_Q_INTDC_BIT)

#define STR_Q_INTRC_BIT  (STR_Q_INTDC_BIT + STR_Q_INTDC_SIZE)
#define STR_Q_INTRC_SIZE 4
#define STR_Q_INTRC_MASK ((1<<STR_Q_INTRC_SIZE+1)-1)
#define STR_Q_INTRC(v)   ((v & STR_Q_INTRC_MASK) << STR_Q_INTRC_BIT)

#define STR_Q_IA(INTRC, INTDC, INTIM, INTSA) \
	(STR_Q_INTRC(INTRC)|STR_Q_INTDC(INTDC)|STR_Q_INTIM(INTIM)|STR_Q_INTSA(INTSA))

/*************************************************************************
* 31 | 30 .. 23 | 22 .. 0 |
* -------------------------
* DT |  FLAGS   |  INTRA  | 
*************************************************************************/ 
#define STR_Q_INTRA_BIT  0
#define STR_Q_INTRA_SIZE 23
#define STR_Q_INTRA_MASK ((1UL<<STR_Q_INTRA_SIZE+1)-1)
#define STR_Q_INTRA(v)   ((v & STR_Q_INTRA_MASK) << STR_Q_INTRA_BIT)

#define STR_Q_FLAG_BIT  (STR_Q_INTRA_BIT+STR_Q_INTRA_SIZE)
#define STR_Q_FLAG_SIZE 8
#define STR_Q_FLAG_MASK ((1<<STR_Q_FLAG_SIZE+1)-1)
#define STR_Q_FLAG(v)   ((v & STR_Q_FLAG_MASK) << STR_Q_FLAG_BIT)

#define STR_Q_DT_BIT  (STR_Q_FLAG_BIT+STR_Q_FLAG_SIZE)
#define STR_Q_DT_SIZE 1
#define STR_Q_DT_MASK ((1<<STR_Q_DT_SIZE+1)-1)
#define STR_Q_DT(v)   ((v & STR_Q_DT_MASK) << STR_Q_DT_BIT)

#define STR_Q_RA(DT, FLAG, INTRA) \
	(STR_Q_DT(DT)|STR_Q_FLAG(FLAG)|STR_Q_INTRA(INTRA))

/*************************************************************************
* 31 | 30 .. 28 | 27 .. 14 | 13 .. 0 |
* ------------------------------------
*    |   QTW    |  INTAI   |   QSZ   | 
*************************************************************************/ 
#define STR_Q_QSZ_BIT  0
#define STR_Q_QSZ_SIZE 14
#define STR_Q_QSZ_MASK ((1UL<<STR_Q_QSZ_SIZE+1)-1)
#define STR_Q_QSZ(v)   ((v & STR_Q_QSZ_MASK) << STR_Q_QSZ_BIT)

#define STR_Q_INTAI_BIT  (STR_Q_QSZ_BIT+STR_Q_QSZ_SIZE)
#define STR_Q_INTAI_SIZE 14
#define STR_Q_INTAI_MASK ((1UL<<STR_Q_INTAI_SIZE+1)-1)
#define STR_Q_INTAI(v)   ((v & STR_Q_INTAI_MASK) << STR_Q_INTAI_BIT)

#define STR_Q_QTW_BIT  (STR_Q_INTAI_BIT+STR_Q_INTAI_SIZE)
#define STR_Q_QTW_SIZE 3
#define STR_Q_QTW_MASK ((1UL<<STR_Q_QTW_SIZE+1)-1)
#define STR_Q_QTW(v)   ((v & STR_Q_QTW_MASK) << STR_Q_QTW_BIT)

#define STR_Q_PM(QTW, INTAI, QSZ) \
	(STR_Q_QTW(QTW)|STR_Q_INTAI(INTAI)|STR_Q_QSZ(QSZ))

#endif
