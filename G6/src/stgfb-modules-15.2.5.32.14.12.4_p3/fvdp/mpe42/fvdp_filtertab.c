/***********************************************************************
 *
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "fvdp_filtertab.h"
#include "fvdp_filtertab_hqvf.h"
#include "fvdp_filtertab_litevf.h"
#include "fvdp_filtertab_litehf.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */
// Main Channel Lite scaler coefficient tables
#define MAIN_HF_UV_LP_0    0x025800
#define MAIN_HF_UV_LP_1    0x025C00
#define MAIN_HF_Y_LP_0     0x026800
#define MAIN_HF_Y_LP_1     0x026C00
#define MAIN_VF_UV_LP_0    0x029800
#define MAIN_VF_UV_LP_1    0x029C00
#define MAIN_VF_Y_LP_0     0x02A800
#define MAIN_VF_Y_LP_1     0x02AC00
// Main Channel HQ scaler coefficient tables
#define HQ_VF_UV_HP_0      0x049000
#define HQ_VF_UV_HP_1      0x049400
#define HQ_VF_UV_LP_0      0x049800
#define HQ_VF_UV_LP_1      0x049C00
#define HQ_VF_Y_HP_0       0x04A000
#define HQ_VF_Y_HP_1       0x04A400
#define HQ_VF_Y_LP_0       0x04A800
#define HQ_VF_Y_LP_1       0x04AC00
#define HQ_HF_UV_HP_0      0x04B000
#define HQ_HF_UV_HP_1      0x04B400
#define HQ_HF_UV_LP_0      0x04B800
#define HQ_HF_UV_LP_1      0x04BC00
#define HQ_HF_Y_HP_0       0x04C000
#define HQ_HF_Y_HP_1       0x04C400
#define HQ_HF_Y_LP_0       0x04C800
#define HQ_HF_Y_LP_1       0x04CC00
//PIP Channel Lite scaler coefficient tables
#define PIP_HF_UV_LP_0     0x085800
#define PIP_HF_Y_LP_0      0x086800
#define PIP_VF_UV_HP_0     0x089000
#define PIP_VF_UV_HP_1     0x089400
#define PIP_VF_UV_LP_0     0x089800
#define PIP_VF_UV_LP_1     0x089C00
#define PIP_VF_Y_HP_0      0x08A000
#define PIP_VF_Y_HP_1      0x08A400
#define PIP_VF_Y_LP_0      0x08A800
#define PIP_VF_Y_LP_1      0x08AC00
// AUX Channel Lite scaler coefficient tables
#define AUX_HF_UV_LP_0     0x095800
#define AUX_HF_Y_LP_0      0x096800
#define AUX_VF_UV_HP_0     0x099000
#define AUX_VF_UV_HP_1     0x099400
#define AUX_VF_UV_LP_0     0x099800
#define AUX_VF_UV_LP_1     0x099C00
#define AUX_VF_Y_HP_0      0x09A000
#define AUX_VF_Y_HP_1      0x09A400
#define AUX_VF_Y_LP_0      0x09A800
#define AUX_VF_Y_LP_1      0x09AC00
// ENC Channel Lite scaler coefficient tables
#define ENC_HF_UV_LP_0     0x0A5800
#define ENC_HF_Y_LP_0      0x0A6800
#define ENC_VF_UV_HP_0     0x0A9000
#define ENC_VF_UV_HP_1     0x0A9400
#define ENC_VF_UV_LP_0     0x0A9800
#define ENC_VF_UV_LP_1     0x0A9C00
#define ENC_VF_Y_HP_0      0x0AA000
#define ENC_VF_Y_HP_1      0x0AA400
#define ENC_VF_Y_LP_0      0x0AA800
#define ENC_VF_Y_LP_1      0x0AAC00

#define BOUNDARY_MARGIN      0x800

/* Private Macros ----------------------------------------------------------- */
static const uint32_t HFLITE_BASE_ADDR[] = {HFLITE_FE_BASE_ADDRESS, HFLITE_PIP_BASE_ADDRESS, HFLITE_AUX_BASE_ADDRESS, HFLITE_ENC_BASE_ADDRESS};
static const uint32_t VFLITE_BASE_ADDR[] = {VFLITE_FE_BASE_ADDRESS, VFLITE_PIP_BASE_ADDRESS, VFLITE_AUX_BASE_ADDRESS, VFLITE_ENC_BASE_ADDRESS};

