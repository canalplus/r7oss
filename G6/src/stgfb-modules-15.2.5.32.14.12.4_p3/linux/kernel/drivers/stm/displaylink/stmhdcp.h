/***************************************************************************
This file is part of Display Link module

COPYRIGHT (C) 2013 STMicroelectronics - All Rights Reserved

License type: Proprietary

ST makes no warranty express or implied including but not limited to, any
warranty of
    (i)  merchantability or fitness for a particular purpose and/or
    (ii) requirements, for a particular purpose in relation to the LICENSED
         MATERIALS, which is provided “AS IS”, WITH ALL FAULTS. ST does not
         represent or warrant that the LICENSED MATERIALS provided here
         under is free of infringement of any third party patents,
         copyrights, trade secrets or other intellectual property rights.
         ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE
         EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW

This file was created by adel.chaouch@st.com on 2013
***************************************************************************/


#ifndef _STMHDCP_H
#define _STMHDCP_H

#if !defined(__KERNEL__)
#include <sys/time.h>
#endif

#define STMHDCPIO_SET_HDCP_STATUS           _IO  ('H', 0x1)
#define STMHDCPIO_SET_ENCRYPTED_FRAME       _IO  ('H', 0x2)
#define STMHDCPIO_STOP_HDCP                 _IO  ('H', 0x3)
#define STMHDCPIO_SET_FORCE_AUTHENTICATION  _IO  ('H', 0x4)
#define STMHDCPIO_SET_SRM_BKSV_VALUE        _IO  ('H', 0x5)
#define STMHDCPIO_GET_HDCP_STATUS           _IO  ('H', 0x6)
#define STMHDCPIO_WAIT_FOR_VSYNC            _IO  ('H', 0x7)

#endif /* _STMHDCP_H */
