/*
 * Raw FLAC demuxer
 * Copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * the following code is partly copied from ffmpeg upstream  ac63ddad2fa29f83aaaac227fa18fd6167741b32
 */

#include <stdint.h>
#include "avformat.h"
#include "common.h"
#include "flac.h"
#include "flacdata.h"
#include "raw.h"
#include "bytestream.h"
#include "bitstream.h"

/* Todo use id3 reader from upstream */
/*#include "id3v2.h"*/
#include "oggdec.h"

#define ID3v2_HEADER_SIZE 10
/* Return this if we don't have enough data for a decisive probe. */
#define AVPROBE_SCORE_UNDECIDED 49

/*todo : dont use duplicate struct from flac decoder */
static const int sample_size_table[] =
{ 0, 8, 12, 0, 16, 20, 24, 0 };

typedef struct {
    int first_packet;
    uint64_t first_data;  
}FLAContext;


/*
 * This function has been copied from ffmpeg upstream libavcodec/flac.c
 */

static void dump_headers(AVCodecContext *avctx, FLACStreaminfo *s)
{  
    av_log(avctx, AV_LOG_DEBUG, "  Max Blocksize: %d\n", s->max_blocksize);
    av_log(avctx, AV_LOG_DEBUG, "  Max Framesize: %d\n", s->max_framesize);
    av_log(avctx, AV_LOG_DEBUG, "  Samplerate: %d\n", s->samplerate);
    av_log(avctx, AV_LOG_DEBUG, "  Channels: %d\n", s->channels);
    av_log(avctx, AV_LOG_DEBUG, "  Bits: %d\n", s->bps);
}

/*
 * This function has been copied from mp3.c
 */

/* buf must be ID3v2_HEADER_SIZE bytes long */
static int id3v2_match(const uint8_t *buf)
{
    return (buf[0] == 'I' &&
            buf[1] == 'D' &&
            buf[2] == '3' &&
            buf[3] != 0xff &&
            buf[4] != 0xff &&
            (buf[6] & 0x80) == 0 &&
            (buf[7] & 0x80) == 0 &&
            (buf[8] & 0x80) == 0 &&
            (buf[9] & 0x80) == 0);
}

/* buf must be an id3v2 header (so it must pass id3v2_match) */
static uint32_t id3v2_get_size(const uint8_t *buf)
{
    uint32_t size = buf[9] + 
        (buf[8] << 7) +
        (buf[7] << 14) +
        (buf[6] << 21);
    return size;
}


/**
 * Parse the metadata block parameters from the header.
 * @param[in]  block_header header data, at least 4 bytes
 * @param[out] last indicator for last metadata block
 * @param[out] type metadata block type
 * @param[out] size metadata block size
 *
 * Note : This function has been copied from ffmpeg upstream libavcodec/flac.c
 */
void ff_flac_parse_block_header(const uint8_t *block_header,
                                int *last, int *type, int *size)
{
    int tmp = bytestream_get_byte(&block_header);
    if (last)
        *last = tmp & 0x80;
    if (type)
        *type = tmp & 0x7F;
    if (size)
        *size = bytestream_get_be24(&block_header);
}

/*
 * This function has been copied from ffmpeg upstream libavcodec/flac.c
 */
void ff_flac_parse_streaminfo(AVCodecContext *avctx, struct FLACStreaminfo *s,
                              const uint8_t *buffer)
{
    GetBitContext gb;
    init_get_bits(&gb, buffer, FLAC_STREAMINFO_SIZE*8);
    
    skip_bits(&gb, 16); /* skip min blocksize */
    s->max_blocksize = get_bits(&gb, 16);
    if (s->max_blocksize < FLAC_MIN_BLOCKSIZE) {
        av_log(avctx, AV_LOG_WARNING, "invalid max blocksize: %d\n",
               s->max_blocksize);
        s->max_blocksize = 16;
    }
    
    skip_bits(&gb, 24); /* skip min frame size */
    s->max_framesize = get_bits_long(&gb, 24);
    
    s->samplerate = get_bits_long(&gb, 20);
    s->channels = get_bits(&gb, 3) + 1;
    s->bps = get_bits(&gb, 5) + 1;
    
    avctx->channels = s->channels;
    avctx->sample_rate = s->samplerate;
    
    if (s->bps > 16)
        avctx->sample_fmt = SAMPLE_FMT_S32;
    else
        avctx->sample_fmt = SAMPLE_FMT_S16;
    
    s->samples  = get_bits_long(&gb, 32) << 4;
    s->samples |= get_bits(&gb, 4);
    
    skip_bits_long(&gb, 64); /* md5 sum */
    skip_bits_long(&gb, 64); /* md5 sum */
    
    dump_headers(avctx, s);
}

/* 
 * Return whether there is a FLAC header at bufptr.
 * Requires that bufptr be at least 4 bytes long.
 */
static int flac_header_match(uint8_t *bufptr) {
    return !memcmp(bufptr, "fLaC", 4);
}

