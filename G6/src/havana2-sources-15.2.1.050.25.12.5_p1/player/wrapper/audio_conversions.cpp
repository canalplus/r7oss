/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/


// Standard C headers and substitutions
#include <stdarg.h>
extern "C" void *memcpy(void *a, const void *b, unsigned int s);
extern "C" void *memset(void *a, int val, unsigned int s);
extern "C" int   memcmp(__const void *a, __const void *b , unsigned int s);


#include "report.h"
#include "osinline.h"

#include "audio_conversions.h"

/*! The max number of channels supported by AC_MODE */
#define STMSEAUDIOCONV_MAX_ACMODE_CHANNELS 8

// Static const arrays

/**
 * @brief AcMode to HDMI InfoFrame and string
 *        HdmiChannelCount set to 1 so that it
 *        get encoded as '0' : refers to stream header
 */
//

#define HDMI_CHANNEL_CNT_REFERRING_TO_STREAM_HEADER  1
#define HDMI_SPKR_MAPPING_REFERRING_TO_STREAM_HEADER 0

static const struct
{
    enum eAccAcMode AudioMode;
    unsigned char   HdmiSpeakerMapping;
    unsigned char   HdmiChannelCount;
    const char     *Text;
} StmSeAudioLUTAcModeName[] =
{
#define E(a,b, c) {a, (unsigned char) (AMODE_HDMI(b)),c, #a}
#define Eh(a, c)  {a, (unsigned char) (AMODE_HDMI(a)),c, #a}
    E(ACC_MODE20t, ACC_HDMI_MODE20, 2),
    E(ACC_MODE10, ACC_HDMI_MODE30, 3),
    E(ACC_MODE20, ACC_HDMI_MODE20, 2),
    E(ACC_MODE30, ACC_HDMI_MODE30, 3),
    E(ACC_MODE21, ACC_HDMI_MODE21, 3),
    E(ACC_MODE31, ACC_HDMI_MODE31, 4),
    E(ACC_MODE22, ACC_HDMI_MODE22, 4),
    E(ACC_MODE32, ACC_HDMI_MODE32, 5),
    E(ACC_MODE23, ACC_HDMI_MODE23, 5),
    E(ACC_MODE33, ACC_HDMI_MODE33, 6),
    E(ACC_MODE24, ACC_HDMI_MODE24, 6),
    E(ACC_MODE34, ACC_HDMI_MODE34, 7),
    E(ACC_MODE42, ACC_HDMI_MODE42, 6),
    E(ACC_MODE44, ACC_HDMI_MODE42, 6), // HDMI_MODE44 doesn't exist
    E(ACC_MODE52, ACC_HDMI_MODE52, 7),
    E(ACC_MODE53, ACC_HDMI_MODE52, 7), // HDMI_MODE52 doesn't exist

    E(ACC_MODEk10_V1V2OFF, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk10_V1ON, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk10_V2ON, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk10_V1V2ON, ACC_HDMI_MODE30, 3),

    E(ACC_MODE30_T100, ACC_HDMI_MODE30, 3),
    E(ACC_MODE30_T200, ACC_HDMI_MODE30, 3),
    E(ACC_MODE22_T010, ACC_HDMI_MODE22, 4),
    E(ACC_MODE32_T020, ACC_HDMI_MODE32, 5),
    E(ACC_MODE23_T100, ACC_HDMI_MODE23, 5),
    E(ACC_MODE23_T010, ACC_HDMI_MODE23, 5),

    E(ACC_MODEk_AWARE, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk_AWARE10, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk_AWARE20, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk_AWARE30, ACC_HDMI_MODE20, 2),

    E(ACC_MODE_undefined_1E, ACC_HDMI_MODE20, 2),
    E(ACC_MODE34SS, ACC_HDMI_MODE34, 7),

    E(ACC_MODEk20_V1V2OFF, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk20_V1ON, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk20_V2ON, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk20_V1V2ON, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk20_V1Left, ACC_HDMI_MODE20, 2),
    E(ACC_MODEk20_V2Right, ACC_HDMI_MODE20, 2),

    E(ACC_MODE_undefined_26, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_27, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_28, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_29, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_2A, ACC_HDMI_MODE20, 2),
    E(ACC_MODE24_DIRECT, ACC_HDMI_MODE24, 6),
    E(ACC_MODE34_DIRECT, ACC_HDMI_MODE34, 7),
    E(ACC_MODE_undefined_2D, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_2E, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_2F, ACC_HDMI_MODE20, 2),

    E(ACC_MODEk30_V1V2OFF, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk30_V1ON, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk30_V2ON, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk30_V1V2ON, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk30_V1Left, ACC_HDMI_MODE30, 3),
    E(ACC_MODEk30_V2Right, ACC_HDMI_MODE30, 3),

    E(ACC_MODE_undefined_36, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_37, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_38, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_39, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_3A, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_3B, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_3C, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_3D, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_3E, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_undefined_3F, ACC_HDMI_MODE20, 2),

    E(ACC_MODE20LPCMA, ACC_HDMI_MODE20, 2),

    Eh(ACC_HDMI_MODE20, 2),
    Eh(ACC_HDMI_MODE30, 3),
    Eh(ACC_HDMI_MODE21, 3),
    Eh(ACC_HDMI_MODE31, 4),
    Eh(ACC_HDMI_MODE22, 4),
    Eh(ACC_HDMI_MODE32, 5),
    Eh(ACC_HDMI_MODE23, 5),
    Eh(ACC_HDMI_MODE33, 6),
    Eh(ACC_HDMI_MODE24, 6),
    Eh(ACC_HDMI_MODE34, 7),
    Eh(ACC_HDMI_MODE40, 4),
    Eh(ACC_HDMI_MODE50, 5),
    Eh(ACC_HDMI_MODE41, 5),
    Eh(ACC_HDMI_MODE51, 6),
    Eh(ACC_HDMI_MODE42, 6),
    Eh(ACC_HDMI_MODE52, 7),

    Eh(ACC_HDMI_MODE32_T100, 6),
    Eh(ACC_HDMI_MODE32_T010, 6),
    Eh(ACC_HDMI_MODE22_T200, 6),
    Eh(ACC_HDMI_MODE42_WIDE, 6),
    Eh(ACC_HDMI_MODE33_T010, 6),
    Eh(ACC_HDMI_MODE33_T100, 6),
    Eh(ACC_HDMI_MODE32_T110, 7),
    Eh(ACC_HDMI_MODE32_T200, 7),
    Eh(ACC_HDMI_MODE52_WIDE, 7),

    Eh(ACC_HDMI_MODE_RESERV0x32, 2),
    Eh(ACC_HDMI_MODE_RESERV0x34, 2),
    Eh(ACC_HDMI_MODE_RESERV0x36, 2),
    Eh(ACC_HDMI_MODE_RESERV0x38, 2),
    Eh(ACC_HDMI_MODE_RESERV0x3A, 2),
    Eh(ACC_HDMI_MODE_RESERV0x3C, 2),
    Eh(ACC_HDMI_MODE_RESERV0x3E, 2),
    Eh(ACC_HDMI_MODE_RESERVED, 2),

    E(ACC_MODE_1p1, ACC_HDMI_MODE20, 2),

    E(ACC_MODE11p20, ACC_HDMI_MODE20, 2),
    E(ACC_MODE10p20, ACC_HDMI_MODE20, 2),
    E(ACC_MODE20p20, ACC_HDMI_MODE20, 2),
    E(ACC_MODE30p20, ACC_HDMI_MODE30, 3),
    Eh(ACC_MODE_NOLFE_RAW, 2),

    E(ACC_MODE20t_LFE, ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE10_LFE, ACC_HDMI_MODE30_LFE, 4),
    E(ACC_MODE20_LFE, ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE30_LFE, ACC_HDMI_MODE30_LFE, 4),
    E(ACC_MODE21_LFE, ACC_HDMI_MODE21_LFE, 4),
    E(ACC_MODE31_LFE, ACC_HDMI_MODE31_LFE, 5),
    E(ACC_MODE22_LFE, ACC_HDMI_MODE22_LFE, 5),
    E(ACC_MODE32_LFE, ACC_HDMI_MODE32_LFE, 6),
    E(ACC_MODE23_LFE, ACC_HDMI_MODE23_LFE, 6),
    E(ACC_MODE33_LFE, ACC_HDMI_MODE33_LFE, 7),
    E(ACC_MODE24_LFE, ACC_HDMI_MODE24_LFE, 7),
    E(ACC_MODE34_LFE, ACC_HDMI_MODE34_LFE, 8),
    E(ACC_MODE42_LFE, ACC_HDMI_MODE42_LFE, 7),
    E(ACC_MODE44_LFE, ACC_HDMI_MODE42_LFE, 7), // HDMI_MODE44 doesn't exist
    E(ACC_MODE52_LFE, ACC_HDMI_MODE52_LFE, 8),
    E(ACC_MODE53_LFE, ACC_HDMI_MODE52_LFE, 8), // HDMI_MODE53 doesn't exist

    E(ACC_MODE30_LFE_T100, ACC_MODE30_LFE, 4),
    E(ACC_MODE30_LFE_T200, ACC_MODE30_LFE, 4),
    E(ACC_MODE22_LFE_T010, ACC_MODE22_LFE, 5),
    E(ACC_MODE32_LFE_T020, ACC_MODE32_LFE, 6),
    E(ACC_MODE23_LFE_T100, ACC_MODE23_LFE, 6),
    E(ACC_MODE23_LFE_T010, ACC_MODE23_LFE, 6),

    E(ACC_MODE34SS_LFE, ACC_MODE34_LFE, 8),

    E(ACC_MODE_ALL, ACC_HDMI_MODE34_LFE, 8),
    E(ACC_MODE_ALL1, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_ALL2, ACC_HDMI_MODE20, 2),
    E(ACC_MODE_ALL3, ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE_ALL4, ACC_HDMI_MODE30_LFE, 4),
    E(ACC_MODE_ALL5, ACC_HDMI_MODE31_LFE, 5),
    E(ACC_MODE_ALL6, ACC_HDMI_MODE32_LFE, 6),
    E(ACC_MODE_ALL7, ACC_HDMI_MODE33_LFE, 7),
    E(ACC_MODE_ALL8, ACC_HDMI_MODE34_LFE, 8),
    E(ACC_MODE_ALL9, ACC_HDMI_MODE34_LFE, 8), // HDMI 1.4 doesn't carry more than 8ch
    E(ACC_MODE_ALL10, ACC_HDMI_MODE34_LFE, 8),// HDMI 1.4 doesn't carry more than 8ch
    E(ACC_MODE24_LFE_DIRECT, ACC_MODE24_LFE, 7),
    E(ACC_MODE34_LFE_DIRECT, ACC_MODE34_LFE, 8),

    Eh(ACC_HDMI_MODE20_LFE, 3),
    Eh(ACC_HDMI_MODE30_LFE, 2),
    Eh(ACC_HDMI_MODE21_LFE, 3),
    Eh(ACC_HDMI_MODE31_LFE, 4),
    Eh(ACC_HDMI_MODE22_LFE, 5),
    Eh(ACC_HDMI_MODE32_LFE, 6),
    Eh(ACC_HDMI_MODE23_LFE, 6),
    Eh(ACC_HDMI_MODE33_LFE, 7),
    Eh(ACC_HDMI_MODE24_LFE, 7),
    Eh(ACC_HDMI_MODE34_LFE, 8),
    Eh(ACC_HDMI_MODE40_LFE, 5),
    Eh(ACC_HDMI_MODE50_LFE, 6),
    Eh(ACC_HDMI_MODE41_LFE, 6),
    Eh(ACC_HDMI_MODE51_LFE, 7),
    Eh(ACC_HDMI_MODE42_LFE, 7),
    Eh(ACC_HDMI_MODE52_LFE, 8),

    Eh(ACC_HDMI_MODE32_T100_LFE, 8),
    Eh(ACC_HDMI_MODE32_T010_LFE, 8),
    Eh(ACC_HDMI_MODE22_T200_LFE, 7),
    Eh(ACC_HDMI_MODE42_WIDE_LFE, 7),
    Eh(ACC_HDMI_MODE33_T010_LFE, 8),
    Eh(ACC_HDMI_MODE33_T100_LFE, 8),
    Eh(ACC_HDMI_MODE32_T110_LFE, 8),
    Eh(ACC_HDMI_MODE32_T200_LFE, 8),

    Eh(ACC_HDMI_MODE52_WIDE_LFE, 8),

    Eh(ACC_HDMI_MODE_RESERV0x32_LFE, 2),
    Eh(ACC_HDMI_MODE_RESERV0x34_LFE, 2),
    Eh(ACC_HDMI_MODE_RESERV0x36_LFE, 2),
    Eh(ACC_HDMI_MODE_RESERV0x38_LFE, 2),
    Eh(ACC_HDMI_MODE_RESERV0x3A_LFE, 2),
    Eh(ACC_HDMI_MODE_RESERV0x3C_LFE, 2),
    Eh(ACC_HDMI_MODE_RESERV0x3E_LFE, 2),

    E(ACC_MODE_1p1_LFE , ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE11p20_LFE, ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE10p20_LFE, ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE20p20_LFE, ACC_HDMI_MODE20_LFE, 3),
    E(ACC_MODE30p20_LFE, ACC_HDMI_MODE30_LFE, 4),

    E(ACC_MODE_ID, ACC_HDMI_MODE20, HDMI_CHANNEL_CNT_REFERRING_TO_STREAM_HEADER)
#undef E
#undef Eh
};

