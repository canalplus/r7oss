/*
 * properties.h
 *
 * Copyright (C) STMicroelectronics Limited 2005. All rights reserved.
 *
 * Stream properties object decoding utility classes
 */

#ifndef H_PROPERTIES
#define H_PROPERTIES

#include "asf.h"

#define WMA_VERSION_1           0x160
#define WMA_VERSION_2_9         0x161
#define WMA_VERSION_9_PRO       0x162
#define WMA_LOSSLESS            0x163

typedef unsigned char asf_byte;
typedef unsigned short asf_u16;
typedef unsigned int asf_u32;

class ASF_StreamPropertiesObject_c {
  public:

    asf_guid_index object_id;
    asf_u32   object_size;
    asf_guid_index stream_type;
    asf_guid_index error_correction_type;
    asf_u32   time_offset;
    asf_u32   time_offset_hi;
    asf_u32   type_specific_data_length;
    asf_u32   error_correction_data_length;
    asf_u32   flags;
    asf_byte  stream_number;
    bool      encrypted_content_flag;
    asf_byte *type_specific_data; // this field has 'odd' lifetime
    asf_byte *error_correction_data; // this field has 'odd' lifetime

    unsigned char *decode(unsigned char *data, unsigned int dataLength);
    void dump(bool verbose=false);
};

class WMA_WaveFormatEx_c {
  public:

    asf_u16 codec_id;
    asf_u16 number_of_channels;
    asf_u32 samples_per_second;
    asf_u32 average_number_of_bytes_per_second;
    asf_u16 block_alignment;
    asf_u16 bits_per_sample;
    asf_u16 codec_specific_data_size;
    asf_byte *codec_specific_data;

    unsigned char *decode(unsigned char *data, unsigned int dataLength);
    void dump(bool verbose=false);
};

class WMA_TypeSpecificData_c {
  public:
    asf_u32 samples_per_block;
    asf_u16 encode_options;
    asf_u32 super_block_align;
    asf_u16 valid_bits_per_sample;
    asf_u16 channel_mask;

    unsigned char *decode(unsigned int formatTag, unsigned char *data, unsigned int dataLength);
    void dump(bool verbose=false);
};

#endif // H_PROPERTIES
