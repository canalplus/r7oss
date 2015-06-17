/***********************************************************************
 *
 * File: include/stmhdmiinfo.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDMI_INFO_H
#define _STM_HDMI_INFO_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * HDMI1.3 General Control Packet
 */
#define HDMI_GCP_TYPE         0x3

#define HDMI_GCP_CLEAR_AVMUTE 0x10
#define HDMI_GCP_SET_AVMUTE   0x1

#define HDMI_GCP_CD_NOT_INDICATED 0x0
#define HDMI_GCP_CD_24BPP         0x4
#define HDMI_GCP_CD_30BPP         0x5
#define HDMI_GCP_CD_36BPP         0x6
#define HDMI_GCP_CD_48BPP         0x7
#define HDMI_GCP_CD_SHIFT         0
#define HDMI_GCP_CD_MASK          (0xF << HDMI_CD_SHIFT)

#define HDMI_GCP_PP_PHASE4        0x0
#define HDMI_GCP_PP_PHASE1        0x1
#define HDMI_GCP_PP_PHASE2        0x2
#define HDMI_GCP_PP_PHASE3        0x3
#define HDMI_GCP_PP_SHIFT         4
#define HDMI_GCP_PP_MASK          (0xF << HDMI_PP_SHIFT)

#define HDMI_GCP_PP_DEFAULT       0x1

/*
 * AVI Info frame EIA/CEA-861B
 */
#define HDMI_AVI_INFOFRAME_TYPE    0x82
#define HDMI_AVI_INFOFRAME_VERSION 2
#define HDMI_AVI_INFOFRAME_LENGTH  0xD

/*
 * Data Byte 1
 */
#define HDMI_AVI_INFOFRAME_NOSCANDATA    (0x0<<0)
#define HDMI_AVI_INFOFRAME_OVERSCAN      (0x1<<0)    /* OverScanned (television)*/
#define HDMI_AVI_INFOFRAME_UNDERSCAN     (0x2<<0)    /* UnderScanned (Computer)*/
#define HDMI_AVI_INFOFRAME_SCANFUTURE    (0x3<<0)    /* Scan information :Future*/
#define HDMI_AVI_INFOFRAME_BARNOTVALID    0x0        /* Bar Data not Valid*/
#define HDMI_AVI_INFOFRAME_V_BARVALID    (0x1<<2)    /* Vertical Bar info Valid*/
#define HDMI_AVI_INFOFRAME_H_BARVALID    (0x1<<3)    /* Horizontal Bar Info Valid*/
#define HDMI_AVI_INFOFRAME_AFI           (0x1<<4)    /* Active Format Information valid*/
#define HDMI_AVI_INFOFRAME_RGB            0x0        /* RGB Output Format*/
#define HDMI_AVI_INFOFRAME_YCBCR422      (0x1<<5)    /* YCbCr422 Output Format*/
#define HDMI_AVI_INFOFRAME_YCBCR444      (0x2<<5)    /* YCbCr444 Output Format*/
#define HDMI_AVI_INFOFRAME_OUTPUTFUTURE  (0x3<<5)    /* RGB or YCbCr: Future*/
#define HDMI_AVI_INFOFRAME_OUTPUT_MASK   (0x3<<5)    /* Output format mask*/

/* AVI Info frame Data Byte 2 */
#define HDMI_AVI_INFOFRAME_AFI_169_TOP       0x2        /* Active Format Aspect Ratio letterboxed 16:9  (top)     */
#define HDMI_AVI_INFOFRAME_AFI_149_TOP       0x3        /* Active Format Aspect Ratio letterboxed 14:9  (top)     */
#define HDMI_AVI_INFOFRAME_AFI_GT_169_CENTER 0x4        /* Active Format Aspect Ratio letterboxed >16:9 (center)  */
#define HDMI_AVI_INFOFRAME_AFI_SAMEP         0x8        /* Active Format Aspect Ratio same as picture aspect ratio*/
#define HDMI_AVI_INFOFRAME_AFI_43_CENTER     0x9        /* Active Format Aspect Ratio pillarboxed 4:3  (center)   */
#define HDMI_AVI_INFOFRAME_AFI_169_CENTER    0xA        /* Active Format Aspect Ratio letterboxed 16:9 (center)   */
#define HDMI_AVI_INFOFRAME_AFI_149_CENTER    0xB        /* Active Format Aspect Ratio letterboxed 14:9 (center)   */
#define HDMI_AVI_INFOFRAME_AFI_43_SAP_14_9   0xD        /* Active Format Aspect Ratio 4:3 with shoot&protect 14:9  center */
#define HDMI_AVI_INFOFRAME_AFI_169_SAP_14_9  0xE        /* Active Format Aspect Ratio 16:9 with shoot&protect 14:9 center */
#define HDMI_AVI_INFOFRAME_AFI_169_SAP_4_3   0xF        /* Active Format Aspect Ratio 16:9 with shoot&protect 4:3  center */
#define HDMI_AVI_INFOFRAME_AFI_MASK          0xF

