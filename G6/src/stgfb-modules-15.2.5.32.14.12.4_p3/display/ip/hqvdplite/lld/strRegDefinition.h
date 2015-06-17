#ifndef __STR_REGDEFINITION_H
#define __STR_REGDEFINITION_H

#include "strRegMapping.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif
/*  ****************** STBUS READ CONFIG REGISTERS **************************** */
/*  STBUS read logical channel number lc registers, with 0 <= lc <= 15 */

/*  External start address */
#define STR_ABS_BREA(base,lc)  (base + (STR_BRDBASECHNB(lc) + 0) * 4)

/*  Address parameters */
#define STR_ABS_BRPM(base,lc)  (base + (STR_BRDBASECHNB(lc) + 1) * 4)

/*  Internal address */
#define STR_ABS_BRIA(base,lc)  (base + (STR_BRDBASECHNB(lc) + 2) * 4)

/*  Internal reload address */
#define STR_ABS_BRRA(base,lc) (base + (STR_BRDBASECHNB(lc) + 3) * 4)

/*  Internal address increment */
#define STR_ABS_BRII(base,lc)  (base + (STR_BRDBASECHNB(lc) + 4) * 4)

/*  Video parameters */
#define STR_ABS_BRVP(base,lc)  (base + (STR_BRDBASECHNB(lc) + 5) * 4)

/*  ****************** STBUS WRITE CONFIG REGISTERS *************************** */
/*  STBUS write logical channel number lc registers, with 0 <= lc <= 15 */

/*  External start address */
#define STR_ABS_BWEA(base,lc)  (base + (STR_BWRBASECHNB(lc) + 0) * 4)

/*  Address parameters */
#define STR_ABS_BWPM(base,lc)  (base + (STR_BWRBASECHNB(lc) + 1) * 4)

/*  Internal address */
#define STR_ABS_BWIA(base,lc)  (base + (STR_BWRBASECHNB(lc) + 2) * 4)

/*  Internal reload address */
#define STR_ABS_BWRA(base,lc) (base + (STR_BWRBASECHNB(lc) + 3) * 4)

/*  Internal address increment */
#define STR_ABS_BWII(base,lc)  (base + (STR_BWRBASECHNB(lc) + 4) * 4)

/*  Video parameters */
#define STR_ABS_BWVP(base,lc)  (base + (STR_BWRBASECHNB(lc) + 5) * 4)

/*  ****************** READ QUEUE CONFIG REGISTERS **************************** */
/*  Read queue number q, logical channel number lc, */
/*   with 0 <= q <= 7 and 0 <= lc <= 7 */

/*  Internal start address */
#define STR_ABS_QRIA(base, q, lc) (base + (STR_QRDBASECHNB(q,lc) + 0) * 4)

/*  Internal reload address */
#define STR_ABS_QRRA(base, q, lc) (base + (STR_QRDBASECHNB(q,lc) + 1) * 4)

/*  Parameters */
#define STR_ABS_QRPM(base, q, lc) (base + (STR_QRDBASECHNB(q,lc) + 2) * 4)

/*  ****************** WRITE QUEUE CONFIG REGISTERS *************************** */
/*  Write queue number q, logical channel number lc, */
/*   with 0 <= q <= 7 and 0 <= lc <= 7 */

/*  Internal start address */
#define STR_ABS_QWIA(base,q, lc) (base + (STR_QWRBASECHNB(q,lc) + 0) * 4)

/*  Internal reload address */
#define STR_ABS_QWRA(base,q, lc) (base + (STR_QWRBASECHNB(q,lc) + 1) * 4)

/*  Parameters */
#define STR_ABS_QWPM(base,q, lc) (base + (STR_QWRBASECHNB(q,lc) + 2) * 4)

/*  ****************** INTERRUPT REGISTERS ************************************ */
/*  Interrupt source number s, with 0 <= s <= 8 */

/*  STBus Interrupt Mask */
#define STR_ABS_BIM(base,s) (base + STR_BIM(s) * 4)

/*  STBus Interrupt Mask */
#define STR_ABS_QIM(base,s) (base + STR_QIM(s) * 4)

/*  Interrupt source enable register */
#define STR_ABS_ISE(base)   (base + STR_ISE    * 4)
#define STR_ABS_SHISE(base) (base + STR_SHISE  * 4)

/*  Fields inside STR_BIM, STR_QIM */
/*   x is the logical channel number for STR_BIM, 0 <= x <= 15 */
/*   x is the queue number for STR_QIM, 0 <= x <= 7 */
#define STR_RIE_BIT(x)    (x)
#define STR_RIE_MASK(x)   (1 << STR_RIE_BIT(x))
#define STR_ALL_RIE_MASK  0xffff

