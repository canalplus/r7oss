/************************************************************************
Copyright (C) 2007, 2008, 2009, 2010 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.
 * V4L2 dvp output device driver for ST SoC display subsystems.
************************************************************************/

#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/videodev.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>

#include "dvb_module.h"
#include "backend.h"
#include "osdev_device.h"
#include "ksound.h"
#include "linux/dvb/stm_ioctls.h"
#include "pseudo_mixer.h"
#include "monitor_inline.h"
#include "st_relay.h"

#include "dvb_avr_audio.h"
#include "dvb_avr_audio_internals.h"

//#define ENABLE_ONE_OUTPUT_ONLY

/******************************
 * Private CONSTANTS and MACROS
 ******************************/

#define AUDIO_CAPTURE_THREAD_PRIORITY	40

#define lengthof(x) (sizeof(x) / sizeof(x[0]))

static snd_pcm_uframes_t AUDIO_PERIOD_FRAMES	= 1024;
static snd_pcm_uframes_t AUDIO_BUFFER_FRAMES	= 20480; /*10240*/

static int  ll_disable_detection    = false;
static int  ll_disable_spdifinfade  = false;
static int  ll_bitexactness_testing = false;

static int  ll_audioterm_timeout    = 1;

static int  ll_force_input = 0;
static int  ll_force_layout= 0;
static int  ll_force_amode = 0;
static int  ll_force_downsample  = false;

static int  ll_input_log_enable  = false;
static int  ll_output_log_enable = false;
static int  ll_audio_enable      = false;

#define AUDIO_DEFAULT_SAMPLE_RATE 	44100
#define AUDIO_CHANNELS 			2
#define AUDIO_CHANNELS_HDMI		8
#define AUDIO_SAMPLE_DEPTH 		32
#if defined (CONFIG_KERNELVERSION)
#	define AUDIO_CAPTURE_CARD 0
#	if defined(CONFIG_CPU_SUBTYPE_STX7200)
#		define AUDIO_CAPTURE_PCM 7
#	elif defined(CONFIG_CPU_SUBTYPE_STX7111)
#		define AUDIO_CAPTURE_PCM 3
#               warning Need to check this value
#	elif defined(CONFIG_CPU_SUBTYPE_STX7141)
#		define AUDIO_CAPTURE_PCM 3
#               warning Need to check this value
#	elif defined(CONFIG_CPU_SUBTYPE_STX7105)  || defined(CONFIG_CPU_SUBTYPE_STX7106) || defined(CONFIG_CPU_SUBTYPE_STX7108)
#		define AUDIO_CAPTURE_PCM 3
#               warning Need to check this value
#	elif defined(CONFIG_CPU_SUBTYPE_STB7100)
#		define AUDIO_CAPTURE_PCM 3
#	else
#		error Unsupported platform!
#	endif
#else /* STLinux 2.2 kernel */
#	define AUDIO_CAPTURE_CARD 6
#	define AUDIO_CAPTURE_PCM 0
#endif

#if SPDIFIN_API_VERSION < 0x090915
// bring in some constants not found in earlier versions of the headers
enum {
	SPDIFIN_IEC60958_DTS1_14 = SPDIFIN_IEC60958_DTS16 + 1,
	SPDIFIN_IEC60958_DTS1_16,
	SPDIFIN_IEC60958_DTS2_14,
	SPDIFIN_IEC60958_DTS2_16,
	SPDIFIN_IEC60958_DTS3_14,
	SPDIFIN_IEC60958_DTS3_16,
};
#endif

#define MID(x,y) ((x+y)/2)

typedef enum
{
    OUTPUT_PCM,      //< Raw PCM
    OUTPUT_IEC60958, //< PCM formatted for SPDIF output
    OUTPUT_FATPIPE,  //< FatPipe encoding
}  OutputEncoding_t;


// sysfs attribute types
enum attribute_id_e 
{
    ATTRIBUTE_ID_input_format,
    ATTRIBUTE_ID_decode_errors,
    ATTRIBUTE_ID_number_channels,
    ATTRIBUTE_ID_sample_frequency,
    ATTRIBUTE_ID_number_of_samples_processed,
    ATTRIBUTE_ID_supported_input_format,
    ATTRIBUTE_ID_supported_input_format_in_game_mode,
    ATTRIBUTE_ID_emergency_mute,
    ATTRIBUTE_ID_MAX
};

/// Number of 128 sample chunks taken for the 'limiter' gain processing module to perform a mute.
#define AVR_LIMITER_MUTE_RAMP_DOWN_PERIOD ( 128 / 128)

/// Number of 128 sample chunks taken for the 'limiter' gain processing module to perform an unmute.
#define AVR_LIMITER_MUTE_RAMP_UP_PERIOD   (1024 / 128)

#define AVR_LOW_LATENCY_BLOCK_SIZE_IN_MS 5

/******************************
 * GLOBAL VARIABLES
 ******************************/

// TODO remove this global variable once Gael has benchmarked the LL transformer and found the best compromise value
const int LLBlockSize = AVR_LOW_LATENCY_BLOCK_SIZE_IN_MS;


#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
// Converts V4L2 setting into ACC APIs. and vice/versa
const char ll_layout[3]= { 
  HDMI_LAYOUT0, HDMI_LAYOUT1, HDMI_HBRA2
};
#endif

// pcm reader 0 is 2 channels capable, while pcm reader 1 is 8 channels capable
static const char NbChannels[AVR_LOW_LATENCY_MAX_INPUT_CARDS] = 
{
    AUDIO_CHANNELS,
    AUDIO_CHANNELS_HDMI
};

/**
 * Lookup table to help the sysfs callbacks locate the context structure.
 * 
 * It really is rather difficult to get hold of a context variable from a sysfs callback. The sysfs module
 * does however provide a means for us to discover the stream id. Since the sysfs attribute is unique to
 * each stream we can use the id to index into a table and thus grab hold of a context variable.
 * 
 * Thus we are pretty much forced to use a global variable but at least it doesn't stop us being
 * multi-instance.
 */
static avr_v4l2_audio_handle_t *AudioContextLookupTable[16];

/******************************
 * FUNCTION PROTOTYPES
 ******************************/

extern struct class*        player_sysfs_get_player_class(void);
extern struct class_device* player_sysfs_get_class_device(void *playback, void*stream);
extern void player_sysfs_new_attribute_notification(struct class_device *stream_dev);
extern int player_sysfs_get_stream_id(struct class_device* stream_dev);

static enum eFsRange TranslateIntegerSamplingFrequencyToRange( unsigned int IntegerFrequency );
static enum eAccFsCode TranslateIntegerSamplingFrequencyToDiscrete( unsigned int IntegerFrequency );
static unsigned int TranslateDiscreteSamplingFrequencyToInteger( enum eAccFsCode DiscreteFrequency );

static const char * LookupAudioMode( enum eAccAcMode DiscreteMode );
static struct snd_pseudo_mixer_channel_assignment TranslateAudioModeToChannelAssignment(
    enum eAccAcMode AudioMode );
static enum eAccAcMode TranslateChannelAssignmentToAudioMode(
    struct snd_pseudo_mixer_channel_assignment ChannelAssignment );

static void MMECallbackLL(    MME_Event_t      Event,
                              MME_Command_t   *CallbackData,
                              void            *UserData );

static int SetGlobalTransformParameters(avr_v4l2_audio_handle_t *AudioContext,
                                         MME_LowlatencySpecializedGlobalParams_t * GlobalParams);

static void AvrAudioLLStateReset(avr_v4l2_audio_handle_t *AudioContext);
static int AvrAudioLLThreadCleanup(avr_v4l2_audio_handle_t *AudioContext);

static void FillOutLLSetGlobalCommand(avr_v4l2_audio_handle_t *AudioContext);

static void FillOutLLTransformCommand(avr_v4l2_audio_handle_t *AudioContext);

static unsigned int FillOutDevicePcmParameters( avr_v4l2_audio_handle_t *AudioContext,
                                                MME_LLChainPcmProcessingGlobalParams_t *PcmParams, 
						int dev_num,
                                                OutputEncoding_t *OutputEncoding,
                                                bool DeployEmergencyMute);

static int ProcessSpecializedTransformStatus( MME_LowlatencySpecializedTransformStatus_t *TransformStatus );

static int InitLLTransformer( avr_v4l2_audio_handle_t *AudioContext );

static int CreateSysFsStreamAndAttributes( avr_v4l2_audio_handle_t *AudioContext );


static int CheckPostMortemTransformerCapability( avr_v4l2_audio_handle_t *AudioContext );
static int CheckPostMortemStatus( avr_v4l2_audio_handle_t *AudioContext );
static int PostMortemSendInterrupt( int id );
static void IssuePostMortemDiagnostics( avr_v4l2_audio_handle_t *AudioContext );
static int CheckLLTransformerCapability( avr_v4l2_audio_handle_t *AudioContext );

static int LaunchLLSetGlobalCommand( avr_v4l2_audio_handle_t *AudioContext );

static int LaunchLLTransformCommand( avr_v4l2_audio_handle_t *AudioContext );

static void DumpMMECommand( MME_Command_t *CmdInfo_p );

static int CreateBufferPool(BufferPool_t * PoolPtr, BufferEntry_t * EntryPtr, unsigned int NbElt, unsigned int EltSize);

static void SendLogBuffers( avr_v4l2_audio_handle_t *AudioContext );

static void AvrAudioLLInstantApply(avr_v4l2_audio_handle_t *AudioContext);

/******************************
 * Utilities
 ******************************/

static inline s64 ktime_to_almost_ms(ktime_t t)
{
	/* We don't need too much accuracy so we'll cheat with a multiply and shift.
	 * This yields a very small overestimation. Least error would actually occur
         * if I multiplied by 134 but this would underestimate and seeing timed waits
         * come back too soon unsettles people.
         */
	return (135 * ktime_to_ns(t)) >> 27;
}

static void DiscloseLatencyTargets( avr_v4l2_audio_handle_t *AudioContext, const char *Why )
{
    avr_v4l2_shared_handle_t *SharedContext = AudioContext->SharedContext;

    DVB_TRACE("[%s] %dms latency with %lums Vsync compensation and [%d, %d, %d, %d] extra per output\n",
	      Why,
	      (int) SharedContext->target_latency + AudioContext->MasterLatencyWhenInitialized,
	      AudioContext->CompensatoryLatency,
	      SharedContext->mixer_settings.chain_latency[0],
	      SharedContext->mixer_settings.chain_latency[1],
	      SharedContext->mixer_settings.chain_latency[2],
	      SharedContext->mixer_settings.chain_latency[3]);
}

/******************************
 * Stringize functions
 ******************************/

static const char *LookupMmeCommandCode(MME_CommandCode_t Code)
{
    switch(Code) {
#define C(x) case x: return #x
    C(MME_SET_GLOBAL_TRANSFORM_PARAMS);
    C(MME_TRANSFORM);
    C(MME_SEND_BUFFERS);
#undef C
    default:
	return "UNKNOWN";
    }
}

/******************************
 * PES HEADER UTILS
 ******************************/


// PES header creation utilities
#define INVALID_PTS_VALUE               0x200000000ull
#define FIRST_PES_START_CODE            0xba
#define LAST_PES_START_CODE             0xbf
#define VIDEO_PES_START_CODE(C)         ((C&0xf0)==0xe0)
#define AUDIO_PES_START_CODE(C)         ((C&0xc0)==0xc0)
#define PES_START_CODE(C)               (VIDEO_PES_START_CODE(C) || AUDIO_PES_START_CODE(C) || \
                                        ((C>=FIRST_PES_START_CODE) && (C<=LAST_PES_START_CODE)))
#define PES_HEADER_SIZE 32 //bytes 
#define MAX_PES_PACKET_SIZE                     65400

#define PES_AUDIO_PRIVATE_HEADER_SIZE   4//16                                // consider maximum private header size.
#define PES_AUDIO_HEADER_SIZE           (32 + PES_AUDIO_PRIVATE_HEADER_SIZE)

#define PES_PRIVATE_STREAM1 0xbd
#define PES_PADDING_STREAM  0xbe
#define PES_PRIVATE_STREAM2 0xbf


typedef struct BitPacker_s
{
    unsigned char*      Ptr;                                    /* write pointer */
    unsigned int        BitBuffer;                              /* bitreader shifter */
    int                 Remaining;                              /* number of remaining in the shifter */
} BitPacker_t;


static void PutBits(BitPacker_t *ld, unsigned int code, unsigned int length)
{
    unsigned int bit_buf;
    int bit_left;

    bit_buf = ld->BitBuffer;
    bit_left = ld->Remaining;

    if (length < bit_left)
    {
        /* fits into current buffer */
        bit_buf = (bit_buf << length) | code;
        bit_left -= length;
    }
    else
    {
        /* doesn't fit */
        bit_buf <<= bit_left;
        bit_buf |= code >> (length - bit_left);
        ld->Ptr[0] = (char)(bit_buf >> 24);
        ld->Ptr[1] = (char)(bit_buf >> 16);
        ld->Ptr[2] = (char)(bit_buf >> 8);
        ld->Ptr[3] = (char)bit_buf;
        ld->Ptr   += 4;
        length    -= bit_left;
        bit_buf    = code & ((1 << length) - 1);
        bit_left   = 32 - length;
    }

    /* writeback */
    ld->BitBuffer = bit_buf;
    ld->Remaining = bit_left;
}


static void FlushBits(BitPacker_t * ld)
{
    ld->BitBuffer <<= ld->Remaining;
    while (ld->Remaining < 32)
    {
        *ld->Ptr++ = ld->BitBuffer >> 24;
        ld->BitBuffer <<= 8;
        ld->Remaining += 8;
    }
    ld->Remaining = 32;
    ld->BitBuffer = 0;
}


static int InsertPrivateDataHeader(unsigned char *data, int payload_size, int sampling_frequency)
{
    BitPacker_t ld2 = {data, 0, 32};
    //int HeaderLength = PES_AUDIO_PRIVATE_HEADER_SIZE;

    PutBits(&ld2, payload_size, 16); // PayloadSize
    PutBits(&ld2, 2, 4);             // ChannelAssignment  :: { 2 = Stereo}
    PutBits(&ld2, sampling_frequency, 4);   // SamplingFrequency  :: { 0 = 44 ; 1 = BD_48k ; 2 = 88k; 3 = 176k; 4 = BD_96k; 5 = BD=192k; 6 = 32k; 7 = 16k; 8 = 22k; 9 = 24k}
    PutBits(&ld2, 0, 2);             // BitsPerSample      :: { 0 = BD_reserved/32bit  ; 1 = BD_16bit ; 2 = BD_20bit; 3 = BD_24bits }
    PutBits(&ld2, 0, 1);             // StartFlag --> EmphasisFlag :: FALSE;
    PutBits(&ld2, 0, 5);             // reserved.

	FlushBits(&ld2);

	return (ld2.Ptr - data);
}


static int InsertPesHeader (unsigned char *data, int size, unsigned char stream_id, unsigned long long int pts, int pic_start_code)
{
    BitPacker_t ld2 = {data, 0, 32};

    if (size>MAX_PES_PACKET_SIZE)
        DVB_DEBUG("Packet bigger than 63.9K eeeekkkkk\n");

    PutBits(&ld2,0x0  ,8);
    PutBits(&ld2,0x0  ,8);
    PutBits(&ld2,0x1  ,8);  // Start Code
    PutBits(&ld2,stream_id ,8);  // Stream_id = Audio Stream
    //4
    PutBits(&ld2,size + 3 + (pts != INVALID_PTS_VALUE ? 5:0) + (pic_start_code ? (5/*+1*/) : 0),16); // PES_packet_length
    //6 = 4+2
    PutBits(&ld2,0x2  ,2);  // 10
    PutBits(&ld2,0x0  ,2);  // PES_Scrambling_control
    PutBits(&ld2,0x0  ,1);  // PES_Priority
    PutBits(&ld2,0x1  ,1);  // data_alignment_indicator
    PutBits(&ld2,0x0  ,1);  // Copyright
    PutBits(&ld2,0x0  ,1);  // Original or Copy
    //7 = 6+1

    if (pts!=INVALID_PTS_VALUE)
        PutBits(&ld2,0x2 ,2);
    else
        PutBits(&ld2,0x0 ,2);  // PTS_DTS flag

    PutBits(&ld2,0x0 ,1);  // ESCR_flag
    PutBits(&ld2,0x0 ,1);  // ES_rate_flag
    PutBits(&ld2,0x0 ,1);  // DSM_trick_mode_flag
    PutBits(&ld2,0x0 ,1);  // additional_copy_ingo_flag
    PutBits(&ld2,0x0 ,1);  // PES_CRC_flag
    PutBits(&ld2,0x0 ,1);  // PES_extension_flag
    //8 = 7+1

    if (pts!=INVALID_PTS_VALUE)
        PutBits(&ld2,0x5,8);
    else
        PutBits(&ld2,0x0 ,8);  // PES_header_data_length
    //9 = 8+1

    if (pts!=INVALID_PTS_VALUE)
    {
        PutBits(&ld2,0x2,4);
        PutBits(&ld2,(pts>>30) & 0x7,3);
        PutBits(&ld2,0x1,1);
        PutBits(&ld2,(pts>>15) & 0x7fff,15);
        PutBits(&ld2,0x1,1);
        PutBits(&ld2,pts & 0x7fff,15);
        PutBits(&ld2,0x1,1);
    }
    //14 = 9+5

    if (pic_start_code)
    {
        PutBits(&ld2,0x0 ,8);
        PutBits(&ld2,0x0 ,8);
        PutBits(&ld2,0x1 ,8);  // Start Code
        PutBits(&ld2,pic_start_code & 0xff ,8);  // 00, for picture start
        PutBits(&ld2,(pic_start_code >> 8 )&0xff,8);  // For any extra information (like in mpeg4p2, the pic_start_code)
        //14 + 4 = 18
    }

    FlushBits(&ld2);

    return (ld2.Ptr - data);
}


/******************************
 * SYSFS TOOLS
 ******************************/

/*{{{  store_xxx_attribute*/
static ssize_t store_decode_errors(struct class_device *class_dev, const char *buf, size_t count)
{
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    //    int LocalDecodeErrors;

/*     sscanf(buf, "%i", &LocalDecodeErrors); */
/*     if (LocalDecodeErrors != 0) */
/*         count = sprintf((char*)buf, "Unable to set decode errors other than to a zero value\n", Decode); */
/*     else */
/*     { */
    down(&AudioContext->DecoderStatusSemaphore);
    AudioContext->LLDecoderStatus.DecodeErrorCount = 0;
    up(&AudioContext->DecoderStatusSemaphore);
/*     } */
    return 0;
}
/*}}}  */

static const char *LookupSpdifInState(int state)
{
	switch (state) {
        case SPDIFIN_STATE_RESET: return "RESET";
	case SPDIFIN_STATE_PCM_BYPASS: return "PCM_BYPASS";
        case SPDIFIN_STATE_COMPRESSED_BYPASS: return "COMPRESSED_BYPASS";
	case SPDIFIN_STATE_UNDERFLOW: return "UNDERFLOW";
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128		
	case SPDIFIN_STATE_COMPRESSED_MUTE: return "COMPRESSED_MUTE";
#endif		
	default: return "INVALID";
	}
}

static const char *TranslatePcToEncoding(int pc)
{
	switch (pc) {
#define C(x) case SPDIFIN_ ## x: return #x;
#define D(x, y) case SPDIFIN_ ## x: return y;
        C(NULL_DATA_BURST);
	C(AC3);
	C(PAUSE_BURST);
	C(MP1L1);
	C(MP1L2L3);
	C(MP2MC);
	C(MP2AAC);
	C(MP2L1LSF);
	C(MP2L2LSF);
	C(MP2L3LSF);
	C(DTS1);
	C(DTS2);
	C(DTS3);
	C(ATRAC);
	C(ATRAC2_3);
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	C(DTS4);
	D(DDPLUS, "DD+");
	D(DTRUEHD, "Dolby TrueHD");
#endif
	D(IEC60958, "PCM");
	C(IEC60958_DTS14);
	C(IEC60958_DTS16);
	D(IEC60958_DTS1_14, "IEC60958_DTS14");
	D(IEC60958_DTS1_16, "IEC60958_DTS16");
	D(IEC60958_DTS2_14, "IEC60958_DTS14");
	D(IEC60958_DTS2_16, "IEC60958_DTS16");
	D(IEC60958_DTS3_14, "IEC60958_DTS14");
	D(IEC60958_DTS3_16, "IEC60958_DTS16");

	default:
		return "UNKNOWN";
#undef C
#undef D
	}
}

/* creation of show_xx methods */
static ssize_t show_input_format(struct class_device *class_dev, char *buf)
{
	avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
	ssize_t attr_size;

	/* short circuit result if the thread is 'stuck' waiting to be asked to stop */
	if (AudioContext->ThreadWaitingForStop)
		return sprintf(buf, "WAITING_FOR_STOP\n");
    
	down(&AudioContext->DecoderStatusSemaphore);

	attr_size = sprintf(buf, "%s", TranslatePcToEncoding(AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC));
    
#if SPDIFIN_API_VERSION >= 0x100319
	if ((AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC == SPDIFIN_DTS1) ||
	    (AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC == SPDIFIN_DTS2) ||
	    (AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC == SPDIFIN_DTS3) ||
	    (AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC == SPDIFIN_DTS4)) {

		switch (AudioContext->LLDecoderStatus.CurrentSpdifStatus.Display & 0x3) {
		case DTS:
#ifndef DISABLE_BAD_X96_EXTENSION_REPORTING
		{
			/* firmware doesn't correctly report the precence of X96 for some
			 * stream types. however we can infer its presence from the reported
			 * sampling frequency.
			 */
			int sfreq = TranslateDiscreteSamplingFrequencyToInteger(
				        (enum eAccFsCode) AudioContext->LLDecoderStatus.CurrentSpdifStatus.SamplingFreq);
			if (sfreq > 48000) {
				DVB_TRACE("Deploying DTS 96/24 reporting workaround (sfreq %d)\n", sfreq);
				attr_size += sprintf(buf+attr_size, " 96/24");
				break;
			}
		}
#endif

			switch ((AudioContext->LLDecoderStatus.CurrentSpdifStatus.Display >> 2) & 0x3) {
			case DTS_51:
				/* do nothing */
				break;
			case DTS_61_MATRIX:
				attr_size += sprintf(buf+attr_size, " ES 6.1 Matrix");
				break;
			case DTS_61_DISCRETE:
				attr_size += sprintf(buf+attr_size, " ES 6.1 Discrete");
				break;
			case DTS_71_DISCRETE:
				attr_size += sprintf(buf+attr_size, " ES 8ch Discrete");
				break;
			}
			break;
		case DTSHD_96k:
			attr_size += sprintf(buf+attr_size, " 96/24");
			break;
		case DTSHD_HR:
			attr_size += sprintf(buf+attr_size, " HD HR");
			break;
		case DTSHD_MA:
			attr_size += sprintf(buf+attr_size, " HD MA");
			break;
		}
	}
#elif SPDIFIN_API_VERSION >= 0x090909
	if ((AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC == SPDIFIN_DTS4) &&
	    (AudioContext->LLDecoderStatus.CurrentSpdifStatus.IsLossless))
		attr_size += sprintf(buf+attr_size, " HD MA");
#else
	/* do nothing */
#endif

#if SPDIFIN_API_VERSION >= 0x100415
	if ((AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC == SPDIFIN_IEC60958) &&
	    (AudioContext->LLDecoderStatus.CurrentSpdifStatus.Silence))
		attr_size += sprintf(buf+attr_size, " SILENT");
#endif

	attr_size += sprintf(buf+attr_size, "\n");

	up(&AudioContext->DecoderStatusSemaphore);
	return attr_size;
}

/* creation of show_xx methods */
static ssize_t show_decode_errors(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    ssize_t attr_size;
    
    down(&AudioContext->DecoderStatusSemaphore);
    attr_size = sprintf(buf, "%i\n", AudioContext->LLDecoderStatus.DecodeErrorCount);
    up(&AudioContext->DecoderStatusSemaphore);
    return(attr_size);
}

