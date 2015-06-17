/*
   (c) Copyright 2010       STMicroelectronics (R&D) Ltd.
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by André Draszik <andre.draszik@st.com>.

   Based on work by Denis Oliver Kropp <dok@directfb.org>,
                    Andreas Hundt <andi@fischlustig.de>,
                    Sven Neumann <neo@directfb.org>,
                    Ville Syrjälä <syrjala@sci.fi> and
                    Claudio Ciccani <klan@users.sf.net>.

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

#include <config.h>

#include <sys/ioctl.h>
#include <string.h>

#include <asm/types.h>
#include <linux/stmfb.h>

#include <directfb.h>

#include <direct/memcpy.h>

#include <core/coretypes.h>
#include <core/screens.h>

#include <misc/conf.h>

#include "stmfbdev.h"
#include "fb.h"
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, u_int32_t)
#endif

D_DEBUG_DOMAIN( STMfbdev_Screen, "STMfbdev/Screen", "STMfb System Module Screen Handling" );


typedef struct __attribute__((__aligned__(32))) _STMfbdevScreenVideoMode {
     enum stmfbio_output_standard stm_standard;

     DFBScreenOutputResolution    resolution;
     DFBScreenEncoderScanMode     scanmode;
     DFBScreenEncoderFrequency    frequency;
     DFBScreenEncoderTVStandards  tv_standard;

     struct _STMfbdevScreenVideoMode *next;

#if D_DEBUG_ENABLED
     const char *frequency_str;
     const char *resolution_str;
     const char *scanmode_str;
     const char *tv_standard_str;
#endif
} STMfbdevScreenVideoMode;

#if D_DEBUG_ENABLED
#  define ASSIGN(x,y) x = y, x##_str = #y
#else /* D_DEBUG_ENABLED */
#  define ASSIGN(x,y) x = y
#endif /* D_DEBUG_ENABLED */

typedef struct {
     int magic;

     int screenid;
     /* the DirectFB indexes, assigned to these encoders */
     unsigned int encoder_main;
     unsigned int encoder_sd;
     unsigned int encoder_analog;
     unsigned int encoder_hdmi;
     unsigned int encoder_dvo;

     STMfbdevScreenVideoMode             *modes; /* list of modes supported */
     STMfbdevScreenVideoMode              mode; /* current mode */

     struct stmfbio_outputinfo            orig_info; /* startup */
     struct stmfbio_output_configuration  orig_config; /* startup */

     FusionSHMPoolShared *shmpool;
} STMfbdevScreenSharedData;

/******************************************************************************/
/* Parallel Count carries out bit counting in a parallel fashion. Consider n
   after the first line has finished executing. Imagine splitting n into
   pairs of bits. Each pair contains the number of ones in those two bit
   positions in the original n. After the second line has finished executing,
   each nibble contains the number of ones in those four bits positions in
   the original n. Continuing this for five iterations, the 64 bits contain
   the number of ones among these sixty-four bit positions in the original n.
   That is what we wanted to compute. */

#define TWO64(c) (0x1llu << (c))
#define MASK64(c) (((unsigned long long)(-1)) / (TWO64(TWO64(c)) + 1llu))
#define COUNT64(x,c) ((x) & MASK64(c)) + (((x) >> (TWO64(c))) & MASK64(c))
static unsigned int
_bitcount64 (unsigned long long n)
{
    n = COUNT64 (n, 0);
    n = COUNT64 (n, 1);
    n = COUNT64 (n, 2);
    n = COUNT64 (n, 3);
    n = COUNT64 (n, 4);
    n = COUNT64 (n, 5);
    return n ;
}


static void
stmfbdevScreen_build_full_videomode( STMfbdevScreenVideoMode * const m )
{
     D_ASSERT( _bitcount64( m->stm_standard ) == 1 );

     /* resolution */
     if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_SMPTE274M ))
          ASSIGN( m->resolution, DSOR_1920_1080 );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_WQFHD5660
                                                | STMFBIO_STD_WQFHD5650) ))
          ASSIGN( m->resolution, DSOR_1440_540 );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_SMPTE296M ))
          ASSIGN( m->resolution, DSOR_1280_720 );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_QFHD1830
                                                | STMFBIO_STD_QFHD1825
                                                | STMFBIO_STD_QFHD3660
                                                | STMFBIO_STD_QFHD3650
                                                | STMFBIO_STD_QFHD5660
                                                | STMFBIO_STD_QFHD5650) ))
          ASSIGN( m->resolution, DSOR_960_540 );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_576P_50
                                                | STMFBIO_STD_625_50) ))
          ASSIGN( m->resolution, DSOR_720_576 );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_480P_60
                                                | STMFBIO_STD_480P_59_94
                                                | STMFBIO_STD_525_60) ))
          ASSIGN( m->resolution, DSOR_720_480 );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_VESA ))
          ASSIGN( m->resolution, DSOR_640_480 );
     else
          ASSIGN( m->resolution, DSOR_UNKNOWN );

     /* scanmode */
     if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_PROGRESSIVE ))
          ASSIGN( m->scanmode, DSESM_PROGRESSIVE );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_INTERLACED ))
          ASSIGN( m->scanmode, DSESM_INTERLACED );
     else
          ASSIGN( m->scanmode, DSESM_UNKNOWN );

     /* frequency */
     if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_1080P_23_976 ))
          ASSIGN( m->frequency, DSEF_23_976HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_1080P_24 ))
          ASSIGN( m->frequency, DSEF_24HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_1080P_25
                                                | STMFBIO_STD_QFHD1825) ))
          ASSIGN( m->frequency, DSEF_25HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_1080P_29_97
                                                | STMFBIO_STD_QFHD1830) ))
          ASSIGN( m->frequency, DSEF_29_97HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_1080P_30 ))
          ASSIGN( m->frequency, DSEF_30HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_1080P_50
                                                | STMFBIO_STD_1080I_50
                                                | STMFBIO_STD_720P_50
                                                | STMFBIO_STD_576P_50
                                                | STMFBIO_STD_625_50
                                                | STMFBIO_STD_QFHD3650
                                                | STMFBIO_STD_WQFHD5650
                                                | STMFBIO_STD_QFHD5650) ))
          ASSIGN( m->frequency, DSEF_50HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_1080P_59_94
                                                | STMFBIO_STD_1080I_59_94
                                                | STMFBIO_STD_720P_59_94
                                                | STMFBIO_STD_480P_59_94
                                                | STMFBIO_STD_VGA_59_94
                                                | STMFBIO_STD_525_60
                                                | STMFBIO_STD_QFHD3660
                                                | STMFBIO_STD_WQFHD5660
                                                | STMFBIO_STD_QFHD5660) ))
          ASSIGN( m->frequency, DSEF_59_94HZ );
     else if (D_FLAGS_IS_SET( m->stm_standard, (STMFBIO_STD_1080P_60
                                                | STMFBIO_STD_1080I_60
                                                | STMFBIO_STD_720P_60
                                                | STMFBIO_STD_480P_60
                                                | STMFBIO_STD_VGA_60) ))
          ASSIGN( m->frequency, DSEF_60HZ );
     else
          ASSIGN( m->frequency, DSEF_UNKNOWN );

     /* analogue standard - DSETV_PAL and DSETV_NTSC denote _any_ specific
        compatible standard */
     if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_PAL_BDGHI ))
          ASSIGN( m->tv_standard, DSETV_PAL | DSETV_PAL_BG | DSETV_PAL_I );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_PAL_M ))
          ASSIGN( m->tv_standard, DSETV_PAL | DSETV_PAL_M );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_PAL_N ))
          ASSIGN( m->tv_standard, DSETV_PAL | DSETV_PAL_N );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_PAL_Nc ))
          ASSIGN( m->tv_standard, DSETV_PAL | DSETV_PAL_NC );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_PAL_60 ))
          ASSIGN( m->tv_standard, DSETV_PAL_60 );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_SECAM ))
          ASSIGN( m->tv_standard, DSETV_SECAM );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_NTSC_M ))
          ASSIGN( m->tv_standard, DSETV_NTSC );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_NTSC_M_JP ))
          ASSIGN( m->tv_standard, DSETV_NTSC | DSETV_NTSC_M_JPN );
     else if (D_FLAGS_IS_SET( m->stm_standard, STMFBIO_STD_NTSC_443 ))
          ASSIGN( m->tv_standard, DSETV_NTSC | DSETV_NTSC_443 );
     else
          ASSIGN( m->tv_standard, DSETV_DIGITAL );
}