/**
 * @brief ChannelId to string
 *
 */
static const struct
{
    uint8_t ChannelId;
    const char *Text;
} StmSeAudioLUTChannelIdName[] =
{
    {STM_SE_AUDIO_CHAN_L            , "L"           },
    {STM_SE_AUDIO_CHAN_R            , "R"           },
    {STM_SE_AUDIO_CHAN_LFE          , "LFE"         },
    {STM_SE_AUDIO_CHAN_C            , "C"           },
    {STM_SE_AUDIO_CHAN_LS           , "LS"          },
    {STM_SE_AUDIO_CHAN_RS           , "RS"          },
    {STM_SE_AUDIO_CHAN_LT           , "LT"          },
    {STM_SE_AUDIO_CHAN_RT           , "RT"          },
    {STM_SE_AUDIO_CHAN_LPLII        , "LPLII"       },
    {STM_SE_AUDIO_CHAN_RPLII        , "RPLII"       },
    {STM_SE_AUDIO_CHAN_CREAR        , "CREAR"       },
    {STM_SE_AUDIO_CHAN_CL           , "CL"          },
    {STM_SE_AUDIO_CHAN_CR           , "CR"          },
    {STM_SE_AUDIO_CHAN_LFEB         , "LFEB"        },
    {STM_SE_AUDIO_CHAN_L_DUALMONO   , "L_DUALMONO"  },
    {STM_SE_AUDIO_CHAN_R_DUALMONO   , "R_DUALMONO"  },
    {STM_SE_AUDIO_CHAN_LWIDE        , "LWIDE"       },
    {STM_SE_AUDIO_CHAN_RWIDE        , "RWIDE"       },
    {STM_SE_AUDIO_CHAN_LDIRS        , "LDIRS"       },
    {STM_SE_AUDIO_CHAN_RDIRS        , "RDIRS"       },
    {STM_SE_AUDIO_CHAN_LSIDES       , "LSIDES"      },
    {STM_SE_AUDIO_CHAN_RSIDES       , "RSIDES"      },
    {STM_SE_AUDIO_CHAN_LREARS       , "LREARS"      },
    {STM_SE_AUDIO_CHAN_RREARS       , "RREARS"      },
    {STM_SE_AUDIO_CHAN_CHIGH        , "CHIGH"       },
    {STM_SE_AUDIO_CHAN_LHIGH        , "LHIGH"       },
    {STM_SE_AUDIO_CHAN_RHIGH        , "RHIGH"       },
    {STM_SE_AUDIO_CHAN_LHIGHSIDE    , "LHIGHSIDE"   },
    {STM_SE_AUDIO_CHAN_RHIGHSIDE    , "RHIGHSIDE"   },
    {STM_SE_AUDIO_CHAN_CHIGHREAR    , "CHIGHREAR"   },
    {STM_SE_AUDIO_CHAN_LHIGHREAR    , "LHIGHREAR"   },
    {STM_SE_AUDIO_CHAN_RHIGHREAR    , "RHIGHREAR"   },
    {STM_SE_AUDIO_CHAN_CLOWFRONT    , "CLOWFRONT"   },
    {STM_SE_AUDIO_CHAN_TOPSUR       , "TOPSUR"      },
    {STM_SE_AUDIO_CHAN_DYNSTEREO_LS , "DYNSTEREO_LS"},
    {STM_SE_AUDIO_CHAN_DYNSTEREO_RS , "DYNSTEREO_RS"},
    {STM_SE_AUDIO_CHAN_UNKNOWN      , "UNKNOWN"     },
    {STM_SE_AUDIO_CHAN_STUFFING     , "STUFFING"    },

    {STM_SE_AUDIO_CHAN_RESERVED     , "RESERVED"    }
};

/**
 * @brief ChannelPair to string
 *
 */
static const struct
{
    enum stm_se_audio_channel_pair Pair;
    const char *Text;
} StmSeAudioLUTChannelPairName[] =
{
    /* Weird values do not change position */
    {STM_SE_AUDIO_CHANNEL_PAIR_DEFAULT            , "DEFAULT"            },
    {STM_SE_AUDIO_CHANNEL_PAIR_L_R                , "L_R"                },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE1          , "CNTR_LFE1"          },
    {STM_SE_AUDIO_CHANNEL_PAIR_LSUR_RSUR          , "LSUR_RSUR"          },
    {STM_SE_AUDIO_CHANNEL_PAIR_LSURREAR_RSURREAR  , "LSURREAR_RSURREAR"  },

    /* Normal values */
    {STM_SE_AUDIO_CHANNEL_PAIR_LT_RT              , "LT_RT"              },
    {STM_SE_AUDIO_CHANNEL_PAIR_LPLII_RPLII        , "LPLII_RPLII"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTRL_CNTRR        , "CNTRL_CNTRR"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_LHIGH_RHIGH        , "LHIGH_RHIGH"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_LWIDE_RWIDE        , "LWIDE_RWIDE"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_LRDUALMONO         , "LRDUALMONO"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_RESERVED1          , "RESERVED1"          },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_0             , "CNTR_0"             },
    {STM_SE_AUDIO_CHANNEL_PAIR_0_LFE1             , "0_LFE1"             },
    {STM_SE_AUDIO_CHANNEL_PAIR_0_LFE2             , "0_LFE2"             },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_0            , "CHIGH_0"            },
    {STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_0        , "CLOWFRONT_0"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CSURR         , "CNTR_CSURR"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGH         , "CNTR_CHIGH"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_TOPSUR        , "CNTR_TOPSUR"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGHREAR     , "CNTR_CHIGHREAR"     },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CLOWFRONT     , "CNTR_CLOWFRONT"     },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_TOPSUR       , "CHIGH_TOPSUR"       },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CHIGHREAR    , "CHIGH_CHIGHREAR"    },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CLOWFRONT    , "CHIGH_CLOWFRONT"    },
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE2          , "CNTR_LFE2"          },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE1         , "CHIGH_LFE1"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE2         , "CHIGH_LFE2"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE1     , "CLOWFRONT_LFE1"     },
    {STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE2     , "CLOWFRONT_LFE2"     },
    {STM_SE_AUDIO_CHANNEL_PAIR_LSIDESURR_RSIDESURR, "LSIDESURR_RSIDESURR"},
    {STM_SE_AUDIO_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE, "LHIGHSIDE_RHIGHSIDE"},
    {STM_SE_AUDIO_CHANNEL_PAIR_LDIRSUR_RDIRSUR    , "LDIRSUR_RDIRSUR"    },
    {STM_SE_AUDIO_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR, "LHIGHREAR_RHIGHREAR"},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_0            , "CSURR_0"            },
    {STM_SE_AUDIO_CHANNEL_PAIR_TOPSUR_0           , "TOPSUR_0"           },
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_TOPSUR       , "CSURR_TOPSUR"       },
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGH        , "CSURR_CHIGH"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGHREAR    , "CSURR_CHIGHREAR"    },
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CLOWFRONT    , "CSURR_CLOWFRONT"    },
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE1         , "CSURR_LFE1"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE2         , "CSURR_LFE2"         },
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGHREAR_0        , "CHIGHREAR_0"        },
    {STM_SE_AUDIO_CHANNEL_PAIR_DSTEREO_LsRs       , "DSTEREO_LsRs"       },
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR0              , "PAIR0"              },
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR1              , "PAIR1"              },
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR2              , "PAIR2"              },
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR3              , "PAIR3"              },

    // Termination
    {STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED      , "NOT_CONNECTED"      }
};

/**
 * @brief Lookup table to match an Acmod <> stm_se_audio_channel_placement
 *
 *
 */
static const struct
{
    enum eAccAcMode  AudioMode;                   //!< AudioMode Lookup
    unsigned char    NamedChannelCount;           //!< Number of active channels in this mode
    unsigned char    InterleavedStuffingCount;    //!< Number of channels unused (excluding those at the end)
    uint8_t          PhysicalChannels[STMSEAUDIOCONV_MAX_ACMODE_CHANNELS]; //!< Corresponding placement
} StmSeAudioLUTAcModeVsChannelIds[] =
{
#define E(mode, n, s, c0, c1, c2, c3, c4, c5, c6, c7) { mode, n, s, { \
                                                 STM_SE_AUDIO_CHAN_ ## c0, \
                                                 STM_SE_AUDIO_CHAN_ ## c1, \
                                                 STM_SE_AUDIO_CHAN_ ## c2, \
                                                 STM_SE_AUDIO_CHAN_ ## c3, \
                                                 STM_SE_AUDIO_CHAN_ ## c4, \
                                                 STM_SE_AUDIO_CHAN_ ## c5, \
                                                 STM_SE_AUDIO_CHAN_ ## c6, \
                                                 STM_SE_AUDIO_CHAN_ ## c7, \
                                                 } }
    // as E but for a different reason: there is already a better candidate for direct output, not a bug