static int flac_probe(AVProbeData *p)
{
    uint8_t *bufptr = p->buf;
    /* Add 5 points to undecided probes if the filename extension is .flac. */
    uint32_t extension_bonus = 0;
    if(p->filename && match_ext(p->filename, "flac")) {
        extension_bonus = 5;
    }

    if(flac_header_match(bufptr)) {
        return AVPROBE_SCORE_MAX;
    }

    if(id3v2_match(bufptr)) {
        /* id3 tag found: can we skip it and still attempt to read a FLAC header? */
        uint32_t id3v2_tag_size = id3v2_get_size(bufptr);
        if(p->buf_size < (ID3v2_HEADER_SIZE + id3v2_tag_size + 4)) {
            return AVPROBE_SCORE_UNDECIDED + extension_bonus;
        }

        bufptr += ID3v2_HEADER_SIZE + id3v2_tag_size;
        if(flac_header_match(bufptr)) {
            return AVPROBE_SCORE_MAX;
        }
        return 0;
    }

    /* No FLAC header and no ID3 tag either: this not the file format you are looking for. */
    return 0;
}



static int flac_read_header(AVFormatContext *s,
                             AVFormatParameters *ap)
{
    int ret, metadata_last=0, metadata_type, metadata_size, found_streaminfo=0;
    AVStream *st = av_new_stream(s, 0);
    uint8_t header[4];
    uint8_t *buffer=NULL;
    FLAContext *flac = s->priv_data;

    
    if (!st)
        return AVERROR(ENOMEM);
    
    st->codec->codec_type = CODEC_TYPE_AUDIO;
    st->codec->codec_id = CODEC_ID_FLAC;
    /* st->need_parsing = AVSTREAM_PARSE_FULL; */

    uint8_t id3v2_header[ID3v2_HEADER_SIZE];
    get_buffer(s->pb, id3v2_header, ID3v2_HEADER_SIZE);
    if(id3v2_match(id3v2_header)) {
        /* Seek past the entire id3v2 tag. */
        uint32_t id3v2_tag_size = id3v2_get_size(id3v2_header);
        url_fseek(s->pb, id3v2_tag_size, SEEK_CUR);
    } else {
        /* The data we read into id3v2_header wasn't an id3 header -- seek back. */
        url_fseek(s->pb, -ID3v2_HEADER_SIZE, SEEK_CUR);
    }
    
    /* if fLaC marker is not found, assume there is no header */
    if (get_le32(s->pb) != MKTAG('f','L','a','C')) {
        url_fseek(s->pb, -4, SEEK_CUR);
        return 0;
    }
        
    /* the parameters will be extracted from the compressed bitstream */
    /* process metadata blocks */
    while (!url_feof(s->pb) && !metadata_last) {
        
        get_buffer(s->pb, header, 4);
        
        ff_flac_parse_block_header(header, &metadata_last, &metadata_type,
                                   &metadata_size);
        switch (metadata_type) {
            
        /* allocate and read metadata block for supported types */
        case FLAC_METADATA_TYPE_STREAMINFO:
        case FLAC_METADATA_TYPE_VORBIS_COMMENT:
            buffer = av_mallocz(metadata_size + FF_INPUT_BUFFER_PADDING_SIZE);
            if (!buffer) {
                return AVERROR_NOMEM;
            }
            if (get_buffer(s->pb, buffer, metadata_size) != metadata_size) {
                av_freep(&buffer);
                return AVERROR_IO;
            }
            break;
            
        case FLAC_METADATA_TYPE_SEEKTABLE:
            /* skip metadata block for unsupported types */
        default:
            ret = url_fseek(s->pb, metadata_size, SEEK_CUR);
            if (ret < 0)
                return ret;
        }
        
        if (metadata_type == FLAC_METADATA_TYPE_STREAMINFO) {
            FLACStreaminfo si;
            
            /* STREAMINFO can only occur once */
            if (found_streaminfo) {
                av_freep(&buffer);
                return AVERROR_INVALIDDATA;
            }
            if (metadata_size != FLAC_STREAMINFO_SIZE) {
                av_freep(&buffer);
                return AVERROR_INVALIDDATA;
            }
            found_streaminfo = 1;
            st->codec->extradata      = buffer;
            st->codec->extradata_size = metadata_size;
            buffer = NULL;
            
            /* get codec params from STREAMINFO header */
            ff_flac_parse_streaminfo(st->codec, &si, st->codec->extradata);
            
            /* set time base and duration */
            if (si.samplerate > 0) {
                av_set_pts_info(st, 64, 1, si.samplerate);
                if (si.samples > 0)
                    st->duration = si.samples;
            }
            
                        
        } else {
            /* STREAMINFO must be the first block */
            if (!found_streaminfo) {
                av_freep(&buffer);
                av_log(NULL, AV_LOG_DEBUG, "error finding stream info %s \n",__FUNCTION__);
                
                /* we can recover from this */
                /* return AVERROR_INVALIDDATA; */
            }
            /* process supported blocks other than STREAMINFO */
            if (metadata_type == FLAC_METADATA_TYPE_VORBIS_COMMENT) {
                if (vorbis_comment(s, buffer, metadata_size)) {
                    av_log(s, AV_LOG_WARNING, "error parsing Vorbis Comment metadata\n");
                }
            }
            av_freep(&buffer);
            
        }
    }

    flac->first_packet = 0;
    flac->first_data= url_ftell(s->pb); 
    url_fseek(s->pb, 0, SEEK_SET);
    
    return 0;
}