#define HFHQS_REG32_Read(Addr)                       REG32_Read(Addr+HQS_HF_BE_BASE_ADDRESS)
#define HFHQS_REG32_Set(Addr,BitsToSet)              REG32_Set(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToSet)
#define VFHQS_REG32_Read(Addr)                       REG32_Read(Addr+HQS_VF_BE_BASE_ADDRESS)
#define VFHQS_REG32_Set(Addr,BitsToSet)              REG32_Set(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToSet)
#define DEINT_REG32_Read(Addr)                       REG32_Read(Addr+DEINT_FE_BASE_ADDRESS)
#define DEINT_REG32_Set(Addr,BitsToSet)              REG32_Set(Addr+DEINT_FE_BASE_ADDRESS,BitsToSet)
#define HFLITE_REG32_Read(Ch,Addr)                     REG32_Read(Addr+HFLITE_BASE_ADDR[Ch])
#define HFLITE_REG32_Set(Ch,Addr,BitsToSet)            REG32_Set(Addr+HFLITE_BASE_ADDR[Ch],BitsToSet)
#define VFLITE_REG32_Read(Ch,Addr)                     REG32_Read(Addr+VFLITE_BASE_ADDR[Ch])
#define VFLITE_REG32_Set(Ch,Addr,BitsToSet)            REG32_Set(Addr+VFLITE_BASE_ADDR[Ch],BitsToSet)


/* Private Variables (static)------------------------------------------------ */
/*******************************************************************************
Load apropriate filter table
*******************************************************************************/

static const ScalerFilterTable_t FilterTable_Dest[TBL_FORM_SIZE][4] =
{
 /*       UV,            Y,            UVPK,            YPK,   */
    {FLTTBL_VF_UV,  FLTTBL_VF_Y,  FLTTBL_VF_UVPK,   FLTTBL_VF_YPK},    //TBL_FORM_VF,
    {FLTTBL_HF_UV,  FLTTBL_HF_Y,  FLTTBL_HF_UVPK,   FLTTBL_HF_YPK},    //TBL_FORM_HF,
    {FLTTBL_VF_UV,  FLTTBL_VF_Y,  FLTTBL_VF_UVPK,   FLTTBL_VF_YPK},    //TBL_FORM_HQVF,
    {FLTTBL_HF_UV,  FLTTBL_HF_Y,  FLTTBL_HF_UVPK,   FLTTBL_HF_YPK},     //TBL_FORM_HQHF,
    {FLTTBL_SIZE,   FLTTBL_SIZE,  FLTTBL_SIZE,       FLTTBL_SIZE},     //TBL_FORM_DF,
};

const uint32_t FilterTable_Adress[FILTER_NUM_CORES][FLTTBL_SIZE] =
{  /*      VF_Y,          VF_UV,           HF_Y,         HF_UV,            VF_YPK,               VF_UVPK,            HF_YPK,          HF_UVPK, */
    {MAIN_VF_Y_LP_0, MAIN_VF_UV_LP_0, MAIN_HF_Y_LP_0, MAIN_HF_UV_LP_0,       0           ,          0          ,        0      ,         0      }, //FILTER_FE
    {HQ_VF_Y_LP_0,   HQ_VF_UV_LP_0,   HQ_HF_Y_LP_0,   HQ_HF_UV_LP_0,     HQ_VF_Y_HP_0    ,     HQ_VF_UV_HP_0   ,   HQ_HF_Y_HP_0,   HQ_HF_UV_HP_0}, //FILTER_BE
    {PIP_VF_Y_LP_0,  PIP_VF_UV_LP_0,  PIP_HF_Y_LP_0,  PIP_HF_UV_LP_0,  0/*PIP_VF_Y_HP_0*/, 0 /*PIP_VF_UV_HP_0*/,      0        ,         0      }, //FILTER_PIP
    {AUX_VF_Y_LP_0,  AUX_VF_UV_LP_0,  AUX_HF_Y_LP_0,  AUX_HF_UV_LP_0,  0/*AUX_VF_Y_HP_0*/, 0 /*AUX_VF_UV_HP_0*/,      0        ,         0      }, //FILTER_AUX
    {ENC_VF_Y_LP_0,  ENC_VF_UV_LP_0,  ENC_HF_Y_LP_0,  ENC_HF_UV_LP_0,  0/*ENC_VF_Y_HP_0*/, 0 /*ENC_VF_UV_HP_0*/,      0        ,         0      }, //FILTER_ENC
};

