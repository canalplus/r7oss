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


// Codec IDs
#define USER_DATA_MPEG2_CODEC_ID       0
#define USER_DATA_H264_CODEC_ID        1
#define USER_DATA_HEVC_CODEC_ID        1 /* Since the HevcUserDataParameters_t and H264UserDataParameters_s are typedef
                                            structures referring to same data, keep the same codec id*/



// The Length of user data start code
#define USER_DATA_START_CODE_LENGTH    4


typedef struct MPEG2UserDataParameters_s
{
    unsigned   reserved: 31; // reserved=31
    unsigned   TopFieldFirst: 1; // top_field_first=1
} MPEG2UserDataParameters_t;


typedef struct H264UserDataParameters_s
{
    unsigned                reserved: 31; //  reserved=31
    unsigned                Is_registered: 1; //  is_registered=1
    unsigned                Itu_t_t35_country_code: 8; // size=8
    unsigned                Itu_t_t35_country_code_extension_byte: 8; // size=8
    unsigned                Itu_t_t35_provider_code: 16; // size=16
    char                    uuid_iso_iec_11578[16]; // size=128
} H264UserDataParameters_t;

typedef struct H264UserDataParameters_s HevcUserDataParameters_t;

typedef union UserDataCodecParameters_s
{
    MPEG2UserDataParameters_t  MPEG2UserDataParameters;
    H264UserDataParameters_t   H264UserDataParameters;
    HevcUserDataParameters_t   HevcUserDataParameters;
} CodecUserDataParameters_t;