#define RAW_PACKET_SIZE 1024
#define RAW_SAMPLES     1024


static int flac_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    int ret, size;
    uint8_t header_part_1, header_part_2;
    FLAContext *flac = s->priv_data;
    /*todo : jump to next packet if we made a seek */
    
    size = RAW_PACKET_SIZE;
    if (av_new_packet(pkt, size) < 0)
        return AVERROR(EIO);
    
    if(flac->first_packet == 0){
        pkt->pts = 0;
        flac->first_packet = 1;
    }
    
    pkt->pos= url_ftell(s->pb);
    pkt->stream_index = 0;
    ret = get_partial_buffer(s->pb, pkt->data, size);
    if (ret <= 0) {
        av_free_packet(pkt);
        return AVERROR(EIO);
    }
    pkt->size = ret;
    
    return 0;
}


static int flac_read_seek(AVFormatContext *s, int stream_index, int64_t pts, int flags)
{
    /* av_log(NULL, AV_LOG_DEBUG, "%s \n",__FUNCTION__);*/
    
    return -1;
    
}


/*
 * PTS derivation :
 *
 * if variable blocksize :
 *    pts = frame_number * blocksize_sample / sample_rate
 * if fixed blocksize :
 *    pts = sample_number / sample_rate
 */
static int64_t flac_read_pts(AVFormatContext *s, int stream_index, int64_t *ppos, int64_t pos_limit)
{
    
    uint8_t header_part_1, header_part_2;
    uint8_t variable_block_size;
    uint8_t temp_info;
    uint8_t chan_assign, sample_size;
    
    int bs_code, sr_code;
    
    int64_t utf8_val=1;
    int64_t pts;
    
    uint64_t blocksize_sample,samplerate;
    FLAContext *flac = s->priv_data;
    
    int64_t current_pos;
    int64_t pos= *ppos;
    
        
    header_part_1 = get_byte(s->pb);
    header_part_2 = get_byte(s->pb);

    /* Note : this test is not a reliable approach
    if( *ppos < flac->first_data  ){
        return AV_NOPTS_VALUE;
    }
    */
    
    for(;;){
        header_part_1 = header_part_2;
        header_part_2 = get_byte(s->pb);
        
        if((header_part_1 == 0xFF) && (header_part_2 == 0xF8) )
            break;
        
        if(!header_part_2)
            return AV_NOPTS_VALUE;
    }
    
    variable_block_size = header_part_2 & 0x01;
    
    temp_info = get_byte(s->pb);
    
    bs_code = (0xF0 & temp_info) >> 4; /*block size in inter channel sample*/
    sr_code = 0x0F & temp_info; /*sample rate*/
    
    temp_info = get_byte(s->pb);/*channel assignation and sample size*/
    
    chan_assign = (0xF0 & temp_info)>> 4;
    sample_size = (0x0E & temp_info)>> 1;
    /* this value is encoded in utf8 */
    GET_UTF8(utf8_val, get_byte(s->pb), utf8_val=-1;)
            
    /* blocksize */
    if (bs_code == 0) {
        av_log(NULL, AV_LOG_ERROR, "unexpected reserved blocksize code: 0\n");
        return  AV_NOPTS_VALUE;
    } else if (bs_code == 6) {
        blocksize_sample = get_byte(s->pb) + 1;
    } else if (bs_code == 7) {
        blocksize_sample = get_le16(s->pb) + 1;
    } else {
        blocksize_sample = ff_flac_blocksize_table[bs_code];
    }
    
    /* sample rate */
    if (sr_code < 12) {
        samplerate = ff_flac_sample_rate_table[sr_code];
    } else if (sr_code == 12) {
        samplerate = get_byte(s->pb) * 1000;
    } else if (sr_code == 13) {
        samplerate = get_le16(s->pb);
    } else if (sr_code == 14) {
        samplerate = get_le16(s->pb) * 10;
    } else {
        av_log(NULL, AV_LOG_ERROR, "illegal sample rate code %d\n", sr_code);
        return  AV_NOPTS_VALUE;
    }
    
    get_byte(s->pb); /* we skip CRC-8 */
    
    if(variable_block_size){
        pts =( utf8_val * blocksize_sample ) / samplerate;
    }else{
        pts = utf8_val* blocksize_sample; 
    }
    
    *ppos= pos;
    
    return pts;
    
}


static int flac_read_close(AVFormatContext *s)
{
    int i;
    
    for(i=0;i<s->nb_streams;i++) {
        AVStream *st = s->streams[i];
        av_free(st->priv_data);
    }
    
    return 0;
}

AVInputFormat flac_demuxer = {
    "flac",
    "raw flac format",
    sizeof(FLAContext),
    flac_probe,
    flac_read_header,
    flac_read_packet,
    flac_read_close,
    NULL,/* flac_read_seek,*/
    NULL,/* flac_read_pts,*/
    .extensions = "flac",
};