#define DUPE(mode, n, s, c0, c1, c2, c3, c4, c5, c6, c7) E(mode, n, s, c0, c1, c2, c3, c4, c5, c6, c7)

    E(ACC_MODE10              , 1, 3, STUFFING  , STUFFING  , STUFFING, C       , STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE10_LFE          , 2, 2, STUFFING  , STUFFING  , LFE     , C       , STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE20t             , 2, 0, LT        , RT        , STUFFING, STUFFING, STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE_1p1            , 2, 0, L_DUALMONO, R_DUALMONO, STUFFING, STUFFING, STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE20              , 2, 0, L         , R         , STUFFING, STUFFING, STUFFING, STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE20      , 2, 0, L         , R         , STUFFING, STUFFING, STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE20_LFE          , 3, 0, L         , R         , LFE     , STUFFING, STUFFING, STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE20_LFE  , 3, 0, L         , R         , LFE     , STUFFING, STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE30              , 3, 1, L         , R         , STUFFING, C       , STUFFING, STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE30      , 3, 1, L         , R         , STUFFING, C       , STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE30_LFE          , 4, 0, L         , R         , LFE     , C       , STUFFING, STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE30_LFE  , 4, 0, L         , R         , LFE     , C       , STUFFING, STUFFING, STUFFING , STUFFING),
    E(ACC_MODE21              , 3, 2, L         , R         , STUFFING, STUFFING, CREAR   , STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE21      , 3, 2, L         , R         , STUFFING, STUFFING, CREAR   , STUFFING, STUFFING , STUFFING),
    E(ACC_MODE21_LFE          , 4, 1, L         , R         , LFE     , STUFFING, CREAR   , STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE21_LFE  , 4, 1, L         , R         , LFE     , STUFFING, CREAR   , STUFFING, STUFFING , STUFFING),
    E(ACC_MODE31              , 4, 1, L         , R         , STUFFING, C       , CREAR   , STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE31      , 4, 1, L         , R         , STUFFING, C       , CREAR   , STUFFING, STUFFING , STUFFING),
    E(ACC_MODE31_LFE          , 5, 0, L         , R         , LFE     , C       , CREAR   , STUFFING, STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE31_LFE  , 5, 0, L         , R         , LFE     , C       , CREAR   , STUFFING, STUFFING , STUFFING),
    E(ACC_MODE22              , 4, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE22      , 4, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , STUFFING , STUFFING),
    E(ACC_MODE22_LFE          , 5, 1, L         , R         , LFE     , STUFFING, LS      , RS      , STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE22_LFE  , 5, 1, L         , R         , LFE     , STUFFING, LS      , RS      , STUFFING , STUFFING),
    E(ACC_MODE32              , 5, 1, L         , R         , STUFFING, C       , LS      , RS      , STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE32      , 5, 1, L         , R         , STUFFING, C       , LS      , RS      , STUFFING , STUFFING),
    E(ACC_MODE32_LFE          , 6, 0, L         , R         , LFE     , C       , LS      , RS      , STUFFING , STUFFING),
    DUPE(ACC_HDMI_MODE32_LFE  , 6, 0, L         , R         , LFE     , C       , LS      , RS      , STUFFING , STUFFING),
    E(ACC_MODE23              , 5, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , CREAR    , STUFFING),
    DUPE(ACC_HDMI_MODE23      , 5, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , CREAR    , STUFFING),
    E(ACC_MODE23_LFE          , 6, 1, L         , R         , LFE     , STUFFING, LS      , RS      , CREAR    , STUFFING),
    DUPE(ACC_HDMI_MODE23_LFE  , 6, 1, L         , R         , LFE     , STUFFING, LS      , RS      , CREAR    , STUFFING),
    E(ACC_MODE33              , 6, 1, L         , R         , STUFFING, C       , LS      , RS      , CREAR    , STUFFING),
    DUPE(ACC_HDMI_MODE33      , 6, 1, L         , R         , STUFFING, C       , LS      , RS      , CREAR    , STUFFING),
    E(ACC_MODE33_LFE          , 7, 0, L         , R         , LFE     , C       , LS      , RS      , CREAR    , STUFFING),
    DUPE(ACC_HDMI_MODE33_LFE  , 7, 0, L         , R         , LFE     , C       , LS      , RS      , CREAR    , STUFFING),
    E(ACC_MODE24              , 6, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , LREARS   , RREARS),
    DUPE(ACC_HDMI_MODE24      , 6, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , LREARS   , RREARS),
    E(ACC_MODE24_LFE          , 7, 1, L         , R         , LFE     , STUFFING, LS      , RS      , LREARS   , RREARS),
    DUPE(ACC_HDMI_MODE24_LFE  , 7, 1, L         , R         , LFE     , STUFFING, LS      , RS      , LREARS   , RREARS),
    E(ACC_MODE34              , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , LREARS   , RREARS),
    DUPE(ACC_HDMI_MODE34      , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , LREARS   , RREARS),
    E(ACC_MODE34_LFE          , 8, 0, L         , R         , LFE     , C       , LS      , RS      , LREARS   , RREARS),
    DUPE(ACC_HDMI_MODE34_LFE  , 8, 0, L         , R         , LFE     , C       , LS      , RS      , LREARS   , RREARS),
    E(ACC_HDMI_MODE40         , 4, 4, L         , R         , STUFFING, STUFFING, STUFFING, STUFFING, CL       , CR),
    E(ACC_HDMI_MODE40_LFE     , 5, 3, L         , R         , LFE     , STUFFING, STUFFING, STUFFING, CL       , CR),
    E(ACC_HDMI_MODE50         , 5, 3, L         , R         , STUFFING, C       , STUFFING, STUFFING, CL       , CR),
    E(ACC_HDMI_MODE50_LFE     , 6, 2, L         , R         , LFE     , C       , STUFFING, STUFFING, CL       , CR),
    E(ACC_HDMI_MODE41         , 5, 3, L         , R         , STUFFING, STUFFING, CREAR   , STUFFING, CL       , CR),
    E(ACC_HDMI_MODE41_LFE     , 6, 2, L         , R         , LFE     , STUFFING, CREAR   , STUFFING, CL       , CR),
    E(ACC_HDMI_MODE51         , 6, 2, L         , R         , STUFFING, C       , CREAR   , STUFFING, CL       , CR),
    E(ACC_HDMI_MODE51_LFE     , 7, 1, L         , R         , LFE     , C       , CREAR   , STUFFING, CL       , CR),
    E(ACC_MODE42              , 6, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , CL       , CR),
    DUPE(ACC_HDMI_MODE42      , 6, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , CL       , CR),
    E(ACC_MODE42_LFE          , 7, 1, L         , R         , LFE     , STUFFING, LS      , RS      , CL       , CR),
    DUPE(ACC_HDMI_MODE42_LFE  , 7, 1, L         , R         , LFE     , STUFFING, LS      , RS      , CL       , CR),
    E(ACC_MODE52              , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , CL       , CR),
    DUPE(ACC_HDMI_MODE52      , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , CL       , CR),
    E(ACC_MODE52_LFE          , 8, 0, L         , R         , LFE     , C       , LS      , RS      , CL       , CR),
    DUPE(ACC_HDMI_MODE52_LFE  , 8, 0, L         , R         , LFE     , C       , LS      , RS      , CL       , CR),
    E(ACC_HDMI_MODE32_T100    , 6, 1, L         , R         , STUFFING, C       , LS      , RS      , CHIGH    , STUFFING),
    E(ACC_HDMI_MODE32_T100_LFE, 7, 0, L         , R         , LFE     , C       , LS      , RS      , CHIGH    , STUFFING),
    E(ACC_HDMI_MODE32_T010    , 6, 1, L         , R         , STUFFING, C       , LS      , RS      , TOPSUR   , STUFFING),
    E(ACC_HDMI_MODE32_T010_LFE, 7, 0, L         , R         , LFE     , C       , LS      , RS      , TOPSUR   , STUFFING),
    E(ACC_HDMI_MODE22_T200    , 6, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , LHIGH    , RHIGH),
    E(ACC_HDMI_MODE22_T200_LFE, 7, 1, L         , R         , LFE     , STUFFING, LS      , RS      , LHIGH    , RHIGH),
    E(ACC_HDMI_MODE42_WIDE    , 6, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , LWIDE    , RWIDE),
    E(ACC_HDMI_MODE42_WIDE_LFE, 7, 1, L         , R         , LFE     , STUFFING, LS      , RS      , LWIDE    , RWIDE),
    E(ACC_HDMI_MODE33_T010    , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , CREAR    , TOPSUR),
    E(ACC_HDMI_MODE33_T010_LFE, 8, 0, L         , R         , LFE     , C       , LS      , RS      , CREAR    , TOPSUR),
    E(ACC_HDMI_MODE33_T100    , 8, 0, L         , R         , LFE     , C       , LS      , RS      , CREAR    , CHIGH),
    E(ACC_HDMI_MODE33_T100_LFE, 8, 0, L         , R         , LFE     , C       , LS      , RS      , CREAR    , CHIGH),
    E(ACC_HDMI_MODE32_T110    , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , CHIGH    , TOPSUR),
    E(ACC_HDMI_MODE32_T110_LFE, 8, 0, L         , R         , LFE     , C       , LS      , RS      , CHIGH    , TOPSUR),
    E(ACC_HDMI_MODE32_T200    , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , LHIGH    , RHIGH),
    E(ACC_HDMI_MODE32_T200_LFE, 8, 0, L         , R         , LFE     , C       , LS      , RS      , LHIGH    , RHIGH),
    E(ACC_HDMI_MODE52_WIDE    , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , LWIDE    , RWIDE),
    E(ACC_HDMI_MODE52_WIDE_LFE, 8, 0, L         , R         , LFE     , C       , LS      , RS      , LWIDE    , RWIDE),

#if PCMPROCESSINGS_API_VERSION >= 0x100325 &&  PCMPROCESSINGS_API_VERSION < 0x101122
    // Unusual speaker topologies (not inclued in CEA-861 E)
    E(ACC_MODE30_T100         , 4, 3, L         , R         , STUFFING, C       , STUFFING, STUFFING, CHIGH    , STUFFING),)
    E(ACC_MODE30_T100_LFE     , 5, 2, L         , R         , LFE     , C       , STUFFING, STUFFING, CHIGH    , STUFFING),)
    E(ACC_MODE30_T200         , 5, 3, L         , R         , STUFFING, C       , STUFFING, STUFFING, LHIGH    , RHIGH),)
    E(ACC_MODE30_T200_LFE     , 6, 2, L         , R         , LFE     , C       , STUFFING, STUFFING, LHIGH    , RHIGH),)
    E(ACC_MODE22_T010         , 5, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , TOPSUR   , STUFFING),)
    E(ACC_MODE22_T010_LFE     , 6, 1, L         , R         , LFE     , STUFFING, LS      , RS      , TOPSUR   , STUFFING),)
    E(ACC_MODE32_T020         , 7, 1, L         , R         , STUFFING, C       , LS      , RS      , LHIGHSIDE, RHIGHSIDE),)
    E(ACC_MODE32_T020_LFE     , 8, 0, L         , R         , LFE     , C       , LS      , RS      , LHIGHSIDE, RHIGHSIDE),)
    E(ACC_MODE23_T100         , 5, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , CHIGH    , STUFFING),)
    E(ACC_MODE23_T100_LFE     , 6, 1, L         , R         , LFE     , STUFFING, LS      , RS      , CHIGH    , STUFFING),)
    E(ACC_MODE23_T010         , 5, 2, L         , R         , STUFFING, STUFFING, LS      , RS      , TOPSUR   , STUFFING),)
    E(ACC_MODE23_T010_LFE     , 6, 1, L         , R         , LFE     , STUFFING, LS      , RS      , TOPSUR   , STUFFING),)
#endif

    // delimiter
    E(ACC_MODE_ID             , 0, 0, STUFFING  , STUFFING  , STUFFING, STUFFING, STUFFING, STUFFING, STUFFING , STUFFING)
#undef DUPE
#undef E
};

/**
 * @brief LUT to match channel pairs to channel ids
 *
 */