static struct {
    int front;
    int back;
    int lfe;
    int misc;
} SpeakerLocationLookupTable[SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED+1] = {
#define E(t, f, b, l, m) [SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## t] = { f, b, l, m }
    E(DEFAULT,			0, 0, 0, 0), // DEFAULT is a special case that must be handled in code
    E(LT_RT,			2, 0, 0, 0),
    E(LPLII_RPLII,		2, 0, 0, 0),
    E(CNTRL_CNTRR,		2, 0, 0, 0),
    E(LHIGH_RHIGH,		2, 0, 0, 0),
    E(LWIDE_RWIDE,		2, 0, 0, 0),
    E(LRDUALMONO,		2, 0, 0, 0),
    E(CNTR_0,			1, 0, 0, 0),
    E(0_LFE1,			0, 0, 1, 0),
    E(0_LFE2,			0, 0, 1, 0),
    E(CHIGH_0,			1, 0, 0, 0),
    E(CLOWFRONT_0,		1, 0, 0, 0),
    E(CNTR_CSURR,		1, 1, 0, 0),
    E(CNTR_CHIGH,		1, 0, 0, 0),
    E(CNTR_TOPSUR,		1, 0, 0, 1),
    E(CNTR_CHIGHREAR,		1, 1, 0, 0),
    E(CNTR_CLOWFRONT,   	2, 0, 0, 0),
    E(CHIGH_TOPSUR,     	1, 0, 0, 1),
    E(CHIGH_CHIGHREAR, 		 1, 1, 0, 0),
    E(CHIGH_CLOWFRONT,  	2, 0, 0, 0),
    E(CNTR_LFE2,        	1, 0, 1, 0),
    E(CHIGH_LFE1,       	1, 0, 1, 0),
    E(CHIGH_LFE2,       	1, 0, 1, 0),
    E(CLOWFRONT_LFE1,   	1, 0, 1, 0),
    E(CLOWFRONT_LFE2,  	 	1, 0, 1, 0),
    E(LSIDESURR_RSIDESURR,	0, 0, 0, 2),
    E(LHIGHSIDE_RHIGHSIDE,      0, 0, 0, 2),
    E(LDIRSUR_RDIRSUR,          0, 2, 0, 0),
    E(LHIGHREAR_RHIGHREAR,      0, 2, 0, 0),
    E(CSURR_0,                  0, 1, 0, 0),
    E(TOPSUR_0,                 0, 0, 0, 1),
    E(CSURR_TOPSUR,             0, 1, 0, 1),
    E(CSURR_CHIGH,              1, 0, 1, 0),
    E(CSURR_CHIGHREAR,          1, 0, 1, 0),
    E(CSURR_CLOWFRONT,          1, 0, 1, 0),
    E(CSURR_LFE1,               0, 1, 1, 0),
    E(CSURR_LFE2,               0, 1, 1, 0),
    E(CHIGHREAR_0,              0, 1, 0, 0),
    E(DSTEREO_LsRs,             2, 0, 0, 0),
    E(NOT_CONNECTED,            0, 0, 0, 0),
#undef E
};

void count_speakers(struct snd_pseudo_mixer_channel_assignment channel_assignment,
		    int *pfront, int *pback, int *plfe, int *pmisc)
{
    int i;
    int front = 0, back = 0, lfe = 0, misc = 0;
    
    for (i=0; i<5; i++) {
	enum snd_pseudo_mixer_channel_pair pair = 0 == i ? channel_assignment.pair0 :
	                                          1 == i ? channel_assignment.pair1 :
	                                          2 == i ? channel_assignment.pair2 :
	                                          3 == i ? channel_assignment.pair3 :
	                                                   channel_assignment.pair4;

	if (SND_PSEUDO_MIXER_CHANNEL_PAIR_DEFAULT == pair) {
	    // treat specially (intepreted differently based on position with a pair)
	    switch (i) {
	    case 0:
		front += 2;
		break;
	    case 1:
		front++;
		lfe++;
		break;
	    case 2:
	    case 3:
		back += 2;
		break;
	    default:
		break;
	    }
	} else if (pair <= SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED) {
	    front += SpeakerLocationLookupTable[pair].front;
	    back += SpeakerLocationLookupTable[pair].back;
	    lfe += SpeakerLocationLookupTable[pair].lfe;
	    misc += SpeakerLocationLookupTable[pair].misc;
	} else {
	    DVB_ERROR("Internal inconsistancy\n");
	}
    }

    *pfront = front;
    *pback = back;
    *plfe = lfe;
    *pmisc = misc;
}


#if SPDIFIN_API_VERSION >= 0x090623
void report_audio(tMMESpdifinStatus * spdifin_status,  int *pfront, int *pback, int *plfe, int *pmisc)
{
  *pfront = spdifin_status->NbFront;
  *pback  = spdifin_status->NbRear;
  *plfe   = spdifin_status->LFE;
  *pmisc  = 0;
}
#endif


/* creation of show_xx methods */
static ssize_t show_number_channels(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    enum eAccAcMode AudioMode;
    struct snd_pseudo_mixer_channel_assignment ChannelAssignment;
    int firmware_front, firmware_back, firmware_lfe, firmware_misc;
    int front, back, lfe, misc, nchan;

    // if we read the value exactly once then we don't need any locking...
    AudioMode = AudioContext->LLDecoderStatus.CurrentDecoderAudioMode;
    
    if (ACC_MODE_1p1 == AudioMode)
	return sprintf(buf, "1+1/0.0\n");

#if SPDIFIN_API_VERSION >= 0x090623
    report_audio(&AudioContext->LLDecoderStatus.CurrentSpdifStatus,
		 &firmware_front, &firmware_back, &firmware_lfe, &firmware_misc);

    nchan = front + back + lfe + misc;
    if (nchan <= 0 || nchan > 8) {
	/* firmware is talking nonsense (i.e. this codec doesn't support channel reporting) */
	ChannelAssignment = TranslateAudioModeToChannelAssignment(AudioMode);
	count_speakers(ChannelAssignment, &front, &back, &lfe, &misc);
    } else {
	front = firmware_front;
        back = firmware_back;
        lfe = firmware_lfe;
        misc = firmware_misc;
    }
#else
    /* firmware doesn't support channel reporting */
    firmware_front = firmware_back = firmware_lfe = firmware_misc = 0;

    ChannelAssignment = TranslateAudioModeToChannelAssignment(AudioMode);
    count_speakers(ChannelAssignment, &front, &back, &lfe, &misc);
#endif  
    DVB_TRACE("%s (0x%02x) %d/%d+%d.%d -> %d/%d.%d\n",
	      LookupAudioMode(AudioMode), AudioMode,
	      firmware_front, firmware_back, firmware_misc, firmware_lfe, front, back+misc, lfe);
    return sprintf(buf, "%d/%d.%d\n", front, back+misc, lfe);
}

/* creation of show_xx methods */
static ssize_t show_sample_frequency(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    ssize_t attr_size;

#if SPDIFIN_API_VERSION >= 0x090623
    enum eAccFsCode fs = (enum eAccFsCode) AudioContext->LLDecoderStatus.CurrentSpdifStatus.SamplingFreq;
#else
    enum eAccFsCode fs = (enum eAccFsCode) AudioContext->LLDecoderStatus.CurrentDecoderFrequency;
#endif  
    
    down(&AudioContext->DecoderStatusSemaphore);
    attr_size = sprintf(buf, "%i\n", TranslateDiscreteSamplingFrequencyToInteger(fs));
    up(&AudioContext->DecoderStatusSemaphore);
    return(attr_size);
}

static bool is_inconsistant(avr_v4l2_audio_handle_t *AudioContext)
{
#if 0
    // We currently don't consider mis-matched sampling frequencies to be inconsistant (because
    // the firmware will play them anyway, albeit at an odd frequency). 
#if SPDIFIN_API_VERSION >= 0x090623
    {
        int fs1 = TranslateDiscreteSamplingFrequencyToInteger(AudioContext->LLDecoderStatus.CurrentSpdifStatus.SamplingFreq);
	int fs2 = TranslateDiscreteSamplingFrequencyToInteger(AudioContext->LLDecoderStatus.CurrentDecoderFrequency);

	if (0 == fs1 || 0 == fs2)
		return true;

	while (fs1 > fs2) fs2 <<= 1;
	while (fs2 > fs1) fs1 <<= 1;

	if (fs1 != fs2)
		return true;
    }
#endif
#endif

    return false;
}

static bool is_supported(avr_v4l2_audio_handle_t *AudioContext)
{
    MME_LxAudioDecoderHDInfo_t *Capability = &AudioContext->AudioDecoderTransformCapability;

    switch (AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC)
    {
        case SPDIFIN_AC3:
        case SPDIFIN_DDPLUS:
        case SPDIFIN_DTRUEHD:
	    return ACC_DECODER_CAPABILITY_EXT_FLAGS(Capability, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_DD);

        case SPDIFIN_DTS1:
        case SPDIFIN_DTS2:
        case SPDIFIN_DTS3:
        case SPDIFIN_DTS4:
        case SPDIFIN_IEC60958_DTS14:
        case SPDIFIN_IEC60958_DTS16:
        case SPDIFIN_IEC60958_DTS1_14:
        case SPDIFIN_IEC60958_DTS1_16:
        case SPDIFIN_IEC60958_DTS2_14:
        case SPDIFIN_IEC60958_DTS2_16:
        case SPDIFIN_IEC60958_DTS3_14:
        case SPDIFIN_IEC60958_DTS3_16:
            return ACC_DECODER_CAPABILITY_EXT_FLAGS(Capability, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_DTS);

        case SPDIFIN_MP2AAC:
	    return ACC_DECODER_CAPABILITY_EXT_FLAGS(Capability, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_MPG);

        case SPDIFIN_IEC60958:
        case SPDIFIN_NULL_DATA_BURST:
        case SPDIFIN_PAUSE_BURST:
	    return true;

        case SPDIFIN_MP1L1:
        case SPDIFIN_MP1L2L3:
        case SPDIFIN_MP2MC:
        case SPDIFIN_MP2L1LSF:
        case SPDIFIN_MP2L2LSF:
        case SPDIFIN_MP2L3LSF:
        case SPDIFIN_ATRAC:
        case SPDIFIN_ATRAC2_3:
        default:
	    break;
    }

    return false;
}

static bool is_supported_in_game_mode(avr_v4l2_audio_handle_t *AudioContext)
{
    int sfreq = TranslateDiscreteSamplingFrequencyToInteger(
                    (enum eAccFsCode) AudioContext->LLDecoderStatus.CurrentDecoderFrequency);
 
    switch (AudioContext->LLDecoderStatus.CurrentSpdifStatus.PC) {
    case SPDIFIN_DTRUEHD:
    case SPDIFIN_DTS3:
    case SPDIFIN_DTS4:
    case SPDIFIN_IEC60958_DTS14:
    case SPDIFIN_IEC60958_DTS16:
    case SPDIFIN_IEC60958_DTS3_14:
    case SPDIFIN_IEC60958_DTS3_16:
	return false;

    case SPDIFIN_AC3:
    case SPDIFIN_DDPLUS:
    case SPDIFIN_DTS1:
    case SPDIFIN_DTS2:
    case SPDIFIN_MP2AAC:
    case SPDIFIN_IEC60958_DTS1_14:
    case SPDIFIN_IEC60958_DTS1_16:
    case SPDIFIN_IEC60958_DTS2_14:
    case SPDIFIN_IEC60958_DTS2_16:
	if (sfreq < 44100)
	    return false;
	break;

    default:
	break;
    }

    return is_supported(AudioContext);
}

/* creation of show_xx methods */
static ssize_t show_supported_input_format(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    bool IsInconsistant;
    bool IsSupported;
    
    down(&AudioContext->DecoderStatusSemaphore);

    IsSupported = is_supported(AudioContext);
    IsInconsistant = is_inconsistant(AudioContext);
    
    up(&AudioContext->DecoderStatusSemaphore);
    return(sprintf(buf, "%i\n", IsSupported && !IsInconsistant));
}

/* creation of show_xx methods */
static ssize_t show_supported_input_format_in_game_mode(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    bool IsSupported;
    bool IsInconsistant;

    down(&AudioContext->DecoderStatusSemaphore);

    IsSupported = is_supported_in_game_mode(AudioContext);
    IsInconsistant = is_inconsistant(AudioContext);

    up(&AudioContext->DecoderStatusSemaphore);
    return(sprintf(buf, "%i\n", IsSupported && !IsInconsistant));
}

/* creation of show_xx methods */
static ssize_t show_number_of_samples_processed(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    ssize_t attr_size;
    
    down(&AudioContext->DecoderStatusSemaphore);
    attr_size = sprintf(buf, "%lld\n", AudioContext->LLDecoderStatus.NumberOfProcessedSamples);

    up(&AudioContext->DecoderStatusSemaphore);
    return(attr_size);
}

static ssize_t store_emergency_mute(struct class_device *class_dev, const char *buf, size_t count)
{
	avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    
	if (count >= 1) {
		switch (buf[0]) {
		case '0':
		case 'n':
		case 'N':
			AvrAudioSetEmergencyMuteReason(AudioContext, EMERGENCY_MUTE_REASON_NONE);
			break;
		case '1':
		case 'y':
		case 'Y':
			AvrAudioSetEmergencyMuteReason(AudioContext, EMERGENCY_MUTE_REASON_USER);
			break;
		default:
			return -EINVAL;
		}
	}
	
	return count;
}

static ssize_t show_emergency_mute(struct class_device *class_dev, char *buf)
{
    // get the private context 
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
    char * str_p;
    
    switch (AudioContext->EmergencyMuteReason)
    {
        case EMERGENCY_MUTE_REASON_NONE:
            str_p = "None   "; break;
        case EMERGENCY_MUTE_REASON_USER:
            str_p = "User"; break;
        case EMERGENCY_MUTE_REASON_ACCELERATED:
            str_p = "Accelerated"; break;
        case EMERGENCY_MUTE_REASON_SAMPLE_RATE_CHANGE:
            str_p = "SampleRate Change"; break;
        case EMERGENCY_MUTE_REASON_ERROR:
            str_p = "Error"; break;
        default:
            str_p = "This status does not exist\n"; break;
    }
    return(sprintf(buf, "%s\n", str_p));
}

static ssize_t store_post_mortem(struct class_device *class_dev, const char *buf, size_t count)
{
	avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;
	ssize_t res;
    
	if (count >= 1) {
		switch (buf[0]) {
		case 'c': // (c)rash
			DVB_TRACE("Forcing firmware post-mortem\n");
			AudioContext->PostMortemForce = true;
			AvrAudioLLInstantApply(AudioContext);
			break;
		case 'i': // (i)nterrupt
		case '3':
			DVB_TRACE("Raising (unhandled) interrupt on CPU #3\n");
			res = PostMortemSendInterrupt(3);
			if (res < 0)
				return res;
			break;
		case '4': // you can probably figure this one out yourself
			DVB_TRACE("Raising (unhandled) interrupt on CPU #4\n");
			res = PostMortemSendInterrupt(4);
			if (res < 0)
				return res;
			break;
		default:
			return -EINVAL;
		}
	}
	
	return count;
}

ssize_t show_post_mortem(struct class_device *class_dev, char *buf)
{
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) class_dev->class_data;

    /* make sure that if a post-mortem is pending we issue it */
    if (!AudioContext->PostMortemAlreadyIssued) {
	    if (0 != CheckPostMortemStatus(AudioContext)) {
		    DVB_TRACE("Firmware issued post-mortem but drivers have not yet observed this\n");
		    IssuePostMortemDiagnostics(AudioContext);
	    }
    }

    return sprintf(buf, "%c\n", AudioContext->PostMortemAlreadyIssued ? 'y' : 'n');
}

/* creation of class_device_attr_xx attributes */

static CLASS_DEVICE_ATTR(input_format, S_IRUGO, show_input_format, NULL);
static CLASS_DEVICE_ATTR(supported_input_format, S_IRUGO, show_supported_input_format, NULL);
static CLASS_DEVICE_ATTR(supported_input_format_in_game_mode, S_IRUGO, show_supported_input_format_in_game_mode, NULL);
static CLASS_DEVICE_ATTR(decode_errors, S_IWUSR | S_IRUGO, show_decode_errors, store_decode_errors);
static CLASS_DEVICE_ATTR(sample_frequency, S_IRUGO, show_sample_frequency, NULL);
static CLASS_DEVICE_ATTR(number_channels, S_IRUGO, show_number_channels, NULL);
static CLASS_DEVICE_ATTR(number_of_samples_processed, S_IRUGO, show_number_of_samples_processed, NULL);
static CLASS_DEVICE_ATTR(emergency_mute, S_IWUSR | S_IRUGO, show_emergency_mute, store_emergency_mute);
static CLASS_DEVICE_ATTR(post_mortem, S_IWUSR | S_IRUGO, show_post_mortem, store_post_mortem);

static const struct class_device_attribute * attribute_array[] = 
{
    &class_device_attr_input_format,
    &class_device_attr_supported_input_format,
    &class_device_attr_supported_input_format_in_game_mode,
    &class_device_attr_decode_errors,    
    &class_device_attr_sample_frequency,    
    &class_device_attr_number_channels,
    &class_device_attr_number_of_samples_processed,
    &class_device_attr_emergency_mute,
    &class_device_attr_post_mortem
};

static const int g_attrCount = sizeof(attribute_array)/sizeof(struct class_device_attribute *);

static int CreateSysFsStreamAndAttributes( avr_v4l2_audio_handle_t *AudioContext )
{
    // SYSFS related stuff...
    // create the stream0 and attributes sysfs elements...
    int result, attr_idx;
    struct class_device * stream_class_device = &AudioContext->StreamClassDevice;
    struct class *        player2_class;
    playback_handle_t     playerplayback;
    
    player2_class = player_sysfs_get_player_class();
    
    memset(stream_class_device, 0, sizeof(struct class_device));
    
    // retrieve the already created playback class device
    result = DvbPlaybackGetPlayerEnvironment (AudioContext->DeviceContext->Playback,
                                           &playerplayback);
    if (result < 0) 
    {
        DVB_ERROR("PlaybackGetPlayerEnvironment failed\n");
        return -ENODEV;
    }
    
    AudioContext->PlaybackClassDevice = player_sysfs_get_class_device(playerplayback, NULL);
    
    if (AudioContext->PlaybackClassDevice == NULL)
    {
        DVB_ERROR("Playback class device does not exist\n");
        return -ENODEV;
    }
    
    stream_class_device->devt = MKDEV(0,0);
    stream_class_device->class = player2_class;
    stream_class_device->dev = NULL;
    stream_class_device->parent = AudioContext->PlaybackClassDevice;
    stream_class_device->class_data = AudioContext;
    // TODO modify this with audio0 in the future...
    snprintf(stream_class_device->class_id, BUS_ID_SIZE, "audio%d", 0);
    
    result = class_device_register(stream_class_device);
    
    if (result || IS_ERR(stream_class_device))
    {
        DVB_ERROR("Unable to create avr stream_class_device (%d)\n", (int)stream_class_device);
        return -ENODEV;
    }
    
    // now create all the attributes...
    for (attr_idx = 0; attr_idx < g_attrCount; attr_idx++)
    {
        /* Create attribute */
        result  = class_device_create_file(&AudioContext->StreamClassDevice, attribute_array[attr_idx]);
        if (result)
        {
            DVB_ERROR("class_device_create_file failed (%d) - attr %d\n", result, attr_idx);
            return -ENODEV;
        }
    }

    player_sysfs_new_attribute_notification(NULL);
    return 0;
}

static int AvrAudioSysfsCreateEmergencyMute (avr_v4l2_audio_handle_t *AudioContext)
{
    int Result 						= 0;
    const struct class_device_attribute* attribute 	= NULL;
    playback_handle_t playerplayback 			= NULL;
    stream_handle_t playerstream 			= NULL;
    int streamid;

    attribute = &class_device_attr_emergency_mute;

    Result = DvbStreamGetPlayerEnvironment (AudioContext->DeviceContext->AudioStream, &playerplayback, &playerstream);
    if (Result < 0) {
	DVB_ERROR("StreamGetPlayerEnvironment failed\n");
	return -1;
    }

    AudioContext->EmergencyMuteClassDevice = player_sysfs_get_class_device(playerplayback, playerstream);
    if (AudioContext->EmergencyMuteClassDevice == NULL) {
    	DVB_ERROR("get_class_device failed -> cannot create attribute \n");
        return -1;
    }
    
    streamid = player_sysfs_get_stream_id(AudioContext->EmergencyMuteClassDevice);
    if (streamid < 0 || streamid >= ARRAY_SIZE(AudioContextLookupTable)) {
	DVB_ERROR("streamid out of range -> cannot create attribute\n");
	return -1;
    }

    AudioContextLookupTable[streamid] = AudioContext;

    Result  = class_device_create_file(AudioContext->EmergencyMuteClassDevice, attribute);
    if (Result) {
	DVB_ERROR("Error in %s: class_device_create_file failed (%d)\n", __FUNCTION__, Result);
        return -1;
    }

    return 0;
}

/* static ssize_t AvrAudioSysfsShowEmergencyMute (struct class_device *class_dev, char *buf) */
/* { */
/*     int streamid = player_sysfs_get_stream_id(class_dev); */
/*     avr_v4l2_audio_handle_t *AudioContext; */
/*     const char *value; */
    
/*     BUG_ON(streamid < 0 || streamid >= ARRAY_SIZE(AudioContextLookupTable)); */
/*     AudioContext = AudioContextLookupTable[streamid]; */

/*     switch (AudioContext->EmergencyMuteReason) */
/*     { */
/*         case 0: */
/*     	value = "None"; break; */
/*         case 1: */
/*     	value = "User"; break; */
/*         case 2: */
/*     	value = "Accelerated"; break; */
/*         case 3: */
/*     	value = "Sample Rate Change"; break; */
/*         case 4: */
/*     	value = "Error"; break; */
/*         default: */
/*     	return sprintf(buf, "This status does not exist\n"); break; */
/*     } */
/*     return sprintf(buf, "%s\n", value); */
/* } */


/******************************
 * INJECTOR THREAD AND RELATED TOOLS
 ******************************/

