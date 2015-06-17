/***********************************************************************
 *
 * File: display/ip/analogsync/hdf/fw_gen/getlevel.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "getlevel.h"

#include <vibe_os.h>
#include <vibe_debug.h>

#define DAC_VOLT 1400

typedef enum scan_type_e {
   progressive,
   interlaced
} scan_type_t;

typedef enum refresh_rate_type_e
{
   fifty_hz,
   sixty_hz
} refresh_rate_type_t;

typedef struct info_s
{
   int alines_in;
   scan_type_t stype_in;
   refresh_rate_type_t rrtype_in;
} info_t;

typedef struct {
    stm_display_mode_id_t TimingMode;
    info_t                info;
} supported_info_t;

static const supported_info_t supported_info[] = {
    { STM_TIMING_MODE_480I59940_13500       , {480  , interlaced    , sixty_hz } }
#if 0
  , { STM_TIMING_MODE_480I60000_12285       , {480  , interlaced    , sixty_hz } }
#endif
  , { STM_TIMING_MODE_480I59940_12273       , {480  , interlaced    , sixty_hz } }
  , { STM_TIMING_MODE_480P60000_27027       , {480  , progressive   , sixty_hz } }
  , { STM_TIMING_MODE_480P59940_27000       , {480  , progressive   , sixty_hz } }
#if 0
  , { STM_TIMING_MODE_480P60000_24570       , {480  , progressive   , sixty_hz } }
#endif
  , { STM_TIMING_MODE_480P60000_25200       , {480  , progressive   , sixty_hz } }
  , { STM_TIMING_MODE_576I50000_13500       , {576  , interlaced    , fifty_hz } }
  , { STM_TIMING_MODE_576I50000_14750       , {576  , interlaced    , fifty_hz } }
  , { STM_TIMING_MODE_576P50000_27000       , {576  , progressive   , fifty_hz } }
  , { STM_TIMING_MODE_1080P30000_74250      , {1080 , progressive   , sixty_hz } }
  , { STM_TIMING_MODE_1080P29970_74176      , {1080 , progressive   , sixty_hz } }
  , { STM_TIMING_MODE_1080P25000_74250      , {1080 , progressive   , fifty_hz } }
  , { STM_TIMING_MODE_1080P24000_74250      , {1080 , progressive   , fifty_hz } }
  , { STM_TIMING_MODE_1080P23976_74176      , {1080 , progressive   , fifty_hz } }
  , { STM_TIMING_MODE_1080I60000_74250      , {1080 , interlaced    , sixty_hz } }
  , { STM_TIMING_MODE_1080I59940_74176      , {1080 , interlaced    , sixty_hz } }
  , { STM_TIMING_MODE_1080I50000_74250_274M , {1080 , interlaced    , fifty_hz } }
  , { STM_TIMING_MODE_1080I50000_72000      , {1080 , interlaced    , fifty_hz } }
  , { STM_TIMING_MODE_720P60000_74250       , {720  , progressive   , sixty_hz } }
  , { STM_TIMING_MODE_720P59940_74176       , {720  , progressive   , sixty_hz } }
  , { STM_TIMING_MODE_720P50000_74250       , {720  , progressive   , fifty_hz } }
  , { STM_TIMING_MODE_1152I50000_48000      , {1152 , interlaced    , fifty_hz } }
  , { STM_TIMING_MODE_COUNT                 , {0    , interlaced    , fifty_hz } }
};


static bool get_info(const stm_display_mode_id_t TimingMode, info_t *info_p);

static bool get_info(const stm_display_mode_id_t TimingMode, info_t *info_p)
{
    int ModeIndex = 0;

    vibe_os_zero_memory(info_p, sizeof(info_t));

    while( (supported_info[ModeIndex].TimingMode != STM_TIMING_MODE_COUNT)
        && (supported_info[ModeIndex].TimingMode != TimingMode) )
    {
        ModeIndex++;
    }

    if(supported_info[ModeIndex].TimingMode == STM_TIMING_MODE_COUNT)
        return FALSE;

    info_p->alines_in = supported_info[ModeIndex].info.alines_in;
    info_p->stype_in  = supported_info[ModeIndex].info.stype_in;
    info_p->rrtype_in = supported_info[ModeIndex].info.rrtype_in;

    return TRUE;
}


bool Get_AS_Levels(stm_display_mode_id_t TimingMode, video_t *Video_p)
{
   const int look_up[3][6] = { { -286,    0,    0,    0,   54,  714}
                             , { -300,    0,    0,    0,    0,  700}
                             , { -300,  300,    0,    0,    0,  700} };

   info_t info_c;
   int i = 0;
   int j = 0;
   int offset;
   uint32_t look_up_dec[6];

    if(!get_info(TimingMode, &info_c))
        return FALSE;

   if (info_c.alines_in == 1080 || info_c.alines_in == 720)
      i = 2;
   else if (info_c.alines_in == 576)
      i = 1;
   else if (info_c.alines_in == 480) {
      if (info_c.stype_in == progressive) i = 1;
      else if (info_c.stype_in == interlaced) i = 0; }

   if (TimingMode == STM_TIMING_MODE_1080I50000_72000 || TimingMode == STM_TIMING_MODE_1152I50000_48000)
      i = 1;

   offset = -1*look_up[i][0];

   for(j=0; j<=5; j++) {look_up_dec[j] = ((look_up[i][j]+offset+20)*1024*2+DAC_VOLT)/(2*DAC_VOLT);}


   Video_p->sync_low = look_up_dec[0];
   Video_p->sync_high = look_up_dec[1];
   Video_p->blank_vbi_lines = look_up_dec[2];
   Video_p->blank_act_vid = look_up_dec[3];
   Video_p->black = look_up_dec[4];
   Video_p->peak_luma = look_up_dec[5];

   return TRUE;
}
