/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_system_vmux.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_ivp.h"
#include "fvdp_color.h"
#include "fvdp_common.h"
#include "fvdp_router.h"
#include "fvdp_scalerlite.h"
#include "fvdp_hqscaler.h"
#include "fvdp_datapath.h"
#include "fvdp_fbm.h"
#include "fvdp_vcpu.h"
#include "fvdp_eventctrl.h"

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_VMUX_Init(FVDP_CH_t CH)
{
    FVDP_Result_t   ErrorCode = FVDP_OK;

    ErrorCode = IVP_Init(CH);

    return ErrorCode;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_VMUX_Config(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t     ErrorCode = FVDP_OK;
    FVDP_CH_t         CH;
    FVDP_Input_t      InputSource;
    FVDP_ColorSpace_t ColorSpace;
    IVP_Src_t         IvpSourceMux;
    FVDP_Frame_t*     CurrentInputFrame_p;
    FVDP_FieldType_t FieldType;
    bool              HighShrinkCompensation = FALSE;

    CH = HandleContext_p->CH;
    CurrentInputFrame_p = HandleContext_p->FrameInfo.CurrentInputFrame_p;

   // one line IVP input cropping for HighShrink doesn't work.
   // Use Input ODDi signal
    if(((CH == FVDP_PIP)||(CH == FVDP_AUX))
        &&(CurrentInputFrame_p->InputVideoInfo.Height/CurrentInputFrame_p->OutputVideoInfo.Height > 1))
    {
        HighShrinkCompensation = TRUE;
    }

     if(HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC)
     {
         CurrentInputFrame_p->InputVideoInfo.ScanType = PROGRESSIVE;
     }

    if((CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
        && (SW_ODD_FIELD_CONTROL == TRUE))
    {
        if (CH == FVDP_MAIN)
        {
            EventCtrl_GetCurrentFieldType_BE(INPUT_SOURCE, &FieldType);
        }
        else
        {
            EventCtrl_GetCurrentFieldType_LITE(CH, INPUT_SOURCE, &FieldType);
        }

        if(CurrentInputFrame_p->InputVideoInfo.FieldType == AUTO_FIELD_TYPE)
        {
            CurrentInputFrame_p->InputVideoInfo.FieldType = FieldType;
        }
        else
        {
            if(CurrentInputFrame_p->InputVideoInfo.FieldType != FieldType)
            {
                if(HighShrinkCompensation == TRUE)
                {
                    IVP_FieldControl(CH, 0);
                }
                else
                {
                    IVP_FieldControl(CH, 1);
                }
            }
            else
            {
                if(HighShrinkCompensation == TRUE)
                {
                    IVP_FieldControl(CH, 1);
                }
                else
                {
                    IVP_FieldControl(CH, 0);
                }
            }
        }
    }
    else //PROGRESSIVE
    {
        if(CurrentInputFrame_p->InputVideoInfo.FieldType == AUTO_FIELD_TYPE)
        {
            CurrentInputFrame_p->InputVideoInfo.FieldType = TOP_FIELD;
        }
    }

    if (HandleContext_p->UpdateFlags.InputUpdateFlag & UPDATE_INPUT_SOURCE)
    {
        InputSource = HandleContext_p->InputSource;
        ColorSpace  = CurrentInputFrame_p->InputVideoInfo.ColorSpace;

        switch(InputSource)
        {
            case MDTP0:
            {
                if (ColorSpace == RGB || ColorSpace == sRGB || ColorSpace == RGB861)
                {
                    IvpSourceMux = IVP_SRC_MDTP0_RGB;
                }
                else
                {
                    IvpSourceMux = IVP_SRC_MDTP0_YUV;
                }
                break;
            }
            case MDTP1:
            {
                if (ColorSpace == RGB || ColorSpace == sRGB || ColorSpace == RGB861)
                {
                    IvpSourceMux = IVP_SRC_MDTP1_RGB;
                }
                else
                {
                    IvpSourceMux = IVP_SRC_MDTP1_YUV;
                }
                break;
            }
            case MDTP2:
            {
                if (ColorSpace == RGB || ColorSpace == sRGB || ColorSpace == RGB861)

                {
                    IvpSourceMux = IVP_SRC_MDTP2_RGB;
                }
                else
                {
                    IvpSourceMux = IVP_SRC_MDTP2_YUV;
                }
                break;
            }
            case VTAC1:
            {
                IvpSourceMux = IVP_SRC_VTAC1;
                break;
            }
            case VTAC2:
            {
                IvpSourceMux = IVP_SRC_VTAC2;
                break;
            }
            case VXI:
                IvpSourceMux = IVP_SRC_DIP;
                break;

            case DTG:
                IvpSourceMux = IVP_SRC_DTG;
                break;

            default:
                return FVDP_ERROR;
                break;
        }

        // Set the ISM Mux
        ErrorCode = IVP_SourceSet(CH, IvpSourceMux);

    }

    if (HandleContext_p->UpdateFlags.InputUpdateFlag &
        (UPDATE_INPUT_RASTER_INFO| UPDATE_INPUT_VIDEO_INFO| UPDATE_CROP_WINDOW))
    {

        IVP_Update(CH, &(CurrentInputFrame_p->InputVideoInfo),
                       &(CurrentInputFrame_p->CropWindow),
                       &(HandleContext_p->InputRasterInfo),
                       HandleContext_p->ProcessingType);
    }
    else
    {
        // Currently InputVideo ColorSampling set to 4:2:2 always in FVDP Input stage.
        // Later on, it may need to handle RGB path as RGB 4:4:4 ColorSpace.
        if(HandleContext_p->ProcessingType != FVDP_RGB_GRAPHIC)
        {
            CurrentInputFrame_p->InputVideoInfo.ColorSpace = sdYUV;
            CurrentInputFrame_p->InputVideoInfo.ColorSampling = FVDP_422;
        }
    }

    return ErrorCode;
}

