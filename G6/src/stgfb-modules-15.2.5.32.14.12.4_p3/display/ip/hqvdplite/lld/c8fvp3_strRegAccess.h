/*********************************************************************************
* Copyright (C) 1999 STMicroelectronics
* strRegAccess.h: $Revision: 1418 $
*
* Last modification done by $Author: achoul $ on $Date:: 2007-08-10 #$
*********************************************************************************/

/* Define to prevent recursive inclusion */
#ifndef _STRREG_ACCESS
#define _STRREG_ACCESS

/* Includes ---------------------------------------------------------------- */

#include "c8fvp3_stddefs.h" /* gvh_u32_t... */
#include "strRegDefinition.h" /* STR_ABS_BREA... */

/* Exported Defines ---------------------------------------------------------- */

#define STR_EXT2D_RELATIVE_LINES_TO_JUMP_BIT    10
#define STR_EXT2D_RELATIVE_LINES_TO_JUMP_MASK   (0xF << STR_EXT2D_RELATIVE_LINES_TO_JUMP_BIT)

#define STR_EXT2D_RELATIVE_MB_PER_LINE_BIT      0
#define STR_EXT2D_RELATIVE_MB_PER_LINE_MASK     (0x3ff << STR_EXT2D_RELATIVE_MB_PER_LINE_BIT)
/* Exported Types ---------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------ */

/* Exported Macros --------------------------------------------------------- */


#ifdef FW_STXP70
#pragma inline_file(WriteBackSTBCh)
static void WriteBackSTBCh(gvh_u32_t base_address, gvh_u32_t chNb, gvh_u32_t rdNWr, gvh_u32_t extadd,
    gvh_u32_t plugam, gvh_u32_t attr, gvh_u32_t extam, gvh_u32_t ext2d, gvh_u32_t extsz, gvh_u32_t ec, gvh_u32_t df,
    gvh_u32_t intsa, gvh_u32_t intra, gvh_u32_t intai, gvh_u32_t intdc, gvh_u32_t intrc, gvh_u32_t into);
#else
extern void WriteBackSTBCh(gvh_u32_t base_address, gvh_u32_t chNb, gvh_u32_t rdNWr, gvh_u32_t extadd,
    gvh_u32_t plugam, gvh_u32_t attr, gvh_u32_t extam, gvh_u32_t ext2d, gvh_u32_t extsz, gvh_u32_t ec, gvh_u32_t df,
    gvh_u32_t intsa, gvh_u32_t intra, gvh_u32_t intai, gvh_u32_t intdc, gvh_u32_t intrc, gvh_u32_t into);
#endif

#ifdef FW_STXP70
#pragma inline_file(WriteBackQCh)
static void WriteBackQCh(gvh_u32_t base_address, gvh_u32_t intsa, gvh_u32_t intra, gvh_u32_t intai, gvh_u32_t intdc,
    gvh_u32_t intrc, gvh_u32_t intim,  gvh_u32_t dt, gvh_u32_t qtw, gvh_u32_t rdNwr, gvh_u32_t chNb, gvh_u32_t phyQ, gvh_u32_t qsz,
    gvh_u32_t fp, gvh_u32_t lp, gvh_u32_t puid);
#else
extern void WriteBackQCh(gvh_u32_t base_address, gvh_u32_t intsa, gvh_u32_t intra, gvh_u32_t intai, gvh_u32_t intdc,
    gvh_u32_t intrc, gvh_u32_t intim,  gvh_u32_t dt, gvh_u32_t qtw, gvh_u32_t rdNwr, gvh_u32_t chNb, gvh_u32_t phyQ, gvh_u32_t qsz,
    gvh_u32_t fp, gvh_u32_t lp, gvh_u32_t puid);
#endif


#ifdef FW_STXP70
/*******************************************************************************
 * Name        : ???
 * Description : ???
 * Parameters  : ???
 * Assumptions : ???
 * Limitations : ???
 * Returns     : ???
 * *******************************************************************************/
