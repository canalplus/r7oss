/*
   ST Microelectronics BDispII driver - primary screen implementation

   (c) Copyright 2007,2009-2010  STMicroelectronics Ltd.
   (c) Copyright 2000-2002       convergence integrated media GmbH.
   (c) Copyright 2002            convergence GmbH.

   All rights reserved.

   Based on the cle266 driver implementation:

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/screens.h>
#include <core/system.h>

#include <fbdev/fbdev.h>

#include <misc/conf.h>

#include <direct/messages.h>

#include <linux/fb.h>
#include <linux/stmfb.h>

#include "stgfx_screen.h"


D_DEBUG_DOMAIN (STGFX_Screen, "STGFX/Screen", "STM Screen (hook)");


enum _stm_outputs {
  STM_OUTPUT_ANALOG,
  STM_OUTPUT_HDMI,
  STM_OUTPUT_DVO,

  STM_OUTPUT_COUNT
};

typedef struct {
  uint32_t originalOutputCaps;
} STGFXScreenSharedData;

typedef struct _STMfbScreenData {
  void        *orig_data;
  ScreenFuncs  orig_funcs;
  struct stmfbio_output_configuration startup_config;
} STMfbScreenData;


static int
stgfxScreenDataSize (void)
{
  D_DEBUG_AT (STGFX_Screen, "%s (%u)\n",
              __func__, sizeof (STGFXScreenSharedData));

  return sizeof (STGFXScreenSharedData);
}


static DFBResult
stgfxInitScreen (CoreScreen           *screen,
                 CoreGraphicsDevice   *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description)
{
  STMfbScreenData       * const data = driver_data;
  STGFXScreenSharedData * const shd = screen_data;
  const FBDev           * const fbdev = dfb_system_data ();
  DFBResult              ret;

  D_DEBUG_AT (STGFX_Screen, "%s @ %p\n", __func__, screen_data);

  D_ASSERT (dfb_system_type() == CORE_FBDEV);

  ret = data->orig_funcs.InitScreen (screen, device,
                                     data->orig_data,
                                     screen_data,
                                     description);
  if (ret != DFB_OK)
    {
      D_FREE (data);
      return ret;
    }

  data->startup_config.outputid = STMFBIO_OUTPUTID_MAIN;
  if (ioctl (fbdev->fd, STMFBIO_GET_OUTPUT_CONFIG, &data->startup_config) < 0)
    {
      D_INFO ("BDisp/screen: no stmfb or wrong version?\n");
      data->startup_config.caps = STMFBIO_OUTPUT_CAPS_NONE;
      shd->originalOutputCaps   = STMFBIO_OUTPUT_CAPS_NONE;
      return DFB_OK;
    }

  snprintf (description->name,
            DFB_SCREEN_DESC_NAME_LENGTH, "%s", fbdev->shared->fix.id);

  /* As we are talking to TVs there is no VESA power management control of the
     display implemented. */
  description->caps &= ~DSCCAPS_POWER_MANAGEMENT;

  shd->originalOutputCaps = data->startup_config.caps;

  if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND)
    {
      D_DEBUG_AT (STGFX_Screen, "  => have mixer background\n");
      description->caps |= DSCCAPS_MIXERS;
      ++description->mixers;
    }

  if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
    {
      D_DEBUG_AT (STGFX_Screen, "  => have denc\n");
      description->caps |= DSCCAPS_ENCODERS;
      ++description->encoders;
    }

  description->caps |= DSCCAPS_OUTPUTS;
  description->outputs = STM_OUTPUT_COUNT;

  return DFB_OK;
}

static DFBResult
stgfxShutdownScreen (CoreScreen *screen,
                     void       *driver_data,
                     void       *screen_data)
{
  STMfbScreenData * const data = driver_data;
  const FBDev     * const fbdev = dfb_system_data ();
  DFBResult        ret = DFB_OK;

  D_DEBUG_AT (STGFX_Screen, "%s\n", __func__);

  if (data->orig_funcs.ShutdownScreen)
    ret = data->orig_funcs.ShutdownScreen (screen,
                                           data->orig_data,
                                           screen_data);

  /* try and restore the framebuffer's extended state. */
  data->startup_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;
  ioctl (fbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &data->startup_config);

  D_FREE (data);

  return ret;
}