static int AvrAudioXrunRecovery(ksnd_pcm_t *handle, int err)
{
    if (err == -EPIPE) {	/* under-run */

	/*err = snd_pcm_prepare(handle); */
	err = ksnd_pcm_prepare(handle);
	if (err < 0)
		DVB_ERROR("Can't recovery from underrun, prepare failed: %d\n", err);
	return 0;
    }

    else if (err == -ESTRPIPE) {
#if 0
	while ((err = snd_pcm_resume(handle)) == -EAGAIN) sleep(1);	/* wait until the suspend flag is released */

	if (err < 0) {
	    err = snd_pcm_prepare(handle);
	    if (err < 0)
		DVB_ERROR("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
	}
#endif
	BUG();
	return -1;
    }
    else
    {
	DVB_ERROR("This error code (err = %d) is not handled\n", err);
	return -1;
    }

    return err;
}


static int AvrAudioCalculateSampleRate (avr_v4l2_audio_handle_t *AudioContext,
	                     snd_pcm_sframes_t initial_delay, snd_pcm_sframes_t final_delay,
	    		     unsigned long long initial_time, unsigned long long final_time)
{
    int ret = 0;
    
    unsigned int SampleRate;
    unsigned int DiscreteSampleRate;

//    if (initial_time == final_time)
//    {
//	DVB_ERROR("Initial and final time are equal\n");
//	return -EINVAL;
//    }

    SampleRate = (((unsigned long long)(AUDIO_PERIOD_FRAMES - initial_delay + final_delay)) * 1000000ull) / (final_time - initial_time);
    //DVB_DEBUG("Calculated SampleRate = %llu\n", SampleRate);

    
    if (SampleRate <= MID(16000,  22050))  {
	SampleRate = 16000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_16000;
    }
    else if (SampleRate <= MID(22050,  24000))  {
	SampleRate = 22050;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_22050;
    }
    else if (SampleRate <= MID(24000,  32000))  {
	SampleRate = 24000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_24000;
    }
    else if (SampleRate <= MID(32000,  44100))  {
	SampleRate = 32000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_32000;
    }
    else if (SampleRate <= MID(44100,  48000))  {
	SampleRate = 44100;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_44100;
    }
    else if (SampleRate <= MID(48000,  64000))  {
	SampleRate = 48000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_48000;
    }
    else if (SampleRate <= MID(64000,  88200))  {
	SampleRate = 64000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_64000;
    }
    else if (SampleRate <= MID(88200,  96000))  {
	SampleRate = 88200;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_88200;
    }
    else if (SampleRate <= MID(96000,  128000)) {
	SampleRate = 96000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_96000;
    }
    else if (SampleRate <= MID(128000,  176400)) {
	SampleRate = 128000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_128000;
    }
    else if (SampleRate <= MID(176400, 192000)) {
	SampleRate = 176000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_176400;
    }
    else {
	SampleRate = 192000;
	DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_192000;
    }

    AudioContext->SampleRate = SampleRate;
    AudioContext->DiscreteSampleRate = DiscreteSampleRate;
    
    if (AudioContext->PreviousSampleRate != SampleRate)
    {
	// ok, if overrrun or underrun, then I have to keep the same frequency I had before, as
	// the new calculated one is not going to be reliable
	if (AudioContext->EmergencyMuteReason == EMERGENCY_MUTE_REASON_ERROR)
		return 0;

        ret = AvrAudioSetEmergencyMuteReason(AudioContext, EMERGENCY_MUTE_REASON_SAMPLE_RATE_CHANGE);
        if (ret < 0) {
		DVB_ERROR("dvp_deploy_emergency_mute failed\n");
		return -EINVAL;
        }

        DVB_DEBUG("New SampleRate = %u\n", SampleRate);
        AudioContext->PreviousSampleRate = SampleRate;

	if ((SampleRate == 64000) || (SampleRate == 128000))
	{
	    DVB_ERROR("SampleRate not supported \n");
	    return 1;
	}
        {
           unsigned int Parameters[MONITOR_PARAMETER_COUNT];
           memset (Parameters, 0, sizeof (Parameters));
           Parameters[0]        = SampleRate;
           MonitorSignalEvent (MONITOR_EVENT_AUDIO_SAMPLING_RATE_CHANGE, Parameters, "New sample rate detected");
        }
    }

    if ((SampleRate == 64000) || (SampleRate == 128000))
	    return 1;

    return 0;
}


static int AvrAudioSendPacketToPlayer(avr_v4l2_audio_handle_t *AudioContext,
		                      snd_pcm_uframes_t capture_frames, snd_pcm_uframes_t capture_offset,
		                      const snd_pcm_channel_area_t *capture_areas,
		                      snd_pcm_sframes_t DelayInSamples, unsigned long long time_average)
{
    unsigned char PesHeader[PES_AUDIO_HEADER_SIZE];
    int Result	 				= 0;
    unsigned int HeaderLength 			= 0;
    unsigned long long audioPts			= 0;
    unsigned long long DelayInMicroSeconds 	= 0;
    unsigned long long time_absolute 		= 0;
    unsigned long long NativeTime 		= 0;
    size_t AudioDataSize 			= 0;
    void* AudioData 				= NULL;

    // Build audio data

    AudioData 		= capture_areas[0].addr + capture_areas[0].first / 8 + capture_offset * capture_areas[0].step / 8;
    AudioDataSize 	= capture_frames * (AUDIO_SAMPLE_DEPTH * AUDIO_CHANNELS / 8);

    // Calculate audioPts

    DelayInMicroSeconds	= (DelayInSamples * 1000000ull) / AudioContext->SampleRate;
    time_absolute	= time_average - DelayInMicroSeconds;
    audioPts 		= (time_absolute * 90ull) / 1000ull;

    // Enable external time mapping

    NativeTime	= audioPts & 0x1ffffffffULL;
    Result = avr_set_external_time_mapping(
		    AudioContext->SharedContext, AudioContext->DeviceContext->AudioStream,
		    NativeTime, time_absolute);
    if (Result < 0) {
	DVB_ERROR("dvp_enable_external_time_mapping failed\n");
    	return -EINVAL;
    }

    // Build and inject audio PES header into player

    memset (PesHeader, '0', PES_AUDIO_HEADER_SIZE);
    HeaderLength 	= InsertPesHeader (PesHeader, AudioDataSize + 4, PES_PRIVATE_STREAM1, audioPts, 0);
    HeaderLength 	+= InsertPrivateDataHeader(&PesHeader[HeaderLength], AudioDataSize, AudioContext->DiscreteSampleRate);
    Result 		= DvbStreamInject (AudioContext->DeviceContext->AudioStream, PesHeader, HeaderLength);
    if (Result < 0) {
	DVB_ERROR("StreamInject failed with PES header\n");
	return -EINVAL;
    }

    // Inject audio data into player

    Result 		= DvbStreamInject (AudioContext->DeviceContext->AudioStream, AudioData, AudioDataSize);
    if (Result < 0) {
	DVB_ERROR("StreamInject failed on Audio data\n");
        return -EINVAL;
    }

    return Result;
}


static int AvrAudioInjectorThreadCleanup(avr_v4l2_audio_handle_t *AudioContext)
{
    int ret = 0;

    if (AudioContext->DeviceContext->AudioStream != NULL)
    {
	ret = DvbStreamDrain(AudioContext->DeviceContext->AudioStream, true);
	if (ret != 0)
	{
	    DVB_ERROR("StreamDrain failed\n" );
	    return -EINVAL;
	}

	ret = DvbPlaybackRemoveStream(AudioContext->DeviceContext->Playback,
			              AudioContext->DeviceContext->AudioStream);
	if (ret != 0)
	{
	    DVB_ERROR("PlaybackRemoveStream failed\n" );
	    return -EINVAL;
	}
	AudioContext->DeviceContext->AudioStream = NULL;
    }

    AudioContext->ThreadShouldStop = false;
    wake_up(&AudioContext->WaitQueue);

    return 0;
}


static int AvrAudioInjectorThread (void *data)
{
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *)data;
    struct DeviceContext_s *Context     = AudioContext->DeviceContext;
    struct timespec time_stamp_as_timespec;
    ksnd_pcm_t* capture_handle 			 = NULL;
    const snd_pcm_channel_area_t *capture_areas  = NULL;
    const snd_pcm_channel_area_t *capture_areas0 = NULL;
    snd_pcm_uframes_t capture_offset 	= 0;
    snd_pcm_uframes_t capture_frames 	= 0;
    snd_pcm_uframes_t capture_offset0 	= 0;
    snd_pcm_uframes_t capture_frames0 	= 0;
    snd_pcm_sframes_t commited_frames 	= 0;
    snd_pcm_sframes_t DelayInSamples 	= 0;
    snd_pcm_sframes_t DelayInSamples0 	= 0;
    snd_pcm_sframes_t DelayInSamples1 	= 0;
    unsigned long long time_average 	= 0;
    unsigned long long t0 		= 0;
    unsigned long long t1 		= 0;
    unsigned long SamplesAvailable	= 0;
    unsigned int packet_number 		= 0;
    int Result 				= 0;
    int ret 				= 0;

    // Add the Stream

    Result = DvbPlaybackAddStream  (Context->Playback,
				    BACKEND_AUDIO_ID,
				    BACKEND_PES_ID,
				    BACKEND_SPDIFIN_ID,
				    DEMUX_INVALID_ID,
				    Context->Id,
				    &Context->AudioStream);
    if (Result < 0) {
	DVB_ERROR("PlaybackAddStream failed with %d\n" , Result);
        goto err2;
    }
    else {
	Context->VideoState.play_state = AUDIO_PLAYING;
    }

    // SYSFS - now that the audio stream has been created, let's create the Emergency Mute attribute

    Result = AvrAudioSysfsCreateEmergencyMute(AudioContext);
    if (Result) {
	DVB_ERROR("Error in %s %d: create_emergency_mute_attribute failed\n",__FUNCTION__, __LINE__);
	goto err2;
    }

    // Set AVD sync - Audio Stream

    Result  = DvbStreamSetOption (Context->AudioStream, PLAY_OPTION_AV_SYNC, 1);
    if (Result < 0) {
	DVB_ERROR("PLAY_OPTION_AV_SYNC set failed\n" );
	goto err2;
    }

    // Audio capture init

    Result = ksnd_pcm_open(&capture_handle, AUDIO_CAPTURE_CARD, AUDIO_CAPTURE_PCM, SND_PCM_STREAM_CAPTURE);
    if (Result < 0) {
	DVB_ERROR("Cannot open ALSA %d,%d\n" , AUDIO_CAPTURE_CARD, AUDIO_CAPTURE_PCM);
	goto err2;
    }

    Result = ksnd_pcm_set_params(capture_handle, AUDIO_CHANNELS, AUDIO_SAMPLE_DEPTH, AUDIO_DEFAULT_SAMPLE_RATE,
	    			 AUDIO_PERIOD_FRAMES, AUDIO_BUFFER_FRAMES);
    if (Result < 0) {
	DVB_ERROR("Cannot initialize ALSA parameters\n" );
	ksnd_pcm_close(capture_handle);
	goto err2;
    }

    // Audio capture start

    ksnd_pcm_start(capture_handle);

    while(AudioContext->ThreadShouldStop == false)
    {
	// Retrieve Audio Data

	capture_frames = ksnd_pcm_avail_update(capture_handle);
	while (capture_frames < AUDIO_PERIOD_FRAMES)
	{
            // timeout if we wait for more than 100ms (selected because this is the time it takes to capture three frames at 32KHz)
	    Result = ksnd_pcm_wait(capture_handle, 100);
	    if (Result <= 0)
	    {
		DVB_ERROR("Failed to wait for capture period expiry: %d)\n", Result );

		ret = AvrAudioSetEmergencyMuteReason(AudioContext, EMERGENCY_MUTE_REASON_ERROR);
	        if (ret < 0)
	            DVB_ERROR("dvp_deploy_emergency_mute failed\n" );

                if (Result < 0)
                {
	            Result = AvrAudioXrunRecovery(capture_handle, Result);
		    if (Result < 0) {
		        DVB_ERROR("Cannot recovery from xrun after waiting for period expiry\n" );
		        break;
		    }
		    ksnd_pcm_start(capture_handle);
                }
	    }

	    if (kthread_should_stop()) {
		Result = -1;
		break;
	    }
	    capture_frames = ksnd_pcm_avail_update(capture_handle);
	}

	if (Result < 0)
	    break;

	//DVB_DEBUG("Captured %lu frames...\n" , capture_frames);

        Result = ksnd_pcm_mtimestamp(capture_handle, &SamplesAvailable, &time_stamp_as_timespec);
        if (Result < 0) {
            DVB_ERROR("Cannot read the sound card timestamp\n" );

            /* error recovery */
            ktime_get_ts(&time_stamp_as_timespec);
            SamplesAvailable = 0;
        }

        time_average = time_stamp_as_timespec.tv_sec * 1000000ll + time_stamp_as_timespec.tv_nsec / 1000;
        DelayInSamples = SamplesAvailable;

        capture_frames = AUDIO_PERIOD_FRAMES;
        Result = ksnd_pcm_mmap_begin(capture_handle, &capture_areas, &capture_offset, &capture_frames);
        if (Result < 0) {
            DVB_ERROR("Failed to mmap capture buffer\n" );
            break;
        }

	// DVB_DEBUG("capture_areas[0].addr=%p, capture_areas[0].first=%d, capture_areas[0].step=%d, capture_offset=%lu, capture_frames=%lu\n",
	//		__FUNCTION__, capture_areas[0].addr, capture_areas[0].first, capture_areas[0].step, capture_offset, capture_frames);

	// First packet: we don't know yet the Sample Rate, so we need to cycle another time before being able to inject it
	// into the player -> for now, let's just store the values

	if (packet_number == 0)
	{
	    t0 = time_average;
	    DelayInSamples0 = DelayInSamples;
	    capture_frames0 = capture_frames;
	    capture_areas0 = capture_areas;
	    capture_offset0 = capture_offset;
	}

	// Second Packet: we can calculate now the Sample Rate and use that to get the remaining values for the first packet
	// and inject it into the Player. After, we can inject the second packet.

	if (packet_number == 1)
	{
	    t1 = time_average;
	    DelayInSamples1 = DelayInSamples;

	    // Calculate sample rate and sample frequency

	    Result = AvrAudioCalculateSampleRate(AudioContext, DelayInSamples0, DelayInSamples1, t0, t1);
	    if (Result == 0)
	    {
		// Build and send first audio packet to player

		Result = AvrAudioSendPacketToPlayer(AudioContext, capture_frames0, capture_offset0, capture_areas0, DelayInSamples0, t0);
		if (Result < 0)
		{
		    DVB_ERROR("dvp_send_audio_packet_to_player failed \n" );
		    goto err2;
		}

		// Build and send second audio packet to player

		Result = AvrAudioSendPacketToPlayer(AudioContext, capture_frames, capture_offset, capture_areas, DelayInSamples1, t1);
		if (Result < 0)
		{
		    DVB_ERROR("dvp_send_audio_packet_to_player failed \n" );
		    goto err2;
		}
	    }
	}

	if (packet_number > 1)
	{
	    t0 = t1;
	    t1 = time_average;
	    DelayInSamples0 = DelayInSamples1;
	    DelayInSamples1 = DelayInSamples;

	    // Calculate sample rate and sample frequency

	    Result = AvrAudioCalculateSampleRate(AudioContext, DelayInSamples0, DelayInSamples1, t0, t1);
	    if (Result == 0)
	    {
		Result = AvrAudioSendPacketToPlayer(AudioContext, capture_frames, capture_offset, capture_areas, DelayInSamples, time_average);
		if (Result < 0)
		{
		    DVB_ERROR("dvp_send_audio_packet_to_player failed \n" );
		    goto err2;
		}
	    }
	}

	commited_frames = ksnd_pcm_mmap_commit(capture_handle, capture_offset, capture_frames);
	if (commited_frames < 0 || (snd_pcm_uframes_t) commited_frames != capture_frames) {
	    //DVB_DEBUG("Capture XRUN! (commit %ld captured %ld)\n" , commited_frames,capture_frames);
	    Result = AvrAudioXrunRecovery(capture_handle, commited_frames >= 0 ? -EPIPE : commited_frames);
	    if (Result < 0) {
		DVB_ERROR("MMAP commit error\n" );
		break;
	    }
	    ksnd_pcm_start(capture_handle);
	}

	packet_number++;

    } // end while

    // Audio capture close

    ksnd_pcm_close(capture_handle);
    AvrAudioInjectorThreadCleanup(AudioContext);

    return 0;

err2:
    AvrAudioInjectorThreadCleanup(AudioContext);
    return -EINVAL;

}

/******************************
 * LOW LATENCY TOOLS
 ******************************/

int AvrAudioLLThread (void *data)
{
    MME_ERROR MMEStatus;

    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *)data;
    //    struct DeviceContext_s *Context       = AudioContext->DeviceContext;
    bool IsVeryFirstTransformCommand = true;

    AvrAudioLLStateReset( AudioContext );

    MMEStatus = CreateSysFsStreamAndAttributes( AudioContext );
    if (MMEStatus) {
	 AvrAudioLLThreadCleanup( AudioContext );
	 return -EINVAL;
    }
    AudioContext->IsSysFsStructureCreated = true;

   
    if (CheckPostMortemTransformerCapability( AudioContext )) {
	    DVB_ERROR("Cannot find post-mortem data - post-mortem trace is disabled\n");
    }

    if (CheckLLTransformerCapability( AudioContext )) {
	 AvrAudioLLThreadCleanup( AudioContext );
	 return -EINVAL;
    }
    
    // instantiate the LL transformer (with the correct post processings, according to the topology...)
    if (InitLLTransformer( AudioContext )) {
	 AvrAudioLLThreadCleanup( AudioContext );
	 return -EINVAL;
    }
    AudioContext->IsLLTransformerInitialized = true;

#if 0
    // configure output/inputs 
    if (LaunchLLSetGlobalCommand( AudioContext ))
        goto err2;

    // wait for the send global to return before sending the log buffers
    do 
    {
        msleep(10);

    } while (!AudioContext->GotSetGlobalCommandCallback);

    AudioContext->GotSetGlobalCommandCallback = false;
#endif

    SendLogBuffers(AudioContext);

    // reset the firmware status structure (causes fresh sysfs events to be
    // generated during capture off/on cycles).
    memset(&AudioContext->LLDecoderStatus, 0, sizeof(AudioContext->LLDecoderStatus));

    // send a MME_TRANSFORM command to make the ll transformer ready to run
    if (LaunchLLTransformCommand( AudioContext ))
        goto err2;

    DVB_DEBUG("First MME_TRANSFORM command issued with success!\n");

    // while the firmware runs, check for possible events coming from the firmware

    while(AudioContext->ThreadShouldStop == false)
    {
	bool AlreadyLaunchedLLSetGlobalCommand;

        // wait for any asynchronous event to come (stop, mixer settigns update or callback from transform) for 5 second...
        // if no answer for 5 seconds, check if the transformer is still alive...
        
        int result = wait_event_interruptible_timeout(AudioContext->WaitQueue, 
                                                      (AudioContext->ThreadShouldStop || AudioContext->UpdateMixerSettings || 
                                                       AudioContext->GotTransformCommandCallback || AudioContext->GotSetGlobalCommandCallback ||
                                                       AudioContext->GotSendBufferCommandCallback), 
                                                      (HZ * 5));

        // a timeout occurred, check if the transformer is still alive...
        if (result == 0)
        {
            if (CheckPostMortemStatus( AudioContext ))
            {
                DVB_ERROR( "Transformer %s does not seem alive any more...\n", LL_MT_NAME );
		IssuePostMortemDiagnostics( AudioContext );
                continue;
            }
        }

        if (AudioContext->ThreadShouldStop)
        {
            // cleanup thread
	    DVB_TRACE("ThreadShouldStop is asserted - terminating\n");
            AvrAudioLLThreadCleanup(AudioContext);
            return 0;
        }
        
        if (AudioContext->UpdateMixerSettings)
        {
	    AlreadyLaunchedLLSetGlobalCommand = true;

            DVB_DEBUG("LL thread: Update Mixer parameters!\n");
	    MMEStatus = LaunchLLSetGlobalCommand( AudioContext );
	    switch (MMEStatus) {
	    case MME_SUCCESS:
		// do nothing
		break;
	    case MME_COMMAND_STILL_EXECUTING:
		AudioContext->NeedAnotherSetGlobalCommand = true;
		break;
	    default:
		// unrecoverable errro
		goto err2;
	    }
        }
	else
	{
	    AlreadyLaunchedLLSetGlobalCommand = false;
	}

        if (AudioContext->GotSendBufferCommandCallback)
        {
            int i;
            AudioContext->GotSendBufferCommandCallback = false;

            for (i = 0; i < AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS; i++)
            {
                BufferEntry_t * TheEntry = AudioContext->BufferEntries[i];
                
                if (TheEntry && TheEntry->IsFree && TheEntry->BytesUsed)
                {
                    unsigned int relay_id;
                    unsigned int stream_nb = (i/AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS);
                    
                    //DVB_DEBUG("relay %d bytes to the buffer %d\n", TheEntry->BytesUsed, i);
                    // relay this buffer!
                    if (stream_nb < AVR_LOW_LATENCY_MAX_INPUT_CARDS)
                    {
                        relay_id = ST_RELAY_TYPE_LOW_LATENCY_INPUT0 + stream_nb;
                    }
                    else
                    {
                        relay_id = ST_RELAY_TYPE_DATA_TO_PCM0 + stream_nb;
                    }

                    st_relayfs_write(relay_id, 
                                     ST_RELAY_SOURCE_LOW_LATENCY, 
                                     (unsigned char*)TheEntry->Ptr, 
                                     TheEntry->BytesUsed,
                                     0);
                    
                    // we shouldn't be doing this...
                    AudioContext->SendBufferCommands[i].DataBuffers_p[0]->ScatterPages_p[0].BytesUsed = 0;
                    
                    // send this buffer back to the firmware
                    MMEStatus = MME_SendCommand(AudioContext->LLTransformerHandle, 
                                                &AudioContext->SendBufferCommands[i]);

#if 0
                    DVB_DEBUG("****Dumping MME comand %x,%d ****\n", (unsigned int )&AudioContext->SendBufferCommands[i], i);
                    DumpMMECommand( &AudioContext->SendBufferCommands[i] );
                    DVB_DEBUG("    \n");
#endif
                    
                    if (MMEStatus != MME_SUCCESS)
                    {
                        DVB_ERROR("Error: Call to MME_SendCommand %s (MME_SEND_BUFFER) returned %d\n",
                               LL_MT_NAME,
                               MMEStatus);
                    } else {
			TheEntry->IsFree = false;
			atomic_inc(&(AudioContext->CommandsInFlight));
			DVB_TRACE("Issued command %08x (MME_SEND_BUFFERS) - %d in flight\n",
				  AudioContext->SendBufferCommands[i].CmdStatus.CmdId,
				  atomic_read(&(AudioContext->CommandsInFlight)));
		    }
                }
            }
        }

        if (AudioContext->GotTransformCommandCallback)
        {
            // we received a transform callback meaning format update
            // store the new status
            // send a new transform command

            MME_LxAudioDecoderFrameStatus_t  * DecoderStatus;
	    MME_LimiterStatus_t * PrimaryLimiterStatus;
            LLDecoderStatus_t * LLDecoderStatus;
            tMMESpdifinStatus * SpdifStatus, *CurrentSpdifStatus;
	    int NewSilence, OldSilence;
            enum eMulticomSpdifinState NewState, OldState;
            enum eMulticomSpdifinPC    NewPC, OldPC;
	    int NewDisplay, OldDisplay;
	    tEmergencyMute NewMute, OldMute;
	    bool CodecChange, MuteChange, FrequencyChange, TopologyChange;

            AudioContext->GotTransformCommandCallback = false;

	    if (AudioContext->TransformCommand.CmdStatus.Error != MME_SUCCESS) {
		    DVB_ERROR("Firmware reports transform command failure. This is impossible!!!\n");
	    }

	    // re-arrange the transform status so we can use the strongly typed structures
	    if (0 != ProcessSpecializedTransformStatus(&AudioContext->TransformStatus))
		    DVB_ERROR("Failed to extract firmware's status. Expect driver to make bad decisions.\n");

            //examine the status of the transform command
            DecoderStatus = &AudioContext->TransformStatus.DecoderFrameStatus;
	    PrimaryLimiterStatus = &AudioContext->TransformStatus.LimiterStatus[0];
            LLDecoderStatus = &AudioContext->LLDecoderStatus;
            SpdifStatus = (tMMESpdifinStatus *) &DecoderStatus->FrameStatus[0];
            CurrentSpdifStatus = &LLDecoderStatus->CurrentSpdifStatus;
            
#if SPDIFIN_API_VERSION >= 0x100415
	    NewSilence = (enum eMulticomSpdifinState) SpdifStatus->Silence;
	    OldSilence = CurrentSpdifStatus->Silence;
#else
	    NewSilence = OldSilence = 0;
#endif

            NewState = (enum eMulticomSpdifinState) SpdifStatus->CurrentState;
            OldState = CurrentSpdifStatus->CurrentState;

            NewPC    = (enum eMulticomSpdifinPC   ) SpdifStatus->PC;
            OldPC    = CurrentSpdifStatus->PC;

#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x100319
	    NewDisplay = SpdifStatus->Display;
	    OldDisplay = CurrentSpdifStatus->Display;
#else
	    NewDisplay = OldDisplay = 0;
#endif

#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	    NewMute = PrimaryLimiterStatus->EmergencyMute.bits;
#else
	    NewMute = 0;
#endif
	    OldMute  = LLDecoderStatus->CurrentMuteStatus;
	    LLDecoderStatus->CurrentMuteStatus = NewMute;

            down(&AudioContext->DecoderStatusSemaphore);

            if (DecoderStatus->DecStatus != MME_SUCCESS)
            {
                LLDecoderStatus->DecodeErrorCount++;
            }
            else
            {
                LLDecoderStatus->NumberOfProcessedSamples += DecoderStatus->NbOutSamples;
            }

            up(&AudioContext->DecoderStatusSemaphore);
            
	    CodecChange = (OldSilence != NewSilence) || (OldState != NewState) || (OldPC != NewPC) || (OldDisplay != NewDisplay);
	    MuteChange = (OldMute.State == 2 && NewMute.State != 2);
	    FrequencyChange = (LLDecoderStatus->CurrentDecoderFrequency != DecoderStatus->DecSamplingFreq);
	    TopologyChange = (LLDecoderStatus->CurrentDecoderAudioMode != DecoderStatus->DecAudioMode);
	    
            if ( CodecChange || MuteChange || FrequencyChange || TopologyChange )
            {
                down(&AudioContext->DecoderStatusSemaphore);
                // save current decoder properties
                LLDecoderStatus->CurrentDecoderFrequency = DecoderStatus->DecSamplingFreq;
                LLDecoderStatus->CurrentDecoderAudioMode = DecoderStatus->DecAudioMode;
                memcpy(CurrentSpdifStatus, SpdifStatus, sizeof(tMMESpdifinStatus));
                up(&AudioContext->DecoderStatusSemaphore);
                
                DVB_TRACE("LL thread: Firmware /%s%s%s%s change: /%s%s-%s/%d/%s/\n",
			  (CodecChange ? "Codec/" : ""), (MuteChange ? "Mute/" : ""),
                          (FrequencyChange ? "Frequency/" : ""), (TopologyChange ? "Topology/": ""),
			  TranslatePcToEncoding(NewPC),
			  (NewSilence ? " SILENT" : ""),
			  LookupSpdifInState(NewState),
			  TranslateDiscreteSamplingFrequencyToInteger(
				  AudioContext->LLDecoderStatus.CurrentDecoderFrequency ),
                          LookupAudioMode(LLDecoderStatus->CurrentDecoderAudioMode));

		// this attribute is *always* made ready (even if the audio format didn't change so
		// applications don't have to poll all the files)
		sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "input_format");

                // notify the format changes to sysfs
		if (CodecChange) {
			sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "supported_input_format");
			sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL,
				      "supported_input_format_in_game_mode");
		}
		if (MuteChange) {
			AvrAudioSetEmergencyMuteReason(AudioContext, (FrequencyChange ?
								      EMERGENCY_MUTE_REASON_SAMPLE_RATE_CHANGE:
								      EMERGENCY_MUTE_REASON_ERROR));
		}
		if (FrequencyChange) {
			sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "sample_frequency");
			if (!CodecChange)
				sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL,
					      "supported_input_format_in_game_mode");
		}
		if (TopologyChange) {
			sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "number_channels");
		}

		// launch an update command (lazily)
		if (!AlreadyLaunchedLLSetGlobalCommand) {
		    MMEStatus = LaunchLLSetGlobalCommand( AudioContext );
		    switch (MMEStatus) {
		    case MME_SUCCESS:
			// do nothing
			break;
		    case MME_COMMAND_STILL_EXECUTING:
			AudioContext->NeedAnotherSetGlobalCommand = true;
			break;
		    default:
			// unrecoverable error
			goto err2;
		    }
		}
	    }

            // notify the format changes to sysfs
            sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "decode_errors");
            sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "number_of_samples_processed");

            if (IsVeryFirstTransformCommand)
            {
                IsVeryFirstTransformCommand = false;
            }
            else if (LLDecoderStatus->CurrentDecoderFrequency != DecoderStatus->DecSamplingFreq)
            {
                // deploy a emergency mute, but it should have been deployed by the firmware by itself...
                AvrAudioSetEmergencyMuteReason(AudioContext, EMERGENCY_MUTE_REASON_SAMPLE_RATE_CHANGE);
            } 
            else if (DecoderStatus->DecStatus != MME_SUCCESS)
            {
                // deploy a emergency mute, but it should have been deployed by the firmware by itself...
                AvrAudioSetEmergencyMuteReason(AudioContext, EMERGENCY_MUTE_REASON_ERROR);
            }

            DVB_DEBUG("LL thread: Got transform callback...!\n");

            // send a new transform comand, we'll get the callback if the input format changes...
            if (LaunchLLTransformCommand( AudioContext ))
                goto err2;
        }

        if (AudioContext->GotSetGlobalCommandCallback)
        {
            AudioContext->GotSetGlobalCommandCallback = false;

	    if (AudioContext->NeedAnotherSetGlobalCommand) {
		AudioContext->NeedAnotherSetGlobalCommand = false;
		AudioContext->UpdateMixerSettings = true;
	    }
        }
    }
        
    AvrAudioLLThreadCleanup(AudioContext);
    return 0;