static DFBResult
stmfbdevScreen_set_mode( const STMfbdev                * const stmfbdev,
                         STMfbdevScreenSharedData      * const shared,
                         const STMfbdevScreenVideoMode * const m,
                         bool                           test )
{
     struct stmfbio_outputinfo info;

     D_DEBUG_AT( STMfbdev_Screen, "%s( mode: %p )\n", __FUNCTION__, m );

     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );
     D_ASSERT( m != NULL );

     /* start with current config */
     info.outputid = shared->orig_info.outputid;

     info.activate = test ? STMFBIO_ACTIVATE_TEST : STMFBIO_ACTIVATE_IMMEDIATE;
     info.standard = m->stm_standard;

     D_DEBUG_AT( STMfbdev_Screen, "  -> %.16llx\n", info.standard );

     if (ioctl( stmfbdev->fd, STMFBIO_SET_OUTPUTINFO, &info ) < 0) {
          typeof (errno) errno_backup = errno;
          D_DEBUG_AT( STMfbdev_Screen, "  => FAILED (%d %m)!\n", errno );
          return errno2result( errno_backup );
     }

     D_DEBUG_AT( STMfbdev_Screen, "  => SUCCESS\n" );

     /* remember new config */
     if (!test) {
          shared->orig_info.standard = info.standard;

          if (&shared->mode != m)
               shared->mode = *m;
     }

     return DFB_OK;
}

static DFBResult
stmfbdevScreen_get_supported_modes( const STMfbdev           * const stmfbdev,
                                    STMfbdevScreenSharedData *shared )
{
     struct stmfbio_outputstandards  standards;
     unsigned int                    n_modes;

     D_DEBUG_AT( STMfbdev_Screen, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     /* get output standards supported on the current platform */
     standards.outputid = shared->orig_info.outputid;
     if (ioctl( stmfbdev->fd, STMFBIO_GET_OUTPUTSTANDARDS, &standards ) < 0) {
          typeof (errno) errno_backup = errno;
          D_PERROR( "STMfbdev/Screen: Could not get supported output mode!\n" );
          return errno2result( errno_backup );
     }

     n_modes = _bitcount64( standards.all_standards );
     D_DEBUG_AT( STMfbdev_Screen, "  -> %u modes\n", n_modes );

     shared->modes = SHCALLOC( shared->shmpool, n_modes ? : 1,
                               sizeof(STMfbdevScreenVideoMode) );
     if (!shared->modes)
          return D_OOSHM();

     /* in the (unlikely) case no modes where reported, try the current
        mode, and if that is not possible, fail. */
     if (!n_modes) {
          *shared->modes = shared->mode;
          if (stmfbdevScreen_set_mode( stmfbdev, shared, shared->modes, true )) {
               D_ERROR( "STMfbdev/Screen: "
                        "No modes announced and current mode not supported!\n" );

               return DFB_INIT;
          }

          D_DEBUG_AT( STMfbdev_Screen, "  -> Modelist\n" );
          D_DEBUG_AT( STMfbdev_Screen, "    +> %.16llx: %s %s %s %s\n",
                      shared->mode.stm_standard, shared->mode.resolution_str,
                      shared->mode.scanmode_str, shared->mode.frequency_str,
                      shared->mode.tv_standard_str );

          return DFB_OK;
     }

     {
     STMfbdevScreenVideoMode *m = shared->modes;

     D_DEBUG_AT( STMfbdev_Screen, "  -> Modelist\n" );
     do {
          m->stm_standard = standards.all_standards & -standards.all_standards;
          standards.all_standards &= ~m->stm_standard;

          stmfbdevScreen_build_full_videomode( m );
          D_DEBUG_AT( STMfbdev_Screen, "    +> %.16llx: %.2x %.2x %.2x %.3x %s %s %s %s\n",
                      m->stm_standard, m->resolution, m->scanmode,
                      m->frequency, m->tv_standard,
                      m->resolution_str, m->scanmode_str,
                      m->frequency_str, m->tv_standard_str );

          m->next = m+1;
          ++m;
     } while (--n_modes > 0);
     --m; m->next = NULL;
     }

     D_ASSUME( n_modes == 0 );
     D_ASSUME( standards.all_standards == STMFBIO_STD_UNKNOWN );

     return DFB_OK;
}

static const STMfbdevScreenVideoMode *
stmfbdevScreen_find_mode (const STMfbdevScreenSharedData * const shared,
                          const DFBScreenEncoderConfig   * const config)
{
     const STMfbdevScreenVideoMode *m, *best_match;

     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );
     D_ASSERT( config != NULL );

     if (!D_FLAGS_ARE_SET (config->flags, (DSECONF_SCANMODE
                                           | DSECONF_FREQUENCY
                                           | DSECONF_RESOLUTION)))
          return NULL;

     D_DEBUG_AT( STMfbdev_Screen, "%s: %.2x %.2x %.2x %s%.3x%s\n",
                 __FUNCTION__,
                 config->resolution, config->scanmode, config->frequency,
                 (config->flags & DSECONF_TV_STANDARD) ? "" : " (",
                  config->tv_standard,
                 (config->flags & DSECONF_TV_STANDARD) ? "" : ")" );

     for (best_match = NULL, m = shared->modes; m; m = m->next) {
          if (config->frequency != m->frequency)
               continue;
          if (config->scanmode != m->scanmode)
               continue;
          if (config->resolution != m->resolution)
               continue;

          if (!(config->flags & DSECONF_TV_STANDARD)) {
               D_DEBUG_AT( STMfbdev_Screen, "  -> exact: %s %s %s (%s)\n",
                           m->resolution_str, m->scanmode_str,
                           m->frequency_str, m->tv_standard_str );
               return m;
          }

          if (config->tv_standard == DSETV_ALL)
               best_match = m;
          else if ((config->tv_standard & m->tv_standard) == config->tv_standard) {
               /* exact match */
               D_DEBUG_AT( STMfbdev_Screen, "  -> exact: %s %s %s %s\n",
                           m->resolution_str, m->scanmode_str,
                           m->frequency_str, m->tv_standard_str );
               return m;
          }
     }

     if (!best_match)
          D_WARN( "  -> couldn't find a matching output standard for res/scan/hz/tv: %.2x %.2x %.2x %s%.3x%s\n",
                  config->resolution, config->scanmode, config->frequency,
                  (config->flags & DSECONF_TV_STANDARD) ? "" : "(",
                  config->tv_standard,
                  (config->flags & DSECONF_TV_STANDARD) ? "" : ")" );
     else
          D_DEBUG_AT( STMfbdev_Screen,
                      "  -> best match: %s %s %s %s\n",
                      best_match->resolution_str, best_match->scanmode_str,
                      best_match->frequency_str,
                      best_match->tv_standard_str );

     return best_match;
}

