/***********************************************************************
 *
 * File: fvdp_test.c
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#if FVDP_IOCTL

/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_test.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : FVDP_Read_Register
Description : Read an FVDP Register
Parameters  : [in] offset reg, for example 0x340c00 to read fvdp register 0xfd340c00
              [in] pointer to write the return value

Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t FVDP_ReadRegister(uint32_t addr, uint32_t* value)
{
    *value = REG32_Read(addr);
    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_Write_Register
Description : Write to an FVDP Register
Parameters  : [in] offset reg, for example 0x340c00 to read fvdp register 0xfd340c00
              [in] value to write
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t FVDP_WriteRegister(uint32_t addr, uint32_t value)
{
    REG32_Write(addr, value);
    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_GetBufferCallback
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t FVDP_GetBufferCallback(uint32_t* AddrScalingComplete, uint32_t* AddrBufferDone)
{
    // TODO: Somehow get address of the callback functions
    //*AddrScalingComplete = &FVDP_DEBUG_ScalingCompleted;
    //*AddrBufferDone = &FVDP_DEBUG_BufferDone;

    return FVDP_OK;
}

#endif // #ifdef FVDP_IOCTL