err2:
    AvrAudioLLThreadCleanup(AudioContext);
    return -EINVAL;
}


void SendLogBuffers( avr_v4l2_audio_handle_t *AudioContext )
{
    MME_ERROR MMEStatus = 0;
    int i = 0;

    if (ll_input_log_enable)
    {
        // make the input log buffers available to the firmware
        for (i = (AudioContext->AudioInputId * AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS); 
             i < AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS; 
             i++)
        {
            MMEStatus = MME_SendCommand(AudioContext->LLTransformerHandle, 
                                        &AudioContext->SendBufferCommands[i]);
        
            if (MMEStatus != MME_SUCCESS)
            {
                DVB_ERROR("Error: Call to MME_SendCommand %s (MME_SEND_BUFFER) returned %d\n",
                       LL_MT_NAME,
                       MMEStatus);
            } else {
		// this buffer has been sent to the firmware, so it has to be released fierst...
		AudioContext->BufferEntries[i]->IsFree = false;
		atomic_inc(&(AudioContext->CommandsInFlight));
		DVB_TRACE("Issued command %x (MME_SEND_BUFFERS) for input log - %d in flight\n",
			  AudioContext->SendBufferCommands[i].CmdStatus.CmdId,
			  atomic_read(&(AudioContext->CommandsInFlight)));
	    }
        }
    }

    if (ll_output_log_enable)
    {
        // make the log buffers available to the firmware
        for (i = AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS; i < AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS; i++)
        {
            MMEStatus = MME_SendCommand(AudioContext->LLTransformerHandle, 
                                        &AudioContext->SendBufferCommands[i]);
            
            if (MMEStatus != MME_SUCCESS)
            {
                DVB_ERROR("Error: Call to MME_SendCommand %s (MME_SEND_BUFFER) returned %d\n",
                       LL_MT_NAME,
                       MMEStatus);
            } else {
                // this buffer has been sent to the firmware, so it has to be released fierst...
		atomic_inc(&(AudioContext->CommandsInFlight));
                AudioContext->BufferEntries[i]->IsFree = false;
		DVB_TRACE("Issued command %x (MME_SEND_BUFFERS) for output log - %d in flight\n",
			  AudioContext->SendBufferCommands[i].CmdStatus.CmdId,
			  atomic_read(&(AudioContext->CommandsInFlight)));
	    }
        }
    }
}

static int CheckSinglePostMortemTransformerCapability( const char *Name,
						       unsigned long *PhysicalAddress,
						       MME_TimeLogPostMortem_t **Buffer )
{
    MME_ERROR MMEStatus;
    MME_TransformerCapability_t Capability = { 0 };
    MME_TimeLogTransformerInfo_t Info = { 0 };

    /* reset any previous mapping */
    if (*Buffer)
	iounmap(*Buffer);
    *PhysicalAddress = 0;
    *Buffer = NULL;

#if DRV_MULTICOM_PERFLOG_VERSION >= 0x090605
    Capability.StructSize = sizeof( Capability );
    Capability.TransformerInfo_p = &Info;
    Capability.TransformerInfoSize = sizeof(Info);
    MMEStatus = MME_GetTransformerCapability( Name, &Capability );
    if( MMEStatus != MME_SUCCESS ) {
        DVB_ERROR( "Error:(%d) - Unable to read capabilities of %s\n", MMEStatus, Name );
        return -EINVAL;
    }

    if (Info.StructSize != sizeof(Info) || 0 == Info.PostMortemPhysicalAddress) {
	DVB_ERROR( "Error: Invalid reply from %s\n", Name );
	return -EINVAL;
    }

    if (Capability.Version != DRV_MULTICOM_PERFLOG_VERSION) {
	DVB_TRACE("Warning: Version skew (0x%x vs 0x%x) detected in post-mortem trace on %s\n",
		  Capability.Version, DRV_MULTICOM_PERFLOG_VERSION, Name);
	return -EINVAL;
    }

    *Buffer = ioremap(Info.PostMortemPhysicalAddress, sizeof(**Buffer));
    if (!*Buffer) {
	DVB_TRACE("Found post-mortem buffer for %s but could not map for IO\n", Name);
	return -EINVAL;
    }

    *PhysicalAddress = Info.PostMortemPhysicalAddress;
#endif

    return 0;
}

static int CheckPostMortemTransformerCapability( avr_v4l2_audio_handle_t *AudioContext )
{
    const char PrimaryName[] = "LOG_PERF_MT_A03";
    const char SecondaryName[] = "LOG_PERF_MT_A04";
    int res;

    res = CheckSinglePostMortemTransformerCapability( PrimaryName,
						      &AudioContext->PostMortemPhysicalAddress,
						      &AudioContext->PostMortemBuffer );
    if (0 != res)
	return res;

    (void) CheckSinglePostMortemTransformerCapability( SecondaryName,
						       &AudioContext->SecondaryPostMortemPhysicalAddress,
						       &AudioContext->SecondaryPostMortemBuffer );

    return 0;
}

static int CheckPostMortemStatus( avr_v4l2_audio_handle_t *AudioContext )
{
    if (AudioContext->PostMortemAlreadyIssued)
	return -ETIMEDOUT;

    if (AudioContext->PostMortemBuffer
	&& AudioContext->PostMortemBuffer->Status != POSTMORTEM_RUNNING)
	return -ETIMEDOUT;

    if (AudioContext->SecondaryPostMortemBuffer
	&& AudioContext->SecondaryPostMortemBuffer->Status != POSTMORTEM_RUNNING)
	return -ETIMEDOUT;

    return 0;
}

static const char *LookupPostMortemStatus(MME_TimeLogPostMortemStatus_t Status)
{
    switch(Status) {
#define C(x) case POSTMORTEM_ ## x: return #x
    C(RUNNING);
    C(CRASH);
    C(LIVELOCK);
    C(DEADLOCK);
#undef C
    default:
	return "UNKNOWN";
    }
}

static int PostMortemSendInterrupt(int id)
{
	unsigned long peripheral_base[5] = { 0 };
	unsigned int *p;

	const int INTMASKSET1 = (0x0120 / 4);
	const int INTSET1 = (0x0048 / 4);

	if (id >= lengthof(peripheral_base))
		return -EINVAL;

#ifdef CONFIG_SND_STM_STX7200
	/* 0 is the host. Tou can crash it if you want but I won't help */
	peripheral_base[1] = 0xfe000000;
	peripheral_base[2] = 0xfe100000;
	peripheral_base[3] = 0xfe200000;
	peripheral_base[4] = 0xfe300000;
#endif

	if (0 == peripheral_base[id]) {
		DVB_ERROR("Unsupported processor id (%d)\n", id);
		return -EHOSTUNREACH;
	}

	p = ioremap(peripheral_base[id], 4096);
	if (!p) {
		DVB_ERROR("Cannot map peripheral base register at 0x%08lx\n", peripheral_base[id]);
		return -ENOMEM;
	}

	/* set bits 32-63 in the mask and test registers. the co-processor on the end of this
         * should take an unhandled interrupt and panic (with a resulting post-mortem)
	 */
	p[INTMASKSET1] = -1; /* set all the bits in the mask register */
	p[INTSET1] = -1; /* set all the bits in the test register */

	iounmap(p);

	return 0;
}

static void PostMortemReportOutstandingCommand(avr_v4l2_audio_handle_t *AudioContext, MME_Command_t *Cmd)
{
    MME_CommandState_t State = Cmd->CmdStatus.State;
 
    if (MME_COMMAND_COMPLETED != State && MME_COMMAND_FAILED != State)
	    printk("  0x%08x (%s)\n",
		   Cmd->CmdStatus.CmdId, LookupMmeCommandCode(Cmd->CmdCode));
}

static void IssueSinglePostMortemDiagnostics( MME_TimeLogPostMortem_t *pm, const char *name )
{
    int i;

    printk("\n");

    if (pm->StructSize != sizeof(MME_TimeLogPostMortem_t) ||
        pm->Version != DRV_MULTICOM_PERFLOG_VERSION)
	printk("WARNING: Version skew detected - contemplate report with great care\n");
	
    printk("Identity: %s\n", name);
    printk("Status:   %s (%d)\n", LookupPostMortemStatus(pm->Status), pm->Status);
    printk("Message:  %s%s", pm->Message, (pm->Message[strlen(pm->Message)-1] == '\n' ? "" : "\n"));
    printk("Release:  %s\n", pm->FirmwareRelease);
    printk("Built by: %s@%s at %s %s\n", pm->UserName, pm->BuildMachine, pm->CompileTime, pm->CompileDate);

    if (pm->Status != POSTMORTEM_RUNNING) {
	printk("\nPC   0x%08x    SP   0x%08x    LINK 0x%08x    PSW  0x%08x\n",
	       pm->PC, pm->SP, pm->LINK, pm->PSW);
	for (i=1; i<60; i+=4)
	    printk("R%-2d  0x%08x    R%-2d  0x%08x    R%-2d  0x%08x    R%-2d  0x%08x\n",
		   i, pm->Regs[i], i+1, pm->Regs[i+1], i+2, pm->Regs[i+2], i+3, pm->Regs[i+3]);
	printk("R61  0x%08x    R62  0x%08x    R63  0x%08x\n\n", pm->Regs[61], pm->Regs[62], pm->Regs[63]);
	printk("BR0  0x%08x    BR1  0x%08x    BR2  0x%08x    BR3  0x%08x\n",
	       (pm->BranchBits & 0x01 ? 1 : 0), (pm->BranchBits & 0x02 ? 1 : 0),
	       (pm->BranchBits & 0x04 ? 1 : 0), (pm->BranchBits & 0x08 ? 1 : 0));
	printk("BR4  0x%08x    BR5  0x%08x    BR6  0x%08x    BR7  0x%08x\n",
	       (pm->BranchBits & 0x10 ? 1 : 0), (pm->BranchBits & 0x20 ? 1 : 0),
	       (pm->BranchBits & 0x40 ? 1 : 0), (pm->BranchBits & 0x80 ? 1 : 0));
	
	printk("\nCall trace:\n");
	if (pm->BackTrace[0]) {
	    printk("  0x%08x\n  0x%08x\n", pm->PC, pm->LINK);
	    for (i=0; i<lengthof(pm->BackTrace) && pm->BackTrace[i]; i++)
		printk("  0x%08x\n", pm->BackTrace[i]);
	} else {
	    printk("  No call trace available (?bad stack pointer?)\n");
	}
    }
}

static void IssuePostMortemDiagnostics( avr_v4l2_audio_handle_t *AudioContext )
{
#if DRV_MULTICOM_PERFLOG_VERSION >= 0x090617
    MME_TimeLogPostMortem_t *pm[2] = { AudioContext->PostMortemBuffer,
				       AudioContext->SecondaryPostMortemBuffer };
    const char *name_map[2] = { "sti7200_audio0", "sti7200_audio1" };
    int kill_map[2] = { 3, 4 };
    unsigned int kill_mask, dead_mask, trap_mask;
    int i;

    if (!pm[0] || 0 != test_and_set_bit(0, &AudioContext->PostMortemAlreadyIssued)) {
	DVB_DEBUG("No post-mortem because %s\n", (pm[0] ? "it is not supported by firmware"
						        : "post-mortem already issued"));
	return;
    }

    /* Try to figure out which, if any firmware, has died (killing any which haven't so we can
     * analyse any potential live locks
     */
    dead_mask = kill_mask = trap_mask = 0;
    for (i=0; i<lengthof(pm); i++) {
	if (pm[i]) {
	    if (POSTMORTEM_RUNNING == pm[i]->Status) {
		kill_mask |= 1 << i;
		PostMortemSendInterrupt(kill_map[i]);
	    } else if (POSTMORTEM_TRAPPED == pm[i]->Status) {
		dead_mask |= 1 << i;
		trap_mask |= 1 << i;
	    } else {
		dead_mask |= 1 << i;
	    }
	}
    }

    /* If we did anything mean, or if one of the firmwares is still
     * dumping its state, then wait for further news. Note that the
     * _TRAPPED state is a transient state used to help us identify
     * failures within the trap handler itself. The co-processor
     * should leave this state shortly after entering it if we simply
     * leave it alone for a little bit (and if it doesn't we need to
     * say that so we only sleep once).
     */
    if (kill_mask || trap_mask) {
	set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout (HZ/10);
    }

    printk("\n"
           "Audio firmware post-mortem report\n"
           "=================================\n");

    printk("\nFound %d crashed processors and killed %d more (guru meditation #%x/%x/%x)\n",
	   hweight8(dead_mask), hweight8(kill_mask), dead_mask, kill_mask, trap_mask);

    for (i=0; i<lengthof(pm); i++) {
	if ((dead_mask ? dead_mask : kill_mask) & (1 << i))
	    IssueSinglePostMortemDiagnostics(pm[i], name_map[i]);
    }

    printk("\nPending MME commands (%d expected):\n", atomic_read(&(AudioContext->CommandsInFlight)));
    for (i = 0; i < AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS; i++)
	PostMortemReportOutstandingCommand(AudioContext, &AudioContext->SendBufferCommands[i]);
    PostMortemReportOutstandingCommand(AudioContext, &AudioContext->TransformCommand);
    PostMortemReportOutstandingCommand(AudioContext, &AudioContext->SetGlobalCommand);
    
    if (atomic_read(&(AudioContext->ReuseOfGlobals)) != 0)
	printk("SetGlobalsCommand structure reused %d times\n",
	       atomic_read(&(AudioContext->ReuseOfGlobals)));
    
    if (atomic_read(&(AudioContext->ReuseOfTransform)) != 0)
	printk("TransformCommand structure reused %d times\n",
	       atomic_read(&(AudioContext->ReuseOfTransform)));
	
    printk("\nEnd of post-mortem report\n\n");
    sysfs_notify(&AudioContext->StreamClassDevice.kobj, NULL, "post_mortem");
#else
    DVB_TRACE("No post-mortem because no support found in driver\n");
#endif
}

// Low Latency transformer check capability
// this function can be used either to retrieve the transformer capability
static int CheckLLTransformerCapability( avr_v4l2_audio_handle_t *AudioContext )
{
    MME_ERROR MMEStatus;
    MME_TransformerCapability_t Capability = { 0 };
    MME_LxAudioDecoderHDInfo_t * LLInfo;

    LLInfo = &AudioContext->AudioDecoderTransformCapability;
    
    Capability.StructSize = sizeof( Capability );
    Capability.TransformerInfo_p = LLInfo;
    Capability.TransformerInfoSize = sizeof( MME_LxAudioDecoderHDInfo_t );
    MMEStatus = MME_GetTransformerCapability( LL_MT_NAME, &Capability );
    
    if( MMEStatus != MME_SUCCESS )
    {
        DVB_ERROR( "Error:(%d) - Unable to read capabilities of %s\n", MMEStatus, LL_MT_NAME );
        return -EINVAL;
    }
    
    DVB_TRACE( "Found %s transformer (version %x)\n", LL_MT_NAME, Capability.Version );
    DVB_DEBUG( "%s capabilities:%s%s%s%s\n", LL_MT_NAME,
	       (ACC_DECODER_CAPABILITY_EXT_FLAGS(LLInfo, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_DD) ? " DD" : ""),
	       (ACC_DECODER_CAPABILITY_EXT_FLAGS(LLInfo, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_DTS) ? " DTS" : ""),
	       (ACC_DECODER_CAPABILITY_EXT_FLAGS(LLInfo, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_MPG) ? " MP2AAC" : ""),
	       (ACC_DECODER_CAPABILITY_EXT_FLAGS(LLInfo, ACC_SPDIFIN) & (1 << ACC_SPDIFIN_WMA) ? " WMA" : ""));

    return 0;
}

static int GetMasterLatencyOffset( avr_v4l2_audio_handle_t *AudioContext )
{
    avr_v4l2_shared_handle_t *SharedContext = AudioContext->SharedContext;
    int MasterLatency = 0;

    if (SND_PSEUDO_MIXER_MAGIC == SharedContext->mixer_settings.magic)
    {
	int master_latency = SharedContext->mixer_settings.master_latency;
	MasterLatency = (master_latency > 0) ? master_latency : 0;
    }

    return MasterLatency;
}