/******************************************************************************/

static int
stmfbdevScreenDataSize( void )
{
     D_DEBUG_AT( STMfbdev_Screen, "%s() <- %u\n",
                 __FUNCTION__, sizeof(STMfbdevScreenSharedData) );

     return sizeof(STMfbdevScreenSharedData);
}

static DFBResult
stmfbdevInitScreen( CoreScreen           *screen,
                    CoreGraphicsDevice   *device,
                    void                 *driver_data,
                    void                 *screen_data,
                    DFBScreenDescription *description )
{
     const STMfbdev           * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;
     u32                       hw_caps;
     int                       n_outputs = -1;
     int                       n_encoders = -1;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p %d )\n", __FUNCTION__, screen,
                 stmfbdev->shared->num_screens + 1 );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_ASSERT( shared != NULL );

     D_MAGIC_SET( shared, STMfbdevScreenSharedData );

     shared->screenid = stmfbdev->shared->num_screens++;

     shared->shmpool = stmfbdev->shared->shmpool;

     shared->encoder_main
          = shared->encoder_sd
          = shared->encoder_analog
          = shared->encoder_hdmi
          = shared->encoder_dvo
          = -1;

     /* remember startup output info, so we can restore it on shutdown*/
     shared->orig_info.outputid = STMFBIO_OUTPUTID_MAIN;
     if (ioctl( stmfbdev->fd, STMFBIO_GET_OUTPUTINFO, &shared->orig_info ) < 0) {
          typeof (errno) errno_backup = errno;
          D_PERROR( "STMfbdev/Screen: Could not get current output mode\n" );
          return errno2result( errno_backup );
     }

/*     D_DEBUG_AT( STMfbdev_Screen,
                 "  -> current: %dx%d @ %d,%d-%dx%d (%d: %s) %ubpp pitch %u @ 0x%08lx\n",
                 pc->source.w, pc->source.h, pc->dest.x, pc->dest.y,
                 pc->dest.dim.w, pc->dest.dim.h, pc->format,
                 dfb_pixelformat_name( stmfb_to_dsbf( pc->format ) ),
                 pc->bitdepth, pc->pitch, pc->baseaddr );
*/
     D_DEBUG_AT( STMfbdev_Screen, "  -> current mode: 0x%.16llx\n",
                  shared->orig_info.standard );

     /* remember stmfb's output config */
     shared->orig_config.outputid = STMFBIO_OUTPUTID_MAIN;
     if (ioctl( stmfbdev->fd,
                STMFBIO_GET_OUTPUT_CONFIG, &shared->orig_config ) < 0) {
          typeof (errno) errno_backup = errno;
          D_PERROR( "STMfbdev/Screen: Could not get current output config\n" );
          return errno2result( errno_backup );
     }
     hw_caps = shared->orig_config.caps;
     D_DEBUG_AT( STMfbdev_Screen, "  -> extended caps: 0x%08x\n", hw_caps );

     /* Set the screen name */
     snprintf( description->name, sizeof (description->name),
               "STMfbdev Screen %d", shared->screenid );

     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC;

     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND )) {
          D_DEBUG_AT( STMfbdev_Screen, "    +> have mixer background\n" );
         description->caps |= DSCCAPS_MIXERS;
         ++description->mixers;
     }

     if (D_FLAGS_IS_SET( hw_caps, (STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG
                                   | STMFBIO_OUTPUT_CAPS_HDMI_CONFIG
                                   | STMFBIO_OUTPUT_CAPS_DVO_CONFIG) )) {
          D_DEBUG_AT( STMfbdev_Screen, "    +> have main encoder\n" );
          description->caps |= DSCCAPS_ENCODERS;
          shared->encoder_main = ++n_encoders;
     }
     /* all additional encoders are slaved off the main one */
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG )) {
          D_DEBUG_AT( STMfbdev_Screen, "    +> have analogue\n" );
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_SDTV_ENCODING ))
               D_DEBUG_AT( STMfbdev_Screen, "      +> with SDTV\n" );
          shared->encoder_analog = ++n_encoders;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_HDMI_CONFIG )) {
          D_DEBUG_AT( STMfbdev_Screen, "    +> have HDMI\n" );
          shared->encoder_hdmi = ++n_encoders;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_DVO_CONFIG )) {
          D_DEBUG_AT( STMfbdev_Screen, "    +> have DVO\n" );
          shared->encoder_dvo = ++n_encoders;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_PSI_MASK )) {
          D_DEBUG_AT( STMfbdev_Screen, "    +> have PSI\n" );
          description->caps |= DSCCAPS_ENCODERS;
          shared->encoder_sd = ++n_encoders;
     }

     description->outputs = ++n_outputs;
     description->encoders = ++n_encoders;

     return DFB_OK;
}

