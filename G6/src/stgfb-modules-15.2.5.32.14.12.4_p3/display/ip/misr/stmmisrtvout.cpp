/***********************************************************************
 *
 * File: display/ip/misr/stmmisr.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <stm_display.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayPlane.h>
#include <display/generic/DisplayDevice.h>
#include <display/ip/displaytiming/stmvtg.h>
#include <display/ip/stmmisrviewport.h>
#include "stmmisrtvout.h"

CSTmMisrTVOut::CSTmMisrTVOut(CDisplayDevice *pDev, uint32_t MisrPFCtrlReg, uint32_t MisrOutputCtrlReg)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  m_pDevReg = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_MisrPFCtrlReg     = MisrPFCtrlReg;
  m_MisrOutputCtrlReg = MisrOutputCtrlReg;
  ResetMisrState();
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmMisrTVOut::~CSTmMisrTVOut() {}

void CSTmMisrTVOut::ResetMisrState(void)
{
  TRC( TRC_ID_MISR, "" );
  vibe_os_zero_memory(&m_MisrData, sizeof(Misr_t));
  m_isMisrConfigured = false;
  ResetMisrCtrlParams(&m_MisrPF, 0);
  ResetMisrCtrlParams(&m_MisrOutput, 1);
}

void CSTmMisrTVOut::ResetMisrCtrlParams(MisrConfig_t *ctrlParams, uint8_t index)
{
  TRC( TRC_ID_MISR, "index : %u", index );
  uint32_t ctrlReg = 0;

  ctrlParams->isMisrCaptureStarted   = false;
  ctrlParams->misrCtrlVal            = TVO_ENABLE_MISR | TVO_COUNTER_INTERNAL | TVO_PROGRESSIVE_MISR | TVO_MISR_STATUS_RSLT_VALID;
  ctrlParams->misrVportMax           = 0x0;
  ctrlParams->misrVportMin           = 0x0;
  ctrlParams->misrStoreIndex         = index;
  if(index == 0)
    ctrlReg = m_MisrPFCtrlReg;
  else if(index == 1)
    ctrlReg = m_MisrOutputCtrlReg;
  WriteMisrReg((ctrlReg + TVO_VPORT_MAX_OFFSET),ctrlParams->misrVportMax);
  WriteMisrReg((ctrlReg + TVO_VPORT_MIN_OFFSET),ctrlParams->misrVportMin);
  WriteMisrReg(ctrlReg, (ctrlParams->misrCtrlVal & ~TVO_ENABLE_MISR));
}

TuningResults CSTmMisrTVOut::CollectMisrValues(void *output)
{
    TRC( TRC_ID_MISR, "misrData : %p", output );
    SetTuningOutputData_t *misrData = (SetTuningOutputData_t *)output;
    TuningResults res = TUNING_INVALID_PARAMETER;

    if(misrData != NULL)
    {
        misrData->Data.Misr = m_MisrData;
        /* clear m_MisrData for next Vsync */
        vibe_os_zero_memory(&m_MisrData, sizeof(Misr_t));
        res = TUNING_OK;
    }
    else
    {
        res = TUNING_NO_DATA_AVAILABLE;
    }

    return res;
}

