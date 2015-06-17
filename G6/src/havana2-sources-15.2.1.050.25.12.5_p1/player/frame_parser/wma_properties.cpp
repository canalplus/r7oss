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

#include "osinline.h"
#include "wma_properties.h"

static inline asf_u32 extract(unsigned char *&data, unsigned int bytes)
{
    unsigned int e = 0;

    if (bytes > 0)
    {
        e = *data++;
    }

    if (bytes > 1)
    {
        e |= (*data++) << 8;
    }

    if (bytes > 2)
    {
        // by ignoring the case where bytes > 3 we can decode type fields directly
        e |= (*data++) << 16;
        e |= (*data++) << 24;
    }

    return e;
}

static inline asf_guid_index lookup_guid(unsigned char *guid)
{
    int i;

    for (i = 0; i < ASF_GUID_MAX; i++)
    {
        if (0 == memcmp(guid, asf_guid_lookup[i], sizeof(asf_guid_t)))
        {
            return static_cast<asf_guid_index>(i);
        }
    }

    return ASF_GUID_INVALID_IDENTIFIER;
}

#undef TRACE_TAG
#define TRACE_TAG "ASF_StreamPropertiesObject_c"

unsigned char *ASF_StreamPropertiesObject_c::decode(unsigned char *data, unsigned int dataLength)
{
    // class contains no virtual members so we can safely use memset ...
    memset(this, 0, sizeof(*this));

    if (dataLength <= 82)
    {
        return NULL;
    }

    object_id = lookup_guid(data);

    if (object_id == ASF_GUID_INVALID_IDENTIFIER)
    {
        return NULL;
    }

    data += sizeof(asf_guid_t);
    object_size = extract(data, 4);
    asf_u32 sizehi = extract(data, 4);

    if (object_size != dataLength || sizehi != 0)
    {
        return NULL;
    }

    stream_type = lookup_guid(data);

    if (stream_type == ASF_GUID_INVALID_IDENTIFIER)
    {
        return NULL;
    }

    data += sizeof(asf_guid_t);
    error_correction_type = lookup_guid(data);

    if (error_correction_type == ASF_GUID_INVALID_IDENTIFIER)
    {
        return NULL;
    }

    data += sizeof(asf_guid_t);
    time_offset = extract(data, 4);
    time_offset_hi = extract(data, 4);
    type_specific_data_length = extract(data, 4);
    error_correction_data_length = extract(data, 4);
    flags = extract(data, 2);
    stream_number = flags & 0x7f;
    encrypted_content_flag = flags & 0x8000;
    asf_u32 reserved = extract(data, 4);
    (void) reserved; // warning suppression
    type_specific_data = data;
    error_correction_data = type_specific_data + type_specific_data_length;
    // TODO: validate type_specific_data_length and error_correction_data_length
    return error_correction_data + error_correction_data_length;
}

void ASF_StreamPropertiesObject_c::dump(bool verbose)
{
    SE_INFO(group_frameparser_audio, "ASF STREAM PROPERTIES OBJECT\n");
    SE_INFO(group_frameparser_audio, "Object id                      %s\n", asf_guid_name_lookup[object_id]);
    SE_INFO(group_frameparser_audio, "Object size                    %d\n", object_size);
    SE_INFO(group_frameparser_audio, "Stream type                    %s\n", asf_guid_name_lookup[stream_type]);
    SE_INFO(group_frameparser_audio, "Error correction type          %s\n", asf_guid_name_lookup[error_correction_type]);
    SE_INFO(group_frameparser_audio, "Time offset                    0x%x%08x\n", time_offset_hi, time_offset);
    SE_INFO(group_frameparser_audio, "Type specific data length      %d\n", type_specific_data_length);
    SE_INFO(group_frameparser_audio, "Error correction data length   %d\n", error_correction_data_length);
    SE_INFO(group_frameparser_audio, "Flags                          %d\n", flags);

    if (verbose)
    {
        SE_INFO(group_frameparser_audio, "    Stream number                  %d\n", stream_number);
        SE_INFO(group_frameparser_audio, "    Encrypted content flags        %s\n", (encrypted_content_flag ? "true" : "false"));
    }

    SE_INFO(group_frameparser_audio, "Type specific data             %p\n", type_specific_data);
    SE_INFO(group_frameparser_audio, "Error correction data          %p\n", error_correction_data);
}