static DFBResult
stmfbdevShutdownScreen( CoreScreen *screen,
                        void       *driver_data,
                        void       *screen_data )
{
     const STMfbdev           * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p )\n", __FUNCTION__, screen );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     /* try and restore the driver's state. */
     shared->orig_info.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     if (ioctl( stmfbdev->fd, STMFBIO_SET_OUTPUTINFO, &shared->orig_info ) < 0)
          D_PERROR( "STMfbdev/Screen: Could not restore output mode\n" );

     shared->orig_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     if (ioctl( stmfbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &shared->orig_config ) < 0)
          D_PERROR( "STMfbdev/Screen: Could not restore output config: %.8x\n",
                    shared->orig_config.failed );

     SHFREE( shared->shmpool, shared->modes );

     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

/* Mixer configuration */
static DFBResult
stmfbdevInitMixer( CoreScreen                *screen,
                   void                      *driver_data,
                   void                      *screen_data,
                   int                        mixer,
                   DFBScreenMixerDescription *description,
                   DFBScreenMixerConfig      *config )
{
     const STMfbdev                 * const stmfbdev = driver_data;
     const STMfbdevScreenSharedData * const shared = screen_data;
     const struct stmfbio_output_configuration *oc;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p %d )\n",
                 __FUNCTION__, screen, mixer );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     (void) stmfbdev;

     description->caps = DSMCAPS_FULL | DSMCAPS_BACKGROUND;

     DFB_DISPLAYLAYER_IDS_ADD( description->layers, DLID_PRIMARY );

     snprintf( description->name, sizeof (description->name),
               "STMfbdev Mixer" );

     config->flags = DSMCONF_TREE | DSMCONF_BACKGROUND;
     config->tree = DSMT_FULL;

     oc = &shared->orig_config;
     config->background.a = (oc->mixer_background >> 24) & 0xff;
     config->background.r = (oc->mixer_background >> 16) & 0xff;
     config->background.g = (oc->mixer_background >>  8) & 0xff;
     config->background.b = (oc->mixer_background >>  0) & 0xff;

     D_DEBUG_AT( STMfbdev_Screen, "  -> background colour: 0x%08x\n",
                 oc->mixer_background );

     return DFB_OK;
}

static DFBResult
stmfbdevDoMixerConfig( CoreScreen                     *screen,
                       const STMfbdev                 * const stmfbdev,
                       const STMfbdevScreenSharedData * const shared,
                       int                             mixer,
                       const DFBScreenMixerConfig     *config,
                       DFBScreenMixerConfigFlags      *failed,
                       bool                            test )
{
     DFBScreenMixerConfigFlags           fail = DSMCONF_NONE;
     struct stmfbio_output_configuration cfg;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p %d ) test: %c\n",
                 __FUNCTION__, screen, mixer, test ? 'y' : 'n' );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     if (!D_FLAGS_ARE_IN( config->flags, DSMCONF_TREE | DSMCONF_BACKGROUND ))
          fail |= (config->flags & ~(DSMCONF_TREE | DSMCONF_BACKGROUND));

     cfg.outputid = STMFBIO_OUTPUTID_MAIN;
     cfg.activate = test ? STMFBIO_ACTIVATE_TEST : STMFBIO_ACTIVATE_IMMEDIATE;
     cfg.caps = 0;

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_BACKGROUND )) {
          D_DEBUG_AT( STMfbdev_Screen, "  -> background %.2x%.2x%.2x%.2x\n",
                      config->background.a, config->background.r,
                      config->background.g, config->background.b );

          cfg.caps |= STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
          cfg.mixer_background = (0
                                  | (config->background.a << 24)
                                  | (config->background.r << 16)
                                  | (config->background.g <<  8)
                                  | (config->background.b <<  0));
     }

     if (ioctl( stmfbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &cfg ) < 0)
          fail |= DSMCONF_BACKGROUND;

     if (D_FLAGS_IS_SET( config->flags, DSMCONF_TREE )
         && D_FLAGS_INVALID( config->tree, DSMT_FULL ))
          fail |= (config->tree & ~DSMT_FULL);

     if (failed)
          *failed = fail;

     if (fail) {
          D_DEBUG_AT( STMfbdev_Screen, "  => FAILED!\n" );
          return DFB_UNSUPPORTED;
     }

     D_DEBUG_AT( STMfbdev_Screen, "  => SUCCESS\n" );

     return DFB_OK;
}

static DFBResult
stmfbdevTestMixerConfig( CoreScreen                 *screen,
                         void                       *driver_data,
                         void                       *screen_data,
                         int                         mixer,
                         const DFBScreenMixerConfig *config,
                         DFBScreenMixerConfigFlags  *failed )
{
     return stmfbdevDoMixerConfig (screen, driver_data, screen_data, mixer,
                                   config, failed, true);
}

static DFBResult
stmfbdevSetMixerConfig( CoreScreen                 *screen,
                        void                       *driver_data,
                        void                       *screen_data,
                        int                         mixer,
                        const DFBScreenMixerConfig *config )
{
     return stmfbdevDoMixerConfig (screen, driver_data, screen_data, mixer,
                                   config, NULL, false);
}