// Low Latency transformer initialisation
static int InitLLTransformer( avr_v4l2_audio_handle_t *AudioContext )
{
    avr_v4l2_shared_handle_t *SharedContext = AudioContext->SharedContext;
    MME_ERROR MMEStatus;
    MME_TransformerInitParams_t InitParams = { 0 };
    MME_LowlatencySpecializedInitParams_t  LLInitParams = { 0 };
    int MasterLatency = GetMasterLatencyOffset( AudioContext );
    
    memset(&AudioContext->SetGlobalParams, 0, sizeof(MME_LowlatencySpecializedGlobalParams_t));
    
    InitParams.StructSize = sizeof(MME_TransformerInitParams_t);
    InitParams.Priority   = MME_PRIORITY_ABOVE_NORMAL;
    //        InitParams.InputType   = ;
    //        InitParams.OutputType   = ;
    InitParams.Callback = MMECallbackLL;
    InitParams.CallbackUserData = AudioContext;
    InitParams.TransformerInitParamsSize = sizeof(MME_LowlatencySpecializedInitParams_t);
    InitParams.TransformerInitParams_p = &LLInitParams;
    
    LLInitParams.StructSize = sizeof(MME_LowlatencySpecializedInitParams_t);
    LLInitParams.InitParams.TimeOut = 200; //TODO; fill a timeout in ms, us, ns?
    LLInitParams.InitParams.BlockSize =  LLBlockSize;

    LLInitParams.InitParams.Features.Latency = SharedContext->target_latency + MasterLatency;

#ifdef LOWLATENCY_API_EPOCH
#if LOWLATENCY_API_VERSION >= 0x100430
    LLInitParams.InitParams.Features.SilenceThresh = AudioContext->SilenceThreshold;
    LLInitParams.InitParams.Features.SilencePeriod = AudioContext->SilenceDuration;
#endif
#endif

    AudioContext->MasterLatencyWhenInitialized = MasterLatency;

    DiscloseLatencyTargets( AudioContext, "Init" );
    
    // update transformer global parameters
    if (0 != SetGlobalTransformParameters(AudioContext, &LLInitParams.GlobalInitParams)) {
	DVB_ERROR("Could not set global transformer parameters (resource conflict?)\n");
	return -EINVAL;
    }
    
    MMEStatus = MME_InitTransformer(LL_MT_NAME,
                                    &InitParams,
                                    &AudioContext->LLTransformerHandle);
    if (MMEStatus != MME_SUCCESS)
    {
        DVB_ERROR("Error: Call to MME_InitTransformer %s returned %d\n",
               LL_MT_NAME,
               MMEStatus);
        return -EINVAL;
    }
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Handle a callback from MME.
///
static void MMECallbackLL(    MME_Event_t      Event,
                              MME_Command_t   *CallbackData,
                              void            *UserData )
{
    avr_v4l2_audio_handle_t *AudioContext = (avr_v4l2_audio_handle_t *) UserData;

    atomic_dec(&(AudioContext->CommandsInFlight));
    DVB_TRACE("Event %d for CmdId %08x (%s) - %d in flight\n",
              Event, CallbackData->CmdStatus.CmdId, LookupMmeCommandCode(CallbackData->CmdCode),
	      atomic_read(&(AudioContext->CommandsInFlight)));
    
    switch (Event)
    {
        case MME_COMMAND_COMPLETED_EVT:
            switch (CallbackData->CmdCode)
            {
                case MME_TRANSFORM:
                    AudioContext->GotTransformCommandCallback = true;
                    wake_up(&AudioContext->WaitQueue);
                    break;
                    
                case MME_SET_GLOBAL_TRANSFORM_PARAMS:
                    AudioContext->GotSetGlobalCommandCallback = true;
                    wake_up(&AudioContext->WaitQueue);
                    break;
                    
                case MME_SEND_BUFFERS:
                {
                    BufferEntry_t * TheEntry;
                    MME_DataBuffer_t * DataBuffer = CallbackData->DataBuffers_p[0];
                    unsigned int Size = DataBuffer->ScatterPages_p[0].BytesUsed;
                    int BufferIndex = (int)DataBuffer->UserData_p;
                    
                    TheEntry = AudioContext->BufferEntries[BufferIndex];

                    if (TheEntry)
                    {
                        TheEntry->IsFree = true;
                        TheEntry->BytesUsed = Size;
                        
                        if (Size)
                        {
                            //DVB_DEBUG("Got a correct SendBuffer callback  (%d - %d)\n", BufferIndex, Size);

                            AudioContext->GotSendBufferCommandCallback = true;
                            wake_up(&AudioContext->WaitQueue);
                        }
                        else
                        {
                            DVB_ERROR("Strange: Got a SendBuffer callback but buffer (%d) is empty\n",
                                   BufferIndex);
                        }
                    }
                    else
                    {
                        DVB_ERROR("Strange: Got a SendBuffer callback but buffer has a wrong index (%d - %x)\n",
                               BufferIndex, (unsigned int)TheEntry);
                    }
                    break;
                }
                default:
                    DVB_ERROR("Unexpected MME CmdCode (%d)\n", CallbackData->CmdCode);
                    break;
            }
            break;
            
        default:
            //MME_DATA_UNDERFLOW_EVT
            //MME_NOT_ENOUGH_MEMORY_EVT
            //MME_NEW_COMMAND_EVT
	    atomic_inc(&(AudioContext->CommandsInFlight));
            DVB_ERROR("Low latency MMe callback: Unexpected MME event (%d)\n", Event);
            break;
    }
    return;
}

static int LaunchLLSetGlobalCommand( avr_v4l2_audio_handle_t *AudioContext )
{
    avr_v4l2_shared_handle_t *SharedContext = AudioContext->SharedContext;
    MME_ERROR MMEStatus;

    memset(&AudioContext->SetGlobalParams, 0, sizeof(MME_LowlatencySpecializedGlobalParams_t));

    AudioContext->UpdateMixerSettings = false;    

    if (AudioContext->SetGlobalCommand.CmdStatus.State != MME_COMMAND_COMPLETED &&
        AudioContext->SetGlobalCommand.CmdStatus.State != MME_COMMAND_FAILED) {
	    DVB_TRACE("Refusing to reuse SetGlobalCommand structure (CmdId %08x still pending)\n",
		      AudioContext->SetGlobalCommand.CmdStatus.CmdId);
	    atomic_inc(&(AudioContext->ReuseOfGlobals));
	    return MME_COMMAND_STILL_EXECUTING;
    }

    // acquire the read mixer settings semaphore
    down_read(&SharedContext->mixer_settings_semaphore);

    FillOutLLSetGlobalCommand(AudioContext);
    
    DVB_DEBUG("Finished setting global params\n");
    
    // free the read mixer settings semaphore
    up_read(&SharedContext->mixer_settings_semaphore);
    
    DVB_DEBUG("Launch a SetGlobal command to LL transformer\n");

    //    DumpMMECommand( &AudioContext->SetGlobalCommand );
            
    MMEStatus = MME_SendCommand(AudioContext->LLTransformerHandle, 
                                &AudioContext->SetGlobalCommand);

    if (MMEStatus != MME_SUCCESS)
    {
        DVB_ERROR("Error: Call to MME_SendCommand %s returned %d\n",
                       LL_MT_NAME,
               MMEStatus);
    } else {
	atomic_inc(&(AudioContext->CommandsInFlight));
	DVB_TRACE("Issued command %x (MME_SET_GLOBAL_TRANSFORM_PARAMS) - %d in flight\n",
		  AudioContext->SetGlobalCommand.CmdStatus.CmdId,
		  atomic_read(&(AudioContext->CommandsInFlight)));
    }


    return MMEStatus;
}


static int LaunchLLTransformCommand( avr_v4l2_audio_handle_t *AudioContext )
{
    MME_ERROR MMEStatus;

    if (AudioContext->TransformCommand.CmdStatus.State != MME_COMMAND_COMPLETED &&
        AudioContext->TransformCommand.CmdStatus.State != MME_COMMAND_FAILED) {
	    DVB_ERROR("Refusing to reuse TransformCommand structure\n");
	    atomic_inc(&(AudioContext->ReuseOfTransform));
	    return MME_COMMAND_STILL_EXECUTING;
    }

    FillOutLLTransformCommand(AudioContext);
    
    MMEStatus = MME_SendCommand(AudioContext->LLTransformerHandle, 
                                &AudioContext->TransformCommand);
    
    if (MMEStatus != MME_SUCCESS)
    {
        DVB_ERROR("Error: Call to MME_SendCommand %s returned %d\n",
               LL_MT_NAME,
               MMEStatus);
    } else {
	atomic_inc(&(AudioContext->CommandsInFlight));
	DVB_TRACE("Issued command %x (MME_TRANSFORM) - %d in flight\n",
		  AudioContext->TransformCommand.CmdStatus.CmdId,
		  atomic_read(&(AudioContext->CommandsInFlight)));
    }

    return (MMEStatus);
}

/**
 * Fill out the global transformer parameters.
 *
 * This is shared beween the initialization and parameter update code.
 * Although it can fail (and will report it) it still makes a best
 * effort to complete its work even when reporting failure. This is
 * because, in the parameter update case there is little choice but to
 * ignore the failure.
 */
static int SetGlobalTransformParameters( avr_v4l2_audio_handle_t *AudioContext,
                                          MME_LowlatencySpecializedGlobalParams_t * GlobalParams)
{
    avr_v4l2_shared_handle_t *SharedContext = AudioContext->SharedContext;
    unsigned int ActualOutputCards = 0;

    MME_SpdifinConfig_t * SpdifInConfig = &GlobalParams->SPDIFin;
    MME_SpdifInFlags_u    SpdifInFlags;
    bool                  mch_enabled = 0;
    int                   result = 0; // function will succeed by default

    // clear value
    SpdifInFlags.U32 = 0;

    // init spdif at init only
    SpdifInConfig->DecoderId = ACC_SPDIFIN_ID;
    SpdifInConfig->StructSize = sizeof(MME_SpdifinConfig_t);

    //default init for the config (the transformer will determine this by itself)
    SpdifInConfig->Config[IEC_SFREQ]           = ACC_FS48k; // default sampling freq
    SpdifInConfig->Config[IEC_NBSAMPLES]       = 0;         // NBSAMPLES will be derived from SpdifinFlags.Bits.BlockSizeInms

    // set the SpdifinDecoder flags 
    //   -> layout flag will be set after the IO settings  (so that layout be assigned according to the connected input)
    SpdifInFlags.Bits.Emphasis       = AudioContext->EmphasisFlag;
    SpdifInFlags.Bits.EnableAAC      = AudioContext->AacDecodeEnable;
    SpdifInFlags.Bits.ForceLookAhead = false;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    SpdifInFlags.Bits.BlockSizeInms  = AVR_LOW_LATENCY_BLOCK_SIZE_IN_MS;
    SpdifInFlags.Bits.HDMIAudioMode  = (ll_force_amode) ? HDMI_AMODE(ll_force_amode) : AudioContext->HdmiAudioMode;
#endif


#if SPDIFIN_API_VERSION >= 0x091005
    SpdifInFlags.Bits.DisableDetection = (ll_disable_detection   || ll_bitexactness_testing);
    SpdifInFlags.Bits.DisableFade      = (ll_disable_spdifinfade || ll_bitexactness_testing);
#endif

            
    // dd+ decoder default configuration (the DRC settings may be updated later when we know we have a good
    // set of mixer parameters).
    SpdifInConfig->DecConfig[DD_CRC_ENABLE]    = ACC_MME_TRUE;
    SpdifInConfig->DecConfig[DD_LFE_ENABLE]    = ACC_MME_TRUE;
    SpdifInConfig->DecConfig[DD_COMPRESS_MODE] = DD_LINE_OUT;
    SpdifInConfig->DecConfig[DD_HDR]           = 0xFF;
    SpdifInConfig->DecConfig[DD_LDR]           = 0xFF;

    GlobalParams->StructSize = sizeof(MME_SpdifinConfig_t) + sizeof(MME_UINT);

    if (SND_PSEUDO_MIXER_MAGIC == SharedContext->mixer_settings.magic)
    {
        struct snd_pseudo_mixer_settings * mixer_settings = &SharedContext->mixer_settings;
        MME_LowLatencyIO_t *IoCfg = GlobalParams->IOCfg;
        struct snd_pseudo_mixer_downstream_topology * Topology = &mixer_settings->downstream_topology;
        unsigned int inp_idx, out_idx;
	bool         mch_enable;
        
	// Use the user settings if the user has requested custom DRC
	if (mixer_settings->drc_enable) {
#if SPDIFIN_API_VERSION >= 0x090623
		SpdifInFlags.Bits.UserDecCfg = true; // use custom decoder settings
#else
		SpdifInFlags.Bits.Align4 = true; // use custom decoder settings
#endif

		SpdifInConfig->DecConfig[DD_COMPRESS_MODE] = mixer_settings->drc_type;
		SpdifInConfig->DecConfig[DD_HDR]           = mixer_settings->hdr;
		SpdifInConfig->DecConfig[DD_LDR]           = mixer_settings->ldr;
	}

        for (inp_idx = 0; inp_idx < AVR_LOW_LATENCY_MAX_INPUT_CARDS; inp_idx ++)
        {
            // configure audio inputs
            IoCfg->ID.DevId = AUDIO_CAPTURE_PCM + inp_idx;
            IoCfg->ID.CardId = AUDIO_CAPTURE_CARD;
            IoCfg->ID.MTId = ACC_LOWLATENCY_MT;
            IoCfg->ID.ExtId = 0;

            IoCfg->StructSize = sizeof(MME_LowLatencyIO_t);

	    // Note : HDMI_LAYOUT1 or HBRA is only achievable on a reader with 8channels;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	    mch_enable = (( AudioContext->HdmiLayout != HDMI_LAYOUT0) &&  (NbChannels[inp_idx] == AUDIO_CHANNELS_HDMI));
#endif		
	    IoCfg->Params.NbChannels   = (mch_enable) ? AUDIO_CHANNELS_HDMI : AUDIO_CHANNELS;  
            IoCfg->Params.WordSize     = ACC_WS32;
            IoCfg->Params.AudioMode    = (mch_enable) ? AudioContext->HdmiAudioMode : ACC_MODE_ID;
            IoCfg->Params.SamplingFreq = ACC_FS48k; // default, will be determined by the firmware

	    // set mch_enabled only if the input is Connected
	    mch_enabled |= (inp_idx == AudioContext->AudioInputId) ? mch_enable : 0;

            // mark the first input as connected
            IoCfg->Connect.Connect = (inp_idx == AudioContext->AudioInputId)?IO_CONNECT:IO_DISCONNECT;
            IoCfg->Connect.Source = inp_idx;
            IoCfg->Connect.Sink = 0;
            IoCfg->Connect.TimeOut = 15; // TODO 20 ms waiting for next block of what?, what size is the block?
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
            IoCfg->Connect.DeviceType = CAPTURE_DEVICE;
            IoCfg->Connect.SBOutEnable = ll_input_log_enable;
#endif
	    DVB_DEBUG("Input card %sconnected card hw:%d,%d (source %d)\n",
		      IoCfg->Connect.Connect == IO_CONNECT ? "" : "dis",
		      IoCfg->ID.CardId, IoCfg->ID.DevId, IoCfg->Connect.Source);
            IoCfg++;
        }

        for( ActualOutputCards=0;
             ActualOutputCards<SND_PSEUDO_MAX_OUTPUTS;
             ActualOutputCards++)
        {
            if (Topology->card[ActualOutputCards].name[0] == '\0')
                break;
        }
    
        DVB_DEBUG("Found %d output cards\n", ActualOutputCards);

#ifdef ENABLE_ONE_OUTPUT_ONLY
        ActualOutputCards = 2;
#endif        

        // then configure outputs
        for (out_idx = 0; out_idx < ActualOutputCards; out_idx ++)
        {
            struct snd_pseudo_mixer_downstream_card * Card = &Topology->card[out_idx];
            const char* alsaname = &Card->alsaname[0];
            unsigned int CardId, DevId;
            enum eIOState IsConnected;
 
            // reuse code from Mixer_mme.cpp
            // TODO: This is a hack version of the card name parsing (only accepts hw:X,Y form).
            //       The alsa name handling should be moved into ksound.
            if (6 != strlen(alsaname) || 0 != strncmp(alsaname, "hw:", 3) ||
                alsaname[3] < '0' || alsaname[4] != ',' || alsaname[5] < '0')
            {
                CardId = 0;
                DevId = 0;
                IsConnected = IO_DISCONNECT;

                DVB_ERROR("Cannot find '%s', try 'hw:X,Y' instead\n", alsaname);
            }
            else
            {
                CardId = alsaname[3] - '0';
                DevId = alsaname[5] - '0';
                IsConnected = IO_CONNECT;
            }
            
            IoCfg->ID.DevId = DevId;
            IoCfg->ID.CardId = CardId;
            IoCfg->ID.MTId = ACC_LOWLATENCY_MT;
            IoCfg->ID.ExtId = 0;

            IoCfg->StructSize = sizeof(MME_LowLatencyIO_t);

            IoCfg->Params.NbChannels = Card->num_channels;
            IoCfg->Params.WordSize = ACC_WS32;
            IoCfg->Params.AudioMode = ACC_MODE_ID;
            IoCfg->Params.SamplingFreq = TranslateIntegerSamplingFrequencyToDiscrete(Card->max_freq);

            IoCfg->Connect.Connect = IsConnected;
            IoCfg->Connect.Source = 0;
            IoCfg->Connect.Sink = out_idx;
            IoCfg->Connect.TimeOut = 15; // TODO 15 ms waiting for next block of what?, what size is the block?
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
            IoCfg->Connect.DeviceType = PLAYBACK_DEVICE;
            IoCfg->Connect.SBOutEnable = ll_output_log_enable;
#endif
            if (AudioContext->SoundcardHandle[out_idx] != NULL)
            {
                ksnd_pcm_close(AudioContext->SoundcardHandle[out_idx]);
                AudioContext->SoundcardHandle[out_idx] = NULL;
            }

            if (0 != ksnd_pcm_open(&AudioContext->SoundcardHandle[out_idx], CardId, DevId, SND_PCM_STREAM_PLAYBACK))
            {
                DVB_ERROR("Cannot open ALSA device %s\n", alsaname);
		result = -1; // record failure
            }
	    else
	    {
                DVB_DEBUG("Configured output card %s (idx %d) DevId %d CardId %d, NbChan %d SamplingFreq %d Connected %d\n", 
			  alsaname, out_idx, IoCfg->ID.DevId, IoCfg->ID.CardId, IoCfg->Params.NbChannels, 
			  IoCfg->Params.SamplingFreq, IoCfg->Connect.Connect);
	    }

            IoCfg++;
        }

        for (out_idx = 0; out_idx < (AVR_LOW_LATENCY_MAX_OUTPUT_CARDS - ActualOutputCards); out_idx ++)
        {
            // unconnected outputs...
            IoCfg->ID.MTId = ACC_LOWLATENCY_MT;
            IoCfg->ID.ExtId = 0;

            IoCfg->StructSize = sizeof(MME_LowLatencyIO_t);

            IoCfg->Params.NbChannels = 2;
            IoCfg->Params.WordSize = ACC_WS32;
            IoCfg->Params.AudioMode = ACC_MODE_ID;
            IoCfg->Params.SamplingFreq = ACC_FS48k;

            IoCfg->Connect.Connect = IO_DISCONNECT;
            IoCfg->Connect.Source = 0;
            IoCfg->Connect.Sink = out_idx + ActualOutputCards;;

            AudioContext->SoundcardHandle[out_idx + ActualOutputCards] = NULL;
            
            DVB_DEBUG("Configured output card (idx %d) DevId %d CardId %d, NbChan %d SamplingFreq %d Connected %d\n", 
                   out_idx, IoCfg->ID.DevId, IoCfg->ID.CardId, IoCfg->Params.NbChannels, 
                   IoCfg->Params.SamplingFreq, IoCfg->Connect.Connect);
            IoCfg++;
        }

        // then configure PCM post processings...
        {
            unsigned int IoCfgSize = sizeof(MME_LowLatencyIO_t) * (AVR_LOW_LATENCY_MAX_INPUT_CARDS + AVR_LOW_LATENCY_MAX_OUTPUT_CARDS);
            MME_LLChainPcmProcessingGlobalParams_t * PcmProcessingsCfg = (MME_LLChainPcmProcessingGlobalParams_t *)((unsigned int)IoCfg + sizeof(MME_LLPcmProcessingGlobalParams_t));
            MME_LLPcmProcessingGlobalParams_t * PcmParams = (MME_LLPcmProcessingGlobalParams_t*) IoCfg;
            unsigned int n, PcmParamsSize;
            OutputEncoding_t OutputEncoding;

            PcmParams->Id = PCMPROCESS_SET_ID(0, ACC_MIX_MAIN);
            PcmParamsSize = sizeof( *PcmParams ); // Just the header - we'll increment as go along

            //these next two would be relevant if doing a single card
            PcmParams->NbPcmProcessings = 0;	    //!< NbPcmProcessings on main[0..3] and aux[4..7]
            PcmParams->AuxSplit = ACC_SPLIT_AUTO;	//! Let the firmware decide where the split should be done
                    
            //otherwise, generate for each card
            for (n = 0; n<ActualOutputCards; n++) 
            {
                PcmParamsSize += FillOutDevicePcmParameters(AudioContext, &PcmProcessingsCfg[n], n, &OutputEncoding, 
                                                          (AudioContext->EmergencyMuteReason != EMERGENCY_MUTE_REASON_NONE));
                
                if( OutputEncoding == OUTPUT_FATPIPE )
                {
                    IoCfg = (MME_LowLatencyIO_t *) &GlobalParams->SPDIFin;
                    IoCfg[AVR_LOW_LATENCY_MAX_INPUT_CARDS + n].Params.SamplingFreq = ACC_FS192k; //TranslateIntegerSamplingFrequencyToDiscrete((Topology->card[out_idx].max_freq)
                }
            }

            PcmParams->StructSize = PcmParamsSize;
            GlobalParams->StructSize += IoCfgSize + PcmParamsSize;
        }
    } // end if (mixer_settings != NULL)
    else
    {
        DVB_ERROR("No valid mixer settings... Set Global will be simple...!\n");
    }
 
    // Set SpdifInFlags  according to the connected IOs.
#if defined(SPDIFIN_API_VERSION) && (SPDIFIN_API_VERSION >= 0x090228)
    SpdifInFlags.Bits.Layout         = (mch_enabled) ? (((ll_bitexactness_testing) && (AudioContext->HdmiLayout != HDMI_LAYOUT0)) ? HDMI_LAYOUT1 : AudioContext->HdmiLayout) : HDMI_LAYOUT0;
#else 
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    SpdifInFlags.Bits.NbChannels     = (mch_enabled) ? AudioContext->HdmiLayout : HDMI_LAYOUT0;
#endif // DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128	
#endif

    SpdifInConfig->Config[IEC_DEEMPH   ]       = SpdifInFlags.U32; // cast to MME_SpdifinFlags_t

    return result;
}

////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between struct
/// ::snd_pseudo_mixer_channel_assignment and enum eAccAcMode.
///
static const struct
{
    enum eAccAcMode AccAcMode;
    const char *Text; // sneaky pre-processor trick used to convert the enumerations to textual equivalents
    struct snd_pseudo_mixer_channel_assignment ChannelAssignment;
    bool SuitableForDirectOutput;
}
ChannelAssignmentLookupTable[] =
{
#define E(mode, p0, p1, p2, p3) { mode, #mode, { SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p0, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p1, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p2, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p3, \
			                         SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED }, true }
// as E but mark the output unsuitable for direct output
#define XXX(mode, p0, p1, p2, p3) { mode, #mode, { SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p0, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p1, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p2, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p3, \
			                         SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED }, false }
// as XXX but for a different reason: there is already a better candidate for direct output, not a bug
#define DUPE(mode, p0, p1, p2, p3) XXX(mode, p0, p1, p2, p3)

    // Weird modes ;-)
  XXX( ACC_MODE10,	    	NOT_CONNECTED,	CNTR_0,		NOT_CONNECTED,	NOT_CONNECTED	), // Mad
    E( ACC_MODE20t,	    	LT_RT,		NOT_CONNECTED, 	NOT_CONNECTED, 	NOT_CONNECTED	),
