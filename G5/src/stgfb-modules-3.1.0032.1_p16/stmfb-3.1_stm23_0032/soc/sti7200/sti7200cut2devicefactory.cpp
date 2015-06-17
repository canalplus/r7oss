/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2devicefactory.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7200cut2localdevice.h"
#include "sti7200cut2remotedevice.h"

/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 * When this is called only g_pIOS will have been initialised.
 * 
 * In this case we actually eventually support two independent devices, well 
 * except for the fact they share the blitter.
 */
CDisplayDevice* AnonymousCreateDevice(unsigned deviceid)
{
  if(deviceid == 0)
    return new CSTi7200Cut2LocalDevice();
  else if (deviceid == 1)
    return new CSTi7200Cut2RemoteDevice();
    
  return 0;
}
