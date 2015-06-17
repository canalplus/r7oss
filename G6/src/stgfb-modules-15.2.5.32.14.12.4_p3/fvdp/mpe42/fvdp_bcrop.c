/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_bcrop.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "fvdp_hostupdate.h"
#include "fvdp_bcrop.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */
static const uint32_t BCROP_BASE_ADDR[] = {BDR_BE_BASE_ADDRESS, BDR_PIP_BASE_ADDRESS, BDR_AUX_BASE_ADDRESS, BDR_ENC_BASE_ADDRESS};

#define BCROP_REG32_Write(Ch,Addr,Val)                REG32_Write(Addr+BCROP_BASE_ADDR[Ch],Val)
#define BCROP_REG32_Read(Ch,Addr)                     REG32_Read(Addr+BCROP_BASE_ADDR[Ch])
#define BCROP_REG32_Set(Ch,Addr,BitsToSet)            REG32_Set(Addr+BCROP_BASE_ADDR[Ch],BitsToSet)
#define BCROP_REG32_Clear(Ch,Addr,BitsToClear)        REG32_Clear(Addr+BCROP_BASE_ADDR[Ch],BitsToClear)
#define BCROP_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                    REG32_ClearAndSet(Addr+BCROP_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
#define BCROP_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)    REG32_SetField(Addr+BCROP_BASE_ADDR[Ch],Field,ValueToSet)

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */
static void BorderCrop_HostUpdate(FVDP_CH_t  CH);

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : BorderCrop_Init
Description : Sets border control values like color compression and color
                conversion settings

Parameters  : CH, selects FVDP channel
              BorderCtrlInitVal - initial value for border settings

Assumptions : This function needs to be called during open channel
Limitations :
Returns     :
*******************************************************************************/

void BorderCrop_Init(FVDP_CH_t CH, uint32_t BorderCtrlInitVal)
{
    BCROP_REG32_Write(CH, BC_CTRL, BorderCtrlInitVal);

    // Set the Host Update flag
    BorderCrop_HostUpdate(CH);

}

/*******************************************************************************
Name        : BorderCrop_Enable
Description : Enables/disables borders on the screen.

Parameters  : CH, selects FVDP channel
              Enable,   TRUE - enable borders. FALSE - disable borders.

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t BorderCrop_Enable(FVDP_CH_t CH, bool Enable)
{

    if(Enable)
    {
        BCROP_REG32_Set(CH, BC_CTRL, BC_EN_MASK);
    }
    else
    {
        BCROP_REG32_Clear(CH, BC_CTRL, BC_EN_MASK);
    }

    // Set the Host Update flag
    BorderCrop_HostUpdate(CH);
    return FVDP_OK;
}

/*******************************************************************************
Name        : BorderCrop_Config
Description : Enables/disables borders on the screen.

Parameters  : CH, selects FVDP channel
              Enable,   TRUE - enable borders. FALSE - disable borders.
              Input_H_Size,     horizontal size of input (active) image
              Input_V_Size,     verticalal size of input (active) image
              Output_H_Size,    horizontal size of output(active + borders) image
              Output_V_Size,    vertical size of output(active + borders) image
              H_Start,          width of the left border
              V_Start,          height of the top border
Assumptions : bottom border height = Output_V_Size - Input_V_Size - V_Start
              right border width   = Output_H_Size - Input_H_Size - H_Start
              border color - black;
Limitations :
Returns     : FVDP_OK, FVDP_ERROR,
*******************************************************************************/
FVDP_Result_t BorderCrop_Config(    FVDP_CH_t  CH,
                                    uint32_t Input_H_Size,
                                    uint32_t Input_V_Size,
                                    uint32_t Output_H_Size,
                                    uint32_t Output_V_Size,
                                    uint32_t H_Start,
                                    uint32_t V_Start)
{
    uint32_t Border1_Left, Border1_Right, Border1_Top, Border1_Bottom;

    if (Output_H_Size < Input_H_Size || Output_V_Size < Input_V_Size || \
    	H_Start > (Output_H_Size - Input_H_Size) || V_Start > (Output_V_Size - Input_V_Size))
    	return FVDP_ERROR;

    /* Input Size */
    BCROP_REG32_ClearAndSet(CH, BC_IMAGE_IN, BC_HACT_IN_MASK, BC_HACT_IN_OFFSET, Input_H_Size);
    BCROP_REG32_ClearAndSet(CH, BC_IMAGE_IN, BC_VACT_IN_MASK, BC_VACT_IN_OFFSET, Input_V_Size);

    /* Crop */
    // No crop implemented

    /* Border # 1 */
    Border1_Left = H_Start;
    Border1_Right = Output_H_Size - H_Start - Input_H_Size;
    Border1_Top = V_Start;
    Border1_Bottom = Output_V_Size - V_Start - Input_V_Size;

    /* Border 1 Dimensions */
    BCROP_REG32_ClearAndSet(CH, BC_HOR_BOR1, BC_BOR1_LFT_MASK, BC_BOR1_LFT_OFFSET, Border1_Left);
    BCROP_REG32_ClearAndSet(CH, BC_HOR_BOR1, BC_BOR1_RGT_MASK, BC_BOR1_RGT_OFFSET, Border1_Right);
    BCROP_REG32_ClearAndSet(CH, BC_VER_BOR1, BC_BOR1_TOP_MASK, BC_BOR1_TOP_OFFSET, Border1_Top);
    BCROP_REG32_ClearAndSet(CH, BC_VER_BOR1, BC_BOR1_BOT_MASK, BC_BOR1_BOT_OFFSET, Border1_Bottom);

    /* Border 1 Color. Black always. */
    BCROP_REG32_ClearAndSet(CH, BC_COL_BOR1, BC_COL1_Y_G_MASK, BC_COL1_Y_G_OFFSET, 64);
    BCROP_REG32_ClearAndSet(CH, BC_COL_BOR1, BC_COL1_U_B_MASK, BC_COL1_U_B_OFFSET, 512);
    BCROP_REG32_ClearAndSet(CH, BC_COL_BOR1, BC_COL1_V_R_MASK, BC_COL1_V_R_OFFSET, 512);

    /* Border 2 */
    // No border 2 used

    // Set the Host Update flag
    BorderCrop_HostUpdate(CH);

    return FVDP_OK;
}

/*******************************************************************************
Name        : BorderCrop_HostUpdate
Description : Requests host update for border crop pending active registers;

Parameters  : CH, selects FVDP channel
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void BorderCrop_HostUpdate(FVDP_CH_t  CH)
{
    if (CH == FVDP_MAIN)
    {
        HostUpdate_SetUpdate_FE(BE_HOST_UPDATE_BDR, SYNC_UPDATE);
    }
    else
    {
        if (CH == FVDP_ENC)
        {
            // ENC channel requires FORCE UPDATE for mem to mem
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_BDR, FORCE_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_BDR, SYNC_UPDATE);
        }
    }
}