/** Mixer configuration **/
static DFBResult
stgfxInitMixer (CoreScreen                *screen,
                void                      *driver_data,
                void                      *screen_data,
                int                        mixer,
                DFBScreenMixerDescription *description,
                DFBScreenMixerConfig      *config)
{
  STMfbScreenData * const data = driver_data;

  D_DEBUG_AT (STGFX_Screen, "%s\n", __func__);

  description->caps = DSMCAPS_FULL | DSMCAPS_BACKGROUND;

  DFB_DISPLAYLAYER_IDS_ADD (description->layers, DLID_PRIMARY);

  snprintf (description->name,
            DFB_SCREEN_MIXER_DESC_NAME_LENGTH, "STM Mixer");

  config->flags = DSMCONF_TREE | DSMCONF_BACKGROUND;
  config->tree = DSMT_FULL;
  config->background.a = (data->startup_config.mixer_background >> 24) & 0xff;
  config->background.r = (data->startup_config.mixer_background >> 16) & 0xff;
  config->background.g = (data->startup_config.mixer_background >> 8)  & 0xff;
  config->background.b = (data->startup_config.mixer_background >> 0)  & 0xff;

  D_DEBUG_AT (STGFX_Screen, " => background colour: 0x%08x\n",
              data->startup_config.mixer_background);

  return DFB_OK;
}

static DFBResult
stgfxDoMixerConfig (CoreScreen                 *screen,
                    void                       *driver_data,
                    void                       *screen_data,
                    int                         mixer,
                    const DFBScreenMixerConfig *config,
                    DFBScreenMixerConfigFlags  *failed,
                    bool                        test)
{
  const STGFXScreenSharedData         * const shd = screen_data;
  const FBDev                         * const fbdev = dfb_system_data ();
  struct stmfbio_output_configuration  cfg = { 0 };

  D_DEBUG_AT (STGFX_Screen, "%s%s\n", __func__, test ? " (test)" : "");

  *failed = 0;
  if ((config->flags & DSMCONF_BACKGROUND)
      && !(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND))
    *failed = DSMCONF_BACKGROUND;

  if (config->flags & DSMCONF_TREE && config->tree != DSMT_FULL)
    *failed |= DSMT_FULL;

  if (config->flags & ~(DSMCONF_TREE | DSMCONF_BACKGROUND))
    *failed |= config->flags & ~(DSMCONF_TREE | DSMCONF_BACKGROUND);

  if (*failed)
    return DFB_INVARG;

  cfg.outputid = STMFBIO_OUTPUTID_MAIN;
  cfg.caps     = STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
  cfg.activate = test ? STMFBIO_ACTIVATE_TEST : STMFBIO_ACTIVATE_IMMEDIATE;
  cfg.mixer_background = (0
                          | (config->background.a << 24)
                          | (config->background.r << 16)
                          | (config->background.g <<  8)
                          | (config->background.b <<  0)
                         );

  if (ioctl (fbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &cfg) < 0)
    {
      *failed = config->flags;
      return (cfg.failed != 0) ? DFB_INVARG : DFB_IO;
    }

  return DFB_OK;
}

static DFBResult
stgfxTestMixerConfig (CoreScreen                 *screen,
                      void                       *driver_data,
                      void                       *screen_data,
                      int                         mixer,
                      const DFBScreenMixerConfig *config,
                      DFBScreenMixerConfigFlags  *failed)
{
  return stgfxDoMixerConfig (screen, driver_data, screen_data, mixer,
                             config, failed, true);
}

static DFBResult
stgfxSetMixerConfig (CoreScreen                 *screen,
                     void                       *driver_data,
                     void                       *screen_data,
                     int                         mixer,
                     const DFBScreenMixerConfig *config)
{
  DFBScreenMixerConfigFlags dummy;
  return stgfxDoMixerConfig (screen, driver_data, screen_data, mixer,
                             config, &dummy, false);
}


/** Encoder configuration **/
typedef unsigned int stmfbio_output_std;
#define STMFBIO_OUTPUT_STD_NONE 0xffffffff
#define STMFBIO_OUTPUT_STD_DIGITAL 0x80000000
struct _output_standard
{
  stmfbio_output_std          stmfb_std;
  DFBScreenEncoderScanMode    scanmode;
  DFBScreenEncoderFrequency   frequency;
  DFBScreenOutputResolution   resolution;
  DFBScreenEncoderTVStandards sdtv_encoding;
};

