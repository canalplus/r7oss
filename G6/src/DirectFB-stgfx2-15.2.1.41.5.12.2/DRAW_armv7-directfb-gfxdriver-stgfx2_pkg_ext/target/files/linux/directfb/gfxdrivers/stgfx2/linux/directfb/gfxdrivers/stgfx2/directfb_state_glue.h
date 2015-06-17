#ifndef __DIRECTFB_STATE_GLUE_H__
#define __DIRECTFB_STATE_GLUE_H__

void bdisp_aq_StateInit (void      *driver_data,
                         void      *device_data,
                         CardState *state);
void bdisp_aq_StateDestroy (void      *driver_data,
                            void      *device_data,
                            CardState *state);

void bdisp_aq_CheckState (void                *driver_data,
                          void                *device_data,
                          CardState           *state,
                          DFBAccelerationMask  accel);

void bdisp_aq_SetState (void                *driver_data,
                        void                *device_data,
                        GraphicsDeviceFuncs *funcs,
                        CardState           *state,
                        DFBAccelerationMask   accel);

char *stgfx2_get_dfb_drawflags_string (DFBSurfaceDrawingFlags flags);
char *stgfx2_get_dfb_blitflags_string (DFBSurfaceBlittingFlags flags);
char *stgfx2_get_dfb_renderoptions_string (DFBSurfaceRenderOptions opts);


#endif /* __DIRECTFB_STATE_GLUE_H__ */