//XXX( ACC_MODE53,              ...                                                             ), // Not 8ch
//XXX( ACC_MODE53_LFE,          ...                                                             ), // Not 8ch

    // CEA-861 (A to D) modes (in numerical order)
    E( ACC_MODE20,  	    	L_R,		NOT_CONNECTED,	NOT_CONNECTED,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE20,         L_R,            NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE20_LFE,       	L_R,		0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE20_LFE,    	L_R,		0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE30,           	L_R,		CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE30,        	L_R,		CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE30_LFE,       	L_R,		CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE30_LFE,    	L_R,		CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE21,	    	L_R,		NOT_CONNECTED,	CSURR_0,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE21,	    	L_R,		NOT_CONNECTED,	CSURR_0,	NOT_CONNECTED	),
    E( ACC_MODE21_LFE,	    	L_R,		0_LFE1,		CSURR_0,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE21_LFE,    	L_R,		0_LFE1,		CSURR_0,	NOT_CONNECTED	),
    E( ACC_MODE31,           	L_R,		CNTR_0,         CSURR_0,        NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE31,        	L_R,		CNTR_0,         CSURR_0,        NOT_CONNECTED   ),
    E( ACC_MODE31_LFE,       	L_R,		CNTR_LFE1,      CSURR_0,        NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE31_LFE,    	L_R,		CNTR_LFE1,      CSURR_0,        NOT_CONNECTED   ),
    E( ACC_MODE22,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE22,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	NOT_CONNECTED	),
    E( ACC_MODE22_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE22_LFE,   	L_R,		0_LFE1,		LSUR_RSUR,	NOT_CONNECTED	),
    E( ACC_MODE32,	    	L_R,		CNTR_0,		LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE32,	    	L_R,		CNTR_0,		LSUR_RSUR,	NOT_CONNECTED	),
    E( ACC_MODE32_LFE,       	L_R,		CNTR_LFE1,	LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE32_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	NOT_CONNECTED	),
  XXX( ACC_MODE23,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CSURR_0		), // Bug 4518
 DUPE( ACC_HDMI_MODE23,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CSURR_0		),
  XXX( ACC_MODE23_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	CSURR_0		), // Bug 4518
 DUPE( ACC_HDMI_MODE23_LFE,    	L_R,		0_LFE1,		LSUR_RSUR,	CSURR_0		),
    E( ACC_MODE33,              L_R,            CNTR_0,         LSUR_RSUR,      CSURR_0         ), // Bug 4518
 DUPE( ACC_HDMI_MODE33,	    	L_R,		CNTR_0,		LSUR_RSUR,	CSURR_0		),
    E( ACC_MODE33_LFE,          L_R,            CNTR_LFE1,      LSUR_RSUR,      CSURR_0         ), // Bug 4518
 DUPE( ACC_HDMI_MODE33_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	CSURR_0		),
  XXX( ACC_MODE24,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	LSURREAR_RSURREAR ),//Bug 4518
 DUPE( ACC_HDMI_MODE24,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	LSURREAR_RSURREAR ),
  XXX( ACC_MODE24_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	LSURREAR_RSURREAR ),//Bug 4518
 DUPE( ACC_HDMI_MODE24_LFE,	L_R,		0_LFE1,		LSUR_RSUR,	LSURREAR_RSURREAR ),
    E( ACC_MODE34,	    	L_R,		CNTR_0,		LSUR_RSUR,	LSURREAR_RSURREAR ),
 DUPE( ACC_HDMI_MODE34,	    	L_R,		CNTR_0,		LSUR_RSUR,	LSURREAR_RSURREAR ),
    E( ACC_MODE34_LFE,	    	L_R,		CNTR_LFE1,	LSUR_RSUR,	LSURREAR_RSURREAR ),
 DUPE( ACC_HDMI_MODE34_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	LSURREAR_RSURREAR ),
  XXX( ACC_HDMI_MODE40,	    	L_R,		NOT_CONNECTED,	NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE40_LFE,	L_R,		0_LFE1,		NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE50,	    	L_R,		CNTR_0,		NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE50_LFE,	L_R,		CNTR_LFE1,	NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE41,	    	L_R,		NOT_CONNECTED,	CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE41_LFE,    	L_R,		0_LFE1,		CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE51,	    	L_R,		CNTR_0,		CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE51_LFE,	L_R,		CNTR_LFE1,	CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_MODE42,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE42,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CNTRL_CNTRR	),
  XXX( ACC_MODE42_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE42_LFE,	L_R,		0_LFE1,		LSUR_RSUR,	CNTRL_CNTRR	), 
  XXX( ACC_MODE52,	    	L_R,		CNTR_0,		LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE52,	    	L_R,		CNTR_0,		LSUR_RSUR,	CNTRL_CNTRR	),
  XXX( ACC_MODE52_LFE,       	L_R,		CNTR_LFE1,	LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE52_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	CNTRL_CNTRR	),

    // CEA-861 (E) modes
  XXX( ACC_HDMI_MODE32_T100,	L_R,		CNTR_0,		LSUR_RSUR,      CHIGH_0		), // Bug 4518
  XXX( ACC_HDMI_MODE32_T100_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CHIGH_0		), // Bug 4518
  XXX( ACC_HDMI_MODE32_T010,	L_R,		CNTR_0,		LSUR_RSUR,      TOPSUR_0	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T010_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      TOPSUR_0	), // Bug 4518
  XXX( ACC_HDMI_MODE22_T200,	L_R,		NOT_CONNECTED,	LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE22_T200_LFE,L_R,		0_LFE1,		LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE42_WIDE,	L_R,		NOT_CONNECTED,	LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518
  XXX( ACC_HDMI_MODE42_WIDE_LFE,L_R,		0_LFE1,		LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T010,	L_R,		CNTR_0,		LSUR_RSUR,      CSURR_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T010_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CSURR_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T100	,L_R,		CNTR_LFE1,	LSUR_RSUR,      CSURR_CHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T100_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CSURR_CHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T110,	L_R,		CNTR_0,		LSUR_RSUR,      CHIGH_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T110_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CHIGH_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T200,	L_R,		CNTR_0,		LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T200_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE52_WIDE,	L_R,		CNTR_0,		LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518
  XXX( ACC_HDMI_MODE52_WIDE_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518

#if PCMPROCESSINGS_API_VERSION >= 0x100325
    // Unusual speaker topologies (not inclued in CEA-861 E)
  XXX( ACC_MODE30_T100,         L_R,            CNTR_0,         NOT_CONNECTED,  CHIGH_0             ),
  XXX( ACC_MODE30_T100_LFE,     L_R,            CNTR_LFE1,      NOT_CONNECTED,  CHIGH_0             ),
  XXX( ACC_MODE30_T200,         L_R,            CNTR_0,         NOT_CONNECTED,  LHIGH_RHIGH         ),
  XXX( ACC_MODE30_T200_LFE,     L_R,            CNTR_LFE1,      NOT_CONNECTED,  LHIGH_RHIGH         ),
  XXX( ACC_MODE22_T010,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0            ),
  XXX( ACC_MODE22_T010_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      TOPSUR_0            ),
  XXX( ACC_MODE32_T020,         L_R,            CNTR_0,         LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE ),
  XXX( ACC_MODE32_T020_LFE,     L_R,            CNTR_LFE1,      LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE ),
  XXX( ACC_MODE23_T100,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      CHIGH_0             ),
  XXX( ACC_MODE23_T100_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      CHIGH_0             ),
  XXX( ACC_MODE23_T010,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0            ),
  XXX( ACC_MODE23_T010_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      TOPSUR_0            ),
#endif

    // DTS-HD speaker topologies (not included in CEA-861)
    // These are all disabled at the moment because there is no matching entry in the AccAcMode
    // enumeration. The automatic fallback code (below) will select ACC_MODE32_LFE automatically
    // (by disconnecting pair3) if the user requests any of these modes.
//XXX( ACC_MODE32_LFE,       	L_R,		CNTR_LFE1,	LSUR_RSUR,	LSIDESURR_RSIDESURR ), No enum

    // delimiter
    E( ACC_MODE_ID,		NOT_CONNECTED,	NOT_CONNECTED,	NOT_CONNECTED,	NOT_CONNECTED	)

#undef E
#undef XXX
};


////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete audio mode (2.0, 5.1, etc) and convert it to a string.
///
const char * LookupAudioMode( enum eAccAcMode DiscreteMode )
{
    int i;
    
    for (i=0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
	if( DiscreteMode == ChannelAssignmentLookupTable[i].AccAcMode )
	    return ChannelAssignmentLookupTable[i].Text;
    
    // cover the delimiter itself...
    if( DiscreteMode == ChannelAssignmentLookupTable[i].AccAcMode )
	return ChannelAssignmentLookupTable[i].Text;
    
    return "UNKNOWN";
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the current topology.
///
/// This method assume that the channel assignment structure has been been
/// pre-filtered ready for main or auxilliary lookups. In other words the
/// forth pair is *always* disconnected (we will be called twice to handle the
/// forth pair).
///
/// This method excludes from the lookup any format for which the firmware
/// cannot automatically derive (correct) downmix coefficients.
///
enum eAccAcMode TranslateChannelAssignmentToAudioMode( struct snd_pseudo_mixer_channel_assignment ChannelAssignment )
{
    int pair, i;

    // we want to use memcmp() to compare the channel assignments so
    // we must explicitly zero the bits we don't care about
    ChannelAssignment.pair4 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;
    ChannelAssignment.reserved0 = 0;
    ChannelAssignment.malleable = 0;
    
    for (pair=3; pair>=0; pair--)
    {
	for (i=0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
	    if (0 == memcmp(&ChannelAssignmentLookupTable[i].ChannelAssignment,
		            &ChannelAssignment, sizeof(ChannelAssignment)) &&
		ChannelAssignmentLookupTable[i].SuitableForDirectOutput)
		return ChannelAssignmentLookupTable[i].AccAcMode;
	
	// Progressively disconnect pairs of outputs until we find something that matches
	if (pair != 0) {
	    switch (pair) {
	    case 1: ChannelAssignment.pair1 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED; break;
	    case 2: ChannelAssignment.pair2 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED; break;
	    case 3: ChannelAssignment.pair3 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED; break;
	    }
	    
	    DVB_TRACE( "Cannot find matching audio mode - disconnecting pair %d\n", pair );
	}
    }

    DVB_TRACE( "Cannot find matching audio mode - falling back to ACC_MODE20t (Lt/Rt stereo)\n" );
    return ACC_MODE20t;
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the 'natural' channel assignment for an audio mode.
///
/// This method and Mixer_Mme_c::TranslateChannelAssignmentToAudioMode are *not* reversible for
/// audio modes that are not marked as suitable for output.
///
struct snd_pseudo_mixer_channel_assignment TranslateAudioModeToChannelAssignment(
	enum eAccAcMode AudioMode )
{
    const struct snd_pseudo_mixer_channel_assignment Malleable = { .malleable = 1 };
    const struct snd_pseudo_mixer_channel_assignment Zeros = { 0 };
    int i;

    if (ACC_MODE_ID == AudioMode)
	    return Malleable;

    for (i=0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++) {
	if (ChannelAssignmentLookupTable[i].AccAcMode == AudioMode) {
	    return ChannelAssignmentLookupTable[i].ChannelAssignment;
	}
    }
    
    // no point in stringizing the mode - we know its not in the table
    DVB_ERROR("Cannot find matching audio mode (%d)\n", AudioMode);
    return Zeros;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Configure the downmix processing appropriately.
///
/// Code reused (and simplified) from mixer_mme.cpp: thanks Dan!
///
/// Contract: The PcmParams structure is zeroed before population. At the time
/// it is handed to this method the CMC settings have already been stored and
/// the downmix settings are untouched (still zero).
///
/// This method is, semanticlly, part of FillOutDevicePcmParameters() (hence
/// it is inlined) but is quite complex enough to justify spliting it out into
/// a seperate method.
///
static int FillOutDeviceDownmixParameters( avr_v4l2_audio_handle_t *AudioContext,
					   MME_LLChainPcmProcessingGlobalParams_t * PcmParams,  
					   int dev_num )
{
    // put these are the top because their values are very transient - don't expect them to maintain their
    // values between blocks...
    union {
        struct snd_pseudo_mixer_channel_assignment ca;
        uint32_t n;
    } TargetOutputId, TargetInputId, OutputId, InputId;
  
    int struct_size = 0;
    const struct snd_pseudo_mixer_downmix_rom *DownmixFirmware = AudioContext->SharedContext->downmix_firmware;
    enum eAccAcMode OutputMode = (enum eAccAcMode) PcmParams->CMC.Config[CMC_OUTMODE_MAIN];
    enum eAccAcMode InputMode = AudioContext->LLDecoderStatus.CurrentDecoderAudioMode;

    bool EnableDMix = (InputMode != OutputMode) && (!ll_bitexactness_testing);

    MME_DMixGlobalParams_t * DMix = &PcmParams->Dmix;
    DMix->Id = PCMPROCESS_SET_ID(ACC_PCM_DMIX_ID, dev_num);
    // Don't unapply the downmix (even if it is a no-op) because we need to allow the firmware
    // to have a downmix enabled if the incoming topology changes to one where downmix is needed.
    DMix->Apply = ACC_MME_ENABLED;
    DMix->StructSize = sizeof( *DMix );
    struct_size += sizeof( *DMix );
    
    DMix->Config[DMIX_USER_DEFINED] = ACC_MME_FALSE;
    DMix->Config[DMIX_STEREO_UPMIX] = ACC_MME_FALSE;
    DMix->Config[DMIX_MONO_UPMIX] = ACC_MME_FALSE;
    DMix->Config[DMIX_MEAN_SURROUND] = ACC_MME_FALSE;
    DMix->Config[DMIX_SECOND_STEREO] = ACC_MME_FALSE;
    DMix->Config[DMIX_MIX_LFE] = ACC_MME_FALSE;
    DMix->Config[DMIX_NORMALIZE] = ACC_MME_TRUE;
    DMix->Config[DMIX_NORM_IDX] = 0;
    DMix->Config[DMIX_DIALOG_ENHANCE] = ACC_MME_FALSE;
    
    // if the downmixer isn't enabled or if we don't need to honour the downmix 'firmware' then we're done
    if (!EnableDMix)
        return struct_size;

    if (DownmixFirmware) {
	enum eAccAcMode EffectiveInputMode = InputMode;
	uint64_t TargetSortValue, CurrentSortValue;
	unsigned int low, high;

	if (ACC_MODE_1p1 == InputMode) {
		if (AudioContext->ChannelSelect == V4L2_AVR_AUDIO_CHANNEL_SELECT_STEREO)
			EffectiveInputMode = ACC_MODE20;
		else
			EffectiveInputMode = ACC_MODE10;
	}

	// generate the target sort value we must search for in the downmix firmware index
	DVB_ASSERT(ACC_MODE_ID == PcmParams->CMC.Config[CMC_OUTMODE_AUX]); // no auxillary output at the moment
	TargetOutputId.ca = TranslateAudioModeToChannelAssignment(OutputMode);
	TargetInputId.ca = TranslateAudioModeToChannelAssignment(EffectiveInputMode);
	TargetSortValue = ((uint64_t) TargetInputId.n << 32) + TargetOutputId.n;

	// single comparision (i.e. fixed time) binary search
	low = 0;
	high = DownmixFirmware->header.num_index_entries;

	while (low < high)
	{
	    unsigned int mid = low + ((high - low) / 2); // because (high + low) / 2 is not overflow-safe
	
	    InputId.ca = DownmixFirmware->index[mid].input_id;
	    OutputId.ca = DownmixFirmware->index[mid].output_id;
	    CurrentSortValue = ((uint64_t) InputId.n << 32) + OutputId.n;
	
	    if( CurrentSortValue < TargetSortValue )
		low = mid + 1;
	    else
		high = mid;
	}
	DVB_ASSERT(low == high);
    
	// regenerate the sort value; it is stale if CurrentSortValue < TargetSortValue during final iteration
	InputId.ca = DownmixFirmware->index[low].input_id;
	OutputId.ca = DownmixFirmware->index[low].output_id;
	CurrentSortValue = ((uint64_t) InputId.n << 32) + OutputId.n;
    
	// malleable surface handling (a malleable output surface can dynamically change its
	// channel topology depending on the data being presented).
	//
	// For a malleable surface the low order 32-bits of the TargetSortValue are 0x80000000. This
	// is specified to matched by any entry in the firmware that has the top bit set (i.e.
	// the lowest 31 bits are ignored). The binary search above has been carefully tuned to
	// select the right value even with an inexact match (the tuning is mostly making the
	// malleable bit the most significant bit).
	//
	// Having got this far we need to detect if we are targeting malleable surface and update
	// the CMC config.
	if (TargetOutputId.ca.malleable && OutputId.ca.malleable) {
	    OutputMode = TranslateChannelAssignmentToAudioMode(OutputId.ca);
	    DVB_TRACE("Downmix firmware requested %s as malleable output surface\n",
		      LookupAudioMode(OutputMode));
		
	    // update the CMC configuration with the value found in the firmware
	    PcmParams->CMC.Config[CMC_OUTMODE_MAIN] = OutputMode;

	    // update the target value since now we know what we were looking for
	    TargetSortValue = CurrentSortValue;
	}

	if( CurrentSortValue == TargetSortValue )
	{
	    const struct snd_pseudo_mixer_downmix_index *index = DownmixFirmware->index + low;
	    const snd_pseudo_mixer_downmix_Q15 *data = (snd_pseudo_mixer_downmix_Q15 *)
		(DownmixFirmware->index + DownmixFirmware->header.num_index_entries);
	    const snd_pseudo_mixer_downmix_Q15 *table = data + index->offset;
	    int x, y, i;
	
	    DMix->Config[DMIX_USER_DEFINED] = ACC_MME_TRUE;

	    // Copy the downmix table from the 'ROM' to the firmware parameters
	    for (x=0; x<index->output_dimension; x++)
		for (y=0; y<index->input_dimension; y++)
		    DMix->MainMixTable[x][y] =	table[x * index->input_dimension + y];

	    // Dual-mono channel selection occurs during downmix. Therefore if we are processing
	    // a dual-mono stream (and have a single stream selected) we must migrate the
	    // coefficients from the centre channel to L or R (depending on mode)
	    if (ACC_MODE_1p1 == InputMode &&
		AudioContext->ChannelSelect != V4L2_AVR_AUDIO_CHANNEL_SELECT_STEREO)
            {
		for (x=0; x<index->output_dimension; x++)
		{
		    const int MAIN_LEFT = 0;
		    const int MAIN_RIGHT = 1;
		    const int MAIN_CNTR = 3;

		    int active_ch = (AudioContext->ChannelSelect == V4L2_AVR_AUDIO_CHANNEL_SELECT_MONO_LEFT ? MAIN_LEFT : MAIN_RIGHT);
			     
		    DMix->MainMixTable[x][active_ch] = DMix->MainMixTable[x][MAIN_CNTR];
		    DMix->MainMixTable[x][MAIN_CNTR] = 0;
		}
	    }
	
	    DVB_DEBUG("Found custom downmix table for %s to %s\n",
		      LookupAudioMode(EffectiveInputMode), LookupAudioMode(OutputMode));
	    for (i=0; i<DMIX_NB_IN_CHANNELS; i++)
		DVB_DEBUG("%04x %04x %04x %04x %04x %04x %04x %04x\n",
			  DMix->MainMixTable[i][0], DMix->MainMixTable[i][1],
			  DMix->MainMixTable[i][2], DMix->MainMixTable[i][3],
			  DMix->MainMixTable[i][4], DMix->MainMixTable[i][5],
			  DMix->MainMixTable[i][6], DMix->MainMixTable[i][7]);
	}
	else
	{
	    // not an error but certainly worthy of note
	    DVB_TRACE("Downmix firmware has no entry for %s to %s - using defaults\n",
		      LookupAudioMode(EffectiveInputMode), LookupAudioMode(OutputMode));
	}
    }

    return struct_size;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Code reused from mixer_mme.cpp: thanks Dan!
///
/// Configure the SpdifOut processing appropriately.
///
/// Contract: The PcmParams structure is zeroed before population. At the time
/// it is handed to this method it may already have been partially filled in.
///
/// This method is, semanticlly, part of FillOutDevicePcmParameters() (hence
/// it is inlined) but is quite complex enough to justify spliting it out into
/// a seperate method.
///
/// The primary responsibility of the function is to set dynamically changing
/// fields (such as the sample rate), static fields are provided by userspace.
/// The general strategy taken to merge the two sources is that is that where 
/// fields are supplied by the implementation (e.g. sample rate,
/// LPCM/other, emphasis) we ignore the values provided by userspace. Otherwise
/// we make every effort to reflect the userspace values, we make no effort
/// to validate their correctness.
///
static int FillOutDeviceSpdifParameters( avr_v4l2_audio_handle_t *AudioContext,
					 MME_LLChainPcmProcessingGlobalParams_t * PcmParams,
					 struct snd_pseudo_mixer_settings * mixer_settings,
					 int dev_num, OutputEncoding_t OutputEncoding )
{
    unsigned int struct_size = 0;
    //
    // Special case FatPipe which uses a different parameter structure and handles channel status
    // differently.
    //
   
    if( OutputEncoding == OUTPUT_FATPIPE )
    {
        MME_FatpipeGlobalParams_t * FatPipe = &PcmParams->FatPipeOrSpdifOut;

        FatPipe->Id = PCMPROCESS_SET_ID(ACC_PCM_SPDIFOUT_ID, dev_num);
        FatPipe->StructSize = sizeof( MME_FatpipeGlobalParams_t );
        struct_size += sizeof( MME_FatpipeGlobalParams_t );
        FatPipe->Apply = ACC_MME_ENABLED;
        FatPipe->Config.Mode = 1; // FatPipe mode
        FatPipe->Config.UpdateSpdifControl = 1;
        FatPipe->Config.UpdateMetaData = 1;
        FatPipe->Config.Upsample4x = 1;
        FatPipe->Config.ForceChannelMap = 0;

        // metadata gotten from the userspace
        FatPipe->Config.FEC = ((mixer_settings->fatpipe_metadata.md[0] >> 6) & 1);
        FatPipe->Config.Encryption = ((mixer_settings->fatpipe_metadata.md[0] >> 5) & 1);
        FatPipe->Config.InvalidStream = ((mixer_settings->fatpipe_metadata.md[0] >> 4) & 1);
        FatPipe->MetaData.LevelShiftValue = mixer_settings->fatpipe_metadata.md[6] & 31;
        FatPipe->MetaData.Lfe_Gain = mixer_settings->fatpipe_metadata.md[2] & 31;
#if PCMPROCESSINGS_API_VERSION >= 0x100415
	FatPipe->MetaData.Effects = mixer_settings->fatpipe_metadata.md[14] & 0xffff;
#endif

        // the firmware will manage non-userspace metadata internally (i.e. get them from the decoder itself).

        memset( FatPipe->Spdifout.Validity, 0xff, sizeof( FatPipe->Spdifout.Validity ));
	
        // FatPipe is entirely prescriptitive w.r.t the channel status bits so we can ignore the userspace
        // provided values entirely
		
        /* consumer, not linear PCM, copyright is asserted, mode 00, pcm encoder,
         * no generation indication, channel 0, source 0, level II clock accuracy,
         * ?44.1KHz?, 24-bit
         */
        FatPipe->Spdifout.ChannelStatus[0] = 0x02020000;
        FatPipe->Spdifout.ChannelStatus[1] = 0x0B000000; // report 24bits wordlength
        
        DVB_DEBUG("Configure FatPipe!\n");
    }
    else
    {
        // A convenience macro to allow the entries below to be copied from the IEC
        // standards document. The sub-macro chops of the high bits (the ones that
        // select which word within the channel mask we need). The full macro then
        // performs an endian swap since the firmware expects big endian values.
#define B(x) (((_B(x) & 0xff) << 24) | ((_B(x) & 0xff00) << 8) | ((_B(x) >> 8)  & 0xff00) | ((_B(x) >> 24) & 0xff))
#define _B(x) (1 << ((x) % 32))

        const unsigned int use_of_channel_status_block = B(0);
        const unsigned int linear_pcm_identification = B(1);
        //const unsigned int copyright_information = B(2);
        const unsigned int additional_format_information = B(3) | B(4) | B(5);
        const unsigned int mode = B(6) | B(7);
        //const unsigned int category_code = 0x0000ff00;
        //const unsigned int source_number = B(16) | B(17) | B(18) | B(19);
        //const unsigned int channel_number = B(20) | B(21) | B(22) | B(23);
        const unsigned int sampling_frequency = B(24) | B(25) | B(26) | B(27);
        //const unsigned int clock_accuracy = B(28) | B(29);
        const unsigned int word_length = B(32) | B(33) | B(34) | B(35);
        //const unsigned int original_sampling_frequency = B(36) | B(37) | B(38) | B(39);
        //const unsigned int cgms_a = B(40) | B(41);
#undef B
#undef _B
    
        const int STSZ = 6;
        unsigned int ChannelStatusMask[STSZ];

        MME_SpdifOutGlobalParams_t * SpdifOut = ((MME_SpdifOutGlobalParams_t *) (&PcmParams->FatPipeOrSpdifOut));
        int i;

        //
        // Copy the userspace mask for the channel status bits. This consists mostly
        // of set bits.
        //
    
        for (i=0; i<STSZ; i++)
            ChannelStatusMask[i] = mixer_settings->iec958_mask.status[i*4 + 0] << 24 |
            mixer_settings->iec958_mask.status[i*4 + 1] << 16 |
            mixer_settings->iec958_mask.status[i*4 + 2] <<  8|
            mixer_settings->iec958_mask.status[i*4 + 3] <<  0;
    
        // these should never be overlaid
        ChannelStatusMask[0] &= ~( use_of_channel_status_block |
                                   linear_pcm_identification |
                                   additional_format_information | /* auto fill in for PCM, 000 for coded data */
                                   mode |
                                   sampling_frequency ); /* auto fill in */
        ChannelStatusMask[1] &= ~( word_length );
    
    
        //
        // Handle, in a unified manner, all the IEC SPDIF formatings
        //
    
        SpdifOut->Id = PCMPROCESS_SET_ID(ACC_PCM_SPDIFOUT_ID, dev_num);
        SpdifOut->StructSize = sizeof( MME_FatpipeGlobalParams_t ); // use fatpipe size (to match frozen structure)
        struct_size += sizeof( MME_FatpipeGlobalParams_t ); // use fatpipe size
    
        if( OutputEncoding == OUTPUT_IEC60958 )
        {
            SpdifOut->Apply = ACC_MME_ENABLED;
            SpdifOut->Config.Mode = 0; // SPDIF mode
            SpdifOut->Config.UpdateSpdifControl = 1;
            SpdifOut->Config.SpdifCompressed = 0;
            SpdifOut->Config.AddIECPreamble = 0;
            SpdifOut->Config.ForcePC = 0; // useless in this case: for compressed mode only
        
            SpdifOut->Spdifout.ChannelStatus[0] = 0x00000000; /* consumer, linear PCM, mode 00 */
            SpdifOut->Spdifout.ChannelStatus[1] = 0x0b000000; /* 24-bit word length */
        }
        else
        {
            SpdifOut->Apply = ACC_MME_DISABLED;
        }
    
        //
        // The mask is now complete so we can overlay the userspace supplied channel
        // status bits.
        //
    
        for (i=0; i<STSZ; i++)
            SpdifOut->Spdifout.ChannelStatus[i] |=
            ChannelStatusMask[i] &
            ( mixer_settings->iec958_metadata.status[i*4 + 0] << 24 |
              mixer_settings->iec958_metadata.status[i*4 + 1] << 16 |
              mixer_settings->iec958_metadata.status[i*4 + 2] <<  8|
              mixer_settings->iec958_metadata.status[i*4 + 3] <<  0 );
    }
    return struct_size;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Code reused (and simplified) from mixer_mme.cpp: thanks Dan!
/// 
/// Fill out the PCM post-processing required for a single physical output.
///
unsigned int FillOutDevicePcmParameters( avr_v4l2_audio_handle_t *AudioContext,
					 MME_LLChainPcmProcessingGlobalParams_t *PcmParams, 
					 int dev_num,
                                         OutputEncoding_t *OutputEncoding,
                                         bool DeployEmergencyMute)
{
    avr_v4l2_shared_handle_t *SharedContext = AudioContext->SharedContext;
    struct snd_pseudo_mixer_settings * mixer_settings = &SharedContext->mixer_settings;
    unsigned int OutputFlags = mixer_settings->downstream_topology.card[dev_num].flags;
    unsigned int struct_size = 0;
    
    // check for fatpipe, iec60958 or pcm output encoding
    if( OutputFlags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING )
    {
        if ((mixer_settings->spdif_encoding == SND_PSEUDO_MIXER_SPDIF_ENCODING_FATPIPE) && (OutputFlags & SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE))
        {
            *OutputEncoding = OUTPUT_FATPIPE;
        }
        else 
        {
            *OutputEncoding = OUTPUT_IEC60958;
        }
    }
    else
    {
        *OutputEncoding = OUTPUT_PCM;
    }

    DVB_DEBUG("Audio encoding for output %d is %d\n", dev_num, *OutputEncoding);

    {
        MME_BassMgtGlobalParams_t * BassMgt = &PcmParams->BassMgt;
        BassMgt->Id = PCMPROCESS_SET_ID(ACC_PCM_BASSMGT_ID, dev_num);
        BassMgt->StructSize = sizeof( *BassMgt );
        struct_size += sizeof( *BassMgt );
        if( mixer_settings->bass_mgt_bypass )
        {
            BassMgt->Apply = ACC_MME_DISABLED;
        }
        else
        {
            int i;
            const int BASSMGT_ATTENUATION_MUTE = 70;

            BassMgt->Apply =  (ll_bitexactness_testing) ? ACC_MME_DISABLED : ACC_MME_ENABLED;
            BassMgt->Config[BASSMGT_TYPE] = BASSMGT_VOLUME_CTRL;
            BassMgt->Config[BASSMGT_LFE_OUT] = ACC_MME_TRUE;
            BassMgt->Config[BASSMGT_BOOST_OUT] = ACC_MME_DISABLED;
            BassMgt->Config[BASSMGT_PROLOGIC_IN] = ACC_MME_TRUE;
            BassMgt->Config[BASSMGT_OUT_WS] = 32;
            BassMgt->Config[BASSMGT_LIMITER] = 0;
            BassMgt->Config[BASSMGT_INPUT_ROUTE] = ACC_MIX_MAIN;
            BassMgt->Config[BASSMGT_OUTPUT_ROUTE] = ACC_MIX_MAIN;
            // this is rather naughty (the gain values are linear everywhere else
            // but are mapped onto a logarithmic scale here)
            memset(BassMgt->Volume, BASSMGT_ATTENUATION_MUTE, sizeof(BassMgt->Volume));
            for( i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++ )
                BassMgt->Volume[i] = -mixer_settings->master_volume[i];
            BassMgt->DelayUpdate = ACC_MME_FALSE;
            // From ACC-BL023-3BD
            BassMgt->CutOffFrequency = 100; // within [50, 200] Hz 
            BassMgt->FilterOrder = 2;       // could be 1 or 2 .

	    DVB_DEBUG("BassMgt[%d]:  %d  %d  %d  %d  %d  %d\n", dev_num,
		      BassMgt->Volume[0], BassMgt->Volume[1],
		      BassMgt->Volume[2], BassMgt->Volume[3],
                      BassMgt->Volume[4], BassMgt->Volume[5]);
        }
    }

    {
        MME_DCRemoveGlobalParams_t * DCRemove = &PcmParams->DCRemove;
        DCRemove->Id = PCMPROCESS_SET_ID(ACC_PCM_DCREMOVE_ID, dev_num);
        DCRemove->StructSize = sizeof( *DCRemove );
        struct_size += sizeof( *DCRemove );
        DCRemove->Apply = ( mixer_settings->dc_remove_enable ? ACC_MME_ENABLED : ACC_MME_DISABLED );
    }

    {
        MME_Resamplex2GlobalParams_t * Resamplex2 = &PcmParams->Resamplex2;
        Resamplex2->Id = PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, dev_num);
        Resamplex2->StructSize = sizeof( *Resamplex2 );
        struct_size += sizeof( *Resamplex2 );

        Resamplex2->Apply = (ll_force_downsample) ? ACC_MME_ENABLED : ACC_MME_AUTO;

        if (*OutputEncoding == OUTPUT_FATPIPE)
        {
            // The fatpipe link is able to convey from 8 to 48kHz signals (upsampled by 4)
            // Streams with a sampling frequency superior to 48 kHz have to be downsampled
            // but this may be a waste of time to upsample stream beneath 32 kHz
            Resamplex2->Range = ACC_FSRANGE_48k;
        }
	else
        {
            Resamplex2->Apply = (ll_bitexactness_testing) ? ACC_MME_DISABLED : Resamplex2->Apply;  
            Resamplex2->Range = TranslateIntegerSamplingFrequencyToRange( mixer_settings->downstream_topology.card[dev_num].max_freq );
        }
    }

    {    
        MME_CMCGlobalParams_t * CMC = &PcmParams->CMC;
        CMC->Id = PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, dev_num);
        CMC->StructSize = sizeof( *CMC );
        struct_size += sizeof( *CMC );

	CMC->Config[CMC_DUAL_MODE] =
	    AudioContext->ChannelSelect == V4L2_AVR_AUDIO_CHANNEL_SELECT_MONO_LEFT  ? ACC_DUAL_LEFT_MONO :
	    AudioContext->ChannelSelect == V4L2_AVR_AUDIO_CHANNEL_SELECT_MONO_RIGHT ? ACC_DUAL_RIGHT_MONO :
									                ACC_DUAL_LR;
        CMC->Config[CMC_PCM_DOWN_SCALED] = ACC_MME_FALSE;
        CMC->CenterMixCoeff = ACC_M3DB;
        CMC->SurroundMixCoeff = ACC_M3DB;
    
        if (*OutputEncoding == OUTPUT_FATPIPE)
        {
	    /* FatPipe is a malleable output surface, mark it as such (ACC_MODE_ID is special cased in FillOutDeviceDownmixParameters) */
	    CMC->Config[CMC_OUTMODE_MAIN] = ACC_MODE_ID;
        }
        else if (ll_bitexactness_testing)
        {
            // if bitexactness test is requested on Fatpipe, then bypass the samples (don't downmix them) 
            CMC->Config[CMC_OUTMODE_MAIN] = ACC_MODE_ID;
        }
	else
        {
            CMC->Config[CMC_OUTMODE_MAIN] = ACC_MODE20;
        }

        CMC->Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
    }

    //

    struct_size += FillOutDeviceDownmixParameters( AudioContext, PcmParams, dev_num );

    //
    
    struct_size += FillOutDeviceSpdifParameters( AudioContext, PcmParams, mixer_settings,
						 dev_num, *OutputEncoding );

    //

    {
        MME_LimiterGlobalParams_t *	Limiter = &PcmParams->Limiter;
        Limiter->Id = PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, dev_num);
        Limiter->StructSize = sizeof( *Limiter );
        struct_size += sizeof( *Limiter );
        Limiter->Apply = (ll_bitexactness_testing) ? ACC_MME_DISABLED : ACC_MME_ENABLED;

        if ((*OutputEncoding == OUTPUT_FATPIPE) || (mixer_settings->chain_limiter_enable[dev_num] == false))
            Limiter->LimiterEnable = ACC_MME_FALSE;
        else
            Limiter->LimiterEnable = ACC_MME_TRUE;

        // soft mute is triggered by a 0 to 1 transition, and unmute by a 1 to 0 transition
        if ( !mixer_settings->chain_enable[dev_num] ||
             ( DeployEmergencyMute &&
               SND_PSEUDO_MIXER_EMERGENCY_MUTE_NO_MUTE != mixer_settings->emergency_mute ) )
        {
            // We deploy the emergency mute only if the route is pre or post mix
            Limiter->SoftMute = 1;

#if defined PCMPROCESSINGS_API_VERSION && (PCMPROCESSINGS_API_VERSION >= 0x090301)
	    Limiter->EmergencyMute.bits.State      = ACC_LIMITER_MUTE;     // request Mute
	    Limiter->EmergencyMute.bits.Chains     = 0xF;           // apply to all chains
	    Limiter->EmergencyMute.bits.Override   = ACC_MME_TRUE;  // apply mute without providing MuteID
	    Limiter->EmergencyMute.bits.AutoUnmute = ACC_MME_FALSE; // prevent the FW to unmute by itself if it triggers emergency mute
	    Limiter->EmergencyMute.bits.MuteId     = 0;
#endif
        }
        else
        {
            Limiter->SoftMute = 0;

#if defined PCMPROCESSINGS_API_VERSION && (PCMPROCESSINGS_API_VERSION >= 0x090301)
	    Limiter->EmergencyMute.bits.State      = ACC_LIMITER_PLAY; // request UnMute
	    Limiter->EmergencyMute.bits.Chains     = 0xF;          // apply to all chains
	    Limiter->EmergencyMute.bits.Override   = ACC_MME_TRUE; // unmute irrespective of MuteID
	    Limiter->EmergencyMute.bits.AutoUnmute = SND_PSEUDO_MIXER_EMERGENCY_MUTE_NO_MUTE == mixer_settings->emergency_mute ?
	      ACC_MME_TRUE : ACC_MME_FALSE; // Let the FW unmute by itself if it triggers emergency mute
	    Limiter->EmergencyMute.bits.MuteId     = 0;
#endif
        }

	// Update the local copy of the mute status bits
	// TODO: Honour the magic time stamp
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	AudioContext->LLDecoderStatus.CurrentMuteStatus.State = Limiter->EmergencyMute.bits.State;
#endif
        Limiter->DelayEnable = 1;  // enable delay engine
        Limiter->MuteDuration = AVR_LIMITER_MUTE_RAMP_DOWN_PERIOD;
        Limiter->UnMuteDuration = AVR_LIMITER_MUTE_RAMP_UP_PERIOD;
	Limiter->ReportStatus = ACC_MME_TRUE; // allows us to observe the emergency mute status
        Limiter->Gain = mixer_settings->chain_volume[dev_num];
        // During initialization (i.e. when MMEInitialized is false) we must set HardGain to 1. This
        // prevents the Limiter from setting the initial gain to -96db and applying gain smoothing
        // (which takes ~500ms to reach 0db) during startup.
        Limiter->HardGain = 1;
	    
        //Limiter->Threshold = ;
        Limiter->DelayBuffer = NULL; // delay buffer will be allocated by firmware
        Limiter->DelayBufSize = 0;
        Limiter->Delay = mixer_settings->chain_latency[dev_num] +
                         AudioContext->CompensatoryLatency; //in ms
	
	DVB_DEBUG("Limiter[%d]  Gain %d  Delay %d\n", dev_num, Limiter->Gain, Limiter->Delay);
    }

#if DRV_MULTICOM_PERFLOG_VERSION >= 0x090605
    if (AudioContext->PostMortemForce)
    {
	DVB_TRACE("Requesting firmware crash (forced post-mortem)\n");
	PcmParams->CMC.Id = PCMPROCESS_SET_ID(ACC_PCM_DEATH_ID, dev_num);
	AudioContext->PostMortemForce = false;
    }
#endif

    return struct_size;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Re-order the transform status so we can access it using the strongly typed values.
///
int ProcessSpecializedTransformStatus( MME_LowlatencySpecializedTransformStatus_t *TransformStatus )
{
	MME_LowlatencySpecializedTransformStatus_t NewStatus = {{ 0 }};
	int Result = 0;

	MME_LxAudioDecoderFrameExtendedStatus_t *ExtendedStatus;
	int BytesRemaining;
	MME_PcmProcessingStatusTemplate_t *Template;

	// Re-interpret TransformStatus as the firmware expects it to be
	ExtendedStatus = (void *) TransformStatus;
	BytesRemaining = ExtendedStatus->PcmStatus.BytesUsed;
	Template = &(ExtendedStatus->PcmStatus.PcmStatus);

	// Copy out the easy bits
	NewStatus.DecoderFrameStatus = ExtendedStatus->DecStatus;
	NewStatus.BytesUsed = sizeof(NewStatus) - sizeof(NewStatus.DecoderFrameStatus);

	// Examine each item in turn and copy into the correct part of NewStatus
	while (BytesRemaining > 0 && Template->StructSize > 0) {
		DVB_DEBUG("Template %p  BytesRemaining %d  Id %08x  StructSize %d\n",
			  Template, BytesRemaining, Template->Id, Template->StructSize);
		switch(Template->Id) {
// TODO: Can't find this macro in the ACF headers
#define PCMPROCESS_GET_ID(id) ((id >> 8) - ACC_PCMPROCESS_MAIN_MT)

		case PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, 0):
		case PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, 1):
		case PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, 2):
		case PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, 3):
			if (sizeof(MME_PcmProcessingOutputChainStatus_t) == Template->StructSize) {
				DVB_DEBUG("Got CMC reply for chain %d\n", PCMPROCESS_GET_ID(Template->Id));
				memcpy(&(NewStatus.OutputChainStatus[PCMPROCESS_GET_ID(Template->Id)]),
				       Template, sizeof(MME_PcmProcessingOutputChainStatus_t));
			} else {
				DVB_ERROR("Bad MME_PcmProcessingOutputChainStatus_t size "
					  "(firmware version skew?)\n");
				Result = -1;
			}
			break;
		       
		case PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, 0):
		case PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, 1):
		case PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, 2):
		case PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, 3):
			if (sizeof(MME_LimiterStatus_t) == Template->StructSize) {
				DVB_DEBUG("Got limiter reply for chain %d\n", PCMPROCESS_GET_ID(Template->Id));
				memcpy(&(NewStatus.LimiterStatus[PCMPROCESS_GET_ID(Template->Id)]),
				       Template, sizeof(MME_LimiterStatus_t));
			} else {
				DVB_ERROR("Bad MME_LimiterStatus_t size (firmware version skew?)\n");
				Result = -1;
			}
			break;

		default:
			DVB_ERROR("Unknown structure in reply - Id %08x, Size %d\n",
				  Template->Id, Template->StructSize);
			break;
#undef PCMPROCESS_GET_ID
		}

		BytesRemaining -= Template->StructSize;
		Template = (void *) (((char *) Template) + Template->StructSize);
	}

	// Commit the rearrangement
	*TransformStatus = NewStatus;
	return Result;
}

