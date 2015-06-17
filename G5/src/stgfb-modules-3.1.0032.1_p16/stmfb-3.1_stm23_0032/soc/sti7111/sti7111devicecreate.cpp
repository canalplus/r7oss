/***********************************************************************
 *
 * File: soc/sti7111/sti7111devicecreate.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7111device.h"

/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 * When this is called only g_pIOS will have been initialised.
 */
CDisplayDevice* AnonymousCreateDevice(unsigned deviceid)
{
  if(deviceid == 0)
    return new CSTi7111Device();

  return 0;
}
