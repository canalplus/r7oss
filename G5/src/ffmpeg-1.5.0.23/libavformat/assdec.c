/*
 * SSA/ASS demuxer
 * Copyright (c) 2008 Michael Niedermayer
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

#include "avformat.h"

#define MAX_LINESIZE 2000

typedef struct ASSEvent{
    int64_t start_time;
    int end_time;
    uint8_t *text;
}ASSEvent;

typedef struct ASSContext{
    uint8_t *event_buffer;
    ASSEvent *event;
    unsigned int event_count;
    unsigned int event_index;
}ASSContext;

static void get_line(ByteIOContext *s, char *buf, int maxlen)
{
    int i = 0;
    char c;

    do{
        c = get_byte(s);
        if (i < maxlen-1)
            buf[i++] = c;
    }while(c != '\n' && c);

    buf[i] = 0;
}

static int probe(AVProbeData *p)
{
    const char *header= "[Script Info]";

    if(   !memcmp(p->buf  , header, strlen(header))
       || !memcmp(p->buf+3, header, strlen(header)))
        return AVPROBE_SCORE_MAX;

    return 0;
}

static int read_close(AVFormatContext *s)
{
    ASSContext *ass = s->priv_data;

    av_freep(&ass->event_buffer);
    av_freep(&ass->event);

    return 0;
}

static int64_t get_pts(const uint8_t *p, int *end_time)
{
    int hour, min, sec, hsec;
    int dhour, dmin, dsec, dhsec;

    if(sscanf(p, "%*[^,],%d:%d:%d%*c%d,%d:%d:%d%*c%d",
              &hour, &min, &sec, &hsec,
              &dhour, &dmin, &dsec, &dhsec) != 8)
        return AV_NOPTS_VALUE;

    //av_log(NULL, AV_LOG_ERROR, "%d %d %d %d [%s]\n", hour, min, sec, hsec, p);

    if (end_time) {
        dmin+= 60*dhour;
        dsec+= 60*dmin;
        *end_time = dsec*100+dhsec;
    }

    min+= 60*hour;
    sec+= 60*min;

    return sec*100+hsec;
}

static int event_cmp(ASSEvent *a, ASSEvent *b)
{
    return a->start_time - b->start_time;
}

static int read_header(AVFormatContext *s, AVFormatParameters *ap)
{
    int i, header_remaining;
    ASSContext *ass = s->priv_data;
    ByteIOContext *pb = s->pb;
    AVStream *st;
    int allocated[2]={0};
    uint8_t *p, **dst[2]={0};
    int pos[2]={0};

    st = av_new_stream(s, 0);
    if (!st)
        return -1;
    av_set_pts_info(st, 64, 1, 100);
    st->codec->codec_type = CODEC_TYPE_SUBTITLE;
    st->codec->codec_id= CODEC_ID_SSA;

    header_remaining= INT_MAX;
    dst[0] = &st->codec->extradata;
    dst[1] = &ass->event_buffer;
    while(!url_feof(pb)){
        uint8_t line[MAX_LINESIZE];

        get_line(pb, line, sizeof(line));

        if(!memcmp(line, "[Events]", 8))
            header_remaining= 2;
        else if(line[0]=='[')
            header_remaining= INT_MAX;

        i= header_remaining==0;

        if(i && get_pts(line, NULL) == AV_NOPTS_VALUE)
            continue;

        p = av_fast_realloc(*(dst[i]), &allocated[i], pos[i]+MAX_LINESIZE);
        if(!p)
            goto fail;
        *(dst[i])= p;
        memcpy(p + pos[i], line, strlen(line)+1);
        pos[i] += strlen(line);
        if(i) ass->event_count++;
        else {
            header_remaining--;
#if 0
            if (!header_remaining) {
                /* write the header into extrada section */
                int len = url_ftell(pb);
                st->codec->extradata = av_mallocz(len + 1);
                st->codec->extradata_size = len;
                url_fseek(pb, 0, SEEK_SET);
                get_buffer(pb, st->codec->extradata, len);
            }
#endif
        }
    }
    st->codec->extradata_size= pos[0];

    if(ass->event_count >= UINT_MAX / sizeof(*ass->event))
        goto fail;

    ass->event= av_malloc(ass->event_count * sizeof(*ass->event));
    p= ass->event_buffer;
    for(i=0; i<ass->event_count; i++){
        ass->event[i].text= p;
        ass->event[i].start_time= get_pts(p, &ass->event[i].end_time);
        while(*p && *p != '\n')
            p++;
        p++;
    }

    qsort(ass->event, ass->event_count, sizeof(*ass->event), event_cmp);

    return 0;

fail:
    read_close(s);

    return -1;
}

static int read_packet(AVFormatContext *s, AVPacket *pkt)
{
    ASSContext *ass = s->priv_data;
    ASSEvent *event;
    uint8_t *p, *end;

    if(ass->event_index >= ass->event_count)
        return AVERROR(EIO);

    event= ass->event + ass->event_index;
    p= event->text;

    end= strchr(p, '\n');
    av_new_packet(pkt, end ? end-p+1 : strlen(p));
    pkt->flags |= PKT_FLAG_KEY;
    pkt->pos= p - ass->event_buffer + s->streams[0]->codec->extradata_size;
    pkt->pts= pkt->dts= event->start_time;
    pkt->duration= event->end_time - event->start_time;
    pkt->stream_index = s->nb_streams - 1;
    memcpy(pkt->data, p, pkt->size);

    ass->event_index++;

    return 0;
}

static int read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
    ASSContext *ass = s->priv_data;

    ass->event_index = 0;
    while (ass->event[ass->event_index].start_time < timestamp &&
           ass->event_index < ass->event_count)
        ass->event_index++;

    return 0;
}

AVInputFormat ass_demuxer = {
    "ass",
    "SSA/ASS format",
    sizeof(ASSContext),
    probe,
    read_header,
    read_packet,
    read_close,
    read_seek,
};