/* Encoder configuration */
static DFBResult
stmfbdevInitEncoder( CoreScreen                   *screen,
                     void                         *driver_data,
                     void                         *screen_data,
                     int                           encoder,
                     DFBScreenEncoderDescription  *description,
                     DFBScreenEncoderConfig       *config )
{
     const STMfbdev           * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;
     DFBResult                 res;
     const struct stmfbio_output_configuration *oc;
     u32                                        hw_caps;
     const STMfbdevScreenVideoMode             *m;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p %d )\n",
                 __FUNCTION__, screen, encoder );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     oc = &shared->orig_config;
     hw_caps = oc->caps;

     if (encoder == shared->encoder_main) {
          /* current mode */
          shared->mode.stm_standard = shared->orig_info.standard;
          stmfbdevScreen_build_full_videomode( &shared->mode );

          res = stmfbdevScreen_get_supported_modes( stmfbdev, shared );
          if (res != DFB_OK)
               return res;

          /* master DFBScreenEncoder - signals and connectors can not be
             changed on this one - only the output standard. */
          snprintf( description->name, sizeof (description->name),
                    "STMfbdev master output" );

          D_ASSUME( D_FLAGS_IS_SET( hw_caps,
                                    (STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG
                                     | STMFBIO_OUTPUT_CAPS_HDMI_CONFIG
                                     | STMFBIO_OUTPUT_CAPS_DVO_CONFIG) ));

          description->type = DSET_DIGITAL;

          /* supported resolutions */
          for (m = shared->modes; m; m = m->next)
               description->all_resolutions |= m->resolution;
          if (description->all_resolutions)
               description->caps = (DSECAPS_SCANMODE | DSECAPS_FREQUENCY
                                    | DSECAPS_RESOLUTION);

          if (D_FLAGS_IS_SET( oc->caps, STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG )) {
               D_DEBUG_AT( STMfbdev_Screen, "    +> have analogue (slave)\n" );
               description->caps |= (DSECAPS_CONNECTORS
                                     | DSECAPS_OUT_SIGNALS);

               description->all_connectors |= DSOC_COMPONENT;
               description->out_signals |= DSOS_RGB | DSOS_YCBCR;

               if (D_FLAGS_IS_SET( oc->caps, STMFBIO_OUTPUT_CAPS_SDTV_ENCODING )) {
                    D_DEBUG_AT( STMfbdev_Screen, "      +> with SDTV\n" );
                    description->type |= DSET_TV;
                    /* add in supported TV standards */
                    for (m = shared->modes; m; m = m->next)
                         description->tv_standards |= m->tv_standard;
                    D_FLAGS_CLEAR( description->tv_standards, DSETV_DIGITAL );
                    if (description->tv_standards)
                         D_FLAGS_SET( description->caps, DSECAPS_TV_STANDARDS );

                    description->all_connectors |= DSOC_SCART | DSOC_YC | DSOC_CVBS;
                    description->out_signals |= DSOS_RGB | DSOS_YC | DSOS_CVBS;
               }
          }
          if (D_FLAGS_IS_SET( oc->caps, STMFBIO_OUTPUT_CAPS_HDMI_CONFIG )) {
               D_DEBUG_AT( STMfbdev_Screen, "    +> have HDMI (slave)\n" );
               description->caps |= (DSECAPS_CONNECTORS
                                     | DSECAPS_OUT_SIGNALS);

               description->all_connectors |= DSOC_HDMI;
               description->out_signals |= DSOS_HDMI | DSOS_RGB | DSOS_YCBCR;
          }
          if (D_FLAGS_IS_SET( oc->caps, STMFBIO_OUTPUT_CAPS_DVO_CONFIG )) {
               D_DEBUG_AT( STMfbdev_Screen, "    +> have DVO (slave)\n" );
               description->caps |= (DSECAPS_CONNECTORS
                                     | DSECAPS_OUT_SIGNALS);

               description->all_connectors |= DSOC_656;
               description->out_signals |= DSOS_656;
          }

          /* current config */
          m = &shared->mode;
          config->resolution = m->resolution;
          config->scanmode = m->scanmode;
          config->frequency = m->frequency;
          config->tv_standard = m->tv_standard;

          config->flags |= (m->resolution != DSOR_UNKNOWN) ? DSECONF_RESOLUTION : 0;
          config->flags |= (m->scanmode != DSESM_UNKNOWN) ? DSECONF_SCANMODE : 0;
          config->flags |= (m->frequency != DSEF_UNKNOWN) ? DSECONF_FREQUENCY : 0;
          config->flags |= (m->tv_standard != DSETV_UNKNOWN) ? DSECONF_TV_STANDARD : 0;
     }
     else if (encoder == shared->encoder_analog) {
          D_ASSUME( D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG ) );

          snprintf( description->name, sizeof (description->name),
                    "STMfbdev Analogue slave" );

          /* caps */
          description->caps |= DSECAPS_OUT_SIGNALS | DSECAPS_CONNECTORS;
          description->all_connectors |= DSOC_COMPONENT;
          description->out_signals = DSOS_RGB | DSOS_YCBCR;
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_SDTV_ENCODING )) {
               description->all_connectors |= DSOC_SCART | DSOC_YC | DSOC_CVBS;
               description->out_signals |= DSOS_CVBS | DSOS_YC;
          }

          /* current config */
          D_FLAGS_SET( config->flags, DSECONF_OUT_SIGNALS | DSECONF_CONNECTORS );
          D_FLAGS_SET( config->out_connectors, DSOC_COMPONENT );
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_SDTV_ENCODING ))
               config->out_connectors |= DSOC_SCART | DSOC_YC | DSOC_CVBS;

          D_FLAGS_SET( config->out_signals, DSOS_NONE );
          if (D_FLAGS_IS_SET( oc->analogue_config, STMFBIO_OUTPUT_ANALOGUE_RGB ))
               D_FLAGS_SET( config->out_signals, DSOS_RGB );
          if (D_FLAGS_IS_SET( oc->analogue_config, STMFBIO_OUTPUT_ANALOGUE_YPrPb ))
               D_FLAGS_SET( config->out_signals, DSOS_YCBCR );
          if (D_FLAGS_IS_SET( oc->analogue_config, STMFBIO_OUTPUT_ANALOGUE_YC ))
               D_FLAGS_SET( config->out_signals, DSOS_YC );
          if (D_FLAGS_IS_SET( oc->analogue_config, STMFBIO_OUTPUT_ANALOGUE_CVBS ))
               D_FLAGS_SET( config->out_signals, DSOS_CVBS );
     }
     else if (encoder == shared->encoder_hdmi) {
          D_ASSUME( D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_HDMI_CONFIG ) );

          snprintf( description->name, sizeof (description->name),
                    "STMfbdev HDMI slave" );

          /* caps */
          description->caps |= DSECAPS_OUT_SIGNALS | DSECAPS_CONNECTORS;
          description->all_connectors |= DSOC_HDMI;
          description->out_signals |= DSOS_HDMI | DSOS_RGB | DSOS_YCBCR;

          /* current config */
          D_FLAGS_SET( config->flags, DSECONF_OUT_SIGNALS | DSECONF_CONNECTORS );
          D_FLAGS_SET( config->out_connectors, DSOC_HDMI );

          if (D_FLAGS_IS_SET( oc->hdmi_config, STMFBIO_OUTPUT_HDMI_DISABLED ))
               D_FLAGS_SET( config->out_signals, DSOS_NONE );
          else {
               D_FLAGS_SET( config->out_signals, DSOS_HDMI );
               /* A bit of bending the interface to specify the HDMI
                  colourspace */
               if (D_FLAGS_IS_SET( oc->hdmi_config, STMFBIO_OUTPUT_HDMI_YUV ))
                    D_FLAGS_SET( config->out_signals, DSOS_YCBCR );
               else
                    D_FLAGS_SET( config->out_signals, DSOS_RGB );
          }
     }
     else if (encoder == shared->encoder_dvo) {
          D_ASSUME( D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_DVO_CONFIG ) );

          snprintf( description->name, sizeof (description->name),
                    "STMfbdev DVO slave" );

          /* caps */
          description->caps |= DSECAPS_OUT_SIGNALS | DSECAPS_CONNECTORS;
          description->all_connectors |= DSOC_656;
          description->out_signals |= DSOS_656;

          /* current config */
          D_FLAGS_SET( config->flags, DSECONF_OUT_SIGNALS | DSECONF_CONNECTORS );
          D_FLAGS_SET( config->out_connectors, DSOC_656 );

          if (D_FLAGS_IS_SET( oc->dvo_config, STMFBIO_OUTPUT_DVO_DISABLED ))
               D_FLAGS_SET( config->out_signals, DSOS_NONE );
          else {
               D_FLAGS_SET( config->out_signals, DSOS_656 );
               /* A bit of bending the interface to specify the DVO
                  colourspace */
               switch( oc->dvo_config & STMFBIO_OUTPUT_DVO_MODE_MASK ) {
                    case STMFBIO_OUTPUT_DVO_YUV_444_16BIT:
                    case STMFBIO_OUTPUT_DVO_YUV_444_24BIT:
                    case STMFBIO_OUTPUT_DVO_YUV_422_16BIT:
                         D_FLAGS_SET( config->out_signals, DSOS_YCBCR );
                         break;

                    case STMFBIO_OUTPUT_DVO_ITUR656:
                    case STMFBIO_OUTPUT_DVO_RGB_24BIT:
                         D_FLAGS_SET( config->out_signals, DSOS_RGB );

                    default:
                         /* should not be reached */
                         break;
               }
          }
     }
     else if (encoder == shared->encoder_sd) {
          D_ASSUME( D_FLAGS_IS_SET( oc->caps, STMFBIO_OUTPUT_CAPS_SDTV_ENCODING ));
          D_ASSUME( D_FLAGS_IS_SET( oc->caps, STMFBIO_OUTPUT_CAPS_PSI_MASK ));

          /* the outputs and signals are handled by the main analogue
             DFBScreenEncoder, this one just adds in the colour
             adjustments. */
          snprintf( description->name, sizeof (description->name),
                    "STMfbdev DENC slave" );

          /* caps and current config in one go */
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_BRIGHTNESS )) {
               /* caps */
               description->caps |= DSECAPS_BRIGHTNESS;
               /* current config */
               config->flags |= DSECONF_ADJUSTMENT;
               config->adjustment.flags |= DCAF_BRIGHTNESS;
               config->adjustment.brightness = ((u16) oc->brightness) << 8;
               D_DEBUG_AT( STMfbdev_Screen, "  => brightness: %u\n",
                           config->adjustment.brightness );
          }
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_CONTRAST )) {
               /* caps */
               description->caps |= DSECAPS_CONTRAST;
               /* current config */
               config->flags |= DSECONF_ADJUSTMENT;
               config->adjustment.flags |= DCAF_CONTRAST;
               config->adjustment.contrast = ((u16) oc->contrast) << 8;
               D_DEBUG_AT( STMfbdev_Screen, "  => contrast: %u\n",
                           config->adjustment.contrast );
          }
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_SATURATION )) {
               /* caps */
               description->caps |= DSECAPS_SATURATION;
               /* current config */
               config->flags |= DSECONF_ADJUSTMENT;
               config->adjustment.flags |= DCAF_SATURATION;
               config->adjustment.saturation = ((u16) oc->saturation) << 8;
               D_DEBUG_AT( STMfbdev_Screen, "  => saturation: %u\n",
                           config->adjustment.saturation );
          }
          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_HUE )) {
               /* caps */
               description->caps |= DSECAPS_HUE;
               /* current config */
               config->flags |= DSECONF_ADJUSTMENT;
               config->adjustment.flags |= DCAF_HUE;
               config->adjustment.hue = ((u16) oc->hue) << 8;
               D_DEBUG_AT( STMfbdev_Screen, "  => hue: %u\n",
                           config->adjustment.hue );
          }
     }
     else
          return DFB_BUG;

     return DFB_OK;
}