void CSTmMisrTVOut::ReadMisrSign(MisrConfig_t *misr, uint32_t ctrlReg)
{
    TRC( TRC_ID_MISR, "misr : %p, ctrlReg : %u", misr, ctrlReg );
    m_MisrData.MisrValue[misr->misrStoreIndex].Control = ReadMisrReg (ctrlReg);
    m_MisrData.MisrValue[misr->misrStoreIndex].Status  = ReadMisrReg (ctrlReg + TVO_STATUS_OFFSET);

    TRC( TRC_ID_MISR, "Wrote %x, Written: MISR Control=%x,Status: %x Max %x Min %x",
        misr->misrCtrlVal, m_MisrData.MisrValue[misr->misrStoreIndex].Control,  m_MisrData.MisrValue[misr->misrStoreIndex].Status, ReadMisrReg (ctrlReg + TVO_VPORT_MAX_OFFSET), ReadMisrReg (ctrlReg + TVO_VPORT_MIN_OFFSET));

    /*
     * Check whether the MISR new signatures have been read on time by host
     */
    if((m_MisrData.MisrValue[misr->misrStoreIndex].Status)& TVO_MISR_STATUS_DATA_LOSS)
    {
      m_MisrData.MisrValue[misr->misrStoreIndex].LostCnt  = 1;
    }
    else
    {
      m_MisrData.MisrValue[misr->misrStoreIndex].LostCnt  = 0;
    }

    /*
     * Check whether the signature results for the tests are valid.
     */
    if((m_MisrData.MisrValue[misr->misrStoreIndex].Status) & TVO_MISR_STATUS_RSLT_VALID)
    {
      // Read MISR signatures
      m_MisrData.MisrValue[misr->misrStoreIndex].Reg1 = ReadMisrReg (ctrlReg + TVO_REG_1_OFFSET);
      m_MisrData.MisrValue[misr->misrStoreIndex].Reg2 = ReadMisrReg (ctrlReg + TVO_REG_2_OFFSET);
      m_MisrData.MisrValue[misr->misrStoreIndex].Reg3 = ReadMisrReg (ctrlReg + TVO_REG_3_OFFSET);
      m_MisrData.MisrValue[misr->misrStoreIndex].Valid = true;

      // Reset Control Register to enable MISR ip for next calculation
      TRC( TRC_ID_MISR, "control reg = %x", misr->misrCtrlVal);
      WriteMisrReg((ctrlReg + TVO_VPORT_MAX_OFFSET),misr->misrVportMax);
      WriteMisrReg((ctrlReg + TVO_VPORT_MIN_OFFSET),misr->misrVportMin);
      WriteMisrReg(ctrlReg, misr->misrCtrlVal);

      TRC( TRC_ID_MISR, "Reg1 = %x, Reg2 = %x, Reg3 =%x", m_MisrData.MisrValue[misr->misrStoreIndex].Reg1,
                m_MisrData.MisrValue[misr->misrStoreIndex].Reg2, m_MisrData.MisrValue[misr->misrStoreIndex].Reg3);
    }
    else
    {
      TRC( TRC_ID_MISR, "Status=0, not captured MISR on this instance");
    }
}

void CSTmMisrTVOut::UpdateMisrCtrl(MisrConfig_t *misr, const stm_display_mode_t *pCurrentMode)
{
    TRC( TRC_ID_MISR, "misr : %p, pCurrentMode : %p", misr, pCurrentMode );
    /*
     * if misr capture is not started yet, we have nothing to do right now.
     * we will do the full configuration in SetMisrCtrl
     */
    if (misr->isMisrCaptureStarted == true) {
        MisrConfigViewPort(pCurrentMode, &misr->misrVportMax, &misr->misrVportMin, &misr->misrCtrlParams);
        MisrConfigScanningMode(pCurrentMode, &misr->misrCtrlVal);
        MisrConfigVsyncValue(&misr->misrCtrlVal, &misr->misrCtrlParams);

        /*
         * do not program HW registers right now since HW is currently used.
         * The new configuration will be automatically taken into account at the
         * next call of ReadMisrSign function
         */
    }
}

TuningResults CSTmMisrTVOut::SetMisrCtrl(MisrConfig_t *misr, uint32_t ctrlReg, SetTuningInputData_t *input, const stm_display_mode_t *pCurrentMode)
{
    TRC( TRC_ID_MISR, "misr : %p, ctrlReg : %u, input : %p, pCurrentMode : %p", misr, ctrlReg, input, pCurrentMode );
    misr->misrStoreIndex = input->MisrStoreIndex;
    if (misr->misrStoreIndex >= MAX_MISR_COUNTER)
    {
      TRC( TRC_ID_ERROR, "Invalid misrStoreIndex!!" );
      return TUNING_INVALID_PARAMETER;
    }

    misr->misrCtrlParams = input->MisrCtrlVal;
    misr->isMisrCaptureStarted = true;

    UpdateMisrCtrl(misr, pCurrentMode);

    WriteMisrReg((ctrlReg + TVO_VPORT_MAX_OFFSET),misr->misrVportMax);
    WriteMisrReg((ctrlReg + TVO_VPORT_MIN_OFFSET),misr->misrVportMin);
    WriteMisrReg(ctrlReg, misr->misrCtrlVal);

    TRC( TRC_ID_MISR, "Ctrl value to set: %x, Ctrl value Read: %x", misr->misrCtrlVal, ReadMisrReg(ctrlReg));

    return TUNING_OK;
}