static const struct
{
    enum stm_se_audio_channel_pair  Pair;       //!< Channel pair lookup
    stm_se_audio_channel_id_t       Channel[2]; //!< Corresponding pair of channel
} StmSeAudioPairVsChannelIdLUT[] =
{
    /* Weid Values Do not change position */
    {STM_SE_AUDIO_CHANNEL_PAIR_DEFAULT            , {STM_SE_AUDIO_CHAN_UNKNOWN     , STM_SE_AUDIO_CHAN_UNKNOWN     }},
    {STM_SE_AUDIO_CHANNEL_PAIR_L_R                , {STM_SE_AUDIO_CHAN_L           , STM_SE_AUDIO_CHAN_R           }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE1          , {STM_SE_AUDIO_CHAN_LFE         , STM_SE_AUDIO_CHAN_C           }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LSUR_RSUR          , {STM_SE_AUDIO_CHAN_LS          , STM_SE_AUDIO_CHAN_RS          }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LSURREAR_RSURREAR  , {STM_SE_AUDIO_CHAN_LREARS      , STM_SE_AUDIO_CHAN_RREARS      }},

    /* Normal values */
    {STM_SE_AUDIO_CHANNEL_PAIR_LT_RT              , {STM_SE_AUDIO_CHAN_LT          , STM_SE_AUDIO_CHAN_RT          }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LPLII_RPLII        , {STM_SE_AUDIO_CHAN_LPLII       , STM_SE_AUDIO_CHAN_RPLII       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTRL_CNTRR        , {STM_SE_AUDIO_CHAN_CL          , STM_SE_AUDIO_CHAN_CR          }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LHIGH_RHIGH        , {STM_SE_AUDIO_CHAN_LHIGH       , STM_SE_AUDIO_CHAN_RHIGH       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LWIDE_RWIDE        , {STM_SE_AUDIO_CHAN_LWIDE       , STM_SE_AUDIO_CHAN_RWIDE       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LRDUALMONO         , {STM_SE_AUDIO_CHAN_L_DUALMONO  , STM_SE_AUDIO_CHAN_R_DUALMONO  }},
    {STM_SE_AUDIO_CHANNEL_PAIR_RESERVED1          , {STM_SE_AUDIO_CHAN_RESERVED    , STM_SE_AUDIO_CHAN_RESERVED    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_0             , {STM_SE_AUDIO_CHAN_STUFFING    , STM_SE_AUDIO_CHAN_C           }},
    {STM_SE_AUDIO_CHANNEL_PAIR_0_LFE1             , {STM_SE_AUDIO_CHAN_LFE         , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_0_LFE2             , {STM_SE_AUDIO_CHAN_LFEB        , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_0            , {STM_SE_AUDIO_CHAN_CHIGH       , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_0        , {STM_SE_AUDIO_CHAN_CLOWFRONT   , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CSURR         , {STM_SE_AUDIO_CHAN_C           , STM_SE_AUDIO_CHAN_CREAR       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGH         , {STM_SE_AUDIO_CHAN_C           , STM_SE_AUDIO_CHAN_CHIGH       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_TOPSUR        , {STM_SE_AUDIO_CHAN_C           , STM_SE_AUDIO_CHAN_TOPSUR      }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CHIGHREAR     , {STM_SE_AUDIO_CHAN_C           , STM_SE_AUDIO_CHAN_CHIGHREAR   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_CLOWFRONT     , {STM_SE_AUDIO_CHAN_C           , STM_SE_AUDIO_CHAN_CLOWFRONT   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_TOPSUR       , {STM_SE_AUDIO_CHAN_CHIGH       , STM_SE_AUDIO_CHAN_TOPSUR      }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CHIGHREAR    , {STM_SE_AUDIO_CHAN_CHIGH       , STM_SE_AUDIO_CHAN_CHIGHREAR   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_CLOWFRONT    , {STM_SE_AUDIO_CHAN_CHIGH       , STM_SE_AUDIO_CHAN_CLOWFRONT   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE2          , {STM_SE_AUDIO_CHAN_C           , STM_SE_AUDIO_CHAN_LFEB        }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE1         , {STM_SE_AUDIO_CHAN_CHIGH       , STM_SE_AUDIO_CHAN_LFE         }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGH_LFE2         , {STM_SE_AUDIO_CHAN_CHIGH       , STM_SE_AUDIO_CHAN_LFEB        }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE1     , {STM_SE_AUDIO_CHAN_CLOWFRONT   , STM_SE_AUDIO_CHAN_LFE         }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CLOWFRONT_LFE2     , {STM_SE_AUDIO_CHAN_CLOWFRONT   , STM_SE_AUDIO_CHAN_LFEB        }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LSIDESURR_RSIDESURR, {STM_SE_AUDIO_CHAN_LSIDES      , STM_SE_AUDIO_CHAN_RSIDES      }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE, {STM_SE_AUDIO_CHAN_LHIGHSIDE   , STM_SE_AUDIO_CHAN_RHIGHSIDE   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LDIRSUR_RDIRSUR    , {STM_SE_AUDIO_CHAN_LDIRS       , STM_SE_AUDIO_CHAN_RDIRS       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR, {STM_SE_AUDIO_CHAN_LHIGHREAR   , STM_SE_AUDIO_CHAN_RHIGHREAR   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_0            , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_TOPSUR_0           , {STM_SE_AUDIO_CHAN_TOPSUR      , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_TOPSUR       , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_TOPSUR      }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGH        , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_CHIGH       }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CHIGHREAR    , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_CHIGHREAR   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_CLOWFRONT    , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_CLOWFRONT   }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE1         , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_LFE         }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CSURR_LFE2         , {STM_SE_AUDIO_CHAN_CREAR       , STM_SE_AUDIO_CHAN_LFEB        }},
    {STM_SE_AUDIO_CHANNEL_PAIR_CHIGHREAR_0        , {STM_SE_AUDIO_CHAN_CHIGHREAR   , STM_SE_AUDIO_CHAN_STUFFING    }},
    {STM_SE_AUDIO_CHANNEL_PAIR_DSTEREO_LsRs       , {STM_SE_AUDIO_CHAN_DYNSTEREO_LS, STM_SE_AUDIO_CHAN_DYNSTEREO_RS}},
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR0              , {STM_SE_AUDIO_CHAN_UNKNOWN     , STM_SE_AUDIO_CHAN_UNKNOWN     }},
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR1              , {STM_SE_AUDIO_CHAN_UNKNOWN     , STM_SE_AUDIO_CHAN_UNKNOWN     }},
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR2              , {STM_SE_AUDIO_CHAN_UNKNOWN     , STM_SE_AUDIO_CHAN_UNKNOWN     }},
    {STM_SE_AUDIO_CHANNEL_PAIR_PAIR3              , {STM_SE_AUDIO_CHAN_UNKNOWN     , STM_SE_AUDIO_CHAN_UNKNOWN     }},

    // Termination
    {STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED      , {STM_SE_AUDIO_CHAN_STUFFING    , STM_SE_AUDIO_CHAN_STUFFING    }}
};

/**
 * @brief Simple lookup table used to convert between
 *         stm_se_audio_channel_assignment and enum eAccAcMode.
 * */
static const struct
{
    enum eAccAcMode AudioMode;
    struct stm_se_audio_channel_assignment ChannelAssignment;
}
StmSeAudioLUTAcModeVsChannelPairs[] =
{
#define EEE(mode, p0, p1, p2, p3) { mode, { \
                                                 STM_SE_AUDIO_CHANNEL_PAIR_ ## p0, \
                                                 STM_SE_AUDIO_CHANNEL_PAIR_ ## p1, \
                                                 STM_SE_AUDIO_CHANNEL_PAIR_ ## p2, \
                                                 STM_SE_AUDIO_CHANNEL_PAIR_ ## p3, \
                                                 STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED } }


    EEE(ACC_MODE10,                 NOT_CONNECTED,  CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE10_LFE,             NOT_CONNECTED,  CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE20t,                LT_RT,          NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE_1p1,               LRDUALMONO,     NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE20,                 L_R,            NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE20_LFE,             L_R,            0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE30,                 L_R,            CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE30_LFE,             L_R,            CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE21,                 L_R,            NOT_CONNECTED,  CSURR_0,        NOT_CONNECTED),
    EEE(ACC_MODE21_LFE,             L_R,            0_LFE1,         CSURR_0,        NOT_CONNECTED),
    EEE(ACC_MODE31,                 L_R,            CNTR_0,         CSURR_0,        NOT_CONNECTED),
    EEE(ACC_MODE31_LFE,             L_R,            CNTR_LFE1,      CSURR_0,        NOT_CONNECTED),
    EEE(ACC_MODE22,                 L_R,            NOT_CONNECTED,  LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_MODE22_LFE,             L_R,            0_LFE1,         LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_MODE32,                 L_R,            CNTR_0,         LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_MODE32_LFE,             L_R,            CNTR_LFE1,      LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_MODE23,                 L_R,            NOT_CONNECTED,  LSUR_RSUR,      CSURR_0),
    EEE(ACC_MODE23_LFE,             L_R,            0_LFE1,         LSUR_RSUR,      CSURR_0),
    EEE(ACC_MODE33,                 L_R,            CNTR_0,         LSUR_RSUR,      CSURR_0),
    EEE(ACC_MODE33_LFE,             L_R,            CNTR_LFE1,      LSUR_RSUR,      CSURR_0),
    EEE(ACC_MODE24,                 L_R,            NOT_CONNECTED,  LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_MODE24_LFE,             L_R,            0_LFE1,         LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_MODE34,                 L_R,            CNTR_0,         LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_MODE34_LFE,             L_R,            CNTR_LFE1,      LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_HDMI_MODE40,            L_R,            NOT_CONNECTED,  NOT_CONNECTED,  CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE40_LFE,        L_R,            0_LFE1,         NOT_CONNECTED,  CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE50,            L_R,            CNTR_0,         NOT_CONNECTED,  CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE50_LFE,        L_R,            CNTR_LFE1,      NOT_CONNECTED,  CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE41,            L_R,            NOT_CONNECTED,  CSURR_0,        CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE41_LFE,        L_R,            0_LFE1,         CSURR_0,        CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE51,            L_R,            CNTR_0,         CSURR_0,        CNTRL_CNTRR),
    EEE(ACC_HDMI_MODE51_LFE,        L_R,            CNTR_LFE1,      CSURR_0,        CNTRL_CNTRR),
    EEE(ACC_MODE42,                 L_R,            NOT_CONNECTED,  LSUR_RSUR,      CNTRL_CNTRR),
    EEE(ACC_MODE42_LFE,             L_R,            0_LFE1,         LSUR_RSUR,      CNTRL_CNTRR),
    EEE(ACC_MODE52,                 L_R,            CNTR_0,         LSUR_RSUR,      CNTRL_CNTRR),
    EEE(ACC_MODE52_LFE,             L_R,            CNTR_LFE1,      LSUR_RSUR,      CNTRL_CNTRR),
    EEE(ACC_MODE44,                 L_R,            CNTRL_CNTRR,    LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_MODE53,                 L_R,            CNTR_CSURR,     LSUR_RSUR,      CNTRL_CNTRR),
#if PCMPROCESSINGS_API_VERSION >= 0x100325 &&  PCMPROCESSINGS_API_VERSION < 0x101122
    EEE(ACC_MODE30_T100,            L_R,            CNTR_0,         NOT_CONNECTED,  CHIGH_0),
    EEE(ACC_MODE30_T200,            L_R,            CNTR_0,         NOT_CONNECTED,  LHIGH_RHIGH),
    EEE(ACC_MODE22_T010,            L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0),
    EEE(ACC_MODE32_T020,            L_R,            CNTR_0,         LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE),
    EEE(ACC_MODE23_T100,            L_R,            NOT_CONNECTED,  LSUR_RSUR,      CSURR_CHIGH),
    EEE(ACC_MODE23_T010,            L_R,            NOT_CONNECTED,  LSUR_RSUR,      CSURR_TOPSUR),
#endif
    EEE(ACC_MODE20LPCMA,            L_R,            NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_HDMI_MODE20,            L_R,            NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_HDMI_MODE20_LFE,        L_R,            0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_HDMI_MODE30,            L_R,            CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_HDMI_MODE30_LFE,        L_R,            CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_HDMI_MODE21,            L_R,            NOT_CONNECTED,  CSURR_0,        NOT_CONNECTED),
    EEE(ACC_HDMI_MODE21_LFE,        L_R,            0_LFE1,         CSURR_0,        NOT_CONNECTED),
    EEE(ACC_HDMI_MODE31,            L_R,            CNTR_0,         CSURR_0,        NOT_CONNECTED),
    EEE(ACC_HDMI_MODE31_LFE,        L_R,            CNTR_LFE1,      CSURR_0,        NOT_CONNECTED),
    EEE(ACC_HDMI_MODE22,            L_R,            NOT_CONNECTED,  LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_HDMI_MODE22_LFE,        L_R,            0_LFE1,         LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_HDMI_MODE32,            L_R,            CNTR_0,         LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_HDMI_MODE32_LFE,        L_R,            CNTR_LFE1,      LSUR_RSUR,      NOT_CONNECTED),
    EEE(ACC_HDMI_MODE23,            L_R,            NOT_CONNECTED,  LSUR_RSUR,      CSURR_0),
    EEE(ACC_HDMI_MODE23_LFE,        L_R,            0_LFE1,         LSUR_RSUR,      CSURR_0),
    EEE(ACC_HDMI_MODE33,            L_R,            CNTR_0,         LSUR_RSUR,      CSURR_0),
    EEE(ACC_HDMI_MODE33_LFE,        L_R,            CNTR_LFE1,      LSUR_RSUR,      CSURR_0),
    EEE(ACC_HDMI_MODE24,            L_R,            NOT_CONNECTED,  LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_HDMI_MODE24_LFE,        L_R,            0_LFE1,         LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_HDMI_MODE34,            L_R,            CNTR_0,         LSUR_RSUR,      LSURREAR_RSURREAR),
    EEE(ACC_HDMI_MODE34_LFE,        L_R,            CNTR_LFE1,      LSUR_RSUR,      LSURREAR_RSURREAR),
    // CEA-861 (EEE) modes
    EEE(ACC_HDMI_MODE32_T100,       L_R,            CNTR_0,         LSUR_RSUR,      CHIGH_0),
    EEE(ACC_HDMI_MODE32_T100_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      CHIGH_0),
    EEE(ACC_HDMI_MODE32_T010,       L_R,            CNTR_0,         LSUR_RSUR,      TOPSUR_0),
    EEE(ACC_HDMI_MODE32_T010_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      TOPSUR_0),
    EEE(ACC_HDMI_MODE22_T200,       L_R,            NOT_CONNECTED,  LSUR_RSUR,      LHIGH_RHIGH),
    EEE(ACC_HDMI_MODE22_T200_LFE,   L_R,            0_LFE1,         LSUR_RSUR,      LHIGH_RHIGH),
    EEE(ACC_HDMI_MODE42_WIDE,       L_R,            NOT_CONNECTED,  LSUR_RSUR,      LWIDE_RWIDE),
    EEE(ACC_HDMI_MODE42_WIDE_LFE,   L_R,            0_LFE1,         LSUR_RSUR,      LWIDE_RWIDE),
    EEE(ACC_HDMI_MODE33_T010,       L_R,            CNTR_0,         LSUR_RSUR,      CSURR_TOPSUR),
    EEE(ACC_HDMI_MODE33_T010_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      CSURR_TOPSUR),
    EEE(ACC_HDMI_MODE33_T100,       L_R,            CNTR_LFE1,      LSUR_RSUR,      CSURR_CHIGH),
    EEE(ACC_HDMI_MODE33_T100_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      CSURR_CHIGH),
    EEE(ACC_HDMI_MODE32_T110,       L_R,            CNTR_0,         LSUR_RSUR,      CHIGH_TOPSUR),
    EEE(ACC_HDMI_MODE32_T110_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      CHIGH_TOPSUR),
    EEE(ACC_HDMI_MODE32_T200,       L_R,            CNTR_0,         LSUR_RSUR,      LHIGH_RHIGH),
    EEE(ACC_HDMI_MODE32_T200_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      LHIGH_RHIGH),
    EEE(ACC_HDMI_MODE52_WIDE,       L_R,            CNTR_0,         LSUR_RSUR,      LWIDE_RWIDE),
    EEE(ACC_HDMI_MODE52_WIDE_LFE,   L_R,            CNTR_LFE1,      LSUR_RSUR,      LWIDE_RWIDE),

    // Special case where Mono is rendered on a 1 or 2ch buffer.
    EEE(ACC_MODE_ALL1,              CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    // DD+ speaker topologies for certification
    EEE(ACC_MODE_ALL2,              PAIR0,          NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE_ALL2,              PAIR1,          NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE_ALL2,              PAIR2,          NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    EEE(ACC_MODE_ALL2,              PAIR3,          NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),

    // delimiter
    EEE(ACC_MODE_ID,                NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED)

#undef EEE
};

//Sorting helper functions
/**
 * @brief Quick sort arrays of uint8_t opttimised for channel lists in increasing
 *
 *
 * http://pages.ripco.net/~jgamble/nw.html
 * @note It may be a good idea to further optimise by looking for sort algorithms optimised for
 * "almost sorted values"
 *
 * @param Array array of (uint8_t) values we are sorting
 * @param n the number of uint8_t to sort
 */
static int U8QuickSortN(uint8_t *A, const int N)
{
    if (NULL == A)
    {
        SE_ERROR("NULL Pointer\n");
        return 1;
    }

#define SWAP(a ,b) {uint8_t __mini; __mini = min(A[a], A[b]); A[b] = max(A[a],A[b]); A[a] = __mini;}

    switch (N)
    {
    case 1:
    {
        return 0;
    }

    case 2:
    {
        SWAP(0, 1);
        return 0;
    }

    case 4:
    {
        SWAP(0, 1); SWAP(2, 3);
        SWAP(0, 2); SWAP(1, 3);
        SWAP(1, 2);
        return 0;
    }

    case 6:
    {
        SWAP(1, 2); SWAP(4, 5);
        SWAP(0, 2); SWAP(3, 5);
        SWAP(0, 1); SWAP(3, 4); SWAP(2, 5);
        SWAP(0, 3); SWAP(1, 4);
        SWAP(2, 4); SWAP(1, 3);
        SWAP(2, 3);
        return 0;
    }

    case 8:
    {
        SWAP(0, 1); SWAP(2, 3); SWAP(4, 5); SWAP(6, 7);
        SWAP(0, 2); SWAP(1, 3); SWAP(4, 6); SWAP(5, 7);
        SWAP(1, 2); SWAP(5, 6); SWAP(0, 4); SWAP(3, 7);
        SWAP(1, 5); SWAP(2, 6);
        SWAP(1, 4); SWAP(3, 6);
        SWAP(2, 4); SWAP(3, 5);
        SWAP(3, 4);
        return 0;
    }

    case 32:
    {
        SWAP(0, 16); SWAP(1, 17); SWAP(2, 18); SWAP(3, 19); SWAP(4, 20); SWAP(5, 21); SWAP(6, 22); SWAP(7, 23);
        SWAP(8, 24); SWAP(9, 25); SWAP(10, 26); SWAP(11, 27); SWAP(12, 28); SWAP(13, 29); SWAP(14, 30); SWAP(15, 31);
        SWAP(0, 8); SWAP(1, 9); SWAP(2, 10); SWAP(3, 11); SWAP(4, 12); SWAP(5, 13); SWAP(6, 14); SWAP(7, 15);
        SWAP(16, 24); SWAP(17, 25); SWAP(18, 26); SWAP(19, 27); SWAP(20, 28); SWAP(21, 29); SWAP(22, 30); SWAP(23, 31);
        SWAP(8, 16); SWAP(9, 17); SWAP(10, 18); SWAP(11, 19); SWAP(12, 20); SWAP(13, 21); SWAP(14, 22); SWAP(15, 23);
        SWAP(0, 4); SWAP(1, 5); SWAP(2, 6); SWAP(3, 7); SWAP(24, 28); SWAP(25, 29); SWAP(26, 30); SWAP(27, 31);
        SWAP(8, 12); SWAP(9, 13); SWAP(10, 14); SWAP(11, 15); SWAP(16, 20); SWAP(17, 21); SWAP(18, 22); SWAP(19, 23);
        SWAP(0, 2); SWAP(1, 3); SWAP(28, 30); SWAP(29, 31);
        SWAP(4, 16); SWAP(5, 17); SWAP(6, 18); SWAP(7, 19); SWAP(12, 24); SWAP(13, 25); SWAP(14, 26); SWAP(15, 27);
        SWAP(0, 1); SWAP(30, 31);
        SWAP(4, 8); SWAP(5, 9); SWAP(6, 10); SWAP(7, 11); SWAP(12, 16); SWAP(13, 17); SWAP(14, 18); SWAP(15, 19);
        SWAP(20, 24); SWAP(21, 25); SWAP(22, 26); SWAP(23, 27);
        SWAP(4, 6); SWAP(5, 7); SWAP(8, 10); SWAP(9, 11); SWAP(12, 14); SWAP(13, 15); SWAP(16, 18); SWAP(17, 19);
        SWAP(20, 22); SWAP(21, 23); SWAP(24, 26); SWAP(25, 27);
        SWAP(2, 16); SWAP(3, 17); SWAP(6, 20); SWAP(7, 21); SWAP(10, 24); SWAP(11, 25); SWAP(14, 28); SWAP(15, 29);
        SWAP(2, 8); SWAP(3, 9); SWAP(6, 12); SWAP(7, 13); SWAP(10, 16); SWAP(11, 17); SWAP(14, 20); SWAP(15, 21);
        SWAP(18, 24); SWAP(19, 25); SWAP(22, 28); SWAP(23, 29);
        SWAP(2, 4); SWAP(3, 5); SWAP(6, 8); SWAP(7, 9); SWAP(10, 12); SWAP(11, 13); SWAP(14, 16); SWAP(15, 17);
        SWAP(18, 20); SWAP(19, 21); SWAP(22, 24); SWAP(23, 25); SWAP(26, 28); SWAP(27, 29);
        SWAP(2, 3); SWAP(4, 5); SWAP(6, 7); SWAP(8, 9); SWAP(10, 11); SWAP(12, 13); SWAP(14, 15); SWAP(16, 17);
        SWAP(18, 19); SWAP(20, 21); SWAP(22, 23); SWAP(24, 25); SWAP(26, 27); SWAP(28, 29);
        SWAP(1, 16); SWAP(3, 18); SWAP(5, 20); SWAP(7, 22); SWAP(9, 24); SWAP(11, 26); SWAP(13, 28); SWAP(15, 30);
        SWAP(1, 8); SWAP(3, 10); SWAP(5, 12); SWAP(7, 14); SWAP(9, 16); SWAP(11, 18); SWAP(13, 20); SWAP(15, 22);
        SWAP(17, 24); SWAP(19, 26); SWAP(21, 28); SWAP(23, 30);
        SWAP(1, 4); SWAP(3, 6); SWAP(5, 8); SWAP(7, 10); SWAP(9, 12); SWAP(11, 14); SWAP(13, 16); SWAP(15, 18);
        SWAP(17, 20); SWAP(19, 22); SWAP(21, 24); SWAP(23, 26); SWAP(25, 28); SWAP(27, 30);
        SWAP(1, 2); SWAP(3, 4); SWAP(5, 6); SWAP(7, 8); SWAP(9, 10); SWAP(11, 12); SWAP(13, 14); SWAP(15, 16);
        SWAP(17, 18); SWAP(19, 20); SWAP(21, 22); SWAP(23, 24); SWAP(25, 26); SWAP(27, 28); SWAP(29, 30);
        return 0;
    }

    default:
        return 1;
    }

#undef SWAP
}

/**
 * Quick sort an array of uint8_t, optimised for arrays of channels
 *
 * @param Array    An array of uint8_t
 * @param NrToSort The number of values we want to sort
 *
 * @return 0 if error
 */
int U8QuickSort(uint8_t *Array, const int NrToSort)
{
    int i;
    int SupportedDiscreteSorts[] = {1, 2, 4, 6, 8, 32}; // Keep in increasing order
    uint8_t LocalArray[32];

    SE_ASSERT(Array != NULL);

    if (2 > NrToSort) //No need to do anything
    {
        return 0;
    }

    //Look for SuportedDiscreteSort greater or equal to NrToSort
    for (i = 0; (i < lengthof(SupportedDiscreteSorts)) && (SupportedDiscreteSorts[i] < NrToSort); i++)
    {
    }

    // Here, if i reached end of array or ( SupportedDiscreteSorts[i] < NrToSort) => can't proceed
    if ((lengthof(SupportedDiscreteSorts) <= i) || (SupportedDiscreteSorts[i] < NrToSort))
    {
        SE_ERROR("U8QuickSort for %d is not supported\n", NrToSort);
        return 1;
    }

    // Here Either (SupportedDiscreteSorts[i] == NrToSort) or ((SupportedDiscreteSorts[i] > NrToSort)
    if (NrToSort == SupportedDiscreteSorts[i])
    {
        if (0 != U8QuickSortN(Array, SupportedDiscreteSorts[i]))
        {
            return 1;
        }
    }
    else
    {
        //Create a local copy at the correct size
        memcpy(LocalArray, Array, NrToSort);
        memset(&LocalArray[NrToSort], 0xFF, SupportedDiscreteSorts[i] - NrToSort);

        if (0 != U8QuickSortN(LocalArray, SupportedDiscreteSorts[i]))
        {
            return 1;
        }

        memcpy(Array, LocalArray, NrToSort);
    }

    return 0;
}

/**
 *
 * @param Placement The list of channels we are evaluating
 * @param Analysis  Results of the parsing
 *
 * @return int not 0 if error encountered
 */
static int GetChannelCounts(StmSeAudioChannelPlacementAnalysis_t *Analysis,
                            const stm_se_audio_channel_placement_t *Placement)
{
    SE_ASSERT((Analysis != NULL) && (Placement != NULL));

    memset(Analysis, 0, sizeof(StmSeAudioChannelPlacementAnalysis_t));

    // count all terminating stuffing channels within buffer width, till active channel
    int i = Placement->channel_count - 1;

    for (; (0 <= i) && (STM_SE_AUDIO_CHAN_STUFFING == Placement->chan[i]); i--)
    {
        Analysis->EndStuffingCount++;
    }

    // count all other channels
    for (; 0 <= i; i--)
    {
        switch (Placement->chan[i])
        {
        case  STM_SE_AUDIO_CHAN_STUFFING:
            Analysis->InterleavedStuffingCount++;
            break;

        case  STM_SE_AUDIO_CHAN_UNKNOWN:
            Analysis->UnnamedChannelCount++;
            break;

        default:
            if ((STM_SE_AUDIO_CHAN_LAST_NAMED < Placement->chan[i]))
            {
                SE_ERROR("[%d] = %s\n", i, StmSeAudioChannelIdGetName((stm_se_audio_channel_id_t)Placement->chan[i]));
                Analysis->ParsingErrors++;
            }
            else
            {
                SE_EXTRAVERB2(group_mixer, group_encoder_audio_preproc, "Placement->chan[%d]:%s\n",
                              i, StmSeAudioChannelIdGetName((stm_se_audio_channel_id_t)Placement->chan[i]));
                Analysis->NamedChannelCount++;
            }
        }
    }

    Analysis->ActiveChannelCount    = Analysis->NamedChannelCount + Analysis->UnnamedChannelCount;
    Analysis->TotalNumberOfChannels = Placement->channel_count;

    return Analysis->ParsingErrors;
}

/**
 *
 *
 * @brief Looks for a physcial AudioMode mapping
 * A physical mapping is found if we can match the named/stuffin/unknown channels
 *
 * @param AudioMode
 *                  The AudioMode found (ACC_MODE_ID if no match found)
 * @param Placement The list of channels we are trying to match
 * @param NamedChannelCount
 *                  Numnber of named channels in the list of channels
 * @param UnnamedChannelsCount
 *                  Number of unnamed channels in the list
 * @param InterleavedStuffingCount
 *                  The number of interleaved channels that have at least one
 *                  active audio channel after them
 *
 * @return 0 if no error occurred
 */
static int LookForPhysicalAcModeMapping(enum eAccAcMode *AudioMode,
                                        const stm_se_audio_channel_placement_t *Placement,
                                        const int32_t NamedChannelCount,
                                        const int32_t UnnamedChannelCount,
                                        const int32_t InterleavedStuffingCount)
{
    int i;

    SE_ASSERT(AudioMode != NULL);

    *AudioMode = ACC_MODE_ID;

    if (0 == UnnamedChannelCount)
    {
        // Without unamed channels we can do a memcmp to check for match
        // Loop on the LUT entries
        for (i = 0; ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelIds[i].AudioMode; i++)
        {
            const int32_t NamedChannelCountRef = StmSeAudioLUTAcModeVsChannelIds[i].NamedChannelCount;
            const int32_t InterleavedStuffingCountRef = StmSeAudioLUTAcModeVsChannelIds[i].InterleavedStuffingCount;

            // If the number of named and stuffing channels matches, then compare
            if ((NamedChannelCountRef == NamedChannelCount) && (InterleavedStuffingCountRef == InterleavedStuffingCount))
            {
                // We need to match named and interleaved stuffing channels
                const int number_of_channels_to_match = NamedChannelCountRef + InterleavedStuffingCountRef;

                SE_EXTRAVERB2(group_mixer, group_encoder_audio_preproc, "memcmp %s %d\n",
                              StmSeAudioAcModeGetName(StmSeAudioLUTAcModeVsChannelIds[i].AudioMode),
                              number_of_channels_to_match);

                if (0 == memcmp(StmSeAudioLUTAcModeVsChannelIds[i].PhysicalChannels, Placement->chan, number_of_channels_to_match))
                {
                    SE_VERBOSE2(group_mixer, group_encoder_audio_preproc, "matching %s\n",
                                StmSeAudioAcModeGetName(StmSeAudioLUTAcModeVsChannelIds[i].AudioMode));
                    *AudioMode = StmSeAudioLUTAcModeVsChannelIds[i].AudioMode;
                    return 0;
                }
            }
        }
    }
    else
    {
        // Loop on the LUT entries
        for (i = 0; ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelIds[i].AudioMode; i++)
        {
            const int32_t NamedChannelCountRef = StmSeAudioLUTAcModeVsChannelIds[i].NamedChannelCount;
            const int32_t InterleavedStuffingCountRef = StmSeAudioLUTAcModeVsChannelIds[i].InterleavedStuffingCount;

            // If the number of active channels matches and number of stuffing channels matches
            if ((NamedChannelCountRef == (NamedChannelCount + UnnamedChannelCount))
                && (InterleavedStuffingCountRef == InterleavedStuffingCount))
            {
                // We need to match both named and interleaved stuffing channels from ref to DUT
                const int32_t number_of_channels_to_match = NamedChannelCountRef + InterleavedStuffingCountRef;
                int32_t ch_idx;

                // Compare one by one the named/stuffing channel
                for (ch_idx = 0; ch_idx < number_of_channels_to_match; ch_idx++)
                {
                    const stm_se_audio_channel_id_t ChanIdRef = (stm_se_audio_channel_id_t)StmSeAudioLUTAcModeVsChannelIds[i].PhysicalChannels[ch_idx];
                    const stm_se_audio_channel_id_t ChanIdDut = (stm_se_audio_channel_id_t)Placement->chan[ch_idx];

                    // unknown channels are a joker against named channels (but not against stuffing)
                    // Test for not match
                    if (!(((STM_SE_AUDIO_CHAN_UNKNOWN == ChanIdDut) && (STM_SE_AUDIO_CHAN_STUFFING != ChanIdRef))
                          || (ChanIdRef == ChanIdDut)))
                    {
                        // Any channel match fails => fails the whole LUT entry
                        break; //stop loop on channels, does not post-increment ch_idx  => back to loop on LUT entries
                    }
                }

                // If we have matched all channels, ac_mode is matched
                if (ch_idx == number_of_channels_to_match)
                {
                    SE_VERBOSE2(group_mixer, group_encoder_audio_preproc, "matching-2 %s\n",
                                StmSeAudioAcModeGetName(StmSeAudioLUTAcModeVsChannelIds[i].AudioMode));
                    *AudioMode = StmSeAudioLUTAcModeVsChannelIds[i].AudioMode;
                    return 0;
                }
            }
        }
    }

    return 0;
}

/**
 * @brief Looks for a soft AudioMode mapping
 *
 * A Soft mapping is found if we can match the named/unknown channels in any order.
 * For this we need to compare sorted arrrays
 * @param CompactChannelPlacement
 *               the already sorted list of channels we are trying to match
 * @param AudioMode
 *               the AudioMode found (ACC_MODE_ID if no match found)
 * @param NamedChannelCount
 *               Numnber of named channels in the list of channels
 * @param UnnamedChannelCount
 *               Number of unnamed channels in the list
 */
static int LookForSoftAcModeMapping(enum eAccAcMode  *AudioMode,
                                    const stm_se_audio_channel_placement_t *CompactChannelPlacement,
                                    const int32_t NamedChannelCount,
                                    const int32_t UnnamedChannelCount)
{
    int i;
    uint8_t CompactRefChannelArray[STMSEAUDIOCONV_MAX_ACMODE_CHANNELS];

    SE_ASSERT((AudioMode != NULL) && (CompactChannelPlacement != NULL));

    *AudioMode = ACC_MODE_ID;

    if (0 == UnnamedChannelCount)
    {
        //Without unknown channels we can use memcmp to match
        // Loop on the LUT entries
        for (i = 0; ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelIds[i].AudioMode; i++)
        {
            const int32_t NamedChannelCountRef = StmSeAudioLUTAcModeVsChannelIds[i].NamedChannelCount;
            const int32_t InterleavedStuffingCountRef = StmSeAudioLUTAcModeVsChannelIds[i].InterleavedStuffingCount;

            // If the number of named channels matches, then copy, order, and compare
            if (NamedChannelCountRef == NamedChannelCount)
            {
                const int32_t number_of_channels_to_match = NamedChannelCountRef; //Only match named channels
                // We need to sort the InterleavingStuffing channesl out of the scope
                const int32_t number_of_channels_to_sort  = NamedChannelCountRef + InterleavedStuffingCountRef;

                //memcpy: Safety check against errors in the LUT
                if (number_of_channels_to_sort > STMSEAUDIOCONV_MAX_ACMODE_CHANNELS)
                {
                    SE_ERROR("[%d] NamedChannelCountRef %d + InterleavedStuffingCountRef %d\n",
                             i, NamedChannelCountRef, InterleavedStuffingCountRef);
                    return 1;
                }

                memcpy(CompactRefChannelArray, StmSeAudioLUTAcModeVsChannelIds[i].PhysicalChannels, number_of_channels_to_sort);

                if (0 != U8QuickSort(CompactRefChannelArray, number_of_channels_to_sort))
                {
                    return 1;
                }

                if (0 == memcmp(CompactRefChannelArray, CompactChannelPlacement->chan, number_of_channels_to_match))
                {
                    SE_VERBOSE2(group_mixer, group_encoder_audio_preproc, "matching %s\n",
                                StmSeAudioAcModeGetName(StmSeAudioLUTAcModeVsChannelIds[i].AudioMode));
                    *AudioMode = StmSeAudioLUTAcModeVsChannelIds[i].AudioMode;
                    return 0;
                }
            }
        }
    }
    else
    {
        // With unknown channels, compare each named channel by hand
        // Loop on the LUT entries
        for (i = 0; ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelIds[i].AudioMode; i++)
        {
            const int32_t NamedChannelCountRef = StmSeAudioLUTAcModeVsChannelIds[i].NamedChannelCount;
            const int32_t InterleavedStuffingCountRef = StmSeAudioLUTAcModeVsChannelIds[i].InterleavedStuffingCount;

            // If the number of active channels matches, then copy, order, and compare
            if (NamedChannelCountRef == NamedChannelCount + UnnamedChannelCount)
            {
                const int32_t number_of_channels_to_match = NamedChannelCount; //Only match named channels from DUT
                const int32_t number_of_channels_to_sort  = NamedChannelCountRef + InterleavedStuffingCountRef; //Need to move STUFFING out of scope
                int ch_idx; // index for channels in CompactChannelPlacement
                memcpy(CompactRefChannelArray, StmSeAudioLUTAcModeVsChannelIds[i].PhysicalChannels, number_of_channels_to_sort);

                if (0 != U8QuickSort(CompactRefChannelArray, number_of_channels_to_sort))
                {
                    return 1;
                }

                // Try to match each named channel from DUT with a named channel in REF
                // Extra named channels in REF are automcatically "mapped" to unnamed channels in DUT
                // Loop on each NamedChannel in DUT
                for (ch_idx = 0; ch_idx < number_of_channels_to_match; ch_idx++)
                {
                    int ch_idx_ref; //channel index in reference LUT entry

                    // Loop on eached named channel in Ref
                    // Because sorted arrays, we start search at ch_idx_ref and stop when Chan[ch_idx_ref] > Chan[ch_idx]
                    for (ch_idx_ref = ch_idx; (ch_idx_ref < NamedChannelCountRef)
                         && ((stm_se_audio_channel_id_t)CompactRefChannelArray[ch_idx_ref] < CompactChannelPlacement->chan[ch_idx]); ch_idx_ref++)
                    {
                    }

                    // here, either (CompactRefChannelArray[ch_idx_ref] == Placement->chan[ch_idx]): OK
                    //  or (CompactRefChannelArray[ch_idx_ref] > Placement->chan[ch_idx]): FAIL
                    // test for not match
                    if (CompactRefChannelArray[ch_idx_ref] != CompactChannelPlacement->chan[ch_idx])
                    {
                        //Any named channel from DUT not matched means fail on this ac_mode, go to next LUT entry
                        break; //stop loop on ch_idx channels, ch_idx not post-incremented => back to loop on LUT entries
                    }
                }

                //If we went through all channels in CompactChannelPlacement without break => success
                if (number_of_channels_to_match == ch_idx)
                {
                    SE_VERBOSE2(group_mixer, group_encoder_audio_preproc, "matching-2 %s\n",
                                StmSeAudioAcModeGetName(StmSeAudioLUTAcModeVsChannelIds[i].AudioMode));
                    *AudioMode = StmSeAudioLUTAcModeVsChannelIds[i].AudioMode;
                    return 0;
                }
            }
        }
    }

    return 0;
}

/* ************************
 *
 *
 * Exportable functions
 *
 *
 **************************
  */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief analyses the channel_placement for Audio Mme compatibility.
 * @param Placement
 *                 The source to be analysed
 * @param SortedPlacement
 *                 To store the ordered channel placement
 * @param Analysis The information parsed from the buffer
 * @param AudioMode
 *                 The functional acmode: will be a ACC_MODE_ID for any invalid
 *                 ac_mode
 * @param AudioModeIsPhysical
 *                 true if the channel postitions correspond to ac_mode
 *
 * @return int 0 if no error
 */
int StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(enum eAccAcMode *AudioMode,
                                                      bool *AudioModeIsPhysical,
                                                      stm_se_audio_channel_placement_t *SortedPlacement,
                                                      StmSeAudioChannelPlacementAnalysis_t *Analysis,
                                                      const stm_se_audio_channel_placement_t *Placement)
{
    int i;

    if ((NULL == AudioMode)
        || (NULL == AudioModeIsPhysical)
        || (NULL == Analysis)
        || (NULL == SortedPlacement)
        || (NULL == Placement)
       )
    {
        SE_ERROR("NULL Pointer(s)\n");
        return 1;
    }

    /* Only Check: update implementation if STMSEAUDIOCONV_MAX_ACMODE_CHANNELS > 8 */
    SE_ASSERT(STMSEAUDIOCONV_MAX_ACMODE_CHANNELS <= 8);

    *AudioMode = ACC_MODE_ID;

    if (32 < STM_SE_MAX_NUMBER_OF_AUDIO_CHAN)
    {
        SE_ERROR("We have only implemented soft acmod searching with 32 channels\n");
        return 1;
    }

    SE_VERBOSE(group_encoder_audio_preproc, "\n");

    if (0 != GetChannelCounts(Analysis, Placement))
    {
        SE_ERROR("GetChannelCounts Errors\n");
        return 1;
    }

    // Create an ordered version of the channel placement: easier to check duplicates
    // Should be the most common case
    memcpy(SortedPlacement, Placement, sizeof(stm_se_audio_channel_placement_t));

    if (0 != U8QuickSort(SortedPlacement->chan, SortedPlacement->channel_count))
    {
        SE_ERROR("U8QuickSort Error\n");
        return 1;
    }

    // Check for duplicates
    for (i = 0; (i < STM_SE_MAX_NUMBER_OF_AUDIO_CHAN) && (i < Analysis->NamedChannelCount); i++)
    {
        stm_se_audio_channel_id_t ChanId = (stm_se_audio_channel_id_t)(SortedPlacement->chan[i]);

        if ((0 != i) && (ChanId == (stm_se_audio_channel_id_t)(SortedPlacement->chan[i - 1])))
        {
            SE_ERROR("Duplicate Chan[%d] = Chan[%d] = %s\n", i - 1, i, StmSeAudioChannelIdGetName(ChanId));
            return 1;
        }
    }

    // We now try to find a match by increasing order of computing complexity
    // and decreasing order of probability.

    // First look for a physical mapping (matches channel names and positions)
    if (0 != LookForPhysicalAcModeMapping(AudioMode,
                                          Placement,
                                          Analysis->NamedChannelCount,
                                          Analysis->UnnamedChannelCount,
                                          Analysis->InterleavedStuffingCount))
    {
        return 1;
    }

    if (ACC_MODE_ID != *AudioMode)
    {
        *AudioModeIsPhysical = true;
    }
    else
    {
        *AudioModeIsPhysical = false;

        //Look for soft mapping (matches channel names, not position)
        if (0 != LookForSoftAcModeMapping(AudioMode, SortedPlacement, Analysis->NamedChannelCount, Analysis->UnnamedChannelCount))
        {
            return 1;
        }
    }

    if (ACC_MODE_ID == *AudioMode)
    {
        SE_VERBOSE2(group_mixer, group_encoder_audio_preproc, "Could not find a matching AC_MODE\n");
    }

    return 0;
}

/**
 * @brief AccMode to ChannelPlacement conversion
 *
 * @param Placement The matched Placement, Placement->channel_count wil be 0 if
 *              no match is found
 * @param Analysis The information parsed from the buffer
 * @param AudioMode
 * @param BufferWidth Width requested for the ChannelPlacement
 *
 * @return int 0 if no error
 */
int StmSeAudioGetChannelPlacementAndAnalysisFromAcmode(stm_se_audio_channel_placement_t *Placement,
                                                       StmSeAudioChannelPlacementAnalysis_t *Analysis,
                                                       const enum eAccAcMode  AudioMode,
                                                       const int32_t BufferWidth)
{
    int i, c;

    if ((NULL == Analysis) || (NULL == Placement))
    {
        SE_ERROR("NULL Pointer(s)\n");
        return 1;
    }

    if (STM_SE_MAX_NUMBER_OF_AUDIO_CHAN < BufferWidth)
    {
        SE_ERROR("Requested channel nr too large for requirements %d > %d\n", BufferWidth , STM_SE_MAX_NUMBER_OF_AUDIO_CHAN);
        return 1;
    }

    SE_VERBOSE(group_encoder_audio_preproc, "\n");

    Placement->channel_count = 0;

    // Look for the AC_MODE in LUT
    for (i = 0; ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelIds[i].AudioMode; i++)
    {
        if (AudioMode == StmSeAudioLUTAcModeVsChannelIds[i].AudioMode)
        {
            // Minimum number of channels to represent this ac_mode physically
            int min_nr_chan = StmSeAudioLUTAcModeVsChannelIds[i].NamedChannelCount +
                              StmSeAudioLUTAcModeVsChannelIds[i].InterleavedStuffingCount;
            int bytes_to_copy;

            if (BufferWidth < min_nr_chan)
            {
                SE_ERROR("Requested channel nr too small for requirements %d < %d\n", BufferWidth , min_nr_chan);
                return 1;
            }

            bytes_to_copy = min(BufferWidth, lengthof(StmSeAudioLUTAcModeVsChannelIds[i].PhysicalChannels));
            // Copy
            memcpy(Placement->chan, StmSeAudioLUTAcModeVsChannelIds[i].PhysicalChannels, bytes_to_copy);

            // Complete up to requested size with STUFFING
            for (c = bytes_to_copy; c < BufferWidth; c++)
            {
                Placement->chan[c] = STM_SE_AUDIO_CHAN_STUFFING;
            }

            Placement->channel_count = BufferWidth;

            if (0 != GetChannelCounts(Analysis, Placement))
            {
                return 1;
            }

            return 0;
        }
    }

    return 0;
}

/**
 *
 *
 * @brief Lookup the most appropriate ACC_MODE for current channel placement.
 *
 * @note This code is adapted from current code in different components
 * @param Assignment The channels for which we want to get the AC_MODE
 * @param AudioMode     The functional acmode: will be a ACC_MODE_ID for any invalid
 *                   ac_mode
 *
 * @return 0 if no error
 */
int StmSeAudioGetAcModeFromChannelAssignment(enum eAccAcMode *AudioMode,
                                             const stm_se_audio_channel_assignment_t *Assignment)
{
    stm_se_audio_channel_assignment_t LocalAssignment;
    uint32_t i;

    if ((NULL == AudioMode) || (NULL == Assignment))
    {
        SE_ERROR("NULL Pointer(s)\n");
        return 1;
    }

    SE_EXTRAVERB(group_mixer, "\n");

    *AudioMode = ACC_MODE_ID;
    memcpy(&LocalAssignment, Assignment, sizeof(stm_se_audio_channel_assignment_t));

    if (LocalAssignment.pair4 != STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED)
    {
        //There is no AC_MODE with 10 channels, no point looking for a match
        return 0;
    }

    // we want to use memcmp() to compare the channel assignments so
    // we must explicitly zero the bits we don't care about
    LocalAssignment.reserved0 = 0;
    LocalAssignment.malleable = 0;

    for (i = 0; ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelPairs[i].AudioMode; i++)
    {
        if (0 == memcmp(&StmSeAudioLUTAcModeVsChannelPairs[i].ChannelAssignment, &LocalAssignment,
                        sizeof(stm_se_audio_channel_assignment_t)))
        {
            *AudioMode = StmSeAudioLUTAcModeVsChannelPairs[i].AudioMode;
            return 0;
        }
    }

    return 0;
}

/**
 * @brief Returns the channel assignment corresponding to a particular AcMode
 *
 * @param Assignment Will be all "not connected" if cannot be matched
 * @param AudioMode AcMode we are trying to match
 *
 * @return int 0 if no error
 */
int StmSeAudioGetChannelAssignmentFromAcMode(stm_se_audio_channel_assignment_t *Assignment,
                                             const enum eAccAcMode AudioMode)
{
    int i;

    if (NULL == Assignment)
    {
        SE_ERROR("NULL Pointer(s)\n");
        return 1;
    }

    SE_EXTRAVERB(group_mixer, "\n");

    Assignment->reserved0 = 0;
    Assignment->malleable = 0;
    Assignment->pair0 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    Assignment->pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    Assignment->pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    Assignment->pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    Assignment->pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

    for (i = 0;
         (ACC_MODE_ID != StmSeAudioLUTAcModeVsChannelPairs[i].AudioMode)
         && !(StmSeAudioLUTAcModeVsChannelPairs[i].AudioMode == AudioMode);
         i++)
    {
    }

    if (StmSeAudioLUTAcModeVsChannelPairs[i].AudioMode == AudioMode)
    {
        *Assignment = StmSeAudioLUTAcModeVsChannelPairs[i].ChannelAssignment;
    }

    return 0;
}

/**
 * @brief AcMode to CEA861 conversion
 *
 *
 * @return CEA861 code corresponding to given AcMode
           refer to "StreamHeader" if no AcMode match found.
 */
void StmSeAudioAcModeToHdmi(const enum eAccAcMode AudioMode, unsigned char *spkr_map, unsigned char   *chan_cnt)
{
    int i;

    for (i = 0;
         (ACC_MODE_ID != StmSeAudioLUTAcModeName[i].AudioMode)
         && !(AudioMode == StmSeAudioLUTAcModeName[i].AudioMode);
         i++)
    {
    }

    if (AudioMode == StmSeAudioLUTAcModeName[i].AudioMode)
    {
        *spkr_map = StmSeAudioLUTAcModeName[i].HdmiSpeakerMapping;
        *chan_cnt = StmSeAudioLUTAcModeName[i].HdmiChannelCount;
    }
    else
    {
        *spkr_map = HDMI_SPKR_MAPPING_REFERRING_TO_STREAM_HEADER;
        *chan_cnt = HDMI_CHANNEL_CNT_REFERRING_TO_STREAM_HEADER;
    }
}

/**
 * @brief AcMode to String conversion
 *
 *
 * @return const char* "" if AudioMode name is not found
 */
const char *StmSeAudioAcModeGetName(const enum eAccAcMode AudioMode)
{
    int i;

    for (i = 0;
         (ACC_MODE_ID != StmSeAudioLUTAcModeName[i].AudioMode)
         && !(AudioMode == StmSeAudioLUTAcModeName[i].AudioMode);
         i++)
    {
    }

    if (AudioMode == StmSeAudioLUTAcModeName[i].AudioMode)
    {
        return StmSeAudioLUTAcModeName[i].Text;
    }

    //Failure
    return ("");
}


/**
 * @brief ChannelId to string convertion
 *
 *
 * @param chan
 *
 * @return const char* "" if named cannot be found
 */
const char *StmSeAudioChannelIdGetName(const stm_se_audio_channel_id_t chan)
{
    int i;

    for (i = 0;
         (STM_SE_AUDIO_CHAN_RESERVED != StmSeAudioLUTChannelIdName[i].ChannelId)
         && !(chan == StmSeAudioLUTChannelIdName[i].ChannelId);
         i++)
    {
    }

    if (chan == StmSeAudioLUTChannelIdName[i].ChannelId)
    {
        return StmSeAudioLUTChannelIdName[i].Text;
    }

    //Failure
    return "";
}

/**
 * @brief ChannelPair to string convertion
 *
 *
 * @param pair
 *
 * @return const char* "" if named cannot be found
 */
const char *StmSeAudioChannelPairGetName(const enum stm_se_audio_channel_pair Pair, const int PairId)
{
    int i;

    // Specific case for the 0 == (int)channel_pair
    if (STM_SE_AUDIO_CHANNEL_PAIR_DEFAULT == Pair)
    {
        return StmSeAudioLUTChannelPairName[PairId + 1].Text;
    }

    for (i = 0;
         (STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED != StmSeAudioLUTChannelPairName[i].Pair)
         && !(Pair == StmSeAudioLUTChannelPairName[i].Pair);
         i++)
    {
    }

    if (Pair == StmSeAudioLUTChannelPairName[i].Pair)
    {
        return StmSeAudioLUTChannelPairName[i].Text;
    }

    //Failure
    return "";
}

static const struct
{
    stm_se_audio_pcm_format_t LpcmFormat;
    enum eAccLpcmWs           LpcmWsCode;
} StmSeAudioLUTLpcmFormatVsLpcmWsCode[] =
{
    {STM_SE_AUDIO_PCM_FMT_S32LE , ACC_LPCM_WS32        },
    {STM_SE_AUDIO_PCM_FMT_S32BE , ACC_LPCM_WS32be      },
    {STM_SE_AUDIO_PCM_FMT_S24LE , ACC_LPCM_WS24        },
    {STM_SE_AUDIO_PCM_FMT_S16LE , ACC_LPCM_WS16le      },
    {STM_SE_AUDIO_PCM_FMT_S16BE , ACC_LPCM_WS16        },
    {STM_SE_AUDIO_PCM_FMT_U16BE , ACC_LPCM_WS16u       },
    {STM_SE_AUDIO_PCM_FMT_U16LE , ACC_LPCM_WS16ule     },
    {STM_SE_AUDIO_PCM_FMT_U8    , ACC_LPCM_WS8u        },
    {STM_SE_AUDIO_PCM_FMT_S8    , ACC_LPCM_WS8s        },
    {STM_SE_AUDIO_PCM_FMT_ALAW_8, ACC_LPCM_WS8A        },
    {STM_SE_AUDIO_PCM_FMT_ULAW_8, ACC_LPCM_WS8Mu       },

    /* Missing matches to be added once they are available, no point
       increasing footprint till then */

    //Termination

    {STM_SE_AUDIO_LPCM_FORMAT_RESERVED, ACC_LPCM_WS_UNDEFINED}
};

int StmSeAudioGetLpcmWsFromLPcmFormat(enum eAccLpcmWs *LpcmWs, const stm_se_audio_pcm_format_t LpcmFormat)
{
    int i;

    if (NULL == LpcmWs)
    {
        return 1;
    }

    *LpcmWs = ACC_LPCM_WS_UNDEFINED;

    /* Loop on LUT entries */
    for (i = 0; StmSeAudioLUTLpcmFormatVsLpcmWsCode[i].LpcmFormat != STM_SE_AUDIO_LPCM_FORMAT_RESERVED; i++)
    {
        if (StmSeAudioLUTLpcmFormatVsLpcmWsCode[i].LpcmFormat == LpcmFormat)
        {
            *LpcmWs = StmSeAudioLUTLpcmFormatVsLpcmWsCode[i].LpcmWsCode;
            return 0;
        }
    }

    if (StmSeAudioLUTLpcmFormatVsLpcmWsCode[i].LpcmFormat == LpcmFormat)
    {
        *LpcmWs = StmSeAudioLUTLpcmFormatVsLpcmWsCode[i].LpcmWsCode;
        return 0;
    }

    return 1;
}

static const struct
{
    stm_se_audio_pcm_format_t LpcmFormat;
    enum eAccWordSizeCode     WordSizeCode;
} StmSeAudioLUTWordSizeCodeVsLpcmWsCode[] =
{
    {STM_SE_AUDIO_PCM_FMT_S32LE, ACC_WS32},
    {STM_SE_AUDIO_PCM_FMT_S16LE, ACC_WS16},
    {STM_SE_AUDIO_PCM_FMT_S8   , ACC_WS8 },

    // Termination, Warning no "undef_value"
    {STM_SE_AUDIO_LPCM_FORMAT_RESERVED, ACC_WS8}
};

int StmSeAudioGetWordsizeCodeFromLPcmFormat(enum eAccWordSizeCode *WordSizeCode, const stm_se_audio_pcm_format_t LpcmFormat)
{
    int i;

    if (NULL == WordSizeCode)
    {
        return 1;
    }

    /* Loop on LUT entries */
    for (i = 0; StmSeAudioLUTWordSizeCodeVsLpcmWsCode[i].LpcmFormat != STM_SE_AUDIO_LPCM_FORMAT_RESERVED; i++)
    {
        if (StmSeAudioLUTWordSizeCodeVsLpcmWsCode[i].LpcmFormat == LpcmFormat)
        {
            *WordSizeCode = StmSeAudioLUTWordSizeCodeVsLpcmWsCode[i].WordSizeCode;
            return 0;
        }
    }

    return 1;
}

int StmSeAudioGetNrBytesFromLpcmFormat(const stm_se_audio_pcm_format_t LpcmFormat)
{
    int BytesPerSample = 0;

    switch (LpcmFormat)
    {
    case STM_SE_AUDIO_PCM_FMT_S32LE  : BytesPerSample = 4; break;
    case STM_SE_AUDIO_PCM_FMT_S32BE  : BytesPerSample = 4; break;
    case STM_SE_AUDIO_PCM_FMT_S24LE  : BytesPerSample = 3; break;
    case STM_SE_AUDIO_PCM_FMT_S24BE  : BytesPerSample = 3; break;
    case STM_SE_AUDIO_PCM_FMT_S16LE  : BytesPerSample = 2; break;
    case STM_SE_AUDIO_PCM_FMT_S16BE  : BytesPerSample = 2; break;
    case STM_SE_AUDIO_PCM_FMT_U16BE  : BytesPerSample = 2; break;
    case STM_SE_AUDIO_PCM_FMT_U16LE  : BytesPerSample = 2; break;
    case STM_SE_AUDIO_PCM_FMT_U8     : BytesPerSample = 1; break;
    case STM_SE_AUDIO_PCM_FMT_S8     : BytesPerSample = 1; break;
    case STM_SE_AUDIO_PCM_FMT_ALAW_8 : BytesPerSample = 1; break;
    case STM_SE_AUDIO_PCM_FMT_ULAW_8 : BytesPerSample = 1; break;
    default: BytesPerSample = 0; break;
    }

    return BytesPerSample;
}

TimeStamp_c StmSeAudioTimeStampFromAccPts(const uMME_BufferFlags  *PTSflag,
                                          const uint64_t *const PTS)
{
    TimeStamp_c Timestamp;
    uint64_t pts_time = INVALID_TIME;

    if ((NULL == PTSflag) || (NULL == PTS))
    {
        return Timestamp;
    }

    if (PTSflag->Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)
    {
        pts_time = *PTS;
    }
    else
    {
        pts_time = UNSPECIFIED_TIME;
    }

    Timestamp.PtsValue(pts_time);
    return Timestamp;
}

void StmSeAudioAccPtsFromTimeStamp(uMME_BufferFlags *PTSflag, uint64_t *PTS, TimeStamp_c TimeStamp)
{
    if ((NULL == PTSflag) || (NULL == PTS))
    {
        return;
    }

    if (TimeStamp.IsValid())
    {
        *PTS = TimeStamp.PtsValue();
        PTSflag->Bits.PtsTimeFormat = PtsTimeFormat90k;
        PTSflag->Bits.PTS_DTS_FLAG |= ACC_PTS_PRESENT;
    }
    else
    {
        *PTS = 0;
        PTSflag->Bits.PTS_DTS_FLAG &= ~ACC_PTS_PRESENT;
    }
}

TimeStamp_c StmSeAudioGetApplyOffsetFrom33BitPtsTimeStamps(TimeStamp_c &Offset,
                                                           bool &IsDelay,
                                                           TimeStamp_c InputTimeStamp,
                                                           TimeStamp_c TransformInputTimeStamp,
                                                           TimeStamp_c TransformOutputTimeStamp)
{
    TimeStamp_c OutputTimeStamp;
    OutputTimeStamp = OutputTimeStamp.TimeFormat(InputTimeStamp.TimeFormat());
    Offset = OutputTimeStamp;

    //Handle Invalid cases
    if ((!TransformInputTimeStamp.IsValid())
        || (!TransformOutputTimeStamp.IsValid()))
    {
        return (OutputTimeStamp); //UNSPECIFIED TIME
    }

    Offset = StmSeAudioGetOffsetFrom33BitPtsTimeStamps(IsDelay, TransformInputTimeStamp, TransformOutputTimeStamp);
    //Convert all to InputTimeStamp.TimeFormat
    Offset = Offset.TimeFormat(InputTimeStamp.TimeFormat());

    SE_EXTRAVERB(group_encoder_audio_preproc, "IPTS %llu (ms), OPTS %lld (ms) => Offset %s %lld (ms)\n",
                 TransformInputTimeStamp.mSecValue(), TransformOutputTimeStamp.mSecValue(),
                 (IsDelay ? "-" : "+"), Offset.mSecValue());

    //Add/Remove Offset from InputTimeStamp to get OutputTimeStamp
    if (!IsDelay)
    {
        OutputTimeStamp = InputTimeStamp + Offset;
    }
    else
    {
        OutputTimeStamp = InputTimeStamp - Offset;
    }

    return OutputTimeStamp;
}

TimeStamp_c StmSeAudioGetOffsetFrom33BitPtsTimeStamps(bool &IsDelay,
                                                      TimeStamp_c TransformInputTimeStamp,
                                                      TimeStamp_c TransformOutputTimeStamp)
{
    TimeStamp_c Offset;

    //Handle Invalid cases
    if ((!TransformInputTimeStamp.IsValid())
        || (!TransformOutputTimeStamp.IsValid()))
    {
        return (Offset); //UNSPECIFIED TIME
    }

    //The only known relationship between input and output PTS is that they can't be too far appart:
    //Between linear and wraparound possibilities we choose the one with the smallest difference
    // Create a time stamps with Max Pts on 33 bit
    TimeStamp_c MaxPts33bit = (TimeStamp_c(0, TIME_FORMAT_PTS) - TimeStamp_c(1, TIME_FORMAT_PTS)).Pts33();
    //Should not be required but for safety limit transform timestamps to 33bit
    TransformInputTimeStamp = TransformInputTimeStamp.Pts33();
    TransformOutputTimeStamp = TransformOutputTimeStamp.Pts33();

    if (TransformOutputTimeStamp.PtsValue() > TransformInputTimeStamp.PtsValue())
    {
        IsDelay = false;
        Offset = TransformOutputTimeStamp - TransformInputTimeStamp;
        TimeStamp_c OffsetCirc  = (MaxPts33bit - TransformOutputTimeStamp) + TransformInputTimeStamp;

        if (OffsetCirc.PtsValue() < Offset.PtsValue())
        {
            SE_EXTRAVERB(group_encoder_audio_coder, "Wrap: Offset %lld OffsetCirc %lld\n", Offset.mSecValue(), OffsetCirc.mSecValue());
            Offset = OffsetCirc;
            IsDelay = true;
        }
    }
    else
    {
        IsDelay = true;
        Offset = TransformInputTimeStamp - TransformOutputTimeStamp;
        TimeStamp_c OffsetCirc = (MaxPts33bit - TransformInputTimeStamp) + TransformOutputTimeStamp;

        if (OffsetCirc.PtsValue() < Offset.PtsValue())
        {
            SE_EXTRAVERB(group_encoder_audio_coder, "Wrap: Offset %lld OffsetCirc %lld\n", Offset.mSecValue(), OffsetCirc.mSecValue());
            Offset = OffsetCirc;
            IsDelay = false;
        }
    }

    SE_EXTRAVERB(group_encoder_audio_coder, "IPTS %llu (ms), OPTS %lld (ms) => Offset %s %lld (ms)\n",
                 TransformInputTimeStamp.mSecValue(), TransformOutputTimeStamp.mSecValue(),
                 (IsDelay ? "-" : "+"), Offset.mSecValue());
    return Offset;
}

////////////////////////////////////////////////////////////////////////////
///
/// lookup tables used to convert between discrete and integer sampling frequencies.
///
///


/// The StmSeSamplingFrequencyLookupTable table provides the code book between enumerated Sfreq code
/// and actual frequency in Hz in an increasing order , enabling "both way" search.
/// The final zero value is very important. In both directions is prevents reads past the
/// end of the table. When indexed on eAccFsCode it also provides the return value when the
/// lookup fails.

typedef struct
{
    enum eAccFsCode Discrete; //!< enumeration of ISO SamplingFrequency as understood by audio-fw
    uint32_t        Integer;  //!< ISO SamplingFrequency in Hz
    unsigned char   Hdmi;     //!< CEA 861 code for this frequency
} StmSeSamplingFrequency_t;


static const StmSeSamplingFrequency_t StmSeSamplingFrequencyLookupTable[] =
{
    /* Range : 2^4  */ { ACC_FS768k, 768000, (unsigned char) CEA861_0k   }, { ACC_FS705k, 705600, (unsigned char) CEA861_0k   }, { ACC_FS512k, 512000, (unsigned char) CEA861_0k },
    /* Range : 2^3  */ { ACC_FS384k, 384000, (unsigned char) CEA861_0k   }, { ACC_FS352k, 352800, (unsigned char) CEA861_0k   }, { ACC_FS256k, 256000, (unsigned char) CEA861_0k },
    /* Range : 2^2  */ { ACC_FS192k, 192000, (unsigned char) CEA861_192k }, { ACC_FS176k, 176400, (unsigned char) CEA861_176k }, { ACC_FS128k, 128000, (unsigned char) CEA861_0k },
    /* Range : 2^1  */ {  ACC_FS96k,  96000, (unsigned char) CEA861_96k  }, {  ACC_FS88k,  88200, (unsigned char) CEA861_88k  }, {  ACC_FS64k,  64000, (unsigned char) CEA861_0k },

    /* Range : 2^0  */ {  ACC_FS48k,  48000, (unsigned char) CEA861_48k  }, {  ACC_FS44k,  44100, (unsigned char) CEA861_44k  }, {  ACC_FS32k,  32000, (unsigned char) CEA861_32k},
    /* Range : 2^-1 */ {  ACC_FS24k,  24000, (unsigned char) CEA861_0k   }, {  ACC_FS22k,  22050, (unsigned char) CEA861_0k   }, {  ACC_FS16k,  16000, (unsigned char) CEA861_0k },
    /* Range : 2^-2 */ {  ACC_FS12k,  12000, (unsigned char) CEA861_0k   }, {  ACC_FS11k,  11025, (unsigned char) CEA861_0k   }, {   ACC_FS8k,   8000, (unsigned char) CEA861_0k },
    /* Range : 2^-3 */ {   ACC_FS6k,   6000, (unsigned char) CEA861_0k   }, {   ACC_FS5k,   5000, (unsigned char) CEA861_0k   }, {   ACC_FS4k,   4000, (unsigned char) CEA861_0k },
    /* Delimiter    */ {   ACC_FS8k,      0, (unsigned char) CEA861_0k }
};

/// The StmSeSamplingFrequencyCodeLookupTable table provides an indexed direct map of
/// enum eAccFsCode to Hz without searching through the table.
static const int32_t StmSeSamplingFrequencyCodeLookupTable[] =
{
    48000,  //  ACC_FS48k
    44100,  //  ACC_FS44k
    32000,  //  ACC_FS32k
    -1 ,    //  ACC_FS_reserved_3,
    96000,  //  ACC_FS96k,
    88200,  //  ACC_FS88k,
    64000,  //  ACC_FS64k,
    -1,     //  ACC_FS_reserved_7,
    192000, //  ACC_FS192k,
    176400, //  ACC_FS176k,
    128000, //  ACC_FS128k,
    -1,     //  ACC_FS_reserved_11,
    384000, //  ACC_FS384k,
    352800, //  ACC_FS352k,
    256000, //  ACC_FS256k,
    -1,     //  ACC_FS_reserved_15,
    12000,  //  ACC_FS12k,
    11025,  //  ACC_FS11k,
    8000,   //  ACC_FS8k,
    -1,     //  ACC_FS_reserved_19,
    24000,  //  ACC_FS24k,
    22050,  //  ACC_FS22k,
    16000,  //  ACC_FS16k,
    -1,     //  ACC_FS_reserved_23,
    768000, //  ACC_FS768k,
    705000, //  ACC_FS705k,
    512000, //  ACC_FS512k,
    -1,     //  ACC_FS_reserved_27,
    6000,   //  ACC_FS6k,
    5000,   //  ACC_FS5k,
    4000,   //  ACC_FS4k,
    -1,     //  ACC_FS_reserved_31,
    -1,     //  ACC_FS_reserved
};


enum eAccFsCode StmSeTranslateIntegerSamplingFrequencyToDiscrete(uint32_t IntegerFrequency)
{
    int i;

    for (i = 0; IntegerFrequency < StmSeSamplingFrequencyLookupTable[i].Integer; i++)
        ; // do nothing

    return StmSeSamplingFrequencyLookupTable[i].Discrete;
}

void StmSeTranslateIntegerSamplingFrequencyToHdmi(uint32_t IntegerFrequency, unsigned char *HdmiFrequency)
{
    int i;

    for (i = 0; IntegerFrequency < StmSeSamplingFrequencyLookupTable[i].Integer; i++)
        ; // do nothing

    *HdmiFrequency = StmSeSamplingFrequencyLookupTable[i].Hdmi;
}


int StmSeTranslateIsoSamplingFrequencyToDiscrete(uint32_t IntegerFrequency, enum eAccFsCode &DiscreteFrequency)
{
    int i;

    for (i = 0; i < lengthof(StmSeSamplingFrequencyLookupTable) ; i++)
    {
        if (StmSeSamplingFrequencyLookupTable[i].Integer == IntegerFrequency)
        {
            DiscreteFrequency = StmSeSamplingFrequencyLookupTable[i].Discrete;
            return 0;
        }
    }

    // Return a default value
    return -EINVAL;
}

int32_t StmSeTranslateDiscreteSamplingFrequencyToInteger(enum eAccFsCode DiscreteFrequency)
{
    int32_t iso_frequency = -1;

    if (DiscreteFrequency < lengthof(StmSeSamplingFrequencyCodeLookupTable))
    {
        iso_frequency = StmSeSamplingFrequencyCodeLookupTable[DiscreteFrequency];
    }

    return iso_frequency;
}

enum ePtsTimeFormat StmSeConvertPlayerTimeFormatToFwTimeFormat(PlayerTimeFormat_t NativeTimeFormat)
{
    enum ePtsTimeFormat FwTimeFormat;
    switch (NativeTimeFormat)
    {
    case TimeFormatUs:
        FwTimeFormat = PtsTimeFormatUs;
        break;

    case TimeFormatPts:
        FwTimeFormat = PtsTimeFormat90k;
        break;

    default:
        FwTimeFormat = PtsTimeFormat90k;
        break;
    }
    return FwTimeFormat;
}

PlayerTimeFormat_t StmSeConvertFwTimeFormatToPlayerTimeFormat(enum ePtsTimeFormat FwTimeFormat)
{
    PlayerTimeFormat_t NativeTimeFormat;
    switch (FwTimeFormat)
    {
    case PtsTimeFormatUs:
        NativeTimeFormat = TimeFormatUs;
        break;

    case PtsTimeFormat90k:
        NativeTimeFormat = TimeFormatPts;
        break;

    default:
        NativeTimeFormat = TimeFormatPts;
        break;
    }
    return NativeTimeFormat;
}

#ifdef __cplusplus
}
#endif