static DFBResult
stmfbdevDoEncoderConfig( CoreScreen                   *screen,
                         void                         *driver_data,
                         void                         *screen_data,
                         int                           encoder,
                         const DFBScreenEncoderConfig *config,
                         DFBScreenEncoderConfigFlags  *failed,
                         bool                          test )
{
     const STMfbdev           * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;
     u32                                 hw_caps;
     struct stmfbio_output_configuration cfg;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p %d ) test: %c\n",
                 __FUNCTION__, screen, encoder, test ? 'y' : 'n' );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     cfg.outputid = STMFBIO_OUTPUTID_MAIN;
     if ( ioctl( stmfbdev->fd, STMFBIO_GET_OUTPUT_CONFIG, &cfg ) < 0 ) {
          *failed = config->flags;
          return DFB_IO;
     }

     hw_caps = shared->orig_config.caps;
     cfg.activate = test ? STMFBIO_ACTIVATE_TEST : STMFBIO_ACTIVATE_IMMEDIATE;
     cfg.caps = 0;

     if (encoder == shared->encoder_main) {
          const STMfbdevScreenVideoMode *mode;

          *failed |= config->flags & ~(DSECONF_SCANMODE | DSECONF_FREQUENCY
                                       | DSECONF_RESOLUTION
                                       | DSECONF_TV_STANDARD);
          if (!*failed) {
               mode = stmfbdevScreen_find_mode( shared, config );
               if (!mode
                   || stmfbdevScreen_set_mode( stmfbdev, shared, mode,
                                               test ) != DFB_OK) {
                    *failed |= (DSECONF_SCANMODE | DSECONF_FREQUENCY
                                | DSECONF_RESOLUTION);
                    *failed |= (config->flags & DSECONF_TV_STANDARD);
               }
          }
     }
     else if (encoder == shared->encoder_analog) {
          D_DEBUG_AT( STMfbdev_Screen, "  -> analogue 0x%.2x (outsig %x flg %x conn %x)\n",
                      config->out_signals, config->out_signals,
                      config->flags, config->out_connectors );

          *failed |= config->flags & ~(DSECONF_CONNECTORS
                                       | DSECONF_OUT_SIGNALS);
          if (config->flags & DSECONF_OUT_SIGNALS
              && D_FLAGS_INVALID( config->out_signals, (DSOS_RGB
                                                        | DSOS_YCBCR
                                                        | DSOS_CVBS
                                                        | DSOS_YC) ))
               *failed |= DSECONF_OUT_SIGNALS;
          /* it's not allowed to change connectors, they merely serve an
             informational purpose. */
          if (config->flags & DSECONF_CONNECTORS
              && config->out_connectors != (DSOC_SCART | DSOC_YC
                                            | DSOC_CVBS | DSOC_COMPONENT) )
               *failed |= DSECONF_CONNECTORS;

          /* you can change the signals coming out of the connectors,
             though. */
          if (!*failed) {
               if (!D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG )) {
                    if (config->out_signals != DSOS_NONE)
                         *failed |= DSECONF_OUT_SIGNALS;
               }
               else {
                    cfg.caps = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

                    D_FLAGS_CLEAR( cfg.analogue_config,
                                   STMFBIO_OUTPUT_ANALOGUE_MASK );

                    if (!config->out_signals)
                         D_DEBUG_AT( STMfbdev_Screen, "    +> off\n" );
                    if (D_FLAGS_IS_SET( config->out_signals, DSOS_RGB )) {
                         D_DEBUG_AT( STMfbdev_Screen, "    +> RGB\n" );
                         cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_RGB;
                    }
                    if (D_FLAGS_IS_SET( config->out_signals, DSOS_YCBCR )) {
                         D_DEBUG_AT( STMfbdev_Screen, "    +> YPrPb\n" );
                         cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YPrPb;
                    }
                    if (D_FLAGS_IS_SET( config->out_signals, DSOS_CVBS )) {
                         D_DEBUG_AT( STMfbdev_Screen, "    +> CVBS\n" );
                         cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CVBS;
                    }
                    if (D_FLAGS_IS_SET( config->out_signals, DSOS_YC )) {
                         D_DEBUG_AT( STMfbdev_Screen, "    +> Y/C\n" );
                         cfg.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YC;
                    }
               }
          }
     }
     else if (encoder == shared->encoder_hdmi) {
          D_DEBUG_AT( STMfbdev_Screen, "  -> HDMI 0x%.2x\n",
                      config->out_signals );

          *failed |= config->flags & ~(DSECONF_CONNECTORS
                                       | DSECONF_OUT_SIGNALS);
          if (config->flags & DSECONF_OUT_SIGNALS
              /* only HDMI + (RGB or YCbCr) supported */
              && (D_FLAGS_INVALID( config->out_signals, (DSOS_HDMI
                                                         | DSOS_RGB
                                                         | DSOS_YCBCR) )
                  /* can't have both, RGB and YCbCr */
                  || D_FLAGS_ARE_SET( config->out_signals, (DSOS_RGB
                                                            | DSOS_YCBCR) )))
               *failed |= DSECONF_OUT_SIGNALS;
          /* it's not allowed to change connectors, they merely serve an
             informational purpose. */
          if (config->flags & DSECONF_CONNECTORS
              && config->out_connectors != DSOC_HDMI)
               *failed |= DSECONF_CONNECTORS;

          if (!*failed) {
               if (!D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_HDMI_CONFIG )) {
                    if (config->out_signals != DSOS_NONE)
                         *failed |= DSECONF_OUT_SIGNALS;
               }
               else {
                    cfg.caps = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;

                    D_FLAGS_CLEAR( cfg.hdmi_config,
                                   (STMFBIO_OUTPUT_HDMI_DISABLED
                                    | STMFBIO_OUTPUT_HDMI_YUV
                                    | STMFBIO_OUTPUT_HDMI_422) );

                    if (D_FLAGS_IS_SET( config->out_signals, DSOS_HDMI )) {
                         cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_ENABLED;
                         /* HDMI colourspace is RGB by default, only change it
                            when YCBCR is specified. */
                         if (D_FLAGS_IS_SET( config->out_signals, DSOS_YCBCR )) {
                              D_DEBUG_AT( STMfbdev_Screen, "    +> YCbCr\n" );
                              cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_YUV;
                              cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_444;
                         }
                         else if (D_FLAGS_IS_SET( config->out_signals,
                                                  DSOS_RGB )) {
                              D_DEBUG_AT( STMfbdev_Screen, "    +> RGB\n" );
                              cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_RGB;
                         }
                         else {
                              D_DEBUG_AT( STMfbdev_Screen, "    +> unknown\n" );
                              *failed |= DSECONF_OUT_SIGNALS;
                         }
                    }
                    else if (config->out_signals == DSOS_NONE) {
                         D_DEBUG_AT( STMfbdev_Screen, "    +> off\n" );
                         cfg.hdmi_config |= STMFBIO_OUTPUT_HDMI_DISABLED;
                    }
               }
          }
     }
     else if (encoder == shared->encoder_dvo) {
          D_DEBUG_AT( STMfbdev_Screen, "  -> DVO 0x%.2x\n",
                      config->out_signals );

          *failed |= config->flags & ~(DSECONF_CONNECTORS
                                       | DSECONF_OUT_SIGNALS);
          if (config->flags & DSECONF_OUT_SIGNALS
              && (D_FLAGS_INVALID( config->out_signals, (DSOS_656
                                                         | DSOS_RGB
                                                         | DSOS_YCBCR) )
                  /* can't have both, RGB and YCbCr */
                  || D_FLAGS_ARE_SET( config->out_signals, (DSOS_RGB
                                                            | DSOS_YCBCR) )))
               *failed |= DSECONF_OUT_SIGNALS;
          /* it's not allowed to change connectors, they merely serve an
             informational purpose. */
          if (config->flags & DSECONF_CONNECTORS
              && config->out_connectors != DSOC_656)
               *failed |= DSECONF_CONNECTORS;

          if (!*failed) {
               if (!D_FLAGS_IS_SET( hw_caps, STMFBIO_OUTPUT_CAPS_DVO_CONFIG )) {
                    if (config->out_signals != DSOS_NONE)
                         *failed |= DSECONF_OUT_SIGNALS;
               }
               else {
                    cfg.caps = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

                    D_FLAGS_CLEAR( cfg.dvo_config,
                                   (STMFBIO_OUTPUT_DVO_DISABLED
                                    | STMFBIO_OUTPUT_DVO_MODE_MASK) );

                    if (D_FLAGS_IS_SET( config->out_signals, DSOS_656 )) {
                         cfg.dvo_config |= STMFBIO_OUTPUT_DVO_ENABLED;
                         /* DVO colourspace is YUV444 16bit by default, only
                            change it when RGB is specified. */
                         if (D_FLAGS_IS_SET( config->out_signals, DSOS_RGB )) {
                              D_DEBUG_AT( STMfbdev_Screen, "    +> RGB 24bit\n" );
                              cfg.dvo_config |= STMFBIO_OUTPUT_DVO_RGB_24BIT;
                         }
                         else if (D_FLAGS_IS_SET( config->out_signals,
                                                  DSOS_YCBCR )) {
                              D_DEBUG_AT( STMfbdev_Screen, "    +> YCbCr 444 16bit\n" );
                              cfg.dvo_config |= STMFBIO_OUTPUT_DVO_YUV_444_16BIT;
                         }
                         else {
                              D_DEBUG_AT( STMfbdev_Screen, "    +> unknown\n" );
                              *failed |= DSECONF_OUT_SIGNALS;
                         }
                    }
                    else if (config->out_signals == DSOS_NONE) {
                         D_DEBUG_AT( STMfbdev_Screen, "    +> off\n" );
                         cfg.dvo_config |= STMFBIO_OUTPUT_DVO_DISABLED;
                    }
               }
          }
     }
     else if (encoder == shared->encoder_sd) {
          D_DEBUG_AT( STMfbdev_Screen, "  -> DENC adjustment 0x%.2x\n",
                      config->out_signals );

          *failed = config->flags & ~DSECONF_ADJUSTMENT;
          if (!*failed) {
               if (config->adjustment.flags & DCAF_BRIGHTNESS) {
                    cfg.caps |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;
                    cfg.brightness = config->adjustment.brightness >> 8;
               }
               if (config->adjustment.flags & DCAF_SATURATION) {
                    cfg.caps |= STMFBIO_OUTPUT_CAPS_SATURATION;
                    cfg.saturation = config->adjustment.saturation >> 8;
               }
               if (config->adjustment.flags & DCAF_CONTRAST) {
                    cfg.caps |= STMFBIO_OUTPUT_CAPS_CONTRAST;
                    cfg.contrast = config->adjustment.contrast >> 8;
               }
               if (config->adjustment.flags & DCAF_HUE) {
                    cfg.caps |= STMFBIO_OUTPUT_CAPS_HUE;
                    cfg.hue = config->adjustment.hue >> 8;
               }
          }
     }
     else
          return DFB_BUG;

     if (!*failed && cfg.caps
         && ioctl( stmfbdev->fd, STMFBIO_SET_OUTPUT_CONFIG, &cfg ) < 0)
          *failed = config->flags;

     if (*failed) {
          D_DEBUG_AT( STMfbdev_Screen, "  => FAILED (0x%.8x)\n", *failed );
          return DFB_INVARG;
     }

     D_DEBUG_AT( STMfbdev_Screen, "  => SUCCESS\n" );
     return DFB_OK;
}