void CSTmMisrTVOut::MisrConfigVsyncValue(uint32_t *misrControlVal, MisrControl_t *MisrCtrlVal)
{
    TRC( TRC_ID_MISR, "misrControlVal : %u, MisrCtrlVal->VsyncVal : %d", *misrControlVal, MisrCtrlVal->VsyncVal );
    uint32_t controlVal   = *misrControlVal;

    /* Reset VsyncVal to default (every vsync) */
    controlVal &= ~(TVO_CAPTURE_VSYNC_MASK);
    /* Handle different vsync value if it is requested by user */
    switch(MisrCtrlVal->VsyncVal)
    {
        case MISR_ALTERNATE_VSYNC:
        {
            controlVal|=TVO_CAPTURE_ALTERNATE_VSYNC;
            break;
        }
        case MISR_FORTH_VSYNC:
        {
            controlVal|=TVO_CAPTURE_4TH_VSYNC;
            break;
        }
        default:
        {
            break;
        }
    }
    *misrControlVal = controlVal;
    TRC( TRC_ID_MISR, "misrControlVal : %u", *misrControlVal );
}

void CSTmMisrTVOut::MisrConfigViewPort(const stm_display_mode_t *pCurrentMode, uint32_t *VportMax, uint32_t *VportMin, MisrControl_t *MisrCtrlVal)
{

  if(MisrCtrlVal->CounterFlag == MISR_NON_COUNTER)
  {
    /* default viewport set by fbset cmd */
    *VportMin =(STCalculateMisrViewportLine(pCurrentMode,  0)<<16)|
                STCalculateMisrViewportPixel(pCurrentMode, 0);
    *VportMax =(STCalculateMisrViewportLine(pCurrentMode, pCurrentMode->mode_params.active_area_height)<<16)|
                STCalculateMisrViewportPixel(pCurrentMode, pCurrentMode->mode_params.active_area_width);

    TRC( TRC_ID_MISR, "Active Area::[Start Line: %x Start Pixel %x Height: %x Width: %x]",  pCurrentMode->mode_params.active_area_start_line, pCurrentMode->mode_params.active_area_start_pixel, pCurrentMode->mode_params.active_area_height, pCurrentMode->mode_params.active_area_width );
  }
  else
  {
    /* MinViewPort defined by the test application */
    *VportMin =(STCalculateMisrViewportLine(pCurrentMode, MisrCtrlVal->VportMinLine)<<16)|
                STCalculateMisrViewportPixel(pCurrentMode, MisrCtrlVal->VportMinPixel);
    TRC( TRC_ID_MISR, "Active Area::[Start Line: %d Start Pixel %d]", MisrCtrlVal->VportMinLine, MisrCtrlVal->VportMinPixel);
    /* MaxViewPort defined by the test application */
    *VportMax =(STCalculateMisrViewportLine(pCurrentMode, MisrCtrlVal->VportMaxLine)<<16)|
                STCalculateMisrViewportPixel(pCurrentMode, MisrCtrlVal->VportMaxPixel);
    TRC( TRC_ID_MISR, "Active Area::[End Line: %d End Pixel %d]", MisrCtrlVal->VportMaxLine, MisrCtrlVal->VportMaxPixel);
  }
  TRC( TRC_ID_MISR, "VportMax = %x VportMin= %x",*VportMax, *VportMin);
}

void CSTmMisrTVOut::MisrConfigScanningMode(const stm_display_mode_t *pCurrentMode, uint32_t *misrControlVal)
{
    if(pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN)
    {
        *misrControlVal &= ~(TVO_PROGRESSIVE_MISR);
    }
    else
    {
        *misrControlVal |= TVO_PROGRESSIVE_MISR;
    }
    TRC( TRC_ID_MISR, "pCurrentMode->mode_params.scan_type : %d, misrControlVal : %u", pCurrentMode->mode_params.scan_type, *misrControlVal );
}

