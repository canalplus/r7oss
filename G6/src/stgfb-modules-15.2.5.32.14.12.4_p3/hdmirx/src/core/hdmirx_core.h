/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_core.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_CORE_H__
#define __HDMIRX_CORE_H__

/*Includes--------------------------------------------------------------*/
#include <stddefs.h>

#include <hdmirx_ifm.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------*/
#define     COUNT_MEAS                      5
#define     GET_CORE_BASE_ADDRS(Handle)     ((U32)(((hdmirx_route_handle_t *)Handle)->BaseAddress))

  /******************************************************************************
   IEC60958-1, 3 definitions
  ******************************************************************************/
#ifndef __IEC_60958__
#define __IEC_60958__

  /*Preemphasis, channel status, IEC60958-3 (page 10)*/
  typedef enum {
    HDRX_IEC60958_AUDIO_PE_NONE = 0,
    HDRX_IEC60958_AUDIO_PE_2CH_50_15US,
    HDRX_IEC60958_AUDIO_PE_RSRV1,
    HDRX_IEC60958_AUDIO_PE_RSRV2
  }
  HDRX_IEC60958_AUDIO_PE_t;

  /*Clock accuracy, channel status, IEC60958-3 (page 13)*/
  typedef enum
  {
    HDRX_IEC60958_AUDIO_CA_LEVEL_2 = 0,
    HDRX_IEC60958_AUDIO_CA_LEVEL_1,
    HDRX_IEC60958_AUDIO_CA_LEVEL_3,
    HDRX_IEC60958_AUDIO_CA_UNMATCHED
  } HDRX_IEC60958_AUDIO_CA_t;

  /*Digital audio standard, IEC60958-1, table B.1.*/
  typedef enum
  {
    HDRX_IEC60958_AUDIO_STD_3 = 0,
    HDRX_IEC60958_AUDIO_STD_4,
    HDRX_IEC60958_AUDIO_STD_61937,	/* IEC 62105 and other */
    HDRX_IEC60958_AUDIO_STD_SMPTE337M	/* and other */
  } HDRX_IEC60958_AUDIO_STD_t;
