#ifndef __BDISP2_DIRECTFB_GLUE_H__
#define __BDISP2_DIRECTFB_GLUE_H__


DFBResult
bdisp_aq_initialize (CoreGraphicsDevice       *device,
                     GraphicsDeviceInfo       *device_info,
                     struct _STGFX2DriverData *drvdata,
                     struct _STGFX2DeviceData *devdata);

void bdisp_aq_EngineReset (void *driver_data,
                           void *device_data);
DFBResult bdisp_aq_EngineSync (void *driver_data,
                               void *device_data);

void bdisp_aq_EmitCommands (void *driver_data,
                            void *device_data);

void bdisp_aq_GetSerial (void               *driver_data,
                         void               *device_data,
                         CoreGraphicsSerial *serial);
DFBResult bdisp_aq_WaitSerial (void                     *driver_data,
                               void                     *device_data,
                               const CoreGraphicsSerial *serial);


bool bdisp_aq_FillRectangle (void         *driver_data,
                             void         *device_data,
                             DFBRectangle *drect);

bool bdisp_aq_DrawRectangle (void         *driver_data,
                             void         *device_data,
                             DFBRectangle *drect);

bool bdisp_aq_Blit (void         *driver_data,
                    void         *device_data,
                    DFBRectangle *srect,
                    int           dx,
                    int           dy);

bool bdisp_aq_StretchBlit (void         *driver_data,
                           void         *device_data,
                           DFBRectangle *srect,
                           DFBRectangle *drect);

bool bdisp_aq_Blit2 (void         *driver_data,
                     void         *device_data,
                     DFBRectangle *srect,
                     int           dx,
                     int           dy,
                     int           sx2,
                     int           sy2);


bool
bdisp_aq_StretchBlit_RLE (void                        *driver_data,
                          void                        *device_data,
                          unsigned long                src_address,
                          unsigned long                src_length,
                          const CoreSurfaceBufferLock *dst,
                          const DFBRectangle          *drect);

void
_bdisp_aq_RGB32_init (struct _STGFX2DriverData *drvdata,
                      struct _STGFX2DeviceData *devdata,
                      uint32_t                  blt_tba,
                      uint16_t                  pitch,
                      DFBRectangle             *drect);
void
_bdisp_aq_RGB32_fixup (struct _STGFX2DriverData *drvdata,
                       struct _STGFX2DeviceData *devdata,
                       uint32_t                  blt_tba,
                       uint16_t                  pitch,
                       DFBRectangle             *drect);


#endif /* __BDISP2_DIRECTFB_GLUE_H__ */