#define STR_WIE_BIT(x)    ((x) + 16)
#define STR_WIE_MASK(x)   (1 << STR_WIE_BIT(x))
#define STR_ALL_WIE_MASK  0xffff

/*  Fields inside STR_ISE */
/*  s is the interrupt context, 0 <= s <= 8 */
#define STR_ISEB_BIT(s)     (s)
#define STR_EIEB_BIT(s)     15

#define STR_ISEB_MASK(s)    (1 << STR_ISEB_BIT(s))
#define STR_EIEB_MASK       (1 << STR_EIEB_BIT)
#define STR_ALL_ISEB_MASK   0xffff

/*  ****************** SYNCHRONIZATION FLAGS REGISTERS ************************ */
/*  STBUS synchronization flags */
#define STR_ABS_BSF(base)         (base + (STR_BSF * 4))

/*  Read/write queue synchronization flags, with qsf 0 <= qsf <= 7 */
#define STR_ABS_RQSF(base, qsf)   (base + (STR_RQSF(((qsf)>>2)) * 4))
#define STR_ABS_WQSF(base, qsf)   (base + (STR_WQSF(((qsf)>>2)) * 4))

/*  Read/write queue increment disable, with qsf 0 <= qsf <= 7 */
#define STR_ABS_RQID(base, qsf)   (base + (STR_RQID(((qsf)>>2)) * 4))
#define STR_ABS_WQID(base, qsf)   (base + (STR_WQID(((qsf)>>2)) * 4))

/*  Shadow registers on synchro flags */
#define STR_ABS_SHBSF(base)       (base + (STR_SHBSF * 4))
#define STR_ABS_SHRQSF(base, qsf) (base + (STR_SHRQSF(((qsf)>>2)) * 4))
#define STR_ABS_SHWQSF(base, qsf) (base + (STR_SHWQSF(((qsf)>>2)) * 4))

/*  Shadow on Increment disable */
#define STR_ABS_SHRQID(base, qsf) (base + (STR_SHRQID(((qsf)>>2)) * 4))
#define STR_ABS_SHWQID(base, qsf) (base + (STR_SHWQID(((qsf)>>2)) * 4))

/*  Alias on Syncro flags locical channel lc, with 0 <= lc <= 7 */
#define STR_ABS_AQSF(base, lc)   (base + (STR_AQSF(lc) * 4))

/*  Alias on Increment disable locical channel lc, with 0 <= lc <= 7 */
#define STR_ABS_AQID(base, lc)   (base + (STR_AQID(lc) * 4))

/*  Clear Queue synchronization flags */
#define STR_ABS_CRQSF(base, qsf) (base + (STR_CRQSF(((qsf)>>2)) * 4))
#define STR_ABS_CWQSF(base, qsf) (base + (STR_CWQSF(((qsf)>>2)) * 4))

/*  Alias on shadow Syncro flags locical channel lc, with 0 <= lc <= 7 */
#define STR_ABS_ASHQSF(base, lc)   (base + (STR_ASHQSF(lc) * 4))

/*  Alias on shadow Increment disable locical channel lc, with 0 <= lc <= 7 */
#define STR_ABS_ASHQID(base, lc)   (base + (STR_ASHQID(lc) * 4))

/*  Fields inside STR_BSF */
/*  lc is the logical channel number, 0 <= lc <= 15 */
#define STR_BRSF_BIT(lc)    ( 0 + (lc))
#define STR_BWSF_BIT(lc)    (16 + (lc))

#define STR_BRSF_MASK(lc)   (1 << STR_BRSF_BIT(lc))
#define STR_BWSF_MASK(lc)   (1 << STR_BWSF_BIT(lc))

/*  Fields inside STR_SHBSF */
/*   lc is the logical channel number, 0 <= lc <= 15 */
#define STR_SHBRSF_BIT(lc)    ( 0 + (lc))
#define STR_SHBWSF_BIT(lc)    (16 + (lc))

#define STR_SHBRSF_MASK(lc)   (1 << STR_BRSF_BIT(lc))
#define STR_SHBWSF_MASK(lc)   (1 << STR_BWSF_BIT(lc))

/*  Fields inside STR_CQSF */
/*   q is the queue number, 0 <= q <= 15 */
/*   lc is the logical channel number, 0 <= lc <= 7 */
#define STR_CQSF_BIT(q, lc)     ((((q) & 0x3) << 3) + (lc))