#undef TRACE_TAG
#define TRACE_TAG "WMA_WaveFormatEx_c"

unsigned char *WMA_WaveFormatEx_c::decode(unsigned char *data, unsigned int dataLength)
{
    // class contains no virtual members so we can safely use memset ...
    memset(this, 0, sizeof(*this));

    if (dataLength < 18)
    {
        return NULL;
    }

    codec_id = extract(data, 2);
    number_of_channels = extract(data, 2);
    samples_per_second = extract(data, 4);
    average_number_of_bytes_per_second = extract(data, 4);
    block_alignment = extract(data, 2);
    bits_per_sample = extract(data, 2);
    codec_specific_data_size = extract(data, 2);
    codec_specific_data = data;
    // TODO: validate codec_specific data size
    return codec_specific_data + codec_specific_data_size;
}

void WMA_WaveFormatEx_c::dump(bool verbose)
{
    SE_INFO(group_frameparser_audio, "WMA_WAVEFORMATEX\n");
    SE_INFO(group_frameparser_audio, "Codec id/format tag            %d\n", codec_id);
    SE_INFO(group_frameparser_audio, "Number of channels             %d\n", number_of_channels);
    SE_INFO(group_frameparser_audio, "Samples per second             %d\n", samples_per_second);
    SE_INFO(group_frameparser_audio, "Avg. num. of bytes per second  %d\n", average_number_of_bytes_per_second);
    SE_INFO(group_frameparser_audio, "Block alignment                %d\n", block_alignment);
    SE_INFO(group_frameparser_audio, "Bits per sample                %d\n", bits_per_sample);
    SE_INFO(group_frameparser_audio, "Codec specific data size       %d\n", codec_specific_data_size);
    SE_INFO(group_frameparser_audio, "Codec specific data            %p\n", codec_specific_data);
}


#undef TRACE_TAG
#define TRACE_TAG "WMA_TypeSpecificData_c"

unsigned char *WMA_TypeSpecificData_c::decode(unsigned int formatTag, unsigned char *data, unsigned int dataLength)
{
    // class contains no virtual members so we can safely use memset ...
    memset(this, 0, sizeof(*this));

    channel_mask                = 3;            // Default to stereo

    switch (formatTag)
    {
    case WMA_VERSION_1:
        break;

    case WMA_VERSION_2_9:
        if (dataLength != 10)
        {
            return NULL;
        }

        samples_per_block       = extract(data, 4);
        encode_options          = extract(data, 2);
        super_block_align       = extract(data, 4);
        break;

    case WMA_VERSION_9_PRO:
    case WMA_LOSSLESS:
        if (dataLength != 18)
        {
            return NULL;
        }

        unsigned int    temp;
        valid_bits_per_sample   = extract(data, 2);
        channel_mask            = extract(data, 4);
        temp                    = extract(data, 4);
        temp                    = extract(data, 4);
        encode_options          = extract(data, 2);
        if (temp) { /* warning removal */ }
        break;

    default:
        return NULL;
    }

    return data;
}

void WMA_TypeSpecificData_c::dump(bool verbose)
{
    SE_INFO(group_frameparser_audio, "WMA TYPE SPECIFIC DATA\n");
    SE_INFO(group_frameparser_audio, "Samples per block              %d\n", samples_per_block);
    SE_INFO(group_frameparser_audio, "Encode options                 %d\n", encode_options);
    SE_INFO(group_frameparser_audio, "Super block align              %d\n", super_block_align);
    SE_INFO(group_frameparser_audio, "Valid Bits Per Sample          %d\n", valid_bits_per_sample);
    SE_INFO(group_frameparser_audio, "Channel Mask                   %d\n", channel_mask);
}