static DFBResult
stmfbdevTestEncoderConfig( CoreScreen                   *screen,
                           void                         *driver_data,
                           void                         *screen_data,
                           int                           encoder,
                           const DFBScreenEncoderConfig *config,
                           DFBScreenEncoderConfigFlags  *failed )
{
     return stmfbdevDoEncoderConfig( screen, driver_data, screen_data,
                                     encoder, config, failed, true );
}

static DFBResult
stmfbdevSetEncoderConfig( CoreScreen                   *screen,
                          void                         *driver_data,
                          void                         *screen_data,
                          int                           encoder,
                          const DFBScreenEncoderConfig *config )
{
     DFBScreenEncoderConfigFlags dummy = DSECONF_NONE;
     return stmfbdevDoEncoderConfig( screen, driver_data, screen_data,
                                     encoder, config, &dummy, false );
}

/* remaining screen things */
static DFBResult
stmfbdevWaitVSync( CoreScreen *screen,
                   void       *driver_data,
                   void       *screen_data )
{
     STMfbdev                 * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;
     static const int          zero = 0;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p )\n", __FUNCTION__, screen );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );
     (void) shared;

     if (dfb_config->pollvsync_none)
          return DFB_OK;

     if (ioctl( stmfbdev->fd, FBIO_WAITFORVSYNC, &zero ))
          return errno2result( errno );

     return DFB_OK;
}

