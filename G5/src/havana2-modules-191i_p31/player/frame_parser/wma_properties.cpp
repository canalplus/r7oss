/*
 * propertes.cpp
 *
 * Copyright (C) STMicroelectronics Limited 2005. All rights reserved.
 *
 * Stream properties object decoding utility classes
 */

//#include <string.h>

extern "C" {
  int memcmp(const void *a, const void *b, int n);
  int memset(void *a, int c, int n);
}

#define NULL (0)

#include "wma_properties.h"
#include "report.h"

#define ASF_TRACE(fmt, args...) (report(severity_note, "%s: " fmt, "frame_wma_audio", ##args))

static inline asf_u32 extract(unsigned char *&data, unsigned int bytes)
{
    unsigned int e = 0;

    if (bytes > 0)
	e = *data++;
    if (bytes > 1)
	e |= (*data++) << 8;
    if (bytes > 2) {
	// by ignoring the case where bytes > 3 we can decode type fields directly
	e |= (*data++) << 16;
	e |= (*data++) << 24;
    }

    return e;
}

static inline asf_guid_index lookup_guid(unsigned char *guid)
{
    int i;

    for(i=0; i<ASF_GUID_MAX; i++) {
	if (0 == memcmp(guid, asf_guid_lookup[i], sizeof(asf_guid_t))) {
	    return static_cast<asf_guid_index>(i);
	}
    }

    return ASF_GUID_INVALID_IDENTIFIER;
}

unsigned char *ASF_StreamPropertiesObject_c::decode(unsigned char *data, unsigned int dataLength)
{
    // class contains no virtual members so we can safely use memset ...
    memset(this, 0, sizeof(*this));

    if (dataLength <= 82) {
	    return NULL;
    }

    object_id = lookup_guid(data);
    if (object_id == ASF_GUID_INVALID_IDENTIFIER) {
	return NULL;
    }
    data += sizeof(asf_guid_t);

    object_size = extract(data, 4);
    asf_u32 sizehi = extract(data, 4);
    if (object_size != dataLength || sizehi != 0) {
	return NULL;
    }

    stream_type = lookup_guid(data);
    if (stream_type == ASF_GUID_INVALID_IDENTIFIER) {
	return NULL;
    }
    data += sizeof(asf_guid_t);

    error_correction_type = lookup_guid(data);
    if (error_correction_type == ASF_GUID_INVALID_IDENTIFIER) {
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
    ASF_TRACE("ASF STREAM PROPERTIES OBJECT\n");
    ASF_TRACE("Object id                      %s\n", asf_guid_name_lookup[object_id]);
    ASF_TRACE("Object size                    %d\n", object_size);
    ASF_TRACE("Stream type                    %s\n", asf_guid_name_lookup[stream_type]);
    ASF_TRACE("Error correction type          %s\n", asf_guid_name_lookup[error_correction_type]);
    ASF_TRACE("Time offset                    0x%x%08x\n", time_offset_hi, time_offset);
    ASF_TRACE("Type specific data length      %d\n", type_specific_data_length);
    ASF_TRACE("Error correction data length   %d\n", error_correction_data_length);
    ASF_TRACE("Flags                          %d\n", flags);
    if (verbose) {
	ASF_TRACE("    Stream number                  %d\n", stream_number);
	ASF_TRACE("    Encrypted content flags        %s\n", (encrypted_content_flag ? "true" : "false"));
    }
    ASF_TRACE("Type specific data             0x%08x\n", type_specific_data);
    ASF_TRACE("Error correction data          0x%08x\n", error_correction_data);
}

unsigned char *WMA_WaveFormatEx_c::decode(unsigned char *data, unsigned int dataLength)
{
    // class contains no virtual members so we can safely use memset ...
    memset(this, 0, sizeof(*this));

    if (dataLength < 18) {
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
    ASF_TRACE("WMA_WAVEFORMATEX\n");
    ASF_TRACE("Codec id/format tag            %d\n", codec_id);
    ASF_TRACE("Number of channels             %d\n", number_of_channels);
    ASF_TRACE("Samples per second             %d\n", samples_per_second);
    ASF_TRACE("Avg. num. of bytes per second  %d\n", average_number_of_bytes_per_second);
    ASF_TRACE("Block alignment                %d\n", block_alignment);
    ASF_TRACE("Bits per sample                %d\n", bits_per_sample);
    ASF_TRACE("Codec specific data size       %d\n", codec_specific_data_size);
    ASF_TRACE("Codec specific data            0x%08x\n", codec_specific_data);
}

unsigned char *WMA_TypeSpecificData_c::decode(unsigned int formatTag, unsigned char *data, unsigned int dataLength)
{
    // class contains no virtual members so we can safely use memset ...
    memset(this, 0, sizeof(*this));

    channel_mask                = 3;            // Default to stereo

    switch (formatTag) {
    case WMA_VERSION_1:
	break;

    case WMA_VERSION_2_9:
	if (dataLength != 10)
	    return NULL;

	samples_per_block       = extract(data, 4);
	encode_options          = extract(data, 2);
	super_block_align       = extract(data, 4);
	break;

    case WMA_VERSION_9_PRO:
    case WMA_LOSSLESS:
	if (dataLength != 18)
	    return NULL;

	unsigned int    temp;
	valid_bits_per_sample   = extract(data, 2);
	channel_mask            = extract(data, 4);
	temp                    = extract(data, 4);
	temp                    = extract(data, 4);
	encode_options          = extract(data, 2);
	break;

    default:
	return NULL;
    }

    return data;
}

void WMA_TypeSpecificData_c::dump(bool verbose)
{
    ASF_TRACE("WMA TYPE SPECIFIC DATA\n");
    ASF_TRACE("Samples per block              %d\n", samples_per_block);
    ASF_TRACE("Encode options                 %d\n", encode_options);
    ASF_TRACE("Super block align              %d\n", super_block_align);
    ASF_TRACE("Valid Bits Per Sample          %d\n", valid_bits_per_sample);
    ASF_TRACE("Channel Mask                   %d\n", channel_mask);
}