#endif				/*#ifndef __IEC_60958__ */

  /*ACP Packet type (HDMI 1.3a spec, table 5-18)*/
  typedef enum
  {
    HDRX_ACP_GENERIC = 0,
    HDRX_ACP_IEC60958,
    HDRX_ACP_DVD,
    HDRX_ACP_SUPER_AUDIO_CD,
    HDRX_ACP_RSRV
  } hdrx_ACPType_t;

  /*Color depth values of General Control Packet (HDMI 1.3a spec, table 6-1)*/
  typedef enum
  {
    HDRX_GCP_CD_NONE = 0,
    HDRX_GCP_CD_RSRV1,
    HDRX_GCP_CD_RSRV2,
    HDRX_GCP_CD_RSRV3,
    HDRX_GCP_CD_24BPP,
    HDRX_GCP_CD_30BPP,
    HDRX_GCP_CD_36BPP,
    HDRX_GCP_CD_48BPP
  } hdrx_GCPColorDepth_t;

  /*Sample frequency values merged from Audio InfoFrame and Audio Channel Status*/
  typedef enum
  {
    //See CEA-861-D, table 18*/
    HDRX_AUDIO_SF_NONE = 0,
    HDRX_AUDIO_SF_32_KHZ,
    HDRX_AUDIO_SF_44_1KHZ,
    HDRX_AUDIO_SF_48_KHZ,
    HDRX_AUDIO_SF_88_2KHZ,
    HDRX_AUDIO_SF_96_KHZ,
    HDRX_AUDIO_SF_176_4KHZ,
    HDRX_AUDIO_SF_192_KHZ,

    /*See IEC60958-3, page 12 */
    HDRX_AUDIO_SF_22_05KHZ,
    HDRX_AUDIO_SF_24_KHZ,
    HDRX_AUDIO_SF_8_KHZ,
    HDRX_AUDIO_SF_768_KHZ,
    HDRX_AUDIO_SF_NOT_INDICATED,
    HDRX_AUDIO_SF_RESERVED
  } hdrx_audio_sample_freq_t;

  typedef enum
  {
    HDRX_STATE_POWER_ON,
    HDRX_STATE_POWER_OFF,
    HDRX_STATE_POWER_STANDBY
  } sthdmirx_power_state_type_t;

  /*HDMI mode (version)*/
  typedef enum
  {
    HDRX_INPUT_SIGNAL_NONE = 0,
    HDRX_INPUT_SIGNAL_DVI,
    HDRX_INPUT_SIGNAL_HDMI_1V1,
    HDRX_INPUT_SIGNAL_HDMI_1V3
  } hdrx_input_signal_type_t;

  /*HDMI packet header (HDMI 1.3a, table 5-7)*/
  typedef struct
  {
    U8 PacketType;
    U8 PacketSpecificData1;
    U8 PacketSpecificData2;
  } HDRX_HEADER_PACKET;

  /*Vendor-Specific InfoFrame (CEA-861-D, table 5)*/
  typedef struct
  {
    U8 Length;	/*VendorPayload length */
    U8 CheckSum;
    U32 IeeeId;	/*24 bits IEEE ID */
    unsigned int Hdmi_Video_Format:3;
    unsigned int structure3D:4;
    unsigned int TDMetaPresent:1;
    U8 Payload[24];
  } HDRX_VENDOR_INFOFRAME;

  /*AVI InfoFrame (HDMI 1.3a, table 8-2)*/
  typedef struct
  {
    U8 CheckSum;	/*length of AVI infoframe */
    unsigned int S:2;	/* gmt_CEA861_AVI_S */
    unsigned int B:2;	/* gmt_CEA861_AVI_B */
    unsigned int A:1;	/* gmt_CEA861_AVI_A */
    unsigned int Y:2;	/* Color space RGB/YCbCr 444/YCbCr 422/Extended */
    unsigned int PB1_Rsvd:1;	/* Reserved */
    unsigned int R:4;	/* Active Format Aspect Ratio */
    unsigned int M:2;	/* Picture Aspect Ration- gmt_CEA861_AVI_M */
    unsigned int C:2;	/* Colorimetry - gmt_CEA861_AVI_C */
    unsigned int SC:2;	/* Picture Scaling -gmt_CEA861_AVI_SC */
    unsigned int Q:2;	/* RGB Quantization Range */
    unsigned int EC:3;	/* Extended Colorimetry */
    unsigned int ITC:1;	/* IT Contents */
    unsigned int VIC:7;	/* Video Identification Code */
    unsigned int PB4_Rsvd:1;	/* Reserved */
    unsigned int PR:4;	/* Pixel Repetition Factor */
    unsigned int CN:2;	/* Content Type */
    unsigned int YQ:2;	/* YCC Quantization range */
    U16 EndOfTopBar;	/* last line@top of the picture */
    U16 StartOfBottomBar;	/* first line@bottom of the pictue */
    U16 EndOfLeftBar;	/* last pixel @ right side of picture */
    U16 StartOfRightBar;	/* first pixel @ left side of picture */
  } HDRX_AVI_INFOFRAME;

  /*Source Product Descriptions (SPD) Packet (CEA-861-D, Table 14)*/
  typedef struct
  {
    U8 CheckSum;	/* Length of SPD Info Frame */
    U8 VendorName[8];	/* Vendor Name Characters */
    U8 ProductName[16];	/* Product description */
    U8 SDI;		/* Source Device Information */
  } HDRX_SPD_INFOFRAME;

  /*Audio InfoFrame (HDMI 1.3a, table 8-5)*/
  typedef struct
  {
    U8 CheckSum;	/* Lenght of Audio Info Frame */
    unsigned int CC:3;	/* Audio Channel Count */
    unsigned int PB1_Rsvd:1;	/* Reserved */
    unsigned int CT:4;	/* Audio Coding Type */
    unsigned int SS:2;	/* Sample Size */
    unsigned int SF:3;	/* Sample Frequency */
    unsigned int PB2_Rsvd:3;	/* Reserved */
    U8 FormatCodingType;	/* Format coding Type */
    U8 CA;		/* Channel Allocation/speaker placement */
    unsigned int LFEP:2;	/* Playback LFE level */
    unsigned int PB3_Rsvd:1;	/* Resereved */
    unsigned int LSV:4;	/* Level shift value */
    unsigned int DM_INH:1;	/* Down Mix Inhibit */
  } HDRX_AUDIO_INFOFRAME;

  /*MPEG InfoFrame (CEA-861-D, Table 23)*/
  typedef struct
  {
    U8 CheckSum;	/* Length of MPEG InfoFrame */
    U32 MpegBitRate;	/* MPEG BIT Rate in Hz */
    unsigned int MF:2;	/* MPEG Frame Information */
    unsigned int F52_53:2;	/* */
    unsigned int FR0:1;	/* Field Repeat ( 3:2 pull down) */
    unsigned int F55_57:3;
    unsigned int F60_67:8;
    unsigned int F70_77:8;
    unsigned int F80_87:8;
    unsigned int F90_97:8;
    unsigned int F100_107:8;
  } HDRX_MPEG_INFOFRAME;

  /*Audio Content Protection (ACP) Packet. TBD  (HDMI 1.3a, table 5-18, 19)*/
  typedef struct
  {
    U8 ACPType;	/* ACP TYPE - hdrx_ACPType_t */
    U8 ACPData[16];	/* ACP info Frame Unformatted Data */
  } HDRX_ACP_PACKET;

  /*ISRC1 Packet (HDMI 1.3a, table 5-21)*/
  typedef struct
  {
//    unsigned int ISRC_Status:3;	/* ISRC Status Bit - HB1 */
//    unsigned int PB1_Rsvd:3;	/* Reserved */
//    unsigned int ISRC_Valid:1;	/* ISRC Valid */
//    unsigned int ISRC_Contd:1;	/* ISRC Continued */
    unsigned int UPC_EAN_ISRC[16];	/* UPC/EAN or ISRC Code */
  } HDRX_ISRC1_PACKET;

  /*ISRC2 Packet (HDMI 1.3a, table 5-23)*/
  typedef struct
  {
    unsigned int UPC_EAN_ISRC[16];
  } HDRX_ISRC2_PACKET;

  /*Audio Clock Regeneration (ACR) Packet (HDMI 1.3a, table 5-11)*/
  typedef struct
  {
    U32 CTS;	/* 20 BIT value of Cycle Time Stamp */
    U32 N;		/* 20 BIT Vale of ACR "N" */
  } HDRX_ACR_PACKET;

  /*General Control Packet (GCP) (HDMI 1.3a, table 5-17)*/
  typedef struct
  {
    unsigned int Set_AVMUTE:1;	/* Set the AV Mute Flag */
    unsigned int GCP_Rsrv1:3;	/* Reserved */
    unsigned int Clear_AVMUTE:1;	/* Clear the AV mute Flag */
    unsigned int GCP_Rsrv2:3;	/* Reserved */
    unsigned int CD:4;	/* Color Depth */
    unsigned int PP:4;	/* Pixel Packing Phase */
    unsigned int Default_Phase:1;	/* Default Phase */
    unsigned int GCP_Rsrv3:7;
    U8 GCP_Rsrv4[4];
  } HDRX_GC_PACKET;

  /*Gamut header (HDMI 1.3a, table 5-30)*/
  typedef struct
  {
    unsigned int Next_Field:1;	/* */
    unsigned int GBD_profile:3;	/* Transmitted Profile Number */
    unsigned int Affected_Gamut_Seq_Num:4;	/* Which video field is relevant to this MetaData */
    unsigned int No_Crnt_GBD:1;	/* No Gamut MetaData Available for currently transmitted Video */
    unsigned int Rsvd:1;	/* Reserved */
    unsigned int Packet_Seq:2;	/* Gamut MetaData packet Sequence */
    unsigned int Current_Gamut_Seq_Num:4;	/* Current Gamut sequence number */
  } HDRX_GAMUT_HEADER_PACKET;

  /*Gamut packet (HDMI 1.3a, table 5-31, 32, 33)*/
  typedef struct
  {
    U8 GBD[28];
  } HDRX_GAMUT_PACKET;

  /*Audio channel status (IEC60958-3, table 2)*/
  typedef struct
  {
    unsigned int Standard:2;	/* status usage (gmt_IEC60958_AUDIO_STD) */
    unsigned int Copyright:1;	/* copyright asserted */
    unsigned int AdditionalInfo:3;	/* depend of Standard (typically pre-emphasis) */
    unsigned int Mode:2;	/* channel status mode */
    unsigned int CategoryCode:8;
    unsigned int SourceNumber:4;
    unsigned int ChannelNumber:4;
    unsigned int SamplingFrequency:4;
    unsigned int ClockAccuracy:2;
    unsigned int Reserved0:2;
    unsigned int WordLength:4;
    /* All other bit unavailable */
  } hdrx_audio_channel_status_t;

  typedef enum
  {
    HDMI_SUBSAMPLER_NONE,
    HDMI_SUBSAMPLER_DIV_1,
    HDMI_SUBSAMPLER_DIV_2,
    HDMI_SUBSAMPLER_AUTO
  } sthdmirx_sub_sampler_mode_t;

  /* Clock Selection & Reset controls*/
  typedef enum
  {
    RESET_CORE_LINK_CLOCK_DOMAIN = 0x0001,
    RESET_CORE_PACKET_PROCESSOR = 0x0002,
    RESET_CORE_VIDEO_LOGIC = 0x0004,
    RESET_CORE_TCLK_DOMAIN = 0x0008,
    RESET_CORE_AUDIO_OUTPUT = 0x0010,
    RESET_CORE_ALL_BLOCKS = (RESET_CORE_LINK_CLOCK_DOMAIN |
    RESET_CORE_PACKET_PROCESSOR |
    RESET_CORE_VIDEO_LOGIC |
    RESET_CORE_TCLK_DOMAIN |
    RESET_CORE_AUDIO_OUTPUT),
    RESET_CORE_CLOCK_DOMAIN_ONLY = (RESET_CORE_LINK_CLOCK_DOMAIN |
    RESET_CORE_TCLK_DOMAIN)
  } sthdmirx_core_reset_ctrl_t;

  typedef enum
  {
    HDMIRX_TCLK_SEL,
    HDMIRX_LINK_CLK_SEL,
    HDMIRX_VCLK_SEL,
    HDMIRX_PIX_CLK_OUT_SEL,
    HDMIRX_AUCLK_SEL
  } sthdmirx_clk_selection_t;

  typedef enum
  {
    CLK_GND,
    HDMIRX_AUDDS_CLK,
    HDMIRX_VCLK,
    HDMIRX_SUB_SAMPLER_CLK,
    HDMIRX_VDDS_CLK,
    HDMIRX_TMDS_CLK,
    HDMIRX_LCLK,
    HDMIRX_TCLK
  } sthdmirx_clk_type_t;

  /* Audio I2S enum types*/
  typedef enum
  {
    I2S_CLOCK_128_FS = 128,
    I2S_CLOCK_256_FS = 256,
    I2S_CLOCK_384_FS = 384,
    I2S_CLOCK_512_FS = 512
  } sthdmirx_I2S_master_clk_t;

  typedef enum
  {
    I2S_DATA_LEFT_ALIGNED_MODE,
    I2S_DATA_RIGHT_ALIGNED_MODE,
    I2S_DATA_I2S_MODE
  } sthdmirx_I2S_data_alignment_t;

  typedef enum
  {
    I2S_FIRST_LEFT_CHANNEL,
    I2S_FIRST_RIGHT_CHANNEL
  } sthdmirx_I2S_word_select_polarity_t;

  typedef enum
  {
    I2S_INPUT_FORMAT_LPCM,
    I2S_INPUT_FORMAT_IEC60958
  } sthdmirx_I2S_input_format_t;

  typedef enum
  {
    I2S_DATA_WIDTH_16_BITS = 16,
    I2S_DATA_WIDTH_24_BITS = 24,
    I2S_DATA_WIDTH_32_BITS = 32
  } sthdmirx_I2S_data_width_t;
  /* End of Audio I2S enum types*/

  typedef struct
  {
    HDRX_AVI_INFOFRAME stAviInfo;
    HDRX_AUDIO_INFOFRAME stAudioInfo;
    hdrx_audio_channel_status_t stAudioChannelStatus;
    HDRX_SPD_INFOFRAME stSpdInfo;
    HDRX_VENDOR_INFOFRAME stVSInfo;
    HDRX_ACP_PACKET stAcpInfo;
    HDRX_ACR_PACKET stAcrInfo;
    HDRX_MPEG_INFOFRAME stMpegInfo;
    HDRX_ISRC1_PACKET stIsrc1Info;
    HDRX_ISRC2_PACKET stIsrc2Info;
    HDRX_GC_PACKET stGcpInfo;
    HDRX_GAMUT_PACKET stMetaDataInfo;
    hdrx_GCPColorDepth_t eColorDepth;
  } sthdmirx_Sdata_pkt_acquition_t;

  typedef struct
  {
    BOOL bIsNewAviInfo;
    BOOL bIsNewAudioInfo;
    BOOL bIsNewAcs;
    BOOL bIsNewSpdInfo;
    BOOL bIsNewVsInfo;
    BOOL bIsNewAcpInfo;
    BOOL bIsNewAcrInfo;
    BOOL bIsNewMpegInfo;
    BOOL bIsNewIsrc1Info;
    BOOL bIsNewIsrc2Info;
    BOOL bIsNewGcpInfo;
    BOOL bIsNewMetaDataInfo;
  } sthdmirx_pkt_new_data_flags_t;

  typedef struct
  {
    U16 bIsAviInfoAvbl:1;
    U16 bIsAudioInfoAvbl:1;
    U16 bIsAcsAvbl:1;
    U16 bIsSpdInfoAvbl:1;
    U16 bIsVsInfoAvbl:1;
    U16 bIsAcpInfoAvbl:1;
    U16 bIsAcrInfoAvbl:1;
    U16 bIsMpegInfoAvbl:1;
    U16 bIsIsrc1InfoAvbl:1;
    U16 bIsIsrc2InfoAvbl:1;
    U16 bIsGcpInfoAvbl:1;
    U16 bIsMetaDataInfoAvbl:1;
  } sthdmirx_pkt_data_available_flags_t;

  typedef enum
  {
    HDRX_AUDIO_STATE_IDLE,	/* No packets/DVI/START */
    HDRX_AUDIO_DETECTION,	/* Audio Detection State */
    HDRX_AUDIO_SETUP,
    HDRX_AUDIO_MUTE,	/* Audio is muted */
    HDRX_AUDIO_OUTPUTTED,	/* Audio output is enabled */
  } sthdmirx_audio_mngr_state_t;

  /* Audio configuration exstracted from either Audio Channel Status or Audio InfoFrame*/
  typedef struct
  {
    U8 uChannelCount;
    U8 uChannelAllocation;
    stm_hdmirx_audio_coding_type_t CodingType;
    stm_hdmirx_audio_sample_size_t SampleSize;
    hdrx_audio_sample_freq_t SampleFrequency;
    stm_hdmirx_audio_stream_type_t eHdmiAudioSelectedStream;
  } sthdmirx_hdmi_audio_config_param_t;

  typedef struct
  {
    sthdmirx_hdmi_audio_config_param_t stAudioConfig;
    sthdmirx_audio_mngr_state_t stAudioMngrState;
    hdrx_audio_sample_freq_t eAudioSampleFreqAcr;
    BOOL bIsAudioPktsPresent;
    BOOL bIsAudioMuted;
    BOOL bAcpPacketPresent;
    U32 ulAcpTimer;
  } sthdmirx_audio_Mngr_ctrl_t;

  typedef enum
  {
    HDRX_STATUS_LINK_INIT = 0x0001,
    HDRX_STATUS_LINK_READY = 0x0002,
    HDRX_STATUS_NO_SIGNAL = 0x0004,
    HDRX_STATUS_UNSTABLE = 0x0008,
    HDRX_STATUS_STABLE = 0x0010
  } sthdmirx_status_flags_t;

  typedef enum
  {
    HDRX_FSM_STATE_IDLE,
    HDRX_FSM_STATE_START,
    HDRX_FSM_STATE_EQUALIZATION_UNDER_PROGRESS,
    HDRX_FSM_STATE_UNSTABLE_TIMING,
    HDRX_FSM_STATE_VERIFY_LINK_HEALTH,
    HDRX_FSM_STATE_TRANSITION_TO_SIGNAL,
    HDRX_FSM_STATE_TRANSITION_TO_NOSIGNAL,
    //   HDRX_FSM_STATE_AUDIO_VIDEO_MUTE,
    HDRX_FSM_STATE_AFR_AFB_TRIGGERED,
    HDRX_FSM_STATE_SIGNAL_STABLE,
    // HDRX_FSM_STATE_POWERDOWN
  } sthdmirx_FSM_states_t;