const uint32_t FilterTable_size[TBL_FORM_SIZE] =
{
    VF_TAB_SIZE,         //TBL_FORM_VF
    HF_TAB_SIZE,         //TBL_FORM_HF
    VF_TAB_SIZE,/*HQVF_TAB_SIZE - 1,*/   //TBL_FORM_HQVF
    HQHF_TAB_SIZE,       //TBL_FORM_HQHF
    VF_TAB_SIZE          //TBL_FORM_DF
};

const ScalerTableFormat_t FilterTable_format[FILTER_NUM_CORES][FLTTBL_SIZE] =
{/*      VF_Y,        VF_UV,         HF_Y,         HF_UV,          VF_YPK,        VF_UVPK,        HF_YPK,      HF_UVPK, */
    {TBL_FORM_VF,   TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF,   TBL_FORM_VF,    TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF},   //FILTER_FE
    {TBL_FORM_HQVF, TBL_FORM_HQVF, TBL_FORM_HQHF, TBL_FORM_HQHF, TBL_FORM_HQVF,  TBL_FORM_HQVF, TBL_FORM_HQHF, TBL_FORM_HQHF}, //FILTER_BE
    {TBL_FORM_VF,   TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF,   TBL_FORM_VF,    TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF},   //FILTER_PIP
    {TBL_FORM_VF,   TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF,   TBL_FORM_VF,    TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF},   //FILTER_AUX
    {TBL_FORM_VF,   TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF,   TBL_FORM_VF,    TBL_FORM_VF,   TBL_FORM_HF,   TBL_FORM_HF},   //FILTER_ENC
};

/* Private Function Prototypes ---------------------------------------------- */
static bool HQScalerCheckBeforeTabLoad(FILTER_Core_t FilterCore, ScalerTableFormat_t TableFormat);
static bool DeintCheckBeforeTabLoad (FILTER_Core_t FilterCore,  ScalerTableFormat_t TableFormat);
static bool ScalerCheckBeforeTabLoad (FILTER_Core_t FilterCore, ScalerTableFormat_t TableFormat);
static FVDP_Result_t CoefTableLoad (FILTER_Core_t FilterCore, ScalerFilterTable_t Table, void const* Table_p);
/* Global Variables --------------------------------------------------------- */

/* Functions ---------------------------------------------------------------- */
static const FilterTblsSet_t* filtTabSet[TBL_FORM_SIZE][2] =
{
    {&FTabLoadVerYuv,   &FTabLoadVerRgb},    //TBL_FORM_VF
    {&FTabLoadHorYuv,   &FTabLoadHorRgb},    //TBL_FORM_HF
    //{&FTabLoadHQVFYuv,  &FTabLoadHQVFRgb},   //TBL_FORM_HQVF
    {&FTabLoadVerYuv,  &FTabLoadVerRgb},   //TBL_FORM_HQVF
    {&FTabLoadHorYuv,   &FTabLoadHorRgb},    //TBL_FORM_HQHF
    {&FTabLoadHQVFYuv,  &FTabLoadHQVFRgb}    //TBL_FORM_DF
};


static const FilterTblsSet_t* filtTabSetGfx[TBL_FORM_SIZE][2] =
{
    {&FTabLoadVerYuv,   &FTabLoadVerRgbGfx},    //TBL_FORM_VF
    {&FTabLoadHorYuv,   &FTabLoadHorRgb},    //TBL_FORM_HF
    //{&FTabLoadHQVFYuv,  &FTabLoadHQVFRgb},   //TBL_FORM_HQVF
    {&FTabLoadVerYuv,  &FTabLoadVerRgbGfx},   //TBL_FORM_HQVF
    {&FTabLoadHorYuv,   &FTabLoadHorRgb},    //TBL_FORM_HQHF
    {&FTabLoadHQVFYuv,  &FTabLoadHQVFRgb}    //TBL_FORM_DF
};

