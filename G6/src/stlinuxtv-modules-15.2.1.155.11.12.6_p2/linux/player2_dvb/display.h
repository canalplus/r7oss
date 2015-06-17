/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : display.h - access to platform specific display info
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
05-Apr-07   Created                                         Julian

************************************************************************/

#ifndef H_DISPLAY
#define H_DISPLAY

#include <stm_registry.h>

#ifdef __cplusplus
extern "C" {
#endif

	int dvb_stm_display_init(void);

	int dvb_stm_display_set_input_window_mode(struct VideoDeviceContext_s
							*Context,
							unsigned int Mode);
	int dvb_stm_display_get_input_window_mode(struct VideoDeviceContext_s
							*Context,
							unsigned int *Mode);
	int dvb_stm_display_set_output_window_mode(struct VideoDeviceContext_s
							*Context,
							unsigned int Mode);
	int dvb_stm_display_get_output_window_mode(struct VideoDeviceContext_s
							*Context,
							unsigned int *Mode);
	int dvb_stm_display_get_input_window_value(struct VideoDeviceContext_s *Context,
					     unsigned int *X, unsigned int *Y,
					     unsigned int *Width,
					     unsigned int *Height);
	int dvb_stm_display_get_output_window_value(struct VideoDeviceContext_s *Context,
					      unsigned int *X, unsigned int *Y,
					      unsigned int *Width,
					      unsigned int *Height);

	int dvb_stm_display_set_input_window_value(struct VideoDeviceContext_s *Context,
					     unsigned int X, unsigned int Y,
					     unsigned int Width,
					     unsigned int Height);
	int dvb_stm_display_set_output_window_value(struct VideoDeviceContext_s *Context,
					      unsigned int X, unsigned int Y,
					      unsigned int Width,
					      unsigned int Height);

	int dvb_stm_display_set_aspect_ratio_conversion_mode(struct
							     VideoDeviceContext_s
							     *Context,
							     unsigned int
							     AspectRatioConversionMode);
	int dvb_stm_display_get_aspect_ratio_conversion_mode(struct
							     VideoDeviceContext_s
							     *Context,
							     unsigned int
							     *AspectRatioConversionMode);

	int dvb_stm_display_set_output_display_aspect_ratio(struct
							    VideoDeviceContext_s
							    *Context,
							    unsigned int
							    OutputDisplayAspectRatio);
	int dvb_stm_display_get_output_display_aspect_ratio(struct
							    VideoDeviceContext_s
							    *Context,
							    unsigned int
							    *OutputDisplayAspectRatio);

#ifdef __cplusplus
}
#endif
#endif