#define STR_CRQSF_MASK(q, lc)   (1 << STR_CQSF_BIT(q, lc))
#define STR_CWQSF_MASK(q, lc)   (1 << STR_CQSF_BIT(q, lc))

/*  Fields inside STR_QSF */
/*   q is the queue number, 0 <= q <= 15 */
/*   lc is the logical channel number, 0 <= lc <= 7 */
#define STR_QSF_BIT(q, lc)     ((((q) & 0x3) << 3) + (lc))

#define STR_RQSF_MASK(q, lc)  (1 << STR_QSF_BIT(q, lc))
#define STR_WQSF_MASK(q, lc)  (1 << STR_QSF_BIT(q, lc))

/*  Fields inside STR_QID */
/*   q is the queue number, 0 <= q <= 15 */
/*   lc is the logical channel number, 0 <= lc <= 7 */
#define STR_QID_BIT(q, lc)     ((((q) & 0x3) << 3) + (lc))

#define STR_RQID_MASK(q, lc)  (1 << STR_QID_BIT(q, lc))
#define STR_WQID_MASK(q, lc)  (1 << STR_QID_BIT(q, lc))

/*  Fields inside STR_SHQID */
/*   q is the queue number, 0 <= q <= 15 */
/*   lc is the logical channel number, 0 <= lc <= 7 */
#define STR_SHQID_BIT(q, lc)    ((((q) & 0x3) << 3) + (lc))

#define STR_SHRQID_MASK(q, lc)  (1 << STR_SHQID_BIT(q, lc))
#define STR_SHWQID_MASK(q, lc)  (1 << STR_SHQID_BIT(q, lc))

/*  Fields inside STR_AQID */
/*   q is the queue number, 0 <= q <= 15 */
#define STR_ARQID_BIT(q)    ( 0 + (q))
#define STR_AWQID_BIT(q)    ( 16 + (q))

#define STR_AWQID_MASK(lc)   (1 << STR_AWQID_MASK(lc))
#define STR_ARQID_MASK(lc)   (1 << STR_ARQID_BIT(lc))

/*  Fields inside STR_ASHQID */
/*   q is the queue number, 0 <= q <= 15 */
#define STR_ASHRQID_BIT(q)    ( 0 + (q))
#define STR_ASHWQID_BIT(q)    ( 16 + (q))

#define STR_ASHRQID_MASK(lc)   (1 << STR_ASHRQID_BIT(lc))
#define STR_ASHWQID_MASK(lc)   (1 << STR_ASHWQID_BIT(lc))

/*  Fields inside STR_SHQSF */
/*   q is the queue number, 0 <= q <= 15 */
/*   lc is the logical channel number, 0 <= lc <= 7 */
#define STR_SHQSF_BIT(q, lc)    ((((q) & 0x3) << 3) + (lc))

#define STR_SHWQSF_MASK(q, lc)  (1 << STR_SHQSF_BIT(q, lc))
#define STR_SHRQSF_MASK(q, lc)  (1 << STR_SHQSF_BIT(q, lc))

/*  Fields inside STR_AQSF */
/*   q is the queue number, 0 <= q <= 15 */
#define STR_ARQSF_BIT(q)    ( 0 + (q))
#define STR_AWQSF_BIT(q)    ( 16 + (q))

#define STR_ARQSF_MASK(q)   (1 << STR_ARQSF_BIT(q))
#define STR_AWQSF_MASK(q)   (1 << STR_AWQSF_BIT(q))

/*  Fields inside STR_ASHQSF */
/*   q is the queue number, 0 <= q <= 15 */
#define STR_ASHRQSF_BIT(q)    ( 0 + (q))
#define STR_ASHWQSF_BIT(q)    ( 16 + (q))

#define STR_ASHRQSF_MASK(q)   (1 << STR_ASHRQSF_BIT(q))
#define STR_ASHWQSF_MASK(q)   (1 << STR_ASHWQSF_BIT(q))

/*  ****************** PRIORITY REGISTERS ************************************* */
#define STR_ABS_PRI  (base + STR_PRI * 4)

/*  ****************** MFU CONFIG REGISTERS *********************************** */
#define STR_ABS_MC   (base + STR_MC * 4)

/*  ****************** STBUS STATUS REGISTERS ********************************* */
#define STR_ABS_BHCS (base + STR_BHCS * 4)

/*  Fields inside STR_BHCS */
#define STR_EBRCH_BIT  0
#define STR_EBWCH_BIT  4
#define STR_BRE_BIT    30
#define STR_BWE_BIT    31