// HDMI Packet ID's
  typedef enum
  {
    HDRX_VENDOR_PACK = 0x01,
    HDRX_AVI_PACK = 0x02,
    HDRX_SPD_PACK = 0x03,
    HDRX_AUDIO_INFO_PACK = 0x04,
    HDRX_MPEG_PACK = 0x05,
    HDRX_ACP_PACK = 0x06,
    HDRX_ISRC1_PACK = 0x07,
    HDRX_ISRC2_PACK = 0x08,
    HDRX_ACR_PACK = 0x09,
    HDRX_UNDEF_PACK = 0x0A,
    HDRX_GC_PACK = 0x0B,
    HDRX_GAMUT_PACK = 0x0C,
    HDRX_PACK_END = 0x0D
  } sthdmirx_packet_id_t;

  /* Digital core measurement types*/
  typedef enum
  {
    CLEAR_PREV_CLOCK_MEAS_DATA = 0x0001,	/*BIT0 */
    CLEAR_PREV_HTIMING_MEAS_DATA = 0x0002,	/*BIT1 */
    CLEAR_PREV_VTIMING_MEAS_DATA = 0x0004,	/*BIT2 */
    SIG_STS_CLOCK_MEAS_DATA_AVBL = 0x0008,	/*BIT3 */
    SIG_STS_HTIMING_MEAS_DATA_AVBL = 0x0010,	/*BIT4 */
    SIG_STS_VTIMING_MEAS_DATA_AVBL = 0x0020,	/*BIT5 */
    SIG_STS_DOUBLE_CLK_MODE_PRESENT = 0x0040,	/*BIT6 */
    SIG_STS_UNSTABLE_TIMING_PRESENT = 0x0080,	/*BIT7 */
    SIG_STS_ABNORMAL_SYNC_TIMER_STARTED = 0x0100,	/*BIT8 */
    SIG_STS_SYNC_STABILITY_CHECK_STARTED = 0x0200	/*BIT9 */
  } meas_status_t;

  typedef struct
  {
    U32 LinkClockKHz;
    U16 HTotal;
    U16 VTotal;
    U16 HActive;
    U16 VActive;
  } signal_timing_info_t;

  typedef struct
  {
    signal_timing_info_t StableTimingInfo;
    U8 HErrorCount;
    U8 VErrorCount;
    U8 ErrorCountIdx;
    U8 tStatus;
  } stable_timing_ctrl_descriptor_t;

  typedef struct
  {
    BOOL IsLinkClkAvailable;
    BOOL IsLinkClkStable;
    U8 LinkClkMeasCount;
    BOOL LinkClockstatus;
    BOOL IsPLLSetupDone;
    U32 MeasTimer;
    U32 ulAbnormalSyncTimer;
    meas_status_t mStatus;
    U16 ClkMeasData[COUNT_MEAS];
    stable_timing_ctrl_descriptor_t SigStabilityCtrl;
    signal_timing_info_t CurrentTimingInfo;
  } sthdmirx_signal_meas_ctrl_t;

#ifdef STHDMIRX_I2S_TX_IP
  typedef struct
  {
    sthdmirx_I2S_master_clk_t MasterClk;
    sthdmirx_I2S_data_alignment_t DataAlignment;
    sthdmirx_I2S_word_select_polarity_t WordSelectPolarity;
    sthdmirx_I2S_input_format_t InputFormat;
    sthdmirx_I2S_data_width_t InputDataWidthForRightAlgn;
    sthdmirx_I2S_data_width_t OutputDataWidth;
  } sthdmirx_I2S_init_params_t;
#endif

  /* Private Constants --------------------------------------------------------------------- */

  /* Private variables (static) --------------------------------------------------------------- */

  /* Global Variables ----------------------------------------------------------------------- */

  /* Private Macros ------------------------------------------------------------------------ */

  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/

  /* ------------------------------- End of file --------------------------------------------- */
#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_CORE_H__ */