static DFBResult
stmfbdevGetVSyncCount( CoreScreen    *screen,
                       void          *driver_data,
                       void          *screen_data,
                       unsigned long *ret_count )
{
     STMfbdev                 * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;
     struct fb_vblank          vblank;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p )\n", __FUNCTION__, screen );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );
     D_ASSERT( ret_count != NULL );
     (void) shared;

     if (!ret_count)
          return DFB_INVARG;

     if (ioctl( stmfbdev->fd, FBIOGET_VBLANK, &vblank ))
          return errno2result( errno );

     if (!D_FLAGS_IS_SET( vblank.flags, FB_VBLANK_HAVE_COUNT ))
          return DFB_UNSUPPORTED;

     *ret_count = vblank.count;

     return DFB_OK;
}

static DFBResult
stmfbdevGetScreenSize( CoreScreen *screen,
                       void       *driver_data,
                       void       *screen_data,
                       int        *ret_width,
                       int        *ret_height )
{
     STMfbdev                 * const stmfbdev = driver_data;
     STMfbdevScreenSharedData * const shared = screen_data;

     D_DEBUG_AT( STMfbdev_Screen, "%s( %p )\n", __FUNCTION__, screen );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevScreenSharedData );

     (void) stmfbdev;

     switch (shared->mode.resolution) {
          case DSOR_1920_1080:
               *ret_width = 1920; *ret_height = 1080; break;
          case DSOR_1280_720:
               *ret_width = 1280; *ret_height =  720; break;
          case DSOR_1440_540:
               *ret_width = 1440; *ret_height =  540; break;
          case DSOR_960_540:
               *ret_width =  960; *ret_height =  540; break;
          case DSOR_720_576:
               *ret_width =  720; *ret_height =  576; break;
          case DSOR_720_480:
               *ret_width =  720; *ret_height =  480; break;
          case DSOR_640_480:
               *ret_width =  640; *ret_height =  480; break;

          default:
               return DFB_FAILURE;
     }

     return DFB_OK;
}


ScreenFuncs _g_stmfbdevScreenFuncs = {
     .ScreenDataSize = stmfbdevScreenDataSize,

     .InitScreen     = stmfbdevInitScreen,
     .ShutdownScreen = stmfbdevShutdownScreen,

     .InitMixer       = stmfbdevInitMixer,
     .TestMixerConfig = stmfbdevTestMixerConfig,
     .SetMixerConfig  = stmfbdevSetMixerConfig,

     .InitEncoder       = stmfbdevInitEncoder,
     .TestEncoderConfig = stmfbdevTestEncoderConfig,
     .SetEncoderConfig  = stmfbdevSetEncoderConfig,

     .WaitVSync      = stmfbdevWaitVSync,
     .GetVSyncCount  = stmfbdevGetVSyncCount,
     .GetScreenSize  = stmfbdevGetScreenSize,
};