#define STR_EBRCH_MASK (0xf << STR_EBRCH_BIT)
#define STR_EBWCH_MASK (0xf << STR_EBWCH_BIT)
#define STR_BRE_MASK   (0x1 << STR_BRE_BIT)
#define STR_BWE_MASK   (0x1 << STR_BWE_BIT)

/*  Values for STR_BRE field */
#define STR_BRE_ERROR     0x1
#define STR_BRE_NOERROR   0x0

/*  Values for STR_BWE field */
#define STR_BWE_ERROR     0x1
#define STR_BWE_NOERROR   0x0

/*  Values for STR_EBRCH field */
#define STR_EBRCH_NOERROR 0x0

/*  Values for STR_EBWCH field */
#define STR_EBWCH_NOERROR 0x0

/*  ****************** STBUS WR CONFIGURATION REGISTER ************************ */
#define STR_ABS_BWC  (base + STR_BWC * 4)

#define STR_EOT_BIT  0
#define STR_EOT_MASK (0x1 << STR_EOT_BIT)

/*  values for EOT field: */
#define STR_EOT_WRITEPOSTING  0
#define STR_EOT_EOT    1

/*  ****************** CONFIGURATION REGISTER ********************************* */
#define STR_ABS_CG0(base)   (base + STR_CG(0) * 4)
#define STR_ABS_CG3(base)   (base + STR_CG(3) * 4)
#define STR_ABS_CG4(base)   (base + STR_CG(4) * 4)
#define STR_ABS_CG5(base)   (base + STR_CG(5) * 4)
#define STR_ABS_CG6(base)   (base + STR_CG(6) * 4)

#define STR_ABS_SID(base)   (base + STR_SID * 4)

#define STR_ABS_CG7(base)   (base + STR_CG(7) * 4)
#define STR_ABS_CG8(base)   (base + STR_CG(8) * 4)
#define STR_ABS_CG9(base)   (base + STR_CG(9) * 4)
#define STR_ABS_CG10(base)  (base + STR_CG(10)* 4)

/*  ****************** Shadow COPY REGISTER *********************************** */
#define STR_ABS_SHCPY(base) (base + STR_SHCPY * 4)

/* ************* FIELDS/VALUES USED BY STBUS CHANNEL REGISTERS **************** */
/*  Values for STR_DF field */
#define STR_DF_NO_DEMUX                    0x0
#define STR_DF_DUAL_BUFFER                 0x1
#define STR_DF_420_MB_OMEGA2_CHROMA_BUFFER 0x2
#define STR_DF_ONE_OUT_OF_TWO_BYTES        0x3
#define STR_DF_OMEGA2_LUMA_WITH_LD16       0x4
#define STR_DF_OMEGA2_CHROMA_WITH_LD16     0x5
#define STR_DF_OMEGA2_LUMA_WITH_LD32       0x6
#define STR_DF_OMEGA2_CHROMA_WITH_LD32     0x7

/*  Values for STR_EXTAM field */
#define STR_EXTAM_LINEAR_BUFFER               0x1
#define STR_EXTAM_420_MB_OMEGA2_LUMA_BUFFER   0x2
#define STR_EXTAM_420_MB_OMEGA2_CHROMA_BUFFER 0x3
#define STR_EXTAM_MB_SHE2_LUMA_BUFFER         0x4
#define STR_EXTAM_MB_SHE2_CB_OR_CR_BUFFER     0x5
#define STR_EXTAM_NONE                        0x7 /*  No video */

/*  Values for STR_EC field */
#define STR_EC_BYTES_SWAPPED               0x1
#define STR_EC_BYTES_NOT_SWAPPED           0x0

/*  Values for STR_PLUGAM field */
#define STR_PLUGAM_RASTER                  0x0
#define STR_PLUGAM_MB_LUMA                 0x1
#define STR_PLUGAM_MB_CHROMA               0x2

/*  Fields inside STR_BRIPM, STR_BWIPM */
/*  Give the bit where the field begins */
#define STR_EXTSZ_BIT               0
#define STR_EXT2D_BIT               14
#define STR_EXT2D_MB_PER_LINE_BIT   14
#define STR_EXT2D_PITCH_BIT         14
#define STR_EXT2D_LINES_TO_JUMP_BIT 24
#define STR_ATTR_BIT                28

