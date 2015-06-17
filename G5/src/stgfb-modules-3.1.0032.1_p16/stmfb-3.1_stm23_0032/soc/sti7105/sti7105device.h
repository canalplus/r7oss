/***********************************************************************
 *
 * File: soc/sti7105/sti7105device.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7105DEVICE_H
#define _STI7105DEVICE_H

#include <soc/sti7111/sti7111device.h>

#ifdef __cplusplus

class CSTi7105Device : public CSTi7111Device
{
public:
  CSTi7105Device  (void);
  ~CSTi7105Device (void);

protected:
  bool CreateOutputs(void);
  bool CreateHDMIOutput(void);

private:
  CSTi7105Device (const CSTi7105Device &);
  CSTi7105Device& operator= (const CSTi7105Device &);
};

#endif /* __cplusplus */


#define STi7105_OUTPUT_IDX_DVO1 ( 5)

#endif // _STI7105DEVICE_H