#define HDMI_AVI_INFOFRAME_PICAR_NODATA  0x0           /* Picture Aspect ratio: no data*/
#define HDMI_AVI_INFOFRAME_PICAR_43      (0x1<<4)      /* Picture Aspect ratio: 4:3 */
#define HDMI_AVI_INFOFRAME_PICAR_169     (0x2<<4)      /* Picture Aspect ratio: 16:9*/
#define HDMI_AVI_INFOFRAME_PICAR_FUTURE  (0x3<<4)      /* Picture Aspect ratio: Future*/
#define HDMI_AVI_INFOFRAME_PICAR_MASK    (0x3<<4)      /* Picture Aspect ratio mask*/

#define HDMI_AVI_INFOFRAME_COLO_NDATA    0x0           /* Colorimetry: no data*/
#define HDMI_AVI_INFOFRAME_COLO_ITU601   (0x1<<6)      /* Colorimetry: SMPTE 170M ITU601*/
#define HDMI_AVI_INFOFRAME_COLO_ITU709   (0x2<<6)      /* Colorimetry: ITU 709*/
#define HDMI_AVI_INFOFRAME_COLO_EXTENDED (0x3<<6)      /* Colorimetry: See EC0..2 in Byte3 */
#define HDMI_AVI_INFOFRAME_COLO_MASK     (0x3<<6)      /* Colorimetry mask */

/* AVI Info frame DATA Byte3 */
#define HDMI_AVI_INFOFRAME_NO_SCALING         0x0      /* No Known non uniform scaling*/
#define HDMI_AVI_INFOFRAME_H_SCALED           (0x1<<0) /* Picture has been scaled Horizentaly*/
#define HDMI_AVI_INFOFRAME_V_SCALED           (0x2<<0) /* Picture has been scaled Vertically*/
#define HDMI_AVI_INFOFRAME_HV_SCALED          (0x3<<0) /* Picture has been scaled Horizentally and Vertically*/
#define HDMI_AVI_INFOFRAME_HV_SCALE_MASK      (0x3<<0)
#define HDMI_AVI_INFOFRAME_RGB_QUANT_DEFAULT  (0x0<<2) /* Depends on video mode CE - Limited, IT - Full */
#define HDMI_AVI_INFOFRAME_RGB_QUANT_LIMITED  (0x1<<2)
#define HDMI_AVI_INFOFRAME_RGB_QUANT_FULL     (0x2<<2)
#define HDMI_AVI_INFOFRAME_RGB_QUANT_MASK     (0x3<<2)
#define HDMI_AVI_INFOFRAME_EC_xvYcc601        (0x0<<4)
#define HDMI_AVI_INFOFRAME_EC_xvYcc709        (0x1<<4)
#define HDMI_AVI_INFOFRAME_EC_sYcc601         (0x2<<4)
#define HDMI_AVI_INFOFRAME_EC_AdobeYcc601     (0x3<<4)
#define HDMI_AVI_INFOFRAME_EC_AdobeRGB        (0x4<<4)
#define HDMI_AVI_INFOFRAME_EC_MASK            (0x7<<4)
#define HDMI_AVI_INFOFRAME_ITC                (0x1<<7)

/* AVI Info Frame Data byte 4 reserved for the video identification codes*/