static const struct _output_standard standards_table[] =
{
  { STMFBIO_OUTPUT_STD_NTSC_M,   DSESM_INTERLACED, DSEF_60HZ, DSOR_720_480, DSETV_NTSC },
  { STMFBIO_OUTPUT_STD_NTSC_J,   DSESM_INTERLACED, DSEF_60HZ, DSOR_720_480, DSETV_NTSC_M_JPN },
  { STMFBIO_OUTPUT_STD_NTSC_443, DSESM_INTERLACED, DSEF_60HZ, DSOR_720_480, DSETV_NTSC_443 },
  { STMFBIO_OUTPUT_STD_PAL_M,    DSESM_INTERLACED, DSEF_60HZ, DSOR_720_480, DSETV_PAL_M },
  { STMFBIO_OUTPUT_STD_PAL_60,   DSESM_INTERLACED, DSEF_60HZ, DSOR_720_480, DSETV_PAL_60 },

  { STMFBIO_OUTPUT_STD_PAL_BDGHI, DSESM_INTERLACED, DSEF_50HZ, DSOR_720_576, DSETV_PAL },
  { STMFBIO_OUTPUT_STD_PAL_N,     DSESM_INTERLACED, DSEF_50HZ, DSOR_720_576, DSETV_PAL_N },
  { STMFBIO_OUTPUT_STD_PAL_Nc,    DSESM_INTERLACED, DSEF_50HZ, DSOR_720_576, DSETV_PAL_NC },
  { STMFBIO_OUTPUT_STD_SECAM,     DSESM_INTERLACED, DSEF_50HZ, DSOR_720_576, DSETV_SECAM },

};


static DFBScreenEncoderTVStandards
__attribute__((pure))
stmfb_sdstd_to_dfb (stmfbio_output_std stmfb_std)
{
  int i;

  for (i = 0; i < D_ARRAY_SIZE (standards_table); ++i)
    {
      if (standards_table[i].stmfb_std == stmfb_std
          && !(standards_table[i].sdtv_encoding & DSETV_DIGITAL))
        return standards_table[i].sdtv_encoding;
    }

  D_WARN ("%s: invalid sdtv encoding %d (0x%x) from kernel\n",
          __func__, stmfb_std, stmfb_std);

  return DSETV_UNKNOWN;
}

static DFBResult
stgfxInitEncoder (CoreScreen                  *screen,
                  void                        *driver_data,
                  void                        *screen_data,
                  int                          encoder,
                  DFBScreenEncoderDescription *description,
                  DFBScreenEncoderConfig      *config)
{
  const STMfbScreenData       * const data = driver_data;
  const STGFXScreenSharedData * const shd = screen_data;

  D_DEBUG_AT (STGFX_Screen, "%s (%d)\n", __func__, encoder);

  if (!(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING))
    return DFB_BUG;

  description->caps = DSECAPS_TV_STANDARDS | DSECAPS_SCANMODE;
  description->type = DSET_TV;
  description->tv_standards = (DSETV_PAL | DSETV_NTSC | DSETV_SECAM
                               | DSETV_PAL_60 | DSETV_PAL_BG | DSETV_PAL_I
                               | DSETV_PAL_M  | DSETV_PAL_N | DSETV_PAL_NC
                               | DSETV_NTSC_M_JPN | DSETV_NTSC_443);

  snprintf (description->name,
            DFB_SCREEN_ENCODER_DESC_NAME_LENGTH, "STM DENC");

  config->flags = DSECONF_NONE;

  config->tv_standard = stmfb_sdstd_to_dfb (data->startup_config.sdtv_encoding);
  if (config->tv_standard != DSETV_UNKNOWN)
    config->flags |= DSECONF_TV_STANDARD;

  D_DEBUG_AT (STGFX_Screen, "  => tv encoding: %u\n", config->tv_standard);

  config->flags |= DSECONF_SCANMODE;
  config->scanmode = DSESM_INTERLACED;

  if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_BRIGHTNESS)
    {
      description->caps |= DSECAPS_BRIGHTNESS;
      config->flags |= DSECONF_ADJUSTMENT;
      config->adjustment.flags |= DCAF_BRIGHTNESS;
      config->adjustment.brightness = ((uint16_t) data->startup_config.brightness) << 8;
      D_DEBUG_AT (STGFX_Screen, "  => brightness: %u\n",
                  config->adjustment.brightness);
    }

  if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_CONTRAST)
    {
      description->caps |= DSECAPS_CONTRAST;
      config->flags |= DSECONF_ADJUSTMENT;
      config->adjustment.flags |= DCAF_CONTRAST;
      config->adjustment.contrast = ((uint16_t) data->startup_config.contrast) << 8;
      D_DEBUG_AT (STGFX_Screen, "  => contrast: %u\n",
                  config->adjustment.contrast);
  }

  if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_SATURATION)
    {
      description->caps |= DSECAPS_SATURATION;
      config->flags |= DSECONF_ADJUSTMENT;
      config->adjustment.flags |= DCAF_SATURATION;
      config->adjustment.saturation = ((uint16_t) data->startup_config.saturation) << 8;
      D_DEBUG_AT (STGFX_Screen, "  => saturation: %u\n",
                  config->adjustment.saturation);
  }

  if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_HUE)
    {
      description->caps |= DSECAPS_HUE;
      config->flags |= DSECONF_ADJUSTMENT;
      config->adjustment.flags |= DCAF_HUE;
      config->adjustment.hue = ((uint16_t) data->startup_config.hue) << 8;
      D_DEBUG_AT (STGFX_Screen, "  => hue: %u\n",
                  config->adjustment.hue);
    }

  return DFB_OK;
}