static void AvrAudioLLConditionalAbort(avr_v4l2_audio_handle_t *AudioContext, MME_Command_t *Cmd)
{
    MME_CommandState_t State = Cmd->CmdStatus.State;
    MME_ERROR Err;
 
    // all commands are initialized to the MME_COMMAND_FAILED state so we don't need
    // to consult anything else to detect if the command is still in flight.
    if (MME_COMMAND_COMPLETED != State && MME_COMMAND_FAILED != State)
    {
	DVB_TRACE("Aborting command %x (%s)\n",
		  Cmd->CmdStatus.CmdId, LookupMmeCommandCode(Cmd->CmdCode));

	Err = MME_AbortCommand(AudioContext->LLTransformerHandle, Cmd->CmdStatus.CmdId);
	if (MME_SUCCESS != Err)
	    DVB_ERROR("Failure whilst aborting command %x (%s)\n",
		      Cmd->CmdStatus.CmdId, LookupMmeCommandCode(Cmd->CmdCode));
	    // no recovery possible
    }
}

static void AvrAudioLLCommandReset(MME_Command_t *Cmd)
{
    memset(Cmd, 0, sizeof(*Cmd));
    Cmd->CmdStatus.State = MME_COMMAND_FAILED;
}

static void AvrAudioLLStateReset(avr_v4l2_audio_handle_t *AudioContext)
{
    AudioContext->UpdateMixerSettings = false;
    AudioContext->GotTransformCommandCallback = false;
    AudioContext->GotSetGlobalCommandCallback = false;
    AudioContext->NeedAnotherSetGlobalCommand = false;
    AudioContext->GotSendBufferCommandCallback = false;

    AvrAudioLLCommandReset(&AudioContext->TransformCommand);
    AvrAudioLLCommandReset(&AudioContext->SetGlobalCommand);
    // don't reset the SendBuffer commands (there are one time initialized during AvrAudioNew and
    // are preconfigured with memory pointers).

    atomic_set(&(AudioContext->CommandsInFlight), 0);
    atomic_set(&(AudioContext->ReuseOfGlobals), 0);
    atomic_set(&(AudioContext->ReuseOfTransform), 0);
}

static int AvrAudioLLThreadCleanup(avr_v4l2_audio_handle_t *AudioContext)
{
    MME_ERROR MMEStatus;
    int i;
    int retries = 8;
    sigset_t allset, oldset;
    ktime_t elapsed = ktime_get();

    // it is possible for us to reach this point due to a runtime error. if
    // that is the case we must block until we are requested to exit (in order
    // to avoid race conditions in the code that reaps the thread)
    if (!AudioContext->ThreadShouldStop) {
	// notify userspace that something is not right
	AudioContext->ThreadWaitingForStop = true;
	if (AudioContext->IsSysFsStructureCreated)
	    sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "input_format");

	DVB_TRACE("Audio thread terminated early - waiting for shutdown request\n");
	wait_event(AudioContext->WaitQueue, AudioContext->ThreadShouldStop);

	AudioContext->ThreadWaitingForStop = false;
    }

    // if the transformer was initialized then we need to tidy that up
    if (AudioContext->IsLLTransformerInitialized) {
	do {
	    AudioContext->GotTransformCommandCallback =
		AudioContext->GotSetGlobalCommandCallback =
  	        AudioContext->GotSendBufferCommandCallback = false;
	    
	    for (i = 0; i < AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS; i++)
		AvrAudioLLConditionalAbort(AudioContext, &AudioContext->SendBufferCommands[i]);
	    
	    AvrAudioLLConditionalAbort(AudioContext, &AudioContext->TransformCommand);
	    AvrAudioLLConditionalAbort(AudioContext, &AudioContext->SetGlobalCommand);

	    // attempt to cleanup the transformer (this will probably fail the first few times)
	    sigfillset(&allset);
	    sigprocmask(SIG_BLOCK, &allset, &oldset);
	    MMEStatus = MME_TermTransformer(AudioContext->LLTransformerHandle);
	    sigprocmask(SIG_SETMASK, &oldset, NULL);
	    if (MMEStatus == MME_SUCCESS)
	    {
		DVB_DEBUG("Successful terminated transformer %s\n", LL_MT_NAME);
		break;
	    }
	    
	    DVB_DEBUG("Waiting to terminate transformer %s (MME_TermTransformer returned %d)\n",
		      LL_MT_NAME, MMEStatus);

	    // wait for one of the commands to return before looping round
	    (void) wait_event_timeout(AudioContext->WaitQueue, 
				      AudioContext->GotTransformCommandCallback ||
				      AudioContext->GotSetGlobalCommandCallback ||
				      AudioContext->GotSendBufferCommandCallback, 
				      (HZ/10));

	    // we don't really care whether the wait reported timeout or not - we're going
	    // to do the same thing anyway...
	} while (--retries > 0);

	AudioContext->IsLLTransformerInitialized = false;
    }

    // signal that we really have finished executing
    AudioContext->ThreadShouldStop = false;
    wake_up(&AudioContext->WaitQueue);

    elapsed = ktime_sub(ktime_get(), elapsed);

    if (retries <= 0)
    {
	DVB_ERROR("Timed out after ~%dms whilst trying to terminate transformer %s\n",
		  (int) ktime_to_almost_ms(elapsed), LL_MT_NAME);
	IssuePostMortemDiagnostics(AudioContext);
	return -ETIMEDOUT;
    }

    DVB_DEBUG("Clean up completed OK after ~%dms\n", (int) ktime_to_almost_ms(elapsed));
    return 0;
}

////////////////////////////////////////////////////////////////////////////
///
/// Code reused from mixer_mme.cpp: thanks Dan!
/// Take a continuous sampling frequency and identify the eFsRange
/// for that frequency.
///
enum eFsRange TranslateIntegerSamplingFrequencyToRange( unsigned int IntegerFrequency )
{
    if( IntegerFrequency < 64000 )
    {
        if( IntegerFrequency >= 32000 )
            return ACC_FSRANGE_48k;
	
        if( IntegerFrequency >= 16000 )
            return ACC_FSRANGE_24k;
        
        return ACC_FSRANGE_12k;
    }
    
    if( IntegerFrequency < 128000 )
        return ACC_FSRANGE_96k;
    
    if( IntegerFrequency < 256000 )
        return ACC_FSRANGE_192k;
    
    return ACC_FSRANGE_384k;
}


////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between discrete and integer sampling frequencies.
///
/// The final zero value is very important. In both directions is prevents reads past the
/// end of the table. When indexed on eAccFsCode it also provides the return value when the
/// lookup fails.
///
static const struct
{
    enum eAccFsCode Discrete;
    unsigned int Integer;
}

SamplingFrequencyLookupTable[] =
{
    /* Range : 2^4  */ { ACC_FS768k, 768000 }, { ACC_FS705k, 705600 }, { ACC_FS512k, 512000 },
    /* Range : 2^3  */ { ACC_FS384k, 384000 }, { ACC_FS352k, 352800 }, { ACC_FS256k, 256000 },
    /* Range : 2^2  */ { ACC_FS192k, 192000 }, { ACC_FS176k, 176400 }, { ACC_FS128k, 128000 },
    /* Range : 2^1  */ {  ACC_FS96k,  96000 }, {  ACC_FS88k,  88200 }, {  ACC_FS64k,  64000 },
    /* Range : 2^0  */ {  ACC_FS48k,  48000 }, {  ACC_FS44k,  44100 }, {  ACC_FS32k,  32000 },
    /* Range : 2^-1 */ {  ACC_FS24k,  24000 }, {  ACC_FS22k,  22050 }, {  ACC_FS16k,  16000 },
    /* Range : 2^-2 */ {  ACC_FS12k,  12000 }, {  ACC_FS11k,  11025 }, {   ACC_FS8k,   8000 },
    /* Delimiter    */                                                 {   ACC_FS8k,      0 }
};

////////////////////////////////////////////////////////////////////////////
///
/// Take a continuous sampling frequency and identify the nearest eAccFsCode
/// for that frequency.
///
enum eAccFsCode TranslateIntegerSamplingFrequencyToDiscrete( unsigned int IntegerFrequency )
{
    int i;
    
    for( i=0; IntegerFrequency < SamplingFrequencyLookupTable[i].Integer; i++ )
        ; // do nothing
    
    return SamplingFrequencyLookupTable[i].Discrete;
}


////////////////////////////////////////////////////////////////////////////
///
/// Code reused from mixer_mme.cpp
/// Take a discrete sampling frequency and convert that to an integer frequency.
///
/// Unexpected values (such as ACC_FS_ID) translate to zero.
///
static unsigned int TranslateDiscreteSamplingFrequencyToInteger( enum eAccFsCode DiscreteFrequency )
{
    int i;
    
    for (i=0; DiscreteFrequency != SamplingFrequencyLookupTable[i].Discrete; i++)
        if( 0 == SamplingFrequencyLookupTable[i].Integer )
            break;
    
    return SamplingFrequencyLookupTable[i].Integer;
}


void FillOutLLSetGlobalCommand(avr_v4l2_audio_handle_t *AudioContext)
{
    MME_Command_t * Command_p = &AudioContext->SetGlobalCommand;
    
    Command_p->StructSize = sizeof(MME_Command_t);
    Command_p->CmdCode    = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    Command_p->CmdEnd     = MME_COMMAND_END_RETURN_NOTIFY;
    Command_p->DueTime    = 0;
    Command_p->NumberInputBuffers  = 0;
    Command_p->NumberOutputBuffers = 0;
    Command_p->DataBuffers_p       = NULL;
    Command_p->CmdStatus.AdditionalInfoSize = sizeof(MME_LowlatencySpecializedTransformStatus_t);
    Command_p->CmdStatus.AdditionalInfo_p   = &AudioContext->SetGlobalStatus;
    Command_p->Param_p                      = &AudioContext->SetGlobalParams;

    memset(&AudioContext->SetGlobalParams, 0, sizeof(MME_LowlatencySpecializedGlobalParams_t));
    
    // update transformer global parameters
    if (0 != SetGlobalTransformParameters(AudioContext, &AudioContext->SetGlobalParams))
    {
 	// this is expected to be unreachable (SetGlobalTransformParameters should have failed
	// before we get to init the transformer)
	DVB_ERROR("Internal failure setting global transform parameters\n");
	// no recovery possible
    }

    Command_p->ParamSize  = AudioContext->SetGlobalParams.StructSize;
}

void FillOutLLTransformCommand(avr_v4l2_audio_handle_t *AudioContext)
{
    MME_Command_t * Command_p = &AudioContext->TransformCommand;
    MME_LowlatencyTransformParams_t * TransformParams_p = &AudioContext->TransformParams;

    Command_p->StructSize = sizeof(MME_Command_t);
    Command_p->CmdCode    = MME_TRANSFORM;
    Command_p->CmdEnd     = MME_COMMAND_END_RETURN_NOTIFY;
    Command_p->DueTime    = 0;
    Command_p->NumberInputBuffers  = 0;
    Command_p->NumberOutputBuffers = 0;
    Command_p->DataBuffers_p       = NULL;
    Command_p->CmdStatus.AdditionalInfoSize = sizeof(MME_LowlatencySpecializedTransformStatus_t);
    Command_p->CmdStatus.AdditionalInfo_p   = &AudioContext->TransformStatus;
    Command_p->Param_p                      = &AudioContext->TransformParams;
    Command_p->ParamSize                    = sizeof(MME_LowlatencyTransformParams_t);

    // transform parameters
    TransformParams_p->Restart = ACC_MME_FALSE;
    TransformParams_p->Cmd = ACC_CMD_PLAY;
}

int InitLLLogBuffers(avr_v4l2_audio_handle_t *AudioContext)
{
    MME_Command_t      *CmdPtr = AudioContext->SendBufferCommands;
    MME_DataBuffer_t   **DataBufferPtr = &AudioContext->DataBufferList[0];
    MME_ScatterPage_t  *ScatterPagePtr = AudioContext->ScatterPages;
    int i;
    
    if (ll_input_log_enable)
    {
        // initialize the pools used to log buffers
        if (CreateBufferPool(&AudioContext->InputPool.Pool, 
                             &AudioContext->InputPool.Entries[0], 
                             AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS, 
                             AVR_LOW_LATENCY_BUFFER_INPUT_SIZE))
        {
            DVB_ERROR("Unable to allocate input buffers for logging\n");
            return 1;
        }
        
        for (i = 0; i < AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS; i++)
        {
            AudioContext->BufferEntries[i] = &AudioContext->InputPool.Entries[i];
        }
    }
    
    if (ll_output_log_enable)
    {
        if (CreateBufferPool(&AudioContext->OutputPool.Pool, 
                             &AudioContext->OutputPool.Entries[0], 
                             AVR_LOW_LATENCY_MAX_LOG_OUTPUT_SEND_BUFFERS, 
                             AVR_LOW_LATENCY_BUFFER_OUTPUT_SIZE))
        {
            DVB_ERROR("Unable to allocate output buffers for logging\n");
            return 1;
        }
        
        for (i = 0; i < AVR_LOW_LATENCY_MAX_LOG_OUTPUT_SEND_BUFFERS; i++)
        {
            AudioContext->BufferEntries[i + AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS] = &AudioContext->OutputPool.Entries[i];
        }
    }

    if (ll_input_log_enable)
    {
        
    // Pre-set the send buffer commands
    for ( i = 0; i < AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS; i++)
    {
        AudioContext->DataBufferList[i] = &AudioContext->DataBuffers[i];
        CmdPtr->StructSize = sizeof(MME_Command_t);
        CmdPtr->CmdCode = MME_SEND_BUFFERS;
        CmdPtr->CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
        CmdPtr->DueTime = 0;
        CmdPtr->NumberInputBuffers = 0;
        CmdPtr->NumberOutputBuffers = 1;
        CmdPtr->DataBuffers_p = DataBufferPtr;
        CmdPtr->ParamSize = 0;
        CmdPtr->Param_p = NULL;
            
        DataBufferPtr[0]->StructSize = sizeof(MME_DataBuffer_t);
        DataBufferPtr[0]->UserData_p = (void*)i;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	DataBufferPtr[0]->Flags = LL_ALSAINPUT_FLAG;
#endif	    
        DataBufferPtr[0]->StreamNumber = i/AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS;
        DataBufferPtr[0]->NumberOfScatterPages = 1;
        DataBufferPtr[0]->ScatterPages_p = ScatterPagePtr;
        DataBufferPtr[0]->TotalSize = AudioContext->BufferEntries[i]->Size;
        DataBufferPtr[0]->StartOffset = 0;
            
        ScatterPagePtr->Page_p = AudioContext->BufferEntries[i]->Ptr;
        ScatterPagePtr->Size = AudioContext->BufferEntries[i]->Size;
        ScatterPagePtr->BytesUsed = 0;
        ScatterPagePtr->FlagsIn = 0;
        ScatterPagePtr->FlagsOut = 0;
            
#if 0
        DVB_DEBUG("****Dumping MME command %x,%d ****\n", (unsigned int )CmdPtr, i);
        DumpMMECommand( CmdPtr );
        DVB_DEBUG("\n");
#else
	(void) &DumpMMECommand; // warning suppression
#endif
            
        CmdPtr++;
        DataBufferPtr++;
        ScatterPagePtr++;
    }
}
        
    if (ll_output_log_enable)
    {
        // Pre-set the send buffer commands
        for ( i = AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS; i < AVR_LOW_LATENCY_MAX_LOG_OUTPUT_SEND_BUFFERS; i++)
        {
            AudioContext->DataBufferList[i] = &AudioContext->DataBuffers[i];
            CmdPtr->StructSize = sizeof(MME_Command_t);
            CmdPtr->CmdCode = MME_SEND_BUFFERS;
            CmdPtr->CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
            CmdPtr->DueTime = 0;
            CmdPtr->NumberInputBuffers = 0;
            CmdPtr->NumberOutputBuffers = 1;
            CmdPtr->DataBuffers_p = DataBufferPtr;
            CmdPtr->ParamSize = 0;
            CmdPtr->Param_p = NULL;
            
            DataBufferPtr[0]->StructSize = sizeof(MME_DataBuffer_t);
            DataBufferPtr[0]->UserData_p = (void*)i;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
            DataBufferPtr[0]->Flags = LL_ALSAOUTPUT_FLAG;
#endif		
            DataBufferPtr[0]->StreamNumber = (i-AVR_LOW_LATENCY_MAX_LOG_INPUT_SEND_BUFFERS)/AVR_LOW_LATENCY_MAX_SIMULTANEOUS_BUFFERS;
            DataBufferPtr[0]->NumberOfScatterPages = 1;
            DataBufferPtr[0]->ScatterPages_p = ScatterPagePtr;
            DataBufferPtr[0]->TotalSize = AudioContext->BufferEntries[i]->Size;
            DataBufferPtr[0]->StartOffset = 0;
            
            ScatterPagePtr->Page_p = AudioContext->BufferEntries[i]->Ptr;
            ScatterPagePtr->Size = AudioContext->BufferEntries[i]->Size;
            ScatterPagePtr->BytesUsed = 0;
            ScatterPagePtr->FlagsIn = 0;
            ScatterPagePtr->FlagsOut = 0;
            
#if 0
            DVB_DEBUG("****Dumping MME command %x,%d ****\n", (unsigned int )CmdPtr, i);
            DumpMMECommand( CmdPtr );
            DVB_DEBUG("        \n");
#endif
            
            CmdPtr++;
            DataBufferPtr++;
            ScatterPagePtr++;
        }
    }
    return 0;
}