/* AVI Info Frame DATA byte5 */
#define HDMI_AVI_INFOFRAME_PIXELREP1         0x00000000   /* NO repetition (ie pixel sent once)*/
#define HDMI_AVI_INFOFRAME_PIXELREP2         0x00000001   /* Pixel sent 2 times (ie repeated once)*/
#define HDMI_AVI_INFOFRAME_PIXELREP3         0x00000002   /* Pixel sent 3 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP4         0x00000003   /* Pixel sent 4 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP5         0x00000004   /* Pixel sent 5 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP6         0x00000005   /* Pixel sent 6 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP7         0x00000006   /* Pixel sent 7 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP8         0x00000007   /* Pixel sent 8 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP9         0x00000008   /* Pixel sent 9 times*/
#define HDMI_AVI_INFOFRAME_PIXELREP10        0x00000009   /* Pixel sent 10 times*/
#define HDMI_AVI_INFOFRAME_PIXELREPMASK      0x0000000F
#define HDMI_AVI_INFOFRAME_CN_SHIFT          (4)
#define HDMI_AVI_INFOFRAME_CN_GRAPHICS       (0x0<<HDMI_AVI_INFOFRAME_CN_SHIFT)
#define HDMI_AVI_INFOFRAME_CN_PHOTO          (0x1<<HDMI_AVI_INFOFRAME_CN_SHIFT)
#define HDMI_AVI_INFOFRAME_CN_CINEMA         (0x2<<HDMI_AVI_INFOFRAME_CN_SHIFT)
#define HDMI_AVI_INFOFRAME_CN_GAME           (0x3<<HDMI_AVI_INFOFRAME_CN_SHIFT)
#define HDMI_AVI_INFOFRAME_CN_MASK           (0x3<<HDMI_AVI_INFOFRAME_CN_SHIFT)
#define HDMI_AVI_INFOFRAME_YCC_QUANT_LIMITED (0x0<<6)
#define HDMI_AVI_INFOFRAME_YCC_QUANT_FULL    (0x1<<6)
#define HDMI_AVI_INFOFRAME_YCC_QUANT_MASK    (0x3<<6)

/*
 * Audio InfoFrame Format
 */
#define HDMI_AUDIO_INFOFRAME_TYPE    0x84
#define HDMI_AUDIO_INFOFRAME_VERSION 1
#define HDMI_AUDIO_INFOFRAME_LENGTH  0xA

/* Audio Info Frame DATA byte 1 */
#define HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_SHIFT (0)
#define HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_MASK  (0x7<<HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_SHIFT)

#define HDMI_AUDIO_INFOFRAME_NOCODING    0x00000000   /* Audio Coding Type: refer to stream Header*/
#define HDMI_AUDIO_INFOFRAME_PCM         0x00000010   /* Audio Coding Type: IEC60958 PCM*/
#define HDMI_AUDIO_INFOFRAME_AC3         0x00000020   /* Audio Coding Type: AC-3 */
#define HDMI_AUDIO_INFOFRAME_MPEG1       0x00000030   /* Audio Coding Type: MPEG 1*/
#define HDMI_AUDIO_INFOFRAME_MP3         0x00000040   /* Audio Coding Type: MP3*/
#define HDMI_AUDIO_INFOFRAME_MPEG2       0x00000050   /* Audio Coding Type: MPEG2*/
#define HDMI_AUDIO_INFOFRAME_AAC         0x00000060   /* Audio Coding Type: AAC*/
#define HDMI_AUDIO_INFOFRAME_DTS         0x00000070   /* Audio Coding Type: DTS*/
#define HDMI_AUDIO_INFOFRAME_ATRAC       0x00000080   /* Audio Coding Type: ATRAC*/

  /* Audio Info FRame DATA byte 2 */
#define HDMI_AUDIO_INFOFRAME_SAMPLE_SIZE_SHIFT  (0)
#define HDMI_AUDIO_INFOFRAME_SAMPLE_SIZE_MASK   (0x3<<HDMI_AUDIO_INFOFRAME_SAMPLE_SIZE_SHIFT)

#define HDMI_AUDIO_INFOFRAME_FREQ_SHIFT  (2)
#define HDMI_AUDIO_INFOFRAME_FREQ_MASK   (0x7<<HDMI_AUDIO_INFOFRAME_FREQ_SHIFT)

#define HDMI_AUDIO_INFOFRAME_LEVELSHIFT_SHIFT (3)
#define HDMI_AUDIO_INFOFRAME_LEVELSHIFT_MASK  (0xf<<HDMI_AUDIO_INFOFRAME_LEVELSHIFT_SHIFT)
#define HDMI_AUDIO_INFOFRAME_DOWNMIX_INHIBIT  (0x1<<7)

/*
 * Source Product Descriptor
 */
#define HDMI_SPD_INFOFRAME_TYPE    0x83
#define HDMI_SPD_INFOFRAME_VERSION 1
#define HDMI_SPD_INFOFRAME_LENGTH  0x19

#define HDMI_SPD_INFOFRAME_VENDOR_START  1
#define HDMI_SPD_INFOFRAME_VENDOR_LENGTH 8

#define HDMI_SPD_INFOFRAME_PRODUCT_START 9
#define HDMI_SPD_INFOFRAME_PRODUCT_LENGTH 16

#define HDMI_SPD_INFOFRAME_SPI_OFFSET 25

