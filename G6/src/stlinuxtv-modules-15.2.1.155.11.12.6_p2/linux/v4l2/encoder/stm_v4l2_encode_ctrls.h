/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Implementation of linux dvb v4l2 encoder controls header
************************************************************************/
#ifndef STM_V4L2_ENCODE_CTRLS_H
#define STM_V4L2_ENCODE_CTRLS_H

int stm_v4l2_vid_enc_init_ctrl(struct stm_v4l2_encoder_device *dev_p);
int stm_v4l2_aud_enc_init_ctrl(struct stm_v4l2_encoder_device *dev_p);


#endif
