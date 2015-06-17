/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407device.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH407DEVICE_H
#define _STIH407DEVICE_H

#ifdef __cplusplus

#include <display/generic/DisplayDevice.h>

class CSTmC8VTG_V8_4;
class CSTmHDF_V5_0;
class CSTmVIP;
class CVideoPlug;
class CSTmTVOutTeletext;
class CSTmFSynthLLA;
class CSTiH407MainMixer;
class CSTiH407AuxMixer;
class CSTmClockLLA;
class CSTiH407DENC;
class CSTmVIPDVO;

class CSTiH407Device : public CDisplayDevice
{
public:
  CSTiH407Device(void);
  virtual ~CSTiH407Device(void);

  bool Create(void);

private:
  CSTmFSynthLLA          *m_pFSynthHD;
  CSTmFSynthLLA          *m_pFSynthSD;
  CSTmClockLLA           *m_pClkDivider;

  CSTmC8VTG_V8_4         *m_pMainVTG;
  CSTmC8VTG_V8_4         *m_pAuxVTG;

  CSTiH407MainMixer      *m_pMainMixer;
  CSTiH407AuxMixer       *m_pAuxMixer;
  CVideoPlug             *m_pVideoPlug1;
  CVideoPlug             *m_pVideoPlug2;

  CSTmHDF_V5_0           *m_pHDFormatter;
  CSTmVIP                *m_pDENCVIP;
  CSTmVIP                *m_pHDFVIP;
  CSTmVIP                *m_pHDMIVIP;
  CSTmVIP                *m_pDVOVIP;

  CSTiH407DENC           *m_pDENC;
  CSTmTVOutTeletext      *m_pTeletext;

  bool CreateClocks(void);
  bool CreateInfrastructure(void);
  bool CreatePlanes(void);
  bool CreateSources(void);
  bool CreateOutputs(void);

  int Freeze(void);
  int Suspend(void);
  int Resume(void);

  void PowerOnSetup(void);
  void PowerDown(void);

  CSTiH407Device(const CSTiH407Device&);
  CSTiH407Device& operator=(const CSTiH407Device&);
};

#endif /* __cplusplus */

enum {
  STiH407_OUTPUT_IDX_MAIN,
  STiH407_OUTPUT_IDX_HDMI,
  STiH407_OUTPUT_IDX_AUX,
  STiH407_OUTPUT_IDX_DVO,
  STiH407_OUTPUT_COUNT
};

enum {
  STiH407_PLANE_IDX_GDP1_MAIN,
  STiH407_PLANE_IDX_GDP2_MAIN,
  STiH407_PLANE_IDX_GDP1_AUX,
  STiH407_PLANE_IDX_GDP2_AUX,
  STiH407_PLANE_IDX_VBI_MAIN,
  STiH407_PLANE_IDX_VID_MAIN,
  STiH407_PLANE_IDX_VID_PIP,
  STiH407_PLANE_COUNT
};

/* HW Mapping for GDPs */
enum {
  STiH407_PLANE_IDX_GDP1 = STiH407_PLANE_IDX_GDP1_AUX,
  STiH407_PLANE_IDX_GDP2 = STiH407_PLANE_IDX_GDP2_AUX,
  STiH407_PLANE_IDX_GDP3 = STiH407_PLANE_IDX_GDP1_MAIN,
  STiH407_PLANE_IDX_GDP4 = STiH407_PLANE_IDX_GDP2_MAIN,
};

// There will be as many Sources as Decoder
// because the link decoder-source will remains connected
#define STiH407_SOURCE_COUNT     50


#endif // _STIH407DEVICE_H