#define HDMI_SPD_INFOFRAME_SPI_UNKNOWN 0x0
#define HDMI_SPD_INFOFRAME_SPI_STB     0x1
#define HDMI_SPD_INFOFRAME_SPI_DVD     0x2
#define HDMI_SPD_INFOFRAME_SPI_DVHS    0x3
#define HDMI_SPD_INFOFRAME_SPI_HDD     0x4
#define HDMI_SPD_INFOFRAME_SPI_DVC     0x5
#define HDMI_SPD_INFOFRAME_SPI_DSC     0x6
#define HDMI_SPD_INFOFRAME_SPI_VCD     0x7
#define HDMI_SPD_INFOFRAME_SPI_GAME    0x8
#define HDMI_SPD_INFOFRAME_SPI_PC      0x9
#define HDMI_SPD_INFOFRAME_SPI_BLURAY  0xA
#define HDMI_SPD_INFOFRAME_SPI_SACD    0xB
#define HDMI_SPD_INFOFRAME_SPI_HDDVD   0xC
#define HDMI_SPD_INFOFRAME_SPI_PMP     0xD


/*
 * Vendor Specific InfoFrame
 */
#define HDMI_VENDOR_INFOFRAME_TYPE 0x81

/*
 * NTSC VBI InfoFrame
 */
#define HDMI_NTSC_INFOFRAME_TYPE    0x86
#define HDMI_NTSC_INFOFRAME_VERSION 0x1

/*
 * ACP Packet
 */
#define HDMI_ACP_PACKET_TYPE   0x4
#define HDMI_ACP_TYPE_GENERIC  0x0
#define HDMI_ACP_TYPE_IEC60958 0x1
#define HDMI_ACP_TYPE_DVDA     0x2
#define HDMI_ACP_TYPE_SACD     0x3

/*
 * ISRC1/2 Packets
 */
#define HDMI_ISRC1_PACKET_TYPE     0x5
#define HDMI_ISRC2_PACKET_TYPE     0x6

#define HDMI_ISRC1_STATUS_STARTING 0x1
#define HDMI_ISRC1_STATUS_PLAYING  0x2
#define HDMI_ISRC1_STATUS_ENDING   0x4
#define HDMI_ISRC1_STATUS_MASK     0x7
#define HDMI_ISRC1_VALID           (1L<<6)
#define HDMI_ISRC1_CONTINUED       (1L<<7)

/*
 * Gamut data Packet
 */
#define HDMI_GAMUT_DATA_PACKET_TYPE            0xA
#define HDMI_GAMUT_HB1_AFFECTED_SEQ_NUM_MASK   (0xf)
#define HDMI_GAMUT_HB1_PROFILE_SHIFT           (4)
#define HDMI_GAMUT_HB1_PROFILE_0               (0)
#define HDMI_GAMUT_HB1_PROFILE_1               (0x1<<HDMI_GAMUT_HB1_PROFILE_SHIFT)
#define HDMI_GAMUT_HB1_PROFILE_2               (0x2<<HDMI_GAMUT_HB1_PROFILE_SHIFT)
#define HDMI_GAMUT_HB1_PROFILE_3               (0x3<<HDMI_GAMUT_HB1_PROFILE_SHIFT)
#define HDMI_GAMUT_HB1_PROFILE_MASK            (0x7<<HDMI_GAMUT_HB1_PROFILE_SHIFT)
#define HDMI_GAMUT_HB1_NEXT_FIELD              (0x1<<7)
#define HDMI_GAMUT_HB2_CURRENT_SEQ_NUM_MASK    (0xf)
#define HDMI_GAMUT_HB2_PACKET_SEQ_SHIFT        (4)
#define HDMI_GAMUT_HB2_PACKET_SEQ_INTERMEDIATE (0)
#define HDMI_GAMUT_HB2_PACKET_SEQ_FIRST        (0x1<<HDMI_GAMUT_HB2_PACKET_SEQ_SHIFT)
#define HDMI_GAMUT_HB2_PACKET_SEQ_LAST         (0x2<<HDMI_GAMUT_HB2_PACKET_SEQ_SHIFT)
#define HDMI_GAMUT_HB2_PACKET_SEQ_ONLY         (0x3<<HDMI_GAMUT_HB2_PACKET_SEQ_SHIFT)
#define HDMI_GAMUT_HB2_PACKET_SEQ_MASK         (0x3<<HDMI_GAMUT_HB2_PACKET_SEQ_SHIFT)
#define HDMI_GAMUT_HB2_NO_CURRENT_GBD          (0x1<<7)


typedef struct
{
  UCHAR type;
  UCHAR version;
  UCHAR length;
  UCHAR data[28];
} stm_hdmi_info_frame_t;


typedef struct
{
  stm_hdmi_info_frame_t isrc1;
  stm_hdmi_info_frame_t isrc2;
} stm_hdmi_isrc_data_t;


#if defined(__cplusplus)
}
#endif


#endif /* _STM_HDMI_INFO_H */