/*******************************************************************************
Name        : FilterTableLoad
Description : Selects table parameters (size, filter destination and byte ordering)
              and calls CoefTableLoad,
Parameters  : CH: channel,  TabType: ScalerTableFormat Scale: scaling ratio IsRGB:YUV or RGB
Assumptions :
Limitations :
Returns     : FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FilterTableLoad(FILTER_Core_t FilterCore, ScalerTableFormat_t TabType, uint32_t Scale, bool IsRGB, bool IsProcessingRGB)
{
    FVDP_Result_t result = FVDP_OK;
    const FilterTblsSet_t* pFilterTblsSet;
    FilterTbls4P_t const* pTable;
    uint16_t count;

    if(IsProcessingRGB)
        pFilterTblsSet = filtTabSetGfx[TabType][IsRGB ? 1 : 0];
    else
        pFilterTblsSet = filtTabSet[TabType][IsRGB ? 1 : 0];

    FilterTable_GetBounderIndex(TabType, Scale, IsRGB, &count, IsProcessingRGB);

    pTable = pFilterTblsSet->pFilterTable + count;
    result |= CoefTableLoad(FilterCore, FilterTable_Dest[TabType][0], pTable->pUV);
    result |= CoefTableLoad(FilterCore, FilterTable_Dest[TabType][1], pTable->pY);
    result |= CoefTableLoad(FilterCore, FilterTable_Dest[TabType][2], pTable->pUVpk);
    result |= CoefTableLoad(FilterCore, FilterTable_Dest[TabType][3], pTable->pYpk);

    return result;
}


/*******************************************************************************
Name        : CoefTableLoad
Description : Load Coef table parameters (size, filter destination and byte ordering)
              Write into target address
Parameters  : CH: channel,  TabDest: table destination pTable:pointer to table
Assumptions :
Limitations :
Returns     : FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t CoefTableLoad(FILTER_Core_t FilterCore,
    ScalerFilterTable_t TabDest, void const* pTable)
{
    uint32_t Addr, Size;
    bool Status;
    uint16_t count;

    if ((FilterCore >= FILTER_NUM_CORES) || (TabDest >= FLTTBL_SIZE))
    {
        return FVDP_ERROR;
    }

    if ((Addr = FilterTable_Adress[FilterCore][TabDest]) == 0)
    {
        return FVDP_ERROR;
    }

    Size = (uint32_t) FilterTable_size[FilterTable_format[FilterCore][TabDest]];

    Size = Size/4;

    //Check block whether block is enabled or not.
    if ((FilterTable_format[FilterCore][TabDest] == TBL_FORM_HQVF)
        ||(FilterTable_format[FilterCore][TabDest] == TBL_FORM_HQHF))
    {
        Status = HQScalerCheckBeforeTabLoad(FilterCore, FilterTable_format[FilterCore][TabDest]);
    }
    if (FilterTable_format[FilterCore][TabDest] == TBL_FORM_DF)
    {
        Status = DeintCheckBeforeTabLoad(FilterCore, FilterTable_format[FilterCore][TabDest]);
    }
    else
    {
        Status = ScalerCheckBeforeTabLoad(FilterCore, FilterTable_format[FilterCore][TabDest]);
    }

    if(Status == TRUE)
    {
        for (count=0;count<=(Size);count++)
        {
            #pragma pack(1)
            union
            {
                uint32_t dw;
                uint8_t b[4];
            } reg;
            #pragma pack()
            reg.dw = 0;
            if ((pTable != 0))
            {
                reg.b[0] = *(uint8_t const*)pTable++;
                reg.b[1] = *(uint8_t const*)pTable++;
                reg.b[2] = *(uint8_t const*)pTable++;
                reg.b[3] = *(uint8_t const*)pTable++;

                REG32_Write(Addr, reg.dw);
            }

            Addr +=4;
            reg.dw = 0;
        }
    }
    else
    {
        return FVDP_ERROR;
    }

    return FVDP_OK;
}


/*******************************************************************************
Name        : HQScalerCheckBeforeTabLoad
Description : Checks is HQ scaler which is prepared for loading (enabled),
              if not function enables it and forces to update.
Parameters  : Channel:
              Input FilterCore information
Returns     : TRUE if HQ scaler was prepared properly, FALSE - otherwise
Example     :
                //to force Deinterlacer be enable
                DeintCheckBeforeTabLoad(CH);
*******************************************************************************/
static bool HQScalerCheckBeforeTabLoad(FILTER_Core_t FilterCore, ScalerTableFormat_t TableFormat)
{
    bool             isValid;

    if (FilterCore != FILTER_BE)
        return FALSE;

    if(TableFormat == TBL_FORM_HQVF)
    {
        if (HFHQS_REG32_Read(HFHQS_CFG)& HFHQS_EN_MASK)
        {
            isValid =  TRUE;
        }
        else
        {
         // enable scaler block
//            HFHQS_REG32_Set(HFHQS_CFG, HFHQS_EN_MASK);
            isValid = FALSE; // block doesn't enable
        }
    }
    else if(TableFormat == TBL_FORM_HQVF)
    {
        if (VFHQS_REG32_Read(VFHQS_CFG)& VFHQS_EN_MASK)
        {
            isValid = TRUE;
        }
        else
        {
         //  enable scaler block
//            VFHQS_REG32_Set(VFHQS_CFG, VFHQS_EN_MASK);
            isValid =FALSE; // block doesn't enable
        }
    }
    else
    {
        isValid =FALSE;
    }

    return isValid;
}
/*******************************************************************************
Name        : DeintCheckBeforeTabLoad
Description : Checks is Deinterlacer prepared for loading (enabled),
              if not function enables it and forces to update.
Parameters  : Channel:
              Input Chanel information (only CH_A has a DeInt at present time)
Returns     : TRUE if Deint was prepared properly, FALSE - otherwise
Example     :
                //to force Deinterlacer be enable
                DeintCheckBeforeTabLoad(CH);
*******************************************************************************/
static bool DeintCheckBeforeTabLoad(FILTER_Core_t FilterCore, ScalerTableFormat_t TableFormat)
{
    if (FilterCore != FILTER_FE)
    {
        return FALSE;
    }

    if(TableFormat == TBL_FORM_DF)
    {
       if ((DEINT_REG32_Read(DEINT_TOP_CTRL) & DEINT_ENABLE_MASK) != 0)
       {
           return TRUE;
       }
       else
       {
        //  enable scaler block
//           DEINT_REG32_Set(DEINTC_CTRL, DEINTC_INT_COEF_EN_MASK);
           return FALSE; // block doesn't enable
       }
    }
    else
    {
        return FALSE;
    }

}

