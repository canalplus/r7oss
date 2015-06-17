/*
   ST Microelectronics BDispII driver - hardware acceleration

   (c) Copyright 2007-2009  STMicroelectronics Ltd.

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __BDISP_ACCEL_H__
#define __BDISP_ACCEL_H__


#include <dfb_types.h>

#include "stm_types.h"



DFBResult bdisp_aq_initialize (CoreGraphicsDevice * const device,
                               GraphicsDeviceInfo * const device_info,
                               STGFX2DriverData   * const stdrv,
                               STGFX2DeviceData   * const stdev);

void bdisp_aq_EngineReset (void * const drv,
                           void * const dev);
DFBResult bdisp_aq_EngineSync (void * const drv,
                               void * const dev);

void bdisp_aq_EmitCommands (void * const drv,
                            void * const dev);

void bdisp_aq_GetSerial (void               * const drv,
                         void               * const dev,
                         CoreGraphicsSerial * const serial);
DFBResult bdisp_aq_WaitSerial (void                     * const drv,
                               void                     * const dev,
                               const CoreGraphicsSerial * const serial);


bool bdisp_aq_FillRectangle_simple (void         * const drv,
                                    void         * const dev,
                                    DFBRectangle * const rect);
bool bdisp_aq_DrawRectangle_simple (void         * const drv,
                                    void         * const dev,
                                    DFBRectangle * const rect);

bool bdisp_aq_FillDraw_nop (void         * const drv,
                            void         * const dev,
                            DFBRectangle * const rect);

bool bdisp_aq_FillRectangle (void         * const drv,
                             void         * const dev,
                             DFBRectangle * const rect);
bool bdisp_aq_DrawRectangle (void         * const drv,
                             void         * const dev,
                             DFBRectangle * const rect);


bool bdisp_aq_Blit_simple (void         * const drv,
                           void         * const dev,
                           DFBRectangle * const rect,
                           int           dx,
                           int           dy);
bool bdisp_aq_Blit_simple_YCbCr422r (void         * const drv,
                                     void         * const dev,
                                     DFBRectangle * const rect,
                                     int           dx,
                                     int           dy);

void bdisp_aq_setup_blit_operation (STGFX2DriverData * const stdrv,
                                    STGFX2DeviceData * const stdev);

bool bdisp_aq_Blit        (void         * const drv,
                           void         * const dev,
                           DFBRectangle * const rect,
                           int           dx,
                           int           dy);
bool bdisp_aq_StretchBlit (void         * const drv,
                           void         * const dev,
                           DFBRectangle * const srect,
                           DFBRectangle * const drect);
bool bdisp_aq_Blit_as_stretch (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const rect,
                               int           dx,
                               int           dy);
bool bdisp_aq_Blit2       (void         * const drv,
                           void         * const dev,
                           DFBRectangle * const rect,
                           int           dx,
                           int           dy,
                           int           sx2,
                           int           sy2);

bool
bdisp_aq_StretchBlit_RLE (void                        * const drv,
                          void                        * const dev,
                          unsigned long                src_address,
                          unsigned long                src_length,
                          const CoreSurfaceBufferLock * const dst,
                          const DFBRectangle          * const drect);

bool bdisp_aq_Blit_shortcut        (void         * const drv,
                                    void         * const dev,
                                    DFBRectangle * const rect,
                                    int           dx,
                                    int           dy);
bool bdisp_aq_StretchBlit_shortcut (void         * const drv,
                                    void         * const dev,
                                    DFBRectangle * const srect,
                                    DFBRectangle * const drect);
bool bdisp_aq_Blit2_shortcut       (void         * const drv,
                                    void         * const dev,
                                    DFBRectangle * const rect,
                                    int           dx,
                                    int           dy,
                                    int           sx2,
                                    int           sy2);
bool bdisp_aq_Blit_shortcut_YCbCr422r        (void         * const drv,
                                              void         * const dev,
                                              DFBRectangle * const rect,
                                              int           dx,
                                              int           dy);
bool bdisp_aq_StretchBlit_shortcut_YCbCr422r (void         * const drv,
                                              void         * const dev,
                                              DFBRectangle * const srect,
                                              DFBRectangle * const drect);
bool bdisp_aq_Blit2_shortcut_YCbCr422r       (void         * const drv,
                                              void         * const dev,
                                              DFBRectangle * const rect,
                                              int           dx,
                                              int           dy,
                                              int           sx2,
                                              int           sy2);
bool bdisp_aq_Blit_shortcut_rgb32        (void         * const drv,
                                          void         * const dev,
                                          DFBRectangle * const rect,
                                          int           dx,
                                          int           dy);
bool bdisp_aq_StretchBlit_shortcut_rgb32 (void         * const drv,
                                          void         * const dev,
                                          DFBRectangle * const srect,
                                          DFBRectangle * const drect);
bool bdisp_aq_Blit2_shortcut_rgb32       (void         * const drv,
                                          void         * const dev,
                                          DFBRectangle * const rect,
                                          int           dx,
                                          int           dy,
                                          int           sx2,
                                          int           sy2);

bool bdisp_aq_Blit_rotate_90_270 (void         * const drv,
                                  void         * const dev,
                                  DFBRectangle * const rect,
                                  int           dx,
                                  int           dy);

bool bdisp_aq_Blit_nop        (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const rect,
                               int           dx,
                               int           dy);
bool bdisp_aq_StretchBlit_nop (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const srect,
                               DFBRectangle * const drect);
bool bdisp_aq_Blit2_nop       (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const rect,
                               int           dx,
                               int           dy,
                               int           sx2,
                               int           sy2);


void
_bdisp_aq_prepare_upload_palette_hw (STGFX2DriverData * const stdrv,
                                     STGFX2DeviceData * const stdev);


void
_bdisp_aq_RGB32_init (STGFX2DriverData * const stdrv,
                      STGFX2DeviceData * const stdev,
                      u32               blt_tba,
                      u16               pitch,
                      DFBRectangle     * const rect);

void
_bdisp_aq_RGB32_fixup (STGFX2DriverData * const stdrv,
                       STGFX2DeviceData * const stdev,
                       u32               blt_tba,
                       u16               pitch,
                       DFBRectangle     * const rect);



#endif /* __BDISP_ACCEL_H__ */