#define STR_EXTSZ_MASK               (0x3fff << STR_EXTSZ_BIT)
#define STR_EXT2D_MASK               (0x3fff << STR_EXT2D_BIT)
#define STR_EXT2D_MB_PER_LINE_MASK   (0x3ff  << STR_EXT2D_MB_PER_LINE_BIT)
#define STR_EXT2D_PITCH_MASK         (0x3fff << STR_EXT2D_PITCH_BIT)
#define STR_EXT2D_LINES_TO_JUMP_MASK (0xf    << STR_EXT2D_LINES_TO_JUMP_BIT)
#define STR_ATTR_MASK                (0xf    << STR_ATTR_BIT)

/*  Fields inside STR_BRIA, STR_BWIA */
#define STR_BINTSA_BIT   0
#define STR_BINTDC_BIT   24
#define STR_BINTRDC_BIT  28

#define STR_BINTSA_MASK  (0x7fffff << STR_BINTSA_BIT)
#define STR_BINTDC_MASK  (0xf << STR_BINTDC_BIT)
#define STR_BINTRDC_MASK (0xf << STR_BINTRDC_BIT)

/*  Fields inside STR_BRIRA, STR_BWIRA */
#define STR_BINTRA_BIT   0
#define STR_BINTRA_MASK  (0x7fffff << STR_BINTRA_BIT)

/*  Fields inside STR_BRII, STR_BWII */
#define STR_BINTAI_BIT   0
#define STR_BINTAI_MASK  (0x3fff << STR_BINTAI_BIT)

/*  Fields inside STR_BRVP, STR_BWVP */
#define STR_INTO_BIT     0
#define STR_PLUGAM_BIT   14
#define STR_EC_BIT       18
#define STR_DF_BIT       19
#define STR_EXTAM_BIT    23

#define STR_INTO_MASK    (0x3fff << STR_INTO_BIT)
#define STR_PLUGAM_MASK  (0xf << STR_PLUGAM_BIT)
#define STR_EC_MASK      (0x1 << STR_EC_BIT)
#define STR_DF_MASK      (0x7 << STR_DF_BIT)
#define STR_EXTAM_MASK   (0x7 << STR_EXTAM_BIT)

/* ************* FIELDS/VALUES USED BY QUEUE CHANNEL REGISTERS **************** */
/*  Fields inside STR_QRAI, STR_QWAI */
#define STR_QINTSA_BIT       0
#define STR_QINTIM_BIT       23
#define STR_QINTDC_BIT       24
#define STR_QINTRDC_BIT      28

#define STR_QINTSA_MASK     (0x7fffff << STR_QINTSA_BIT)
#define STR_QINTIM_MASK     (0x1 << STR_QINTIM_BIT)
#define STR_QINTDC_MASK     (0xf << STR_QINTDC_BIT)
#define STR_QINTRDC_MASK    (0xf << STR_QINTRDC_BIT)

/*  Fields inside STR_QRRA, STR_QWRA */
#define STR_QINTRA_BIT       0
#define STR_QFLAGS_BIT       23
#define STR_QFLAG_FP_BIT     23
#define STR_QFLAG_LP_BIT     24
#define STR_QFLAG_PUID_BIT   27
#define STR_QDT_BIT          31

#define STR_QINTRA_MASK      (0x7fffff << STR_QINTRA_BIT)
#define STR_QFLAGS_MASK      (0xff << STR_QFLAG_FP_BIT)
#define STR_QFLAG_FP_MASK    (0x1 << STR_QFLAG_FP_BIT)
#define STR_QFLAG_LP_MASK    (0x1 << STR_QFLAG_LP_BIT)
#define STR_QFLAG_PUID_MASK  (0xf << STR_QFLAG_PUID_BIT)
#define STR_QDT_MASK         (0x1 << STR_QDT_BIT)

/*  Fields inside STR_QRPM, STR_QWPM */
#define STR_QSZ_BIT         0
#define STR_QINTAI_BIT      14
#define STR_QTW_BIT         28 /*  Present only in STR_QRA and STR_QWA */

#define STR_QSZ_MASK        (0x3fff << STR_QSZ_BIT)
#define STR_QINTAI_MASK     (0x3fff << STR_QINTAI_BIT)
#define STR_QTW_MASK        (0x3 << STR_QTW_BIT)

/*  Values for STR_QTW field */
#define STR_QTW_1_BYTE      0
#define STR_QTW_2_BYTES     1
#define STR_QTW_4_BYTES     2
#define STR_QTW_8_BYTES     3

/*  Values for STR_QTW field */
#define STR_QINTIM_POSTINCR_MODE      0
#define STR_QINTIM_PREINCR_MODE       1

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __STR_REGDEFINITION_H */