int CreateBufferPool(BufferPool_t * PoolPtr, BufferEntry_t * EntryPtr, unsigned int NbElt, unsigned int EltSize)
{
    unsigned int i;
    unsigned int size = NbElt * EltSize;
    
    PoolPtr->PhysPtr = OSDEV_MallocPartitioned(SYS_LMI_PARTITION, size);

    //    if( AllocatorOpenEx( MemoryAllocatorDevice, NbElt * EltSize, true, SYS_LMI_PARTITION ) != allocator_ok )
    if (!PoolPtr->PhysPtr)
    {
        DVB_ERROR("Failed to allocate pool memory\n");
        return 1;
    }

    PoolPtr->CachedPtr = ioremap_cache((unsigned int)PoolPtr->PhysPtr, size);
    
    PoolPtr->EltCount = NbElt;
    PoolPtr->EltSize = EltSize;
    PoolPtr->Size = NbElt * EltSize;
    
    for (i = 0; i < NbElt; i++)
    {
        EntryPtr->Ptr = PoolPtr->CachedPtr + i * EltSize;
        EntryPtr->Size = EltSize;
        EntryPtr->BytesUsed = 0;
        EntryPtr->IsFree = true;
        EntryPtr++;
    }

    // means no error
    return 0;
}


/******************************
 * EXTERNAL INTERFACE
 ******************************/


avr_v4l2_audio_handle_t *AvrAudioNew(avr_v4l2_shared_handle_t *SharedContext)
{
	avr_v4l2_audio_handle_t *AudioContext = NULL;
	int i;

	AudioContext = kzalloc(sizeof(avr_v4l2_audio_handle_t), GFP_KERNEL);
	
	if (AudioContext) 
        {
		AudioContext->SharedContext = SharedContext;
		AudioContext->DeviceContext = &SharedContext->avr_device_context;

		AudioContext->AacDecodeEnable = true;
		AudioContext->CompensatoryLatency = 0;
		AudioContext->EmergencyMuteReason = EMERGENCY_MUTE_REASON_NONE;
		AudioContext->FormatRecogniserEnable = true;
		AudioContext->SilenceThreshold = -80;
		AudioContext->SilenceDuration = 1000;
		AudioContext->AudioInputId = 0;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
		AudioContext->HdmiLayout   = ll_layout[ll_force_layout];
		AudioContext->HdmiAudioMode= HDMI_AMODE(ll_force_amode);
#endif		
		
        if (!ll_audio_enable)
        {
            AudioContext->EmergencyMuteClassDevice = NULL;
        }
		
		AudioContext->SampleRate = 48000;
		AudioContext->PreviousSampleRate = 0;
		AudioContext->DiscreteSampleRate = AVR_AUDIO_DISCRETE_SAMPLE_RATE_48000;
		sema_init(&AudioContext->ThreadStatusSemaphore, 1);		
		AudioContext->ThreadHandle = NULL;
		AudioContext->ThreadShouldStop = false;
		AudioContext->ThreadWaitingForStop = false;

        AudioContext->UpdateMixerSettings = false;
        AudioContext->GotTransformCommandCallback = false;

	// mark all commands as being in an unused state
	AudioContext->TransformCommand.CmdStatus.State = MME_COMMAND_FAILED;
	AudioContext->SetGlobalCommand.CmdStatus.State = MME_COMMAND_FAILED;
	for(i=0; i<AVR_LOW_LATENCY_MAX_TOTAL_SEND_BUFFERS; i++)
	    AudioContext->SendBufferCommands[i].CmdStatus.State = MME_COMMAND_FAILED;

        AudioContext->GotSendBufferCommandCallback = false;
        AudioContext->GotSetGlobalCommandCallback = false;
	AudioContext->NeedAnotherSetGlobalCommand = false;

        AudioContext->IsSysFsStructureCreated = false;

        sema_init(&AudioContext->DecoderStatusSemaphore, 1);

        if (InitLLLogBuffers(AudioContext))
        {
            kfree(AudioContext);
            return NULL;
        }

        init_waitqueue_head(&AudioContext->WaitQueue);
	}

	DVB_DEBUG("Audio avr initialized (%p)\n", AudioContext);

	return AudioContext;
}


int AvrAudioDelete(avr_v4l2_audio_handle_t *AudioContext)
{
    DVB_DEBUG("Deleting audio context (%p)\n", AudioContext);

    if (AudioContext->ThreadHandle)
	return -EBUSY;

    if (ll_audio_enable)
    {
        if (ll_input_log_enable && AudioContext->InputPool.Pool.PhysPtr)
        {
            // We need to free the memory regions we allocated for such buffers...
            if ( OSDEV_NoError != OSDEV_FreePartitioned( SYS_LMI_PARTITION, AudioContext->InputPool.Pool.PhysPtr ) )
            {
                DVB_ERROR("Error while trying to free the input pool buffer\n");
            }
        }

        if (ll_output_log_enable && AudioContext->OutputPool.Pool.PhysPtr)
        {
            if ( OSDEV_NoError != OSDEV_FreePartitioned( SYS_LMI_PARTITION, AudioContext->OutputPool.Pool.PhysPtr ) )
            {
                DVB_ERROR("Error while trying to free the output pool buffer\n");
            }
        }
    }

    if (AudioContext->PostMortemBuffer)
	iounmap(AudioContext->PostMortemBuffer);

    if (AudioContext->SecondaryPostMortemBuffer)
	iounmap(AudioContext->SecondaryPostMortemBuffer);

    kfree(AudioContext);
	
    return 0;
}

static void AvrAudioLLAggressiveApply(avr_v4l2_audio_handle_t *AudioContext)
{
	if (AudioContext->ThreadHandle) {
		int res = AvrAudioIoctlOverlayStop(AudioContext);
		if (0 == res)
			res = AvrAudioIoctlOverlayStart(AudioContext);

		if (0 != res)
			DVB_ERROR("Internal error - cannot toggle the overlay\n");
	}
}

static void AvrAudioLLInstantApply(avr_v4l2_audio_handle_t *AudioContext)
{
	if (ll_audio_enable) {
		AudioContext->UpdateMixerSettings = true;
		wake_up(&AudioContext->WaitQueue);
	} else {
		AvrAudioLLAggressiveApply(AudioContext);
	}
}

bool AvrAudioGetAacDecodeEnable(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->AacDecodeEnable;
}

void AvrAudioSetAacDecodeEnable(avr_v4l2_audio_handle_t *AudioContext, bool Enabled)
{
        AudioContext->AacDecodeEnable = Enabled;
	AvrAudioLLInstantApply(AudioContext);
}

long long AvrAudioGetCompensatoryLatency(avr_v4l2_audio_handle_t *AudioContext)
{
    if (!AudioContext)
    {
	return -EINVAL;
    }

    return AudioContext->CompensatoryLatency; 
}

void AvrAudioSetCompensatoryLatency(avr_v4l2_audio_handle_t *AudioContext, long long MicroSeconds)
{
    if (AudioContext)
    {
	if( MicroSeconds < 0 || MicroSeconds > 100000)
	{
	    DVB_ERROR( "Ignoring invalid compensatory latency (%lld)\n", MicroSeconds );
	    AudioContext->CompensatoryLatency = 0;
	}
	else
	{
	    // convert to milliseconds and rounded *up* (to prevent audio leading video)
	    AudioContext->CompensatoryLatency = (MicroSeconds + 999) / 1000;
	}

	DiscloseLatencyTargets( AudioContext, "Vsync" );
	AvrAudioLLInstantApply(AudioContext);
    }
}

avr_audio_emergency_mute_reason_t AvrAudioGetEmergencyMuteReason(avr_v4l2_audio_handle_t *AudioContext)
{
    if (!AudioContext)
    {
        return -EINVAL;
    }

	return AudioContext->EmergencyMuteReason;
}

int AvrAudioSetEmergencyMuteReason(avr_v4l2_audio_handle_t *AudioContext, avr_audio_emergency_mute_reason_t reason)
{
    int ret = 0;
    if (!ll_audio_enable)
    {
        
        if (!AudioContext->DeviceContext->AudioStream) return -EINVAL;
        
        if (AudioContext->EmergencyMuteReason != EMERGENCY_MUTE_REASON_NONE)
        {
            ret = DvbStreamEnable (AudioContext->DeviceContext->AudioStream, reason);
            if (ret < 0) {
                DVB_ERROR("StreamEnable failed\n");
                return -EINVAL;
            }
        }
        AudioContext->EmergencyMuteReason = reason;

        // SYSFS - emergency mute attribute change notification
        sysfs_notify (&((*(AudioContext->EmergencyMuteClassDevice)).kobj), NULL, "emergency_mute");
    }
    else
    {
        // deploy the emergency mute in the firmware
        AudioContext->EmergencyMuteReason = reason;
	AvrAudioLLInstantApply(AudioContext);
        
        sysfs_notify (&AudioContext->StreamClassDevice.kobj, NULL, "emergency_mute");
    }
    return ret;
}


bool AvrAudioGetFormatRecogniserEnable(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->FormatRecogniserEnable;
}


void AvrAudioSetFormatRecogniserEnable(avr_v4l2_audio_handle_t *AudioContext, bool Enabled)
{
	AudioContext->FormatRecogniserEnable = Enabled;
	// not instantly applied (requires restart anyway to change latency)
}

int  AvrAudioGetInput(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->AudioInputId;
}

void AvrAudioSetInput(avr_v4l2_audio_handle_t *AudioContext, unsigned int InputId)
{
    unsigned int MaxId = 1;

    if (ll_force_input)
	InputId = ll_force_input-1;

    if (cpu_data->type == CPU_STX7200 && cpu_data->cut_major == 1)
	MaxId = 2;

    if (InputId > 2)
	InputId = 0;

    AudioContext->AudioInputId = InputId;

    // Handling this with a 'simple' parameter update causes the firmware (BL028_3-6) to fail
    // so for now we are using the aggresively applying the change.
#if 0
    AvrAudioLLInstantApply(AudioContext);
#else
    AvrAudioLLAggressiveApply(AudioContext);
#endif
}


void AvrAudioSetEmphasis(avr_v4l2_audio_handle_t *AudioContext, bool Emphasis)
{
	AudioContext->EmphasisFlag = Emphasis;
	AvrAudioLLInstantApply(AudioContext);
}
bool AvrAudioGetEmphasis(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->EmphasisFlag;
}

void AvrAudioSetHdmiLayout(avr_v4l2_audio_handle_t *AudioContext, int layout)
{
	int l = (ll_force_layout) ? ll_force_layout : layout;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	AudioContext->HdmiLayout    = ll_layout[l];
#endif
	// firmware ignores more subtle approaches because it must reconfigure the PCM reader...
	AvrAudioLLAggressiveApply(AudioContext);
}
void AvrAudioSetHdmiAudioMode(avr_v4l2_audio_handle_t *AudioContext, int audio_mode)
{
	int amode = (ll_force_amode) ? ll_force_amode : audio_mode;
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	AudioContext->HdmiAudioMode = HDMI_AMODE(amode);
#endif	
	AvrAudioLLInstantApply(AudioContext);
}

int AvrAudioGetHdmiLayout(avr_v4l2_audio_handle_t *AudioContext)
{
	int layout;

	switch(AudioContext->HdmiLayout) 
	{
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128		
		case HDMI_LAYOUT0:
			layout = V4L2_HDMI_AUDIO_LAYOUT0;
			break;

		case HDMI_LAYOUT1:
			layout = V4L2_HDMI_AUDIO_LAYOUT1;
			break;

		case HDMI_HBRA:
			layout = V4L2_HDMI_AUDIO_HBR;
			break;
#endif
		default:
			layout =-1;
			break;
	}

	return layout;
}
int AvrAudioGetHdmiAudioMode(avr_v4l2_audio_handle_t *AudioContext)
{
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
	return AMODE_HDMI(AudioContext->HdmiAudioMode);
#else
	return 0;
#endif	
}

void AvrAudioSetChannelSelect(avr_v4l2_audio_handle_t *AudioContext,
			      enum v4l2_avr_audio_channel_select ChannelSelect)
{
	AudioContext->ChannelSelect = ChannelSelect;
	AvrAudioLLInstantApply(AudioContext);
}

enum v4l2_avr_audio_channel_select AvrAudioGetChannelSelect(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->ChannelSelect;
}

void AvrAudioSetSilenceThreshold(avr_v4l2_audio_handle_t *AudioContext, int Threshold)
{
	AudioContext->SilenceThreshold = Threshold;
	AvrAudioLLAggressiveApply(AudioContext);
}

int  AvrAudioGetSilenceThreshold(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->SilenceThreshold;
}

void AvrAudioSetSilenceDuration(avr_v4l2_audio_handle_t *AudioContext, int Duration)
{
	AudioContext->SilenceDuration = Duration;
	AvrAudioLLAggressiveApply(AudioContext);
}

unsigned long  AvrAudioGetSilenceDuration(avr_v4l2_audio_handle_t *AudioContext)
{
	return AudioContext->SilenceDuration;
}


int AvrAudioIoctlOverlayStart(avr_v4l2_audio_handle_t *AudioContext)
{
	struct sched_param Param_audio;
        int (*ThreadFn)(void *data) ;
        const char* ThreadName;
        
	DVB_DEBUG("Starting audio overlay (%p)\n", AudioContext);
	
	down(&AudioContext->ThreadStatusSemaphore);

	if (AudioContext->ThreadHandle) {
		DVB_TRACE("DVP Audio Capture Thread already started\n" );
		up(&AudioContext->ThreadStatusSemaphore);
		return 0; //-EBUSY;	
	}
    
        if (ll_audio_enable)
        {
            // specific low latency transformer driver
            ThreadFn = AvrAudioLLThread;
            ThreadName = "AvrAudioLLThread";
        }
        else
        {
            // thread that injects data into the player
            ThreadFn = AvrAudioInjectorThread;
            ThreadName = "AvrAudioInjectorThread";
        }
             
	AudioContext->ThreadWaitingForStop = false;
        AudioContext->ThreadHandle = kthread_create(ThreadFn, AudioContext, ThreadName);
	if (IS_ERR(AudioContext->ThreadHandle))
	{
	    DVB_ERROR("Unable to start audio thread\n" );
	    AudioContext->ThreadHandle = NULL;
	    up(&AudioContext->ThreadStatusSemaphore);
	    return -EINVAL;
	}

	DVB_DEBUG("ThreadHandle %p\n", AudioContext->ThreadHandle);
	up(&AudioContext->ThreadStatusSemaphore);

	// Switch to real time scheduling
	Param_audio.sched_priority = AUDIO_CAPTURE_THREAD_PRIORITY;
	if (sched_setscheduler(AudioContext->ThreadHandle, SCHED_RR, &Param_audio))
	{
	    DVB_ERROR("FAILED to set scheduling parameters to priority %d (SCHED_RR)\n", Param_audio.sched_priority);
	    return -EINVAL;
	}

	wake_up_process(AudioContext->ThreadHandle);
	
	return 0;
}

int AvrAudioIoctlOverlayStop(avr_v4l2_audio_handle_t *AudioContext)
{
	int res;
	int out_idx;
	ktime_t elapsed = ktime_get();

	DVB_DEBUG("Stopping audio overlay (%p)\n", AudioContext);

	if (AudioContext->ThreadHandle) {
	        AudioContext->ThreadShouldStop = true;
		wake_up(&AudioContext->WaitQueue);

		// wait until the thread terminates (clears ThreadShouldStop)
		res = wait_event_timeout(AudioContext->WaitQueue, !AudioContext->ThreadShouldStop, (HZ * ll_audioterm_timeout));
		
		elapsed = ktime_sub(ktime_get(), elapsed);

		if(AudioContext->ThreadShouldStop) {
			DVB_ERROR("%s after ~%dms waiting for audio thread to terminate\n",
                                  0 == res ? "Timed out" : "Error whilst", (int) ktime_to_almost_ms(elapsed) );
			IssuePostMortemDiagnostics(AudioContext);
			return -ETIMEDOUT;
		}
		DVB_DEBUG("Stop completed OK after ~%dms: res %d  ThreadShouldstop %d\n",
			  (int) ktime_to_almost_ms(elapsed), res, AudioContext->ThreadShouldStop);

		// mark the thread as not running
		AudioContext->ThreadHandle = NULL;
	}

	// release the low-level sound hardware
	for (out_idx = 0; out_idx < AVR_LOW_LATENCY_MAX_OUTPUT_CARDS; out_idx ++) {
		if (AudioContext->SoundcardHandle[out_idx] != NULL) {
			ksnd_pcm_close(AudioContext->SoundcardHandle[out_idx]);
			AudioContext->SoundcardHandle[out_idx] = NULL;
		}
	}

	// unmount all sysfs structures
	if (AudioContext->IsSysFsStructureCreated)
	{
		unsigned int attr_idx;
        
		// now remove all the attributes...
		for (attr_idx = 0; attr_idx < g_attrCount; attr_idx++)
		{
			class_device_remove_file(&AudioContext->StreamClassDevice, attribute_array[attr_idx]);
		}

		// now unregister the stream class
		//        class_device_unregister (&AudioContext->StreamClassDevice);
		class_device_del(&AudioContext->StreamClassDevice);

		player_sysfs_new_attribute_notification(NULL);
		AudioContext->IsSysFsStructureCreated = false;
	}
	
	return 0;
}

/*!
 * Notify us that the mixer settings (held in the shared context) may have been updated.
 */
void AvrAudioMixerSettingsChanged(avr_v4l2_audio_handle_t *AudioContext)
{
	if (AudioContext) {
		DiscloseLatencyTargets(AudioContext, "Mixer");

		// Check if any of the mixer settings that influence initialization parameters have
		// changed (which determines which type of instant apply we need to deploy)
		if (AudioContext->MasterLatencyWhenInitialized != GetMasterLatencyOffset(AudioContext))
			AvrAudioLLAggressiveApply(AudioContext);
		else
			AvrAudioLLInstantApply(AudioContext);
	}
}

void   DumpMMECommand( MME_Command_t *CmdInfo_p )
{
    printk("StructSize                             = %d\n", CmdInfo_p->StructSize );
    printk("CmdCode                                = %d\n", CmdInfo_p->CmdCode );
    printk("CmdEnd                                 = %d\n", CmdInfo_p->CmdEnd );
    printk("DueTime                                = %d\n", CmdInfo_p->DueTime );
    printk("NumberInputBuffers                     = %d\n", CmdInfo_p->NumberInputBuffers );
    printk("NumberOutputBuffers                    = %d\n", CmdInfo_p->NumberOutputBuffers );
    printk("DataBuffers_p                          = %08x\n", (unsigned int)CmdInfo_p->DataBuffers_p );
    {
        int buf_idx;
        MME_DataBuffer_t* DataBuffer = NULL;

        if (CmdInfo_p->DataBuffers_p != NULL)
        {
            DataBuffer = *CmdInfo_p->DataBuffers_p;
        }

        for (buf_idx = 0; buf_idx < (CmdInfo_p->NumberInputBuffers + CmdInfo_p->NumberOutputBuffers); buf_idx ++)
        {
            int NbScatterPages = DataBuffer->NumberOfScatterPages, j;
            MME_ScatterPage_t* Scatter_p = DataBuffer->ScatterPages_p;

            printk("\tStructSize             = %d\n", DataBuffer->StructSize );
            printk("\tUserData_p             = %d\n", (unsigned int)DataBuffer->UserData_p );
            printk("\tFlags                  = %x\n", DataBuffer->Flags );
            printk("\tStreamNumber           = %d\n", DataBuffer->StreamNumber );
            printk("\tNumberOfScatterPages   = %d\n", NbScatterPages );
            printk("\tScatterPages_p         = %x\n", (unsigned int)DataBuffer->ScatterPages_p );

            for ( j = 0; j < NbScatterPages; j++)
            {
                printk("\t\tPage_p      = %x\n", (unsigned int)Scatter_p->Page_p );
                printk("\t\tSize        = %d\n", Scatter_p->Size );
                printk("\t\tBytesUsed   = %d\n", Scatter_p->BytesUsed );
                printk("\t\tFlagsIn     = %d\n", Scatter_p->FlagsIn );
                printk("\t\tFlagsOut    = %d\n", Scatter_p->FlagsOut );
                Scatter_p++;
            }
            printk("\tTotalSize              = %d\n", DataBuffer->TotalSize );
            printk("\tStartOffset            = %d\n", DataBuffer->StartOffset );
            DataBuffer++;
        }
    }

    printk("CmdStatus\n" );
    printk("    CmdId                              = %d\n", CmdInfo_p->CmdStatus.CmdId );
    printk("    State                              = %d\n", CmdInfo_p->CmdStatus.State );
    printk("    ProcessedTime                      = %d\n", CmdInfo_p->CmdStatus.ProcessedTime );
    printk("    Error                              = %d\n", CmdInfo_p->CmdStatus.Error );
    printk("    AdditionalInfoSize                 = %d\n", CmdInfo_p->CmdStatus.AdditionalInfoSize );
    printk("    AdditionalInfo_p                   = %08x\n", (unsigned int)CmdInfo_p->CmdStatus.AdditionalInfo_p );
    printk("ParamSize                              = %d\n", CmdInfo_p->ParamSize );
    printk("Param_p                                = %08x\n", (unsigned int)CmdInfo_p->Param_p );

//
}

#undef bool

module_param(ll_input_log_enable, bool, S_IRUGO|S_IWUSR);
module_param(ll_output_log_enable, bool, S_IRUGO|S_IWUSR);
module_param(ll_audio_enable, bool, S_IRUGO|S_IWUSR);

module_param(ll_force_input , int, S_IRUGO|S_IWUSR);
module_param(ll_force_layout, int, S_IRUGO|S_IWUSR);
module_param(ll_force_amode , int, S_IRUGO|S_IWUSR);
module_param(ll_force_downsample , bool, S_IRUGO|S_IWUSR);

// Added for BitExactness testing
module_param(ll_disable_detection   , int, S_IRUGO|S_IWUSR);
module_param(ll_disable_spdifinfade , int, S_IRUGO|S_IWUSR);
module_param(ll_bitexactness_testing, int, S_IRUGO|S_IWUSR);
module_param(ll_audioterm_timeout   , int, S_IRUGO|S_IWUSR);