/*******************************************************************************
Name        : ScalerCheckBeforeTabLoad
Description : to check if Scaler block is prepared for loading (otherwize f/w can stuck here!)
                and prepares block to avoid hang.
Parameters  : CH: channel,
Assumptions :
Limitations :
Returns     : TRUE if Block was prepared properly, FALSE - otherwise
*******************************************************************************/
bool ScalerCheckBeforeTabLoad(FILTER_Core_t FilterCore, ScalerTableFormat_t TableFormat)
{
    bool isValid;
    FVDP_CH_t CH;

   switch(FilterCore)
   {
       case FILTER_FE:
           CH = FVDP_MAIN;
       break;

       case FILTER_PIP:
           CH = FVDP_PIP;
       break;

       case FILTER_AUX:
           CH = FVDP_AUX;
       break;

       case FILTER_ENC:
           CH = FVDP_ENC;
       break;

        default:
           FilterCore = FILTER_FE;
           CH = FVDP_MAIN;
       break;
   }

   //Check block  whether it is enabled or not
    if(TableFormat == TBL_FORM_VF)
    {
        isValid = ((VFLITE_REG32_Read(CH, VFLITE_CFG) & VFLITE_EN_MASK) != 0);
    }
    else if(TableFormat == TBL_FORM_HF)
    {
        isValid = ((HFLITE_REG32_Read(CH, HFLITE_CFG) & HFLITE_EN_MASK) != 0);
    }
    else
    {
        isValid = TRUE;
    }

    return isValid;

}

/*******************************************************************************
Name        : FilterTable_GetBounderIndex
Description : get the FilterTable boundar index
Parameters  : CH: channel,  TabType: ScalerTableFormat,
              Scale: scaling ratio, IsRGB:YUV or RGB
Assumptions :
Limitations :
Returns     : TRUE
*******************************************************************************/
FVDP_Result_t FilterTable_GetBounderIndex(ScalerTableFormat_t TabType, uint32_t Scale, bool IsRGB, uint16_t *Index, bool IsProcessingRGB)
{
    FVDP_Result_t result = FVDP_OK;
    const FilterTblsSet_t* pFilterTblsSet;
    uint16_t count;

    if(IsProcessingRGB)
        pFilterTblsSet = filtTabSetGfx[TabType][IsRGB ? 1 : 0];
    else
        pFilterTblsSet = filtTabSet[TabType][IsRGB ? 1 : 0];

    //Increase scaling value to give 0x800 margin which will use to select the index number in Bounder table
     Scale += BOUNDARY_MARGIN;

    //Search coefficient Table base on scaling ratio
    for (count = 0;  (Scale > *(pFilterTblsSet->pBounders + count)) && (count < pFilterTblsSet->BoundersNum); count++);

    *Index = count;

    return result;
}
