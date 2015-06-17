/***************************************************************************
This file is part of display_engine

COPYRIGHT (C) 2014 STMicroelectronics - All Rights Reserved

License type: GPLv2
display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with display_engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
The display_engine Library may alternatively be licensed under a proprietary
license from ST.

This file was created by STMicroelectronics on 2014-04-28
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