static DFBResult
stgfxDoEncoderConfig (CoreScreen                  *screen,
                      void                        *driver_data,
                      void                        *screen_data,
                      int                          encoder,
                      DFBScreenEncoderConfig      *config,
                      DFBScreenEncoderConfigFlags *failed,
                      bool                         test)
{
  const FBDev                         * const fbdev = dfb_system_data ();
  struct stmfbio_output_configuration  cfg = { 0 };

  D_DEBUG_AT (STGFX_Screen, "%s%s\n", __func__, test ? " (test)" : "");

  cfg.outputid = STMFBIO_OUTPUTID_MAIN;
  cfg.activate = test ? STMFBIO_ACTIVATE_TEST : STMFBIO_ACTIVATE_IMMEDIATE;

  *failed = config->flags & ~(DSECONF_SCANMODE
                              | DSECONF_TV_STANDARD
                              | DSECONF_ADJUSTMENT);

  if ((config->flags & DSECONF_SCANMODE)
      && (config->scanmode != DSESM_INTERLACED))
    {
      D_DEBUG_AT (STGFX_Screen, " => encoder scanmode must be interlaced\n");
      *failed |= DSECONF_SCANMODE;
    }

  if (*failed)
    {
      D_DEBUG_AT (STGFX_Screen, " => encoder config failed: 0x%08x\n",
                  *failed);
      return DFB_INVARG;
    }

  if (config->flags & DSECONF_TV_STANDARD)
    {
      int i;

      for (i = 0; i < D_ARRAY_SIZE (standards_table); ++i)
        {
          if (standards_table[i].sdtv_encoding == config->tv_standard)
            {
              cfg.caps |= STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
              cfg.sdtv_encoding = standards_table[i].stmfb_std;
              break;
            }
        }

      if (!(cfg.caps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
          || cfg.sdtv_encoding & STMFBIO_OUTPUT_STD_DIGITAL)
        {
          D_DEBUG_AT (STGFX_Screen, " => unknown tv encoding 0x%08x\n",
                      config->tv_standard);
          *failed |= DSECONF_TV_STANDARD;
          return DFB_INVARG;
        }
    }

  if (config->flags & DSECONF_ADJUSTMENT)
    {
      if (config->adjustment.flags & DCAF_BRIGHTNESS)
        {
          cfg.caps |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;
          cfg.brightness = config->adjustment.brightness >> 8;
        }

      if (config->adjustment.flags & DCAF_SATURATION)
        {
          cfg.caps |= STMFBIO_OUTPUT_CAPS_SATURATION;
          cfg.saturation = config->adjustment.saturation >> 8;
        }

      if (config->adjustment.flags & DCAF_CONTRAST)
        {
          cfg.caps |= STMFBIO_OUTPUT_CAPS_CONTRAST;
          cfg.contrast = config->adjustment.contrast >> 8;
        }

      if (config->adjustment.flags & DCAF_HUE)
        {
          cfg.caps |= STMFBIO_OUTPUT_CAPS_HUE;
          cfg.hue = config->adjustment.hue >> 8;
        }
    }

  if (ioctl (fbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &cfg) < 0)
    {
      if(cfg.failed != 0)
        {
          /* Only report failures on test. If this was a set then we read back
             the actual config and pass that back. This avoids unnecessary
             failures when trying to set a tv standard in a mode that doesn't
             support it.

             Unfortunately even just changing an adjustment control will also
             try and set the tv standard as well. This is where the DirectFB
             semantics don't exactly match the way the hardware works. */
          if (test)
            {
              D_DEBUG_AT (STGFX_Screen, " => output config ioctl failed 0x%08x\n",
                          cfg.failed);
              if (cfg.failed & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
                *failed |= DSECONF_TV_STANDARD;

              if (cfg.failed & STMFBIO_OUTPUT_CAPS_PSI_MASK)
                *failed |= DSECONF_ADJUSTMENT;

              return DFB_INVARG;
            }
        }
      else
        {
          D_DEBUG_AT (STGFX_Screen, " => ioctl IO error\n");
          *failed = config->flags;
          return DFB_IO;
        }
    }

  if (!test)
    {
      struct stmfbio_output_configuration cfg = { .outputid = STMFBIO_OUTPUTID_MAIN };
      /* Read back configuration */
      if (ioctl (fbdev->fd, STMFBIO_GET_OUTPUT_CONFIG, &cfg) < 0)
        {
          D_DEBUG_AT (STGFX_Screen, " => do encoder, reading back output config failed!\n");
          return DFB_IO;
        }

      if (cfg.caps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
        config->tv_standard = stmfb_sdstd_to_dfb (cfg.sdtv_encoding);

      if (cfg.caps & STMFBIO_OUTPUT_CAPS_BRIGHTNESS)
        {
          config->adjustment.flags |= DCAF_BRIGHTNESS;
          config->adjustment.brightness = (uint16_t)cfg.brightness << 8;
        }

      if (cfg.caps & STMFBIO_OUTPUT_CAPS_SATURATION)
        {
          config->adjustment.flags |= DCAF_SATURATION;
          config->adjustment.saturation = (uint16_t)cfg.saturation << 8;
        }

      if (cfg.caps & STMFBIO_OUTPUT_CAPS_CONTRAST)
        {
          config->adjustment.flags |= DCAF_CONTRAST;
          config->adjustment.contrast = (uint16_t)cfg.contrast << 8;
        }

      if (cfg.caps & STMFBIO_OUTPUT_CAPS_HUE)
        {
          config->adjustment.flags |= DCAF_HUE;
          config->adjustment.hue = (uint16_t)cfg.hue << 8;
        }
    }

  return DFB_OK;
}

static DFBResult
stgfxTestEncoderConfig (CoreScreen                   *screen,
                        void                         *driver_data,
                        void                         *screen_data,
                        int                           encoder,
                        const DFBScreenEncoderConfig *config,
                        DFBScreenEncoderConfigFlags  *failed)
{
  DFBScreenEncoderConfig tmp = *config;

  return stgfxDoEncoderConfig (screen, driver_data, screen_data, encoder,
                               &tmp, failed, true);
}

static DFBResult
stgfxSetEncoderConfig (CoreScreen                   *screen,
                       void                         *driver_data,
                       void                         *screen_data,
                       int                           encoder,
                       const DFBScreenEncoderConfig *config)
{
  DFBScreenEncoderConfigFlags dummy;

  return stgfxDoEncoderConfig (screen, driver_data, screen_data, encoder,
                               (DFBScreenEncoderConfig *) config, &dummy,
                               false);
}


/** Output configuration **/
static DFBResult
stgfxInitOutput (CoreScreen                 *screen,
                 void                       *driver_data,
                 void                       *screen_data,
                 int                         output,
                 DFBScreenOutputDescription *description,
                 DFBScreenOutputConfig      *config)
{
  const STMfbScreenData       * const data = driver_data;
  const STGFXScreenSharedData * const shd = screen_data;

  switch (output)
    {
    case STM_OUTPUT_ANALOG:
      if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG)
        {
          description->caps = DSOCAPS_SIGNAL_SEL;
          description->all_signals = DSOS_RGB | DSOS_YCBCR;
          if (shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
            description->all_signals |= DSOS_CVBS | DSOS_YC;

          snprintf (description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH,
                    "STM Analogue Output");

          config->flags = DSOCONF_SIGNALS;
          config->out_signals = 0;
          if (data->startup_config.analogue_config & STMFBIO_OUTPUT_ANALOGUE_RGB)
            config->out_signals |= DSOS_RGB;

          if (data->startup_config.analogue_config & STMFBIO_OUTPUT_ANALOGUE_YPrPb)
            config->out_signals |= DSOS_YCBCR;

          if (data->startup_config.analogue_config & STMFBIO_OUTPUT_ANALOGUE_YC)
            config->out_signals |= DSOS_YC;

          if (data->startup_config.analogue_config & STMFBIO_OUTPUT_ANALOGUE_CVBS)
            config->out_signals |= DSOS_CVBS;
        }
      break;

    case STM_OUTPUT_HDMI:
      if(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_HDMI_CONFIG)
        {
          description->caps = DSOCAPS_SIGNAL_SEL;
          description->all_signals = DSOS_HDMI | DSOS_RGB | DSOS_YCBCR;

          snprintf (description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH,
                    "STM HDMI Output");

          config->flags = DSOCONF_SIGNALS;
          if (data->startup_config.hdmi_config & STMFBIO_OUTPUT_HDMI_DISABLED)
            config->out_signals = DSOS_NONE;
          else
            {
              config->out_signals = DSOS_HDMI;
              /* A bit of bending the interface to specify the HDMI colourspace */
              if(data->startup_config.hdmi_config & STMFBIO_OUTPUT_HDMI_YUV)
                config->out_signals |= DSOS_YCBCR;
              else
                config->out_signals |= DSOS_RGB;
            }
        }
      break;

    case STM_OUTPUT_DVO:
      if(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_DVO_CONFIG)
        {
          description->caps = DSOCAPS_SIGNAL_SEL;
          description->all_signals = DSOS_656;

          snprintf (description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH,
                    "STM DVO Output");

          config->flags = DSOCONF_SIGNALS;
          if(data->startup_config.dvo_config & STMFBIO_OUTPUT_DVO_DISABLED)
            config->out_signals = DSOS_NONE;
          else
            config->out_signals = DSOS_656;
        }
      break;

    default:
      return DFB_INVARG;
  }

  return DFB_OK;
}

static DFBResult
stgfxDoOutputConfig (CoreScreen                  *screen,
                     void                        *driver_data,
                     void                        *screen_data,
                     int                          output,
                     const DFBScreenOutputConfig *config,
                     DFBScreenOutputConfigFlags  *failed,
                     bool                         test)
{
  const FBDev                         * const fbdev = dfb_system_data ();
  const STGFXScreenSharedData         * const shd = screen_data;
  struct stmfbio_output_configuration  cfg = { 0 };

  cfg.outputid = STMFBIO_OUTPUTID_MAIN;
  if (ioctl (fbdev->fd, STMFBIO_GET_OUTPUT_CONFIG, &cfg) < 0)
    {
      *failed = config->flags;
      return DFB_IO;
    }

  cfg.activate = test ? STMFBIO_ACTIVATE_TEST : STMFBIO_ACTIVATE_IMMEDIATE;

  *failed = config->flags & ~DSOCONF_SIGNALS;
  if (*failed)
    return DFB_INVARG;

  switch (output)
    {
    case STM_OUTPUT_ANALOG:
      if (config->out_signals & ~(DSOS_RGB | DSOS_YCBCR | DSOS_CVBS | DSOS_YC))
        *failed = config->flags;

      if (!(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG))
        {
          if(config->out_signals == DSOS_NONE)
            return DFB_OK;
          else
            {
              *failed = config->flags;
              return DFB_INVARG;
            }
        }
      else
        {
          cfg.caps = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

          cfg.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_MASK;
          if (config->out_signals & DSOS_RGB)
            cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_RGB;
          if (config->out_signals & DSOS_YCBCR)
            cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YPrPb;
          if (config->out_signals & DSOS_CVBS)
            cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CVBS;
          if (config->out_signals & DSOS_YC)
            cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YC;
        }
      break;

    case STM_OUTPUT_HDMI:
      if (config->out_signals & ~(DSOS_HDMI | DSOS_RGB | DSOS_YCBCR))
        *failed = config->flags;

      if ((config->out_signals & (DSOS_RGB | DSOS_YCBCR)) == (DSOS_RGB | DSOS_YCBCR))
        *failed = config->flags;

      if (!(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_HDMI_CONFIG))
        {
          if(config->out_signals == DSOS_NONE)
            return DFB_OK;
          else
            {
              *failed = config->flags;
              return DFB_INVARG;
            }
        }
      else
        {
          cfg.caps = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;

          cfg.hdmi_config &= ~(STMFBIO_OUTPUT_HDMI_DISABLED
                               | STMFBIO_OUTPUT_HDMI_YUV);

          if (config->out_signals & DSOS_HDMI)
            {
              cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_ENABLED;
              /* HDMI colourspace is RGB by default, only change it when YCBCR
                 is specified. */
              if(config->out_signals & DSOS_YCBCR)
                cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_YUV;
            }
          else
            cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_DISABLED;
        }
      break;

    case STM_OUTPUT_DVO:
      if (config->out_signals & ~DSOS_656)
        *failed = config->flags;

      if (!(shd->originalOutputCaps & STMFBIO_OUTPUT_CAPS_DVO_CONFIG))
        {
          if (config->out_signals == DSOS_NONE)
            return DFB_OK;
          else
            {
              *failed = config->flags;
              return DFB_INVARG;
            }
        }
      else
        {
          cfg.caps = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

          cfg.dvo_config &= ~STMFBIO_OUTPUT_DVO_DISABLED;

          if(config->out_signals == DSOS_656)
            cfg.dvo_config |= STMFBIO_OUTPUT_DVO_ENABLED;
          else
            cfg.dvo_config |= STMFBIO_OUTPUT_DVO_DISABLED;
        }
      break;

    default:
      *failed = config->flags;
      return DFB_INVARG;
    }

  if (*failed)
    return DFB_INVARG;

  if (ioctl (fbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &cfg) < 0)
    {
      *failed = config->flags;
      return (cfg.failed != 0) ? DFB_INVARG : DFB_IO;
    }

  return DFB_OK;
}

static DFBResult
stgfxTestOutputConfig (CoreScreen                  *screen,
                       void                        *driver_data,
                       void                        *screen_data,
                       int                          output,
                       const DFBScreenOutputConfig *config,
                       DFBScreenOutputConfigFlags  *failed)
{
  return stgfxDoOutputConfig (screen, driver_data, screen_data, output,
                              config, failed, true);
}

static DFBResult
stgfxSetOutputConfig (CoreScreen                  *screen,
                      void                        *driver_data,
                      void                        *screen_data,
                      int                          output,
                      const DFBScreenOutputConfig *config)
{
  DFBScreenOutputConfigFlags dummy;

  return stgfxDoOutputConfig (screen, driver_data, screen_data, output,
                              config, &dummy, false);
}


static ScreenFuncs stgfxPrimaryScreenFuncs = {
  .ScreenDataSize    = stgfxScreenDataSize,

  .InitScreen        = stgfxInitScreen,
  .ShutdownScreen    = stgfxShutdownScreen,

  .InitMixer         = stgfxInitMixer,
  .TestMixerConfig   = stgfxTestMixerConfig,
  .SetMixerConfig    = stgfxSetMixerConfig,

  .InitEncoder       = stgfxInitEncoder,
  .TestEncoderConfig = stgfxTestEncoderConfig,
  .SetEncoderConfig  = stgfxSetEncoderConfig,

  .InitOutput        = stgfxInitOutput,
  .TestOutputConfig  = stgfxTestOutputConfig,
  .SetOutputConfig   = stgfxSetOutputConfig,
};

void
stmfb_screen_hook_fbdev (void)
{
  if (dfb_system_type() == CORE_FBDEV)
    {
      STMfbScreenData *data = D_CALLOC (1, sizeof (*data));
      if (data)
        dfb_screens_hook_primary ((void *) 1, data,
                                  &stgfxPrimaryScreenFuncs,
                                  &data->orig_funcs,
                                  &data->orig_data);
    }
}
