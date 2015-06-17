/***********************************************************************
 * File: linux/kernel/drivers/media/video/stmvout_mc.h
 * Copyright (c) 2005-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ***********************************************************************/

#ifndef __STMVOUT_MC_H
#define __STMVOUT_MC_H

stm_display_plane_h stmvout_get_plane_hdl_from_entity(struct media_entity *
						      entity);
stm_display_output_h stmvout_get_output_hdl_from_connected_plane_entity(struct
									media_entity
									*entity);

#endif