#pragma inline_file(WriteBackSTBCh)
static void WriteBackSTBCh(gvh_u32_t base_address, gvh_u32_t chNb, gvh_u32_t rdNWr, gvh_u32_t extsa,
    gvh_u32_t plugam, gvh_u32_t attr, gvh_u32_t extam, gvh_u32_t ext2d, gvh_u32_t extsz, gvh_u32_t ec, gvh_u32_t df,
    gvh_u32_t intsa, gvh_u32_t intra, gvh_u32_t intai, gvh_u32_t intdc, gvh_u32_t intrc, gvh_u32_t into)
{
    gvh_u32_t baseReg;
    gvh_u32_t data;

    /*************************************************************************
    * 31  .. 0 |
    * ----------------------------------------------------
    *   extsa  |
    *************************************************************************/ 
    data = extsa;
    baseReg = (rdNWr == 0) ? STR_ABS_BWEA(base_address, chNb): STR_ABS_BREA(base_address, chNb);
    WriteRegister(baseReg, data);

    /*************************************************************************
    * | 30 .. 28 | 27 .. 14 | 13 .. 0 |
    * ----------------------------------
    * |  attr   |   ext2d  |  extsz  |
    *************************************************************************/ 

    data =  ((attr << STR_ATTR_BIT) & STR_ATTR_MASK)
          | ((ext2d << STR_EXT2D_BIT) & STR_EXT2D_MASK)
	  | ((extsz << STR_EXTSZ_BIT) & STR_EXTSZ_MASK);
    baseReg = (rdNWr == 0) ? STR_ABS_BWPM(base_address, chNb): STR_ABS_BRPM(base_address, chNb);
    WriteRegister(baseReg, data);

    /*************************************************************************
    * 31 .. 28 | 27 .. 24 | 22 ..0 |
    * ---------------------------------
    *  INTRC   |  INTDC   | INTSA  |
    *************************************************************************/ 
    data =  ((intrc << STR_BINTRDC_BIT) & STR_BINTRDC_MASK)
          | ((intdc << STR_BINTDC_BIT) & STR_BINTDC_MASK)
          | ((intsa << STR_BINTSA_BIT) & STR_BINTSA_MASK);

    baseReg = (rdNWr == 0) ? STR_ABS_BWIA(base_address, chNb): STR_ABS_BRIA(base_address, chNb);
    WriteRegister(baseReg, data);

    /*************************************************************************
    * 31 .. 23 | 22 .. 0 |
    * ------------------------------------------
    *  -      |  INTRA  | 
    *************************************************************************/ 
    data = (intra << STR_BINTRA_BIT) & STR_BINTRA_MASK;
    baseReg = (rdNWr == 0) ? STR_ABS_BWRA(base_address, chNb): STR_ABS_BRRA(base_address, chNb);
    WriteRegister(baseReg, data);


    /*************************************************************************
    * 31 .. 14 | 13 .. 0 |
    * ------------------------------------------
    *  -      |  INTAI  | 
    *************************************************************************/ 
    data = (intai << STR_BINTAI_BIT) & STR_BINTAI_MASK;
    baseReg = (rdNWr == 0) ? STR_ABS_BWII(base_address, chNb): STR_ABS_BRII(base_address, chNb);
    WriteRegister(baseReg, data);

    /*************************************************************************
    * 25  23 | 21 19 | 18 | 17 .. 14 | 13 .. 0 |
    * -----------------------------------
    *  EXTAM | DF    | EC |  PLUGAM  |   INTO  |
    *************************************************************************/ 
    data =  ((extam << STR_EXTAM_BIT) & STR_EXTAM_MASK)
          | ((df << STR_DF_BIT) & STR_DF_MASK)
          | ((ec << STR_EC_BIT) & STR_EC_MASK)
          | ((plugam << STR_PLUGAM_BIT) & STR_PLUGAM_MASK)
          | ((into << STR_INTO_BIT) & STR_INTO_MASK);
    baseReg = (rdNWr == 0) ? STR_ABS_BWVP(base_address, chNb): STR_ABS_BRVP(base_address, chNb);
    WriteRegister(baseReg, data); 
}
#endif /* ifdef FW_STXP70 */

#ifdef FW_STXP70
/*******************************************************************************
 * Name        : ???
 * Description : ???
 * Parameters  : ???
 * Assumptions : ???
 * Limitations : ???
 * Returns     : ???
 * *******************************************************************************/
#pragma inline_file(WriteBackQCh)
static void WriteBackQCh(gvh_u32_t base_address, gvh_u32_t intsa, gvh_u32_t intra, gvh_u32_t intai, gvh_u32_t intdc, gvh_u32_t intrc, gvh_u32_t intim, gvh_u32_t qtw, gvh_u32_t dt, gvh_u32_t rdNwr, gvh_u32_t chNb, gvh_u32_t phyQ, gvh_u32_t qsz, gvh_u32_t fp, gvh_u32_t lp, gvh_u32_t puid)
{
    gvh_u32_t baseReg;
    gvh_u32_t data;
    /*************************************************************************
    * 31 .. 28 | 27 .. 24  |  23   | 22 ..0 |
    * --------------------------
    *  INTRC   |  INTDC   | INTIM | INTSA  |
    *************************************************************************/ 
    data =  ((intrc << STR_QINTRDC_BIT) & STR_QINTRDC_MASK)
          | ((intdc << STR_QINTDC_BIT) & STR_QINTDC_MASK)
          | ((intim << STR_QINTIM_BIT) & STR_QINTIM_MASK)
          | ((intsa << STR_QINTSA_BIT) & STR_QINTSA_MASK);
    baseReg = (rdNwr == 0) ? STR_ABS_QWIA(base_address, phyQ, chNb): STR_ABS_QRIA(base_address, phyQ, chNb);
    WriteRegister(baseReg, data);

    /*************************************************************************
    *  31 | 30 .. 23 | 22 .. 0 |
    * ------------------------------------------
    *  DT |  FLAGS   |  INTRA  | 
    *************************************************************************/ 
    data =  ((dt?1:0 /*& STR_Q_DT_MASK*/) << STR_Q_DT_BIT)
          | ((puid << STR_QFLAG_PUID_BIT) & STR_QFLAG_PUID_MASK)
          | ((lp << STR_QFLAG_LP_BIT) & STR_QFLAG_LP_MASK)
          | ((fp << STR_QFLAG_FP_BIT) & STR_QFLAG_FP_MASK)
          | ((intra << STR_QINTRA_BIT) & STR_QINTRA_MASK);
    baseReg = (rdNwr == 0) ? STR_ABS_QWRA(base_address, phyQ, chNb): STR_ABS_QRRA(base_address, phyQ, chNb);
    WriteRegister(baseReg, data);

    /*************************************************************************
    * 29. 28  |27 .. 14 | 13 ..0 |
    * ----------------------------
    *  QTW  |  INTAI  | QSZ    |
    ************************************************************************/ 
    data =  ((qtw << STR_QTW_BIT) & STR_QTW_MASK)
          | ((intai << STR_QINTAI_BIT) & STR_QINTAI_MASK)
          | ((qsz << STR_QSZ_BIT) & STR_QSZ_MASK);
    baseReg = (rdNwr == 0) ? STR_ABS_QWPM(base_address, phyQ, chNb): STR_ABS_QRPM(base_address, phyQ, chNb);
    WriteRegister(baseReg, data);
}
#endif /* ifdef FW_STXP70 */

#endif  /* #ifndef _STRREG_ACESS */



