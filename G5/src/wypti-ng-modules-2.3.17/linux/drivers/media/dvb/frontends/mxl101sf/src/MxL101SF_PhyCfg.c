/*******************************************************************************
 *
 * FILE NAME          : MxL101SF_PhyCfg.cpp
 * 
 * AUTHOR             : Brenndon Lee
 *
 * DATE CREATED       : 1/22/2008
 *
 * DESCRIPTION        : This file contains control parameters for only MxL101SF 
 *                             
 *******************************************************************************
 *                Copyright (c) 2006, MaxLinear, Inc.
 ******************************************************************************/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "dvb_frontend.h"

#include <linux/rtc.h>
#include <linux/kdev_t.h>
#include <linux/idr.h>

#include "MxL101SF_PhyDefs.h"

REG_CTRL_INFO_T MxL_101OverwriteDefault[] = 
{
  {0xC8, 0xFF, 0x40},  
  {0x8D, 0x01, 0x01},  
  {0x42, 0xFF, 0x33},  
  {0x74, 0xFF, 0xC4},  
  {0x71, 0xFF, 0xE6},  
  {0x83, 0xFF, 0x64},  
  {0x85, 0xFF, 0x64},  
  {0x88, 0xFF, 0xF0},  
  {0x6F, 0xFF, 0xB7},  
  {0x00, 0xFF, 0x01},  
  {0x81, 0xFF, 0x11},  
  {0xF4, 0xFF, 0x07},  
  {0xD4, 0x1F, 0x2F},  
  {0xD6, 0xFF, 0x0C},  
  {0x00, 0xFF, 0x00},  
  {0, 0, 0}
};

REG_CTRL_INFO_T MxL_101SuspendMode[] =
{
  {0x1C, 0xFF, 0x00}, 
  {0x01, 0xFF, 0x00}, 
  {0x04, 0xFF, 0x02}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_101WakeUp[] =
{
  {0x04, 0xFF, 0x00},
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_MpegDataOutToTSIF[] =
{
  {0x17, 0xC0, 0xC0},
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_PhySoftReset[] =     
{
  {0xFF, 0xFF, 0x00}, 
  {0x02, 0xFF, 0x01}, 
  {0,    0,    0}
};

REG_CTRL_INFO_T MxL_TunerDemodMode[] =
{
  {0x03, 0xFF, 0x01},  
  {0x7D, 0x40, 0x00},  
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_TunerMode[] =
{
  {0x03, 0xFF, 0x00}, 
  {0x7D, 0x40, 0x40},  
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_TopMasterEnable[] =
{
  {0x01, 0xFF, 0x01}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_TopMasterDisable[] =
{
  {0x01, 0xFF, 0x00}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_PhyTuneRF[] = 
{
  {0x1D, 0x7F, 0x00},  
  {0x1E, 0xFF, 0x00},  
  {0x1F, 0xFF, 0x00},  
  {0,    0,    0}
};

REG_CTRL_INFO_T MxL_IrqClear[] =
{
  {0x0E, 0xFF, 0xFF}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_ResetPerCount[] =
{
  {0x20, 0x01, 0x01}, 
  {0x20, 0x01, 0x00}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_CableSettings[] =
{
  {0x0A, 0x10, 0x10}, 
  {0x0A, 0x0F, 0x04}, 
  {0x3F, 0x3F, 0x01}, 
  {0x44, 0xF0, 0x60}, 
  {0x46, 0xF0, 0x10}, 
  {0x48, 0xF0, 0x70}, 
  {0x48, 0x0F, 0x0C}, 
  {0x0D, 0x03, 0x02}, 
  {0x4D, 0xFF, 0x40}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_EnableCellId[] =
{
  {0x00, 0xFF, 0x01}, 
  {0x7E, 0xFF, 0x05}, 
  {0x00, 0xFF, 0x00}, 
  {0,    0,    0}
}; 

REG_CTRL_INFO_T MxL_DisableCellId[] =
{
  {0x00, 0xFF, 0x01}, 
  {0x7E, 0xFF, 0x04}, 
  {0x00, 0xFF, 0x00}, 
  {0,    0,    0}
}; 


/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_ProgramRegisters
--|
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/15/2008
--|
--| DESCRIPTION   : This function writes multiple registers with provided data array.
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_ProgramRegisters(PREG_CTRL_INFO_T ctrlRegInfoPtr)
{
  UINT8 status = MXL_TRUE;
  UINT16 i;
  UINT8 tmp;
  
  for (i = 0; ctrlRegInfoPtr[i].regAddr|ctrlRegInfoPtr[i].mask|ctrlRegInfoPtr[i].data; i++)
  {
    // Check if partial bits of register were updated
    if (ctrlRegInfoPtr[i].mask != 0xFF)  
    {
      status = Ctrl_ReadRegister(ctrlRegInfoPtr[i].regAddr, &tmp);
      if (status != MXL_TRUE) break;;
    }
    
    tmp &= ~ctrlRegInfoPtr[i].mask;
    tmp |= ctrlRegInfoPtr[i].data;

    status |= Ctrl_WriteRegister(ctrlRegInfoPtr[i].regAddr, tmp);
    if (status != MXL_TRUE) break;
  }

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_PhyTune
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/15/2008
--|
--| DESCRIPTION   : Tune to the specified RF frequency with bandwidth
--|                 
--|                 The provided input frequency and bandwidth's unit is MHz. 
--|
--| RETURN VALUE  : PREG_CTRL_INFO_T
--|
--|---------------------------------------------------------------------------*/

PREG_CTRL_INFO_T Ctrl_PhyTune(REAL32 frequency, UINT8 bandwidth)
{
  UINT32 freq; 
  UINT8 filt_bw;
  UINT32 tmp;

  // Set Channel Bandwidth
  switch (bandwidth)
  {
    case 6:
      filt_bw = 21;  
      break;

    case 7:
      filt_bw = 42;  
      break;

    case 8:
      filt_bw = 63;  
      break;
  
    default:
      filt_bw = 0x63;  
      break;
  }
  
  // Calculate RF Channel
  tmp = frequency % 1000000;
  frequency /= 1000000;

  frequency *= 64;
  tmp *= 64;
  tmp += 500000;
  tmp /= 1000000;

  // Do round
//  frequency += 0.5;
  freq = (UINT32)frequency + (UINT32)tmp;
 
  // Abort Tune. This control is in the arrary
  // Set Bandwidth
  MxL_PhyTuneRF[0].data = filt_bw;

  // Set RF
  MxL_PhyTuneRF[1].data = (freq & 0xFF);       // Fractional part
  MxL_PhyTuneRF[2].data = (freq >> 8 ) & 0xFF; // Integer part

  // Start Tune. This control is in the arrary 
  return MxL_PhyTuneRF;
}
