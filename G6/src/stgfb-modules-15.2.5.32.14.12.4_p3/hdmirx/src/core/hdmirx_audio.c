/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_audio.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes -----------------------------------------------------*/

/* Include of Other module interface headers ----------------------------------*/

/* Local Includes --------------------------------------------------------*/
#include <hdmirx_drv.h>
#include <hdmirx_core.h>
#include <hdmirx_core_export.h>
#include <InternalTypes.h>
#include <hdmirx_RegOffsets.h>
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
#include "hdmirx_clkgen.h"
#endif

/* Private Typedef -----------------------------------------------*/
//#define ENABLE_DBG_HDRX_AUDIO_INFO

#ifdef ENABLE_DBG_HDRX_AUDIO_INFO
#define DBG_HDRX_AUDIO_INFO		HDMI_PRINT
#else
#define DBG_HDRX_AUDIO_INFO(a,b...)
#endif

/*Audio output mute mode*/
typedef enum
{
  HDMI_AUDIO_MM_ZERO_OUTPUT = 0,	/*Zero audio output */
  HDMI_AUDIO_MM_FREEZE_OUTPUT	/*Output last value */
} sthdmirx_audio_mute_mode_t;

/*L-PCM audio stream error correction type*/
typedef enum
{
  HDMI_AUDIO_EC_DISABLED = 0,	/*Correction is disabled */
  HDMI_AUDIO_EC_SKIP,	/*Error sample skipped (not sent out) */
  HDMI_AUDIO_EC_REPEAT	/*Repeat previous sample is case of error */
} sthdmirx_audio_correction_type_t;

/*L-PCM audio output mode. Determines if L-PCM audio goes through DAC (I2S) or decoder (SPDIF)*/
typedef enum
{
  HDMI_AUDIO_OUTPUT_DAC = 0,
  HDMI_AUDIO_OUTPUT_DECODER
} sthdmirx_audio_output_type_t;

/* Private Defines ------------------------------------------------*/
#define HDRX_AUDIO_STREAM_TYPE_ALL      			0x0f
#define HDRX_AUDIO_CHANNEL_ALLOCATION_INIT	        0xff

/*If ACP has not come within 600ms then declare ACP is lost (HDMI 1.3 spec, section 9.3)*/
#define HDRX_ACP_PACKET_TIMEOUT                     600

/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/*patch for audio test: Bugz 51668*/
U32 Current_N=10, Current_CTS=10;
#define DELTA_N_PARAMETER   5
#define DELTA_CTS_PARAMETER 5


typedef struct
{
  hdrx_audio_sample_freq_t SampleFreqId;
  U16 W_SampleFreqValue;	/*KHz*64 */
} sthdmirx_audio_sample_freq_t;

/*Sample frequency values merged from Audio InfoFrame and Audio Channel Status*/
sthdmirx_audio_sample_freq_t const Sa_SampleFreq[] =
{
  /*See CEA-861-D, table 18 */
  {HDRX_AUDIO_SF_32_KHZ, 2048},
  {HDRX_AUDIO_SF_44_1KHZ, 2822},
  {HDRX_AUDIO_SF_48_KHZ, 3072},
  {HDRX_AUDIO_SF_88_2KHZ, 5645},
  {HDRX_AUDIO_SF_96_KHZ, 6144},
  {HDRX_AUDIO_SF_176_4KHZ, 11290},
  {HDRX_AUDIO_SF_192_KHZ, 12288},

  /*See IEC60958-3, page 12 */
  {HDRX_AUDIO_SF_22_05KHZ, 1411},
  {HDRX_AUDIO_SF_24_KHZ, 1536},
  {HDRX_AUDIO_SF_8_KHZ, 512},
  {HDRX_AUDIO_SF_768_KHZ, 49152U}
};

/* Sample frecuency convert table (IEC60958)*/
static const hdrx_audio_sample_freq_t a_IEC60958SampleFrequencyTable[16] =
{
  HDRX_AUDIO_SF_44_1KHZ, HDRX_AUDIO_SF_NOT_INDICATED,
  HDRX_AUDIO_SF_48_KHZ, HDRX_AUDIO_SF_32_KHZ,
  HDRX_AUDIO_SF_22_05KHZ, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_24_KHZ, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_88_2KHZ, HDRX_AUDIO_SF_768_KHZ,
  HDRX_AUDIO_SF_96_KHZ, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_176_4KHZ, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_192_KHZ, HDRX_AUDIO_SF_RESERVED
};

/* Sample frecuency convert table (IEC 61937-1)*/
static const hdrx_audio_sample_freq_t a_IEC61937SampleFrequencyTable[16] =
{
  HDRX_AUDIO_SF_44_1KHZ, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_48_KHZ, HDRX_AUDIO_SF_32_KHZ,
  HDRX_AUDIO_SF_RESERVED, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_RESERVED, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_RESERVED, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_RESERVED, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_RESERVED, HDRX_AUDIO_SF_RESERVED,
  HDRX_AUDIO_SF_RESERVED, HDRX_AUDIO_SF_RESERVED
};

/* Sample word size convert table (IEC 60958-3, IEC 61937-1)*/
static const stm_hdmirx_audio_sample_size_t a_SampleSizeTable[16] =
{
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE, STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE,	/* not indicated */
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_16_BITS, STM_HDMIRX_AUDIO_SAMPLE_SIZE_20_BITS,
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_18_BITS, STM_HDMIRX_AUDIO_SAMPLE_SIZE_22_BITS,
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE, STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE,	/* reserved by IEC 60958-3 */
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_19_BITS, STM_HDMIRX_AUDIO_SAMPLE_SIZE_23_BITS,
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_20_BITS, STM_HDMIRX_AUDIO_SAMPLE_SIZE_24_BITS,
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_17_BITS, STM_HDMIRX_AUDIO_SAMPLE_SIZE_21_BITS,
  STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE, STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE	/* reserved by IEC 60958-3 */
};

/* Private functions prototypes ------------------------------------*/
hdrx_audio_sample_freq_t sthdmirx_audio_find_sample_clk_id_by_ACRpack(U16
    W_Ratio);
void sthdmirx_audio_update_Lpcm_sample_size(const hdmirx_handle_t Handle,
    stm_hdmirx_audio_sample_size_t SampleSize);
U8 sthdmirx_audio_HW_audio_coding_type_get(const hdmirx_handle_t Handle);
U8 sthdmirx_audio_HW_audio_channel_count_get(const hdmirx_handle_t Handle);
BOOL sthdmirx_handle_audio_channel_status_data(
  hdmirx_route_handle_t *const pInpHandle);
void sthdmirx_audio_setup_Iec_stream(const hdmirx_handle_t Handle,
                                     sthdmirx_audio_output_type_t OutputType,
                                     sthdmirx_audio_correction_type_t ErrorCorrection);
void sthdmirx_audio_setup_Hbr_stream(const hdmirx_handle_t Handle,
                                     BOOL B_Is8Channel);
void sthdmirx_audio_select_out_clk(const hdmirx_handle_t Handle, U16 W_ClkDiv);
void sthdmirx_audio_type_selectHW(hdmirx_handle_t Handle,
                                  stm_hdmirx_audio_stream_type_t estAudioStream);
U8 sthdmirx_audio_channel_status_formatted_get(const hdmirx_handle_t Handle,
    hdrx_audio_channel_status_t *Sp_Acs);

/* Interface procedures/functions -------------------------------------------------------------------------*/

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_enable_DSD_mode
 USAGE        :     Enable the Audio stream type "DSD" hardware processing.
 INPUT        :     B_Enabled = TRUE ( Enables the DSD mode) / FALSE ( Disable DSD packet processing)
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_audio_enable_DSD_mode(const hdmirx_handle_t Handle,
                                    BOOL B_Enabled)
{
  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_enable_DSD_mode B_Enabled = %d\n",
   B_Enabled);

  if (B_Enabled)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_OUT_CNTRL),
                             HDRX_DSD_MODE_EN);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_OUT_CNTRL),
                               HDRX_DSD_MODE_EN);
    }
}

/**************************************************************************************
 FUNCTION     :     sthdmirx_reset_audio_config_prop
 USAGE        :     Reset the software Audio Config propeties structure
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
**************************************************************************************/
void sthdmirx_reset_audio_config_prop(sthdmirx_audio_Mngr_ctrl_t *AudMngr)
{
  AudMngr->stAudioConfig.uChannelAllocation =
    HDRX_AUDIO_CHANNEL_ALLOCATION_INIT;
  AudMngr->stAudioConfig.uChannelCount = 0;
  AudMngr->stAudioConfig.CodingType = STM_HDMIRX_AUDIO_CODING_TYPE_NONE;
  AudMngr->stAudioConfig.SampleSize = STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE;
  AudMngr->stAudioConfig.SampleFrequency = HDRX_AUDIO_SF_NONE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_reset
 USAGE        :     Reset the Audio blocks
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_audio_reset(hdmirx_handle_t const Handle)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  InpHandle_p->stAudioMngr.stAudioConfig.eHdmiAudioSelectedStream =
    STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN;
  InpHandle_p->stAudioMngr.stAudioMngrState = HDRX_AUDIO_STATE_IDLE;

  sthdmirx_reset_audio_config_prop(&InpHandle_p->stAudioMngr);

  InpHandle_p->bIsAudioOutPutStarted = FALSE;
  InpHandle_p->bIsAudioPropertyChanged = FALSE;
  InpHandle_p->stAudioMngr.bAcpPacketPresent = FALSE;
  HDMI_MEM_SET(&(InpHandle_p->stInfoPacketData.stAudioInfo), 0xff,
               sizeof(HDRX_AUDIO_INFOFRAME));
  HDMI_MEM_SET(&(InpHandle_p->stInfoPacketData.stAudioChannelStatus),
               0xff, sizeof(hdrx_audio_channel_status_t));

  Current_N=10;
  Current_CTS=10;

  DBG_HDRX_AUDIO_INFO("Audio Reset\n");
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_initialize
 USAGE        :     Initialise the Audio processing pipe.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_audio_initialize(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress + HDRX_SDP_AUD_IRQ_CTRL),
                      0x0);

  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress + HDRX_SDP_AUD_MUTE_CTRL),
                      (HDRX_SDP_AUD_MUTE_AUTO_EN | HDRX_SDP_AUD_MUTE_SOFT
                       | HDRX_AU_MUTE_ON_SGNL_NOISE |
                       HDRX_AU_MUTE_ON_NO_SGNL));

  /* Configure audio samples buffer */
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress + HDRX_SDP_AUD_BUF_CNTRL),
                      0x00);
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress + HDRX_SDP_AUD_BUF_DELAY),
                      0x20);
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress +
                       HDRX_SDP_AUD_LINE_THRESH), 0x08);
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress +
                       HDRX_SDP_AUD_BUFPTR_DIF_THRESH_A), 0x40);
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress +
                       HDRX_SDP_AUD_BUFPTR_DIF_THRESH_B), 0x40);

  /* Set default channel relocation */
  HDMI_WRITE_REG_WORD((U32)
                      (InpHandle_p->BaseAddress +
                       HDRX_SDP_AUD_CHAN_RELOC), 0xE4);

  /* Enable audio conversion */
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress + HDRX_AU_CONV_CTRL),
                      HDRX_AUDIO_CONV_EN);

  /* Set timeout that no audio info frame in the channel */
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress +
                       HDRX_AU_INFO_TIMEOUT_THRESH), 0xF);

  /* Setup DSD interface, audio clocks and enable outputs */
  sthdmirx_audio_enable_DSD_mode(InpHandle_p, FALSE);

  /*Configure audio clock division ratio. */
  sthdmirx_audio_select_out_clk(Handle, 128);

  /*Enable HDMI audio out. Do the same as gm_HdmiAudioOutEnable does */
  HDMI_SET_REG_BITS_BYTE((U32)
                         (InpHandle_p->BaseAddress +
                          HDRX_SDP_AUD_OUT_CNTRL),
                         HDRX_SDP_AUD_OUT_EN | HDRX_SDP_AUD_OUT_DEN);

  sthdmirx_audio_reset(Handle);

}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_soft_mute
 USAGE        :     Software mute control
 INPUT        :     B_Mute = TRUE ( Audio muted), FALSE ( Unmute)
 RETURN       :     None
 USED_REGS    :     HDRX_SDP_AUD_MUTE_CTRL.HDRX_SDP_AUD_MUTE_SOFT
******************************************************************************/
void sthdmirx_audio_soft_mute(const hdmirx_handle_t Handle, BOOL B_Mute)
{
  if (B_Mute == TRUE)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_MUTE_CTRL),
                             HDRX_SDP_AUD_MUTE_SOFT);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_MUTE_CTRL),
                               HDRX_SDP_AUD_MUTE_SOFT);
    }
}

/******************************************************************************
 FUNCTION     :    sthdmirx_audio_clear_channel_status
 USAGE        :     Clears the audio channel status
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_audio_clear_channel_status(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  /* Set control bit */
  HDMI_SET_REG_BITS_BYTE((U32)
                         (InpHandle_p->BaseAddress + HDRX_SDP_AUD_CTRL),
                         HDRX_CLR_AU_CH_STS);

  /* Set some delay so hardware will clear the channel status */
  CPU_DELAY(10);

  /* Clear status bit */
  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress +
                       HDRX_SDP_AUDIO_IRQ_STATUS),
                      HDRX_SDP_AUD_STATUS_IRQ_STS);

  /* Clear control bit */
  HDMI_CLEAR_REG_BITS_BYTE((U32)
                           (InpHandle_p->BaseAddress + HDRX_SDP_AUD_CTRL),
                           HDRX_CLR_AU_CH_STS);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_clear_hardware
 USAGE        :     Clears the audio block hardware
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     HDRX_SDP_AUD_CTRL.HDRX_CLR_AU_BLOCK
******************************************************************************/
void sthdmirx_audio_clear_hardware(const hdmirx_handle_t Handle)
{
  HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                          HDRX_SDP_AUD_CTRL), HDRX_CLR_AU_BLOCK);

  CPU_DELAY(10);

  HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                            HDRX_SDP_AUD_CTRL), HDRX_CLR_AU_BLOCK);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_enable_clk_correction
 USAGE        :     Setup the audio clock correction mode
 INPUT        :     B_Enabled- TRUE ( Enable correction), FALSE = Disable the clock correction
 RETURN       :     None
 USED_REGS    :     HDRX_SDP_AUD_BUF_CNTRL.  HDRX_SDP_AUD_CLK_COR_EN
******************************************************************************/
void sthdmirx_audio_enable_clk_correction(const hdmirx_handle_t Handle,
    BOOL B_Enabled)
{
  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_enable_clk_correction, B_Enabled = %d\n",
   B_Enabled);

  if (B_Enabled)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_BUF_CNTRL),
                             HDRX_SDP_AUD_CLK_COR_EN);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_BUF_CNTRL),
                               HDRX_SDP_AUD_CLK_COR_EN);
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_InOut_routing
 USAGE        :     Setup the Audio channel route ( mapping of input channel with output channel)
 INPUT        :     B_InChannel = Input channel, B_OutChannel= Output channel
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_audio_InOut_routing(const hdmirx_handle_t Handle, U8 B_InChannel,
                                  U8 B_OutChannel)
{
  B_OutChannel *= 2;
  HDMI_CLEAR_AND_SET_REG_BITS_WORD((GET_CORE_BASE_ADDRS(Handle) +
                                    HDRX_SDP_AUD_CHAN_RELOC),
                                   HDRX_SDP_AUD_CH0_REL << B_OutChannel,
                                   B_InChannel << B_OutChannel);
}

/******************************************************************************
 FUNCTION     :  sthdmirx_audio_out_enable
 USAGE        :  Enable/Disable HDMI audio output data
 INPUT        :  B_Enable = TRUE , Audio Enable
                         = FALSe, Disable Audio
 RETURN       :  None
 USED_REGS    :  HDRX_SDP_AUD_OUT_CNTRL
******************************************************************************/
void sthdmirx_audio_out_enable(const hdmirx_handle_t Handle, BOOL B_Enable)
{
  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_out_enable, B_Enabled = %d\n", B_Enable);
  if (B_Enable)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_OUT_CNTRL),
                             HDRX_SDP_AUD_OUT_EN |
                             HDRX_SDP_AUD_OUT_DEN);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_OUT_CNTRL),
                               HDRX_SDP_AUD_OUT_EN |
                               HDRX_SDP_AUD_OUT_DEN);
    }

}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_select_out_clk
 USAGE        :     Select the division ratio for producing audio output
                    channels 0/1/2/3 sample rate
 INPUT        :     W_ClkDiv - AU_MCLK divider. Possible values are 128, 192, 256,
                    384, 512, 640, 768, 1024, 1152
 RETURN       :     None
 USED_REGS    :     HDMI_SDP_AUD_OUT_CNTRL
******************************************************************************/
void sthdmirx_audio_select_out_clk(const hdmirx_handle_t Handle, U16 W_ClkDiv)
{
  U8 B_ClockMode = 0x00;

  switch (W_ClkDiv)
    {
    case 128U:
      B_ClockMode = 0x00;
      break;

    case 192U:
      B_ClockMode = 0x01;
      break;

    case 256U:
      B_ClockMode = 0x02;
      break;

    case 384U:
      B_ClockMode = 0x03;
      break;

    case 512U:
      B_ClockMode = 0x04;
      break;

    case 640U:
      B_ClockMode = 0x05;
      break;

    case 768U:
      B_ClockMode = 0x06;
      break;

    case 1024U:
      B_ClockMode = 0x07;
      break;

    case 1152U:
      B_ClockMode = 0x09;
      break;

    default:
      /* Invalid clock divisor */
      HDMI_PRINT
      ("Error! sthdmirx_audio_select_out_clk: Illegal divider value\n");
      return;
    }

  HDMI_CLEAR_AND_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                    HDRX_SDP_AUD_OUT_CNTRL),
                                   HDRX_SDP_AUD_CLK_MODE,
                                   (B_ClockMode <<
                                    HDRX_SDP_AUD_CLK_MODE_SHIFT));
  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_select_out_clk, W_ClkDiv = %d\n",
   W_ClkDiv);

}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_set_mute_mode
 USAGE        :     Set audio mute mode
 INPUT        :     MuteMode may have one of the following values:
                    gmd_HDMI_AUDIO_MM_ZERO_OUTPUT - Zero audio output
                    gmd_HDMI_AUDIO_MM_FREEZE_OUTPUT - Output last value
 RETURN       :     None
 USED_REGS    :     HDMI_SDP_AUD_MUTE_CTRL
******************************************************************************/

void sthdmirx_audio_set_mute_mode(const hdmirx_handle_t Handle,
                                  sthdmirx_audio_mute_mode_t MuteMode)
{
  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_set_mute_mode, MuteMode = %d", MuteMode);

  if (HDMI_AUDIO_MM_ZERO_OUTPUT == MuteMode)
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_MUTE_CTRL),
                               HDRX_AU_MUTE_MD);
    }
  else if (HDMI_AUDIO_MM_FREEZE_OUTPUT == MuteMode)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_MUTE_CTRL),
                             HDRX_AU_MUTE_MD);
    }
  else
    {
      DBG_HDRX_AUDIO_INFO
      ("Error! sthdmirx_audio_set_mute_mode: Invalid mute mode specified: %x",
       MuteMode);
    }

}

/******************************************************************************
 FUNCTION     :    sthdmirx_audio_swap_out
 USAGE        :     Swap HDMI audio output channel sample stream left and right
 INPUT        :     B_OutChannel - Output HDMI audio channel number (0-3)
                    B_LRSwap - FALSE: No swap, TRUE: Swap
 RETURN       :     None
 USED_REGS    :     HDMI_SDP_AUD_CHAN_RELOC
******************************************************************************/
void sthdmirx_audio_swap_out(const hdmirx_handle_t Handle, U8 B_OutChannel,
                             BOOL B_LRSwap)
{
  if (B_OutChannel > 3)
    {
      HDMI_PRINT("Error! Invalid B_OutChannel value %d\n",
                 B_OutChannel);
      return;
    }

  if (!B_LRSwap)
    {
      HDMI_CLEAR_REG_BITS_WORD((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_CHAN_RELOC),
                               HDRX_SDP_AUD_CH01_SWAP <<
                               B_OutChannel);
    }
  else
    {
      HDMI_SET_REG_BITS_WORD((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_CHAN_RELOC),
                             HDRX_SDP_AUD_CH01_SWAP << B_OutChannel);
    }

}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_setup_Iec_stream
 USAGE        :     Setup IEC60958 stream processing parameters.
 INPUT        :     OutputType - target of outputs (DAC, external decoder)
                    ErrorCorrection - error correction policy
 RETURN       :     None
 USED_REGS    :     HDMI_SDP_AUD_BUF_CNTRL, HDMI_SDP_AUD_CNTRL
******************************************************************************/
void sthdmirx_audio_setup_Iec_stream(const hdmirx_handle_t Handle,
                                     sthdmirx_audio_output_type_t OutputType,
                                     sthdmirx_audio_correction_type_t ErrorCorrection)
{

  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_setup_Iec_stream, OutputType = %d  ",
   OutputType);
  DBG_HDRX_AUDIO_INFO
  ("API call: sthdmirx_audio_setup_Iec_stream, ErrorCorrection = %d\n",
   ErrorCorrection);

  /* Program additional mute condition */
  if (HDMI_AUDIO_OUTPUT_DAC == OutputType)
    {
      HDMI_SET_REG_BITS_BYTE((U32)
                             (InpHandle_p->BaseAddress +
                              HDRX_SDP_AUD_MUTE_CTRL),
                             HDRX_AU_MUTE_ON_COMPR);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((U32)
                               (InpHandle_p->BaseAddress +
                                HDRX_SDP_AUD_MUTE_CTRL),
                               HDRX_AU_MUTE_ON_COMPR);
    }

  if (InpHandle_p->stAudioMngr.stAudioConfig.CodingType ==
      STM_HDMIRX_AUDIO_CODING_TYPE_PCM)
    {
      sthdmirx_audio_update_Lpcm_sample_size(InpHandle_p,
                                             InpHandle_p->stAudioMngr.stAudioConfig.SampleSize);

      switch (ErrorCorrection)
        {
        default:
          DBG_HDRX_AUDIO_INFO
          ("Error! sthdmirx_audio_setup_Iec_stream: Invalid audio L-PCM correction mode %d\n",
           ErrorCorrection);
          /* Fall through */

        case HDMI_AUDIO_EC_DISABLED:
          HDMI_CLEAR_REG_BITS_BYTE((U32)
                                   (InpHandle_p->BaseAddress +
                                    HDRX_SDP_AUD_BUF_CNTRL),
                                   HDRX_SDP_AUD_REP_SAMPLE_ON_ERR
                                   |
                                   HDRX_SDP_AUD_SKIP_SAMPLE_ERR);
          break;

        case HDMI_AUDIO_EC_SKIP:
          /* Error correction should be on */
          HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                           (InpHandle_p->BaseAddress +
                                            HDRX_SDP_AUD_BUF_CNTRL),
                                           HDRX_SDP_AUD_REP_SAMPLE_ON_ERR,
                                           HDRX_SDP_AUD_SKIP_SAMPLE_ERR
                                           |
                                           HDRX_SDP_AUD_CLK_COR_EN);
          break;

        case HDMI_AUDIO_EC_REPEAT:
          HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                           (InpHandle_p->
                                            BaseAddress +
                                            HDRX_SDP_AUD_BUF_CNTRL),
                                           HDRX_SDP_AUD_SKIP_SAMPLE_ERR,
                                           HDRX_SDP_AUD_REP_SAMPLE_ON_ERR);
          break;
        }
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_update_Lpcm_sample_size
 USAGE        :     Program registers to conform current sample size for LPCM.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     HDRX_SDP_AUD_BUF_CNTRL
******************************************************************************/
void sthdmirx_audio_update_Lpcm_sample_size(const hdmirx_handle_t Handle,
    stm_hdmirx_audio_sample_size_t SampleSize)
{
  switch (SampleSize)
    {

    default:
      DBG_HDRX_AUDIO_INFO
      ("Error! sthdmirx_audio_update_Lpcm_sample_size: Invalid sample size %d\n",
       SampleSize);
      /* Fall through */

    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_16_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_17_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_18_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_19_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_20_BITS:
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                              HDRX_SDP_AUD_BUF_CNTRL),
                             HDRX_AU_WIDTH20_EN);
      break;

    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_21_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_22_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_23_BITS:
    case STM_HDMIRX_AUDIO_SAMPLE_SIZE_24_BITS:
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                HDRX_SDP_AUD_BUF_CNTRL),
                               HDRX_AU_WIDTH20_EN);
      break;

    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_setup_Hbr_stream
 USAGE        :     Setup HDMI hardware for High Bit Rate audio stream processing
 INPUT        :     B_Is8Channel - Select whether HBR stream is distributed over 8 channels to be transmitted
                    further over I2S (TRUE) or 2 channels are transmitted over SPDIF (FALSE)
 RETURN       :     None
 USED_REGS    :     HDMI_SDP_AUD_BUF_CNTRL, HDMI_SDP_AUD_CNTRL
******************************************************************************/
void sthdmirx_audio_setup_Hbr_stream(const hdmirx_handle_t Handle,
                                     BOOL B_Is8Channel)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  DBG_HDRX_AUDIO_INFO
  ("API call: gm_HdmiAudioSetupHbrStream, B_Is8Channel = %d\n",
   B_Is8Channel);

  if (B_Is8Channel == TRUE)
    {
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                       (InpHandle_p->BaseAddress +
                                        HDRX_SDP_AUD_BUF_CNTRL),
                                       HDRX_BUF_EXT_EN,
                                       HDRX_DST_WIDE_EN);
      HDMI_WRITE_REG_BYTE((U32)
                          (InpHandle_p->BaseAddress +
                           HDRX_SDP_AUD_BUF_DELAY), 0x20);
    }
  else
    {
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                       (InpHandle_p->BaseAddress +
                                        HDRX_SDP_AUD_BUF_CNTRL),
                                       HDRX_DST_WIDE_EN,
                                       HDRX_BUF_EXT_EN);
      HDMI_WRITE_REG_BYTE((U32)
                          (InpHandle_p->BaseAddress +
                           HDRX_SDP_AUD_BUF_DELAY), 0x38);
    }

}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_select_stream
 USAGE        :     Setup HDMI Hardware for the selected audio stream type
 INPUT        :     estAudioStream =  Audio Stream Type ( IEC/DSD/DST/HBR)
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_audio_select_stream(hdmirx_handle_t Handle,
                                  stm_hdmirx_audio_stream_type_t estAudioStream)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  /*Reset All software variables, releated to Audio */
  sthdmirx_reset_audio_config_prop(&InpHandle_p->stAudioMngr);

  /* Write the common Register configuration */
  //HDMI_SET_REG_BITS_BYTE((U32)(InpHandle_p->BaseAddress+HDRX_SDP_AUD_MUTE_CTRL), HDRX_AU_MUTE_ON_COMPR);

  /* Reset buffer control settings */
  HDMI_CLEAR_REG_BITS_BYTE((U32)
                           (InpHandle_p->BaseAddress +
                            HDRX_SDP_AUD_BUF_CNTRL),
                           HDRX_SDP_AUD_CLK_COR_EN | HDRX_AU_WIDTH20_EN |
                           HDRX_DST_WIDE_EN | HDRX_BUF_EXT_EN |
                           HDRX_REPLACE_NVALID |
                           HDRX_SDP_AUD_SKIP_SAMPLE_ERR |
                           HDRX_SDP_AUD_REP_SAMPLE_ON_ERR);

  HDMI_WRITE_REG_BYTE((U32)
                      (InpHandle_p->BaseAddress + HDRX_SDP_AUD_BUF_DELAY), 0x20);

  sthdmirx_audio_type_selectHW(Handle, estAudioStream);

  if (estAudioStream == STM_HDMIRX_AUDIO_STREAM_TYPE_DSD)
    {
      sthdmirx_audio_enable_DSD_mode(Handle, TRUE);
    }
  else
    {
      sthdmirx_audio_enable_DSD_mode(Handle, FALSE);
    }

  /* Clear the Audio Hardare status & channel status */
  sthdmirx_audio_clear_hardware(Handle);
  sthdmirx_audio_clear_channel_status(Handle);

  /* Clear Audio Info frame */
  sthdmirx_CORE_clear_packet(Handle, HDRX_AUDIO_INFO_PACK);

  /* Clear ACR packet */
  sthdmirx_CORE_clear_packet(Handle, HDRX_ACR_PACK);

  return TRUE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_stream_get
 USAGE        :     Get the Audio stream type from the hardware
 INPUT        :     None
 RETURN       :     Audio Stream Type
 USED_REGS    :     HDRX_SDP_AUD_STS
******************************************************************************/
stm_hdmirx_audio_stream_type_t sthdmirx_audio_stream_get(const hdmirx_handle_t
    Handle)
{
  U8 uStreamType;
  stm_hdmirx_audio_stream_type_t stAudStream =
    STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN;
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  uStreamType = (U8)((U32)HDMI_READ_REG_DWORD((U32)(InpHandle_p->BaseAddress +
                     HDRX_SDP_AUD_STS)) >> HDRX_AU_SAMPLE_PRSNT_SHIFT);

  uStreamType &= HDRX_AUDIO_STREAM_TYPE_ALL;

  if (uStreamType & BIT0)
    {
      stAudStream = STM_HDMIRX_AUDIO_STREAM_TYPE_IEC;
    }
  else if (uStreamType & BIT1)
    {
      stAudStream = STM_HDMIRX_AUDIO_STREAM_TYPE_DSD;
    }
  else if (uStreamType & BIT2)
    {
      stAudStream = STM_HDMIRX_AUDIO_STREAM_TYPE_DST;
    }
  else if (uStreamType & BIT3)
    {
      stAudStream = STM_HDMIRX_AUDIO_STREAM_TYPE_HBR;
    }
  else
    {
      stAudStream = STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN;
    }

  return stAudStream;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_manager
 USAGE        :     Audio manager which manages the audio processing
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
sthdmirx_audio_mngr_state_t sthdmirx_audio_manager(hdmirx_handle_t Handle)
{
  stm_hdmirx_audio_stream_type_t aStreamType;
  sthdmirx_audio_Mngr_ctrl_t *AudMngr;
  hdmirx_route_handle_t *pInpHandle;
  BOOL bIsAudioConfigChg = FALSE;

  pInpHandle = (hdmirx_route_handle_t *) Handle;
  AudMngr = &pInpHandle->stAudioMngr;

  /* Check the DVI signal */
  if (pInpHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)
    {
      /*Reset the Audio Block & Put to audio Mute state. */
    }

  aStreamType = sthdmirx_audio_stream_get(Handle);

  if (aStreamType != AudMngr->stAudioConfig.eHdmiAudioSelectedStream)
    {
      sthdmirx_audio_type_selectHW(Handle, aStreamType);
    }
  /* Check the audio pkts present Status */
  AudMngr->bIsAudioPktsPresent = TRUE;
  if (!
      (HDMI_READ_REG_WORD
       ((U32) (pInpHandle->BaseAddress + HDRX_SDP_AUD_STS)) &
       HDRX_SDP_AUD_PKT_PRSNT))
    {
      /* Audio packets are not present, Take actions. */
      AudMngr->bIsAudioPktsPresent = FALSE;
    }

  /* if audio packets are not present & HDMI signal or Packet Noise is detected, don't need  to process just return */
  if ((AudMngr->bIsAudioPktsPresent == FALSE) ||
      (pInpHandle->bIsPacketNoisedetected == TRUE) ||
      (aStreamType == STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN))
    {
      DBG_HDRX_AUDIO_INFO(" Noise:%d  ",
                          pInpHandle->bIsPacketNoisedetected);
      DBG_HDRX_AUDIO_INFO("Pkt present:%d  ",
                          AudMngr->bIsAudioPktsPresent);
      DBG_HDRX_AUDIO_INFO("streamType:%d\n", aStreamType);

      if (AudMngr->stAudioMngrState != HDRX_AUDIO_STATE_IDLE)
        {
          DBG_HDRX_AUDIO_INFO
          (" Audio Pkt Present Status <<%d>>  ",
           AudMngr->bIsAudioPktsPresent);
          DBG_HDRX_AUDIO_INFO(" Packet Noise Status<<%d>>\n",
                              pInpHandle->bIsPacketNoisedetected);
          sthdmirx_audio_reset(Handle);
        }
      return AudMngr->stAudioMngrState;
    }

  /* Check the ACP present status and start the timer */
  if ((HDMI_READ_REG_WORD
       ((U32) (pInpHandle->BaseAddress + HDRX_SDP_PRSNT_STS)) &
       HDRX_ACP_PRSNT_STS))
    {
      pInpHandle->stAudioMngr.ulAcpTimer = stm_hdmirx_time_now();
      HDMI_CLEAR_REG_BITS_BYTE((U32)
                               (pInpHandle->BaseAddress +
                                HDRX_SDP_PRSNT_STS), HDRX_ACP_PRSNT_STS);
    }

  if (aStreamType != AudMngr->stAudioConfig.eHdmiAudioSelectedStream)
    {
      HDMI_PRINT("Audio Stream Type[Prev:%d ]",
                 AudMngr->stAudioConfig.eHdmiAudioSelectedStream);
      HDMI_PRINT("Audio Stream TypeCurrent:%d ]\n", aStreamType);

      /* Software variables update */
      AudMngr->stAudioConfig.eHdmiAudioSelectedStream = aStreamType;

      sthdmirx_audio_select_stream(Handle, aStreamType);

      /* Clear any event generated signals */
      pInpHandle->stDataAvblFlags.bIsAudioInfoAvbl = 0;
      pInpHandle->stDataAvblFlags.bIsAcrInfoAvbl = 0;
      pInpHandle->stDataAvblFlags.bIsAcsAvbl = 0;
      pInpHandle->stNewDataFlags.bIsNewAudioInfo = FALSE;
      AudMngr->stAudioMngrState = HDRX_AUDIO_SETUP;
    }
  if ((pInpHandle->stDataAvblFlags.bIsAudioInfoAvbl == 1) &&
      (AudMngr->stAudioMngrState == HDRX_AUDIO_SETUP))
    {
      /* Update the event send. */

      AudMngr->stAudioMngrState = HDRX_AUDIO_MUTE;
      AudMngr->stAudioConfig.uChannelAllocation =
        pInpHandle->stInfoPacketData.stAudioInfo.CA;
      AudMngr->stAudioConfig.uChannelCount =
        pInpHandle->stInfoPacketData.stAudioInfo.CC;
      AudMngr->stAudioConfig.SampleFrequency =
        pInpHandle->stInfoPacketData.stAudioInfo.SF;

      if (AudMngr->stAudioConfig.uChannelCount != 0)
        {
          /* SEA 861D: Channel count is one greater the code, except zero value */
          AudMngr->stAudioConfig.uChannelCount++;
        }

      if (STM_HDMIRX_AUDIO_STREAM_TYPE_IEC ==
          AudMngr->stAudioConfig.eHdmiAudioSelectedStream)
        {
          AudMngr->stAudioConfig.CodingType =
            pInpHandle->stInfoPacketData.stAudioInfo.CT;
          if (pInpHandle->stInfoPacketData.stAudioInfo.CT >
              STM_HDMIRX_AUDIO_CODING_TYPE_WMA_PRO)
            {
              AudMngr->stAudioConfig.CodingType =
                STM_HDMIRX_AUDIO_CODING_TYPE_NONE;
            }

          AudMngr->stAudioConfig.SampleSize =
            STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE;
          sthdmirx_audio_setup_Iec_stream(Handle,
                                          HDMI_AUDIO_OUTPUT_DECODER,
                                          HDMI_AUDIO_EC_DISABLED);
        }
      else
        {
          AudMngr->stAudioConfig.CodingType =
            pInpHandle->stInfoPacketData.stAudioInfo.CT;
          AudMngr->stAudioConfig.SampleSize =
            pInpHandle->stInfoPacketData.stAudioInfo.SS;

          if (STM_HDMIRX_AUDIO_STREAM_TYPE_HBR ==
              AudMngr->stAudioConfig.eHdmiAudioSelectedStream)
            {
              sthdmirx_audio_setup_Hbr_stream(Handle, FALSE);
            }
        }

      bIsAudioConfigChg = TRUE;
      DBG_HDRX_AUDIO_INFO
      ("Audio Info Frame is available for Event Notification\n");
    }

  if ((AudMngr->stAudioMngrState == HDRX_AUDIO_MUTE)
      || (AudMngr->stAudioMngrState == HDRX_AUDIO_OUTPUTTED))
    {
      /* Check the Audio InfoFrame related  when there is a new audio infoFrame */
      if ((pInpHandle->stDataAvblFlags.bIsAudioInfoAvbl == 1) &&
          (pInpHandle->stNewDataFlags.bIsNewAudioInfo == TRUE))
        {
          /* HDMI 1.3 spec, 8.2.2:
             Coding type, Sample size shall be zero, i.e. "Refer to stream".
             Sample frequency valid for DSD and DST streams, overwise shall be zero.
           */
          switch (AudMngr->stAudioConfig.eHdmiAudioSelectedStream)
            {
            case STM_HDMIRX_AUDIO_STREAM_TYPE_IEC:
              /* Audio channel allocation not valid for compressed stream  See HDMI 1.3 spec, 8.2.2 */
              if (AudMngr->stAudioConfig.uChannelAllocation !=
                  pInpHandle->stInfoPacketData.stAudioInfo.CA)
                {
                  HDMI_PRINT
                  ("Channel Allocation  in AIF changed Previous %d ",
                   AudMngr->stAudioConfig.uChannelAllocation);
                  HDMI_PRINT
                  ("Channel Allocation  in AIF changed Current %d ",
                   pInpHandle->stInfoPacketData.stAudioInfo.CA);
                  AudMngr->stAudioConfig.uChannelAllocation =
                    pInpHandle->stInfoPacketData.stAudioInfo.CA;
                  bIsAudioConfigChg = TRUE;
                }
              if (AudMngr->stAudioConfig.CodingType !=
                  pInpHandle->stInfoPacketData.stAudioInfo.CT)
                {
                  HDMI_PRINT("\nCoding Type [ Old:%d  ]",
                             AudMngr->stAudioConfig.CodingType);
                  HDMI_PRINT("\nCoding Type[New:%d]  \n",
                             pInpHandle->stInfoPacketData.stAudioInfo.CT);
                  AudMngr->stAudioConfig.CodingType =
                    pInpHandle->stInfoPacketData.stAudioInfo.CT;
                  if (pInpHandle->stInfoPacketData.stAudioInfo.CT >
                      STM_HDMIRX_AUDIO_CODING_TYPE_WMA_PRO)
                    {
                      AudMngr->stAudioConfig.CodingType =
                        STM_HDMIRX_AUDIO_CODING_TYPE_NONE;
                    }
                  bIsAudioConfigChg = TRUE;
                }
              break;
            case STM_HDMIRX_AUDIO_STREAM_TYPE_DSD:
            case STM_HDMIRX_AUDIO_STREAM_TYPE_DST:
              /* For this streams sample frequency may have valid values */
              if (AudMngr->stAudioConfig.SampleFrequency !=
                  pInpHandle->stInfoPacketData.stAudioInfo.SF)
                {
                  HDMI_PRINT
                  ("Sample frequency in AIF changed Previous %d",
                   AudMngr->stAudioConfig.SampleFrequency);
                  HDMI_PRINT
                  ("Sample frequency in AIF changed Current %d\n",
                   pInpHandle->stInfoPacketData.stAudioInfo.SF);
                  AudMngr->stAudioConfig.SampleFrequency =
                    pInpHandle->stInfoPacketData.stAudioInfo.SF;
                  bIsAudioConfigChg = TRUE;
                }
              if (AudMngr->stAudioConfig.uChannelAllocation !=
                  pInpHandle->stInfoPacketData.stAudioInfo.CA)
                {
                  HDMI_PRINT
                  ("Channel Allocation  in AIF changed Previous %d",
                   AudMngr->stAudioConfig.uChannelAllocation);
                  HDMI_PRINT
                  ("Channel Allocation  in AIF changed Current %d",
                   pInpHandle->stInfoPacketData.stAudioInfo.CA);
                  AudMngr->stAudioConfig.uChannelAllocation =
                    pInpHandle->stInfoPacketData.stAudioInfo.CA;
                  bIsAudioConfigChg = TRUE;
                }
              break;
            case STM_HDMIRX_AUDIO_STREAM_TYPE_HBR:
              /* HBR - is compressed IEC60958, i.e. IEC60937  HDMI 1.3 spec, 7.1 */
              break;
            default:
              HDMI_PRINT("Error! Audio Stream Type = %d\n",
                         AudMngr->stAudioConfig.eHdmiAudioSelectedStream);
              break;
            }
#if 0
          else
            {
              /* HDMI 1.1 spec, 8.2.2: Coding type, Sample size, Sample frequency shall be zero, i.e. "Refer to stream". */
              if (AudMngr->stAudioConfig.uChannelAllocation !=
                  pInpHandle->stInfoPacketData.stAudioInfo.
                  CA)
                {
                  HDMI_PRINT
                  ("Channel Allocation  in AIF changed from %d  to %d \n",
                   AudMngr->stAudioConfig.
                   uChannelAllocation,
                   pInpHandle->stInfoPacketData.
                   stAudioInfo.CA);
                  AudMngr->stAudioConfig.
                  uChannelAllocation =
                    pInpHandle->stInfoPacketData.
                    stAudioInfo.CA;
                  bIsAudioConfigChg = TRUE;
                }
            }
#endif

          if ((AudMngr->stAudioConfig.uChannelCount !=
               pInpHandle->stInfoPacketData.stAudioInfo.CC)
              && (pInpHandle->stInfoPacketData.stAudioInfo.CC !=
                  0))
            {
              /* SEA 861D: Channel count is one greater the code, except zero value */
              AudMngr->stAudioConfig.uChannelCount =
                pInpHandle->stInfoPacketData.stAudioInfo.CC + 1;
              bIsAudioConfigChg = TRUE;
            }
          pInpHandle->stNewDataFlags.bIsNewAudioInfo = FALSE;
        }

      /* Check the ACS data */
      if ((pInpHandle->stDataAvblFlags.bIsAcsAvbl == 1)
          && (pInpHandle->stNewDataFlags.bIsNewAcs == TRUE))
        {
          if (((pInpHandle->stInfoPacketData.stAudioChannelStatus.Standard != HDRX_IEC60958_AUDIO_STD_3)
               && (pInpHandle->stInfoPacketData.stAudioChannelStatus.Standard !=
                   HDRX_IEC60958_AUDIO_STD_61937))
              || (pInpHandle->stInfoPacketData.stAudioChannelStatus.Mode != 0))
            {
              HDMI_PRINT("Audio Channel Status Error \n");
            }
          else if (pInpHandle->stInfoPacketData.stAudioChannelStatus.Standard ==
                   HDRX_IEC60958_AUDIO_STD_3)
            {
              HDMI_PRINT
              ("ACS Standard  :IEC60958_AUDIO_STD_3 \n");
              HDMI_PRINT("SF-ACS        :%d\n",
                         pInpHandle->stInfoPacketData.stAudioChannelStatus.SamplingFrequency);
              HDMI_PRINT("CT-ACS        :L-PCM  CT:%d\n",
                         AudMngr->stAudioConfig.CodingType);
              HDMI_PRINT("SS-ACS        :%d     \n",
                         pInpHandle->stInfoPacketData.stAudioChannelStatus.WordLength);
              HDMI_PRINT("SS            :%d\n",
                         AudMngr->stAudioConfig.SampleSize);
              AudMngr->stAudioConfig.CodingType =
                STM_HDMIRX_AUDIO_CODING_TYPE_PCM;
              bIsAudioConfigChg = TRUE;
            }
          else
            {
              HDMI_PRINT
              ("ACS Standard  :IEC61937_AUDIO_STD \n");
              HDMI_PRINT("SF-ACS        :%d\n",
                         pInpHandle->stInfoPacketData.stAudioChannelStatus.SamplingFrequency);
              //HDMI_PRINT("Coding Type     :%d\n",AudMngr->stAudioConfig.CodingType);
              HDMI_PRINT("SS-ACS        :%d	    \n",
                         pInpHandle->stInfoPacketData.stAudioChannelStatus.WordLength);
              HDMI_PRINT("SS            :%d\n",
                         AudMngr->stAudioConfig.SampleSize);
              //if (AudMngr->stAudioConfig.CodingType != STHDMIRX_AUDIO_CODING_TYPE_AC3)
              {
                AudMngr->stAudioConfig.CodingType =
                  STM_HDMIRX_AUDIO_CODING_TYPE_AC3;
                bIsAudioConfigChg = TRUE;
              }
            }

          pInpHandle->stNewDataFlags.bIsNewAcs = FALSE;
        }

      /*if audio channel count is not available in the Audio Info Frame, then check the hardware value */
      if (AudMngr->stAudioConfig.uChannelCount == 0)
        {
          U8 uChCount =
            sthdmirx_audio_HW_audio_channel_count_get(Handle);
          if (uChCount != 0)
            {
              AudMngr->stAudioConfig.uChannelCount =
                uChCount + 1;
              HDMI_PRINT(" HdmiRx HW Ch Count :%d\n",
                         AudMngr->stAudioConfig.uChannelCount);
              bIsAudioConfigChg = TRUE;
            }
        }
      /*if Audio Coding Type is NONE from info frame, check Audio Channel Status */
      if (AudMngr->stAudioConfig.CodingType ==
          STM_HDMIRX_AUDIO_CODING_TYPE_NONE)
        {
          hdrx_audio_channel_status_t Sp_Acs;
          sthdmirx_audio_channel_status_formatted_get(Handle,
              &Sp_Acs);
          if (((Sp_Acs.Standard != HDRX_IEC60958_AUDIO_STD_3)
               && (Sp_Acs.Standard !=
                   HDRX_IEC60958_AUDIO_STD_61937))
              || (Sp_Acs.Mode != 0))
            {
              HDMI_PRINT("Audio Channel Status Error \n");
            }
          else if (Sp_Acs.Standard == HDRX_IEC60958_AUDIO_STD_3)
            {
              AudMngr->stAudioConfig.CodingType =
                STM_HDMIRX_AUDIO_CODING_TYPE_PCM;
              bIsAudioConfigChg = TRUE;
            }
          else
            {
              AudMngr->stAudioConfig.CodingType =
                STM_HDMIRX_AUDIO_CODING_TYPE_AC3;
              bIsAudioConfigChg = TRUE;
            }
        }
      /* Check for ACR packets */
      if (pInpHandle->stDataAvblFlags.bIsAcrInfoAvbl == 1)
        {
          if (pInpHandle->stInfoPacketData.stAcrInfo.CTS == 0)
            {
              /*invalid Value..Don't do anything :) */
            }
          else
            {
              U32 DW_Tmp;
              U16 W_Tmp;
              hdrx_audio_sample_freq_t AcrSampleFreq;

              /*Divide by 2 to prevent U32 overflow to high TDMS clk */
              W_Tmp =
                (U16) (pInpHandle->stInfoPacketData.stAcrInfo.N / 2UL);
              DW_Tmp =
                (pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz) * (W_Tmp);

              AcrSampleFreq =
                sthdmirx_audio_find_sample_clk_id_by_ACRpack
                ((U16)
                 (DW_Tmp /
                  pInpHandle->stInfoPacketData.stAcrInfo.CTS));


              /*DBG_HDRX_AUDIO_INFO("\n sthdmirx_audio_manager: N: %d, CTS: %d, Current_N: %d, Current_CTS: %d \n", pInpHandle->stInfoPacketData.stAcrInfo.N, pInpHandle->stInfoPacketData.stAcrInfo.CTS, Current_N, Current_CTS );*/
              if ((pInpHandle->stInfoPacketData.stAcrInfo.N > (Current_N+DELTA_N_PARAMETER))||
                (pInpHandle->stInfoPacketData.stAcrInfo.N < (Current_N-DELTA_N_PARAMETER))||
                (pInpHandle->stInfoPacketData.stAcrInfo.CTS > (Current_CTS+DELTA_CTS_PARAMETER))||
                (pInpHandle->stInfoPacketData.stAcrInfo.CTS < (Current_CTS-DELTA_CTS_PARAMETER)))
                {
                  sthdmirx_clkgen_DDS_openloop_force(pInpHandle->stDdsConfigInfo.estAudDds,
                                                     pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz * 1000,
                                                     pInpHandle->MappedClkGenAddress);

                  Current_N= pInpHandle->stInfoPacketData.stAcrInfo.N ;
                  Current_CTS= pInpHandle->stInfoPacketData.stAcrInfo.CTS;

                  /*Write the value of N/CTS*/
                  HDMI_WRITE_REG_DWORD((U32) (pInpHandle->BaseAddress + HDRX_SDP_ACR_N_VALUE), Current_N);

                  HDMI_WRITE_REG_DWORD((U32)(pInpHandle->BaseAddress + HDRX_SDP_ACR_CTS_VALUE ), Current_CTS);

                  STHDMIRX_DELAY_1ms(1);

                  DBG_HDRX_AUDIO_INFO("\n sthdmirx_audio_manager: Update de N and CTS \n");

                  HDMI_SET_REG_BITS_BYTE((U32)(pInpHandle->BaseAddress + HDRX_SDP_ACR_NCTS_STB),
                                       BIT_HDRX_SDP_ACR_NCTS_STB);

                  DBG_HDRX_AUDIO_INFO("\n sthdmirx_audio_manager: Update de N and CTS \n");

                  STHDMIRX_DELAY_1ms(1);

                  sthdmirx_clkgen_DDS_closeloop_force(pInpHandle->stDdsConfigInfo.estAudDds,
                                                      pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz * 1000,
                                                      pInpHandle->MappedClkGenAddress);
                }

              if (AcrSampleFreq != AudMngr->stAudioConfig.SampleFrequency)
                {
                  bIsAudioConfigChg = TRUE;
                  AudMngr->stAudioConfig.SampleFrequency = AcrSampleFreq;
                  HDMI_PRINT("ACR Sample Freq changed to:%d\n", AcrSampleFreq);
                }
            }
        }

      /*Check for ACP packet Info */
      if ((AudMngr->bAcpPacketPresent == TRUE) &&
          (stm_hdmirx_time_minus
           (stm_hdmirx_time_now(),
            AudMngr->ulAcpTimer) >
           (M_NUM_TICKS_PER_MSEC(HDRX_ACP_PACKET_TIMEOUT))))
        {
          AudMngr->bAcpPacketPresent = FALSE;
          pInpHandle->stInfoPacketData.stAcpInfo.ACPType = 0;
          pInpHandle->stNewDataFlags.bIsNewAcpInfo = TRUE;
          HDMI_PRINT
          (" ACP packet lost timeout, set ACP type->0\n");
          sthdmirx_CORE_clear_packet(pInpHandle, HDRX_ACP_PACK);
        }
    }

  if (pInpHandle->bIsAudioOutPutStarted == TRUE)
    {
      sthdmirx_audio_soft_mute(Handle, FALSE);
    }
  else
    {
      sthdmirx_audio_soft_mute(Handle, TRUE);
    }
  if (bIsAudioConfigChg == TRUE)
    {
      pInpHandle->bIsAudioPropertyChanged = TRUE;
      HDMI_PRINT
      ("\n******AUDIO INFO FRAME DATA********************\n");
      HDMI_PRINT("Audio Stream Type    :%d \n",
                 AudMngr->stAudioConfig.eHdmiAudioSelectedStream);
      HDMI_PRINT("Coding Type          :%d \n",
                 AudMngr->stAudioConfig.CodingType);
      HDMI_PRINT("Channel Count        :%d \n",
                 AudMngr->stAudioConfig.uChannelCount);
      HDMI_PRINT("Channel Allocation   :%d \n",
                 AudMngr->stAudioConfig.uChannelAllocation);
      HDMI_PRINT("SampleFrequency      :%d \n",
                 AudMngr->stAudioConfig.SampleFrequency);
      HDMI_PRINT("SampleSize           :%d \n",
                 AudMngr->stAudioConfig.SampleSize);
      HDMI_PRINT("DownMix Inhibit      :%d \n",
                 pInpHandle->stInfoPacketData.stAudioInfo.DM_INH);
      HDMI_PRINT("Level shift          :%d \n",
                 pInpHandle->stInfoPacketData.stAudioInfo.LSV);
      HDMI_PRINT("*******************END***********************\n\n");
    }
  return AudMngr->stAudioMngrState;
}

/******************************************************************************
 FUNCTION		: sthdmirx_audio_find_sample_clk_id_by_ACRpack
 USAGE			: Find sample frequency ID corresponding to TMDS_CLK*N/CTS ratio taken from ACR packet
 INPUT			: W_Ratio - TMDS_CLK*N/CTS ratio taken from ACR packet
 OUTPUT			: hdrx_audio_sample_freq_t
 USED_REGS		: None
******************************************************************************/
hdrx_audio_sample_freq_t sthdmirx_audio_find_sample_clk_id_by_ACRpack(U16 W_Ratio)
{
  U8 i, B_MinIndex = 0xff;
  U16 W_MinDiff = ~(U16) (0);

  for (i = 0;
       i < sizeof(Sa_SampleFreq) / sizeof(sthdmirx_audio_sample_freq_t);
       i++)
    {
      S32 SDW_Diff =
        (S32) Sa_SampleFreq[i].W_SampleFreqValue - (S32) W_Ratio;
      U16 W_Diff = (U16) ((SDW_Diff > 0) ? SDW_Diff : -SDW_Diff);

      if (W_Diff < W_MinDiff)
        {
          B_MinIndex = i;
          W_MinDiff = W_Diff;
        }
    }

  if (B_MinIndex != 0xff)
    {
      return Sa_SampleFreq[B_MinIndex].SampleFreqId;
    }
  else
    {
      return HDRX_AUDIO_SF_NONE;
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_HW_audio_channel_count_get
 USAGE        :     Get the Audio channels count from the Hdmi Hardware.
 INPUT        :     None
 RETURN       :     No of channels present in the audio stream.
 USED_REGS    :     HDRX_SDP_AUD_STS
******************************************************************************/
U8 sthdmirx_audio_HW_audio_channel_count_get(const hdmirx_handle_t Handle)
{
  U16 ulAudStat;
  ulAudStat =
    HDMI_READ_REG_WORD(GET_CORE_BASE_ADDRS(Handle) + HDRX_SDP_AUD_STS);

  return (U8) ((ulAudStat & HDRX_SDP_AUD_CHNL_COUNT) >>
               HDRX_SDP_AUD_CHNL_COUNT_SHIFT);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_HW_audio_coding_type_get
 USAGE        :     Get the Audio coding type from the Hdmi Hardware.
 INPUT        :     None
 RETURN       :     Coding type available in the Hardware.
 USED_REGS    :     HDRX_SDP_AUD_STS
******************************************************************************/
U8 sthdmirx_audio_HW_audio_coding_type_get(const hdmirx_handle_t Handle)
{
  U16 ulAudStat;
  ulAudStat =
    HDMI_READ_REG_WORD(GET_CORE_BASE_ADDRS(Handle) + HDRX_SDP_AUD_STS);

  return (U8) ((ulAudStat & HDRX_SDP_AUD_CODING_TYPE) >>
               HDRX_SDP_AUD_CODING_TYPE_SHIFT);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_ISR_SDP_audio_client
 USAGE        :     SDP Audio Interrupt client, which checks the all corresponds interrupt status
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     HDRX_SDP_AUDIO_IRQ_STATUS
******************************************************************************/
void sthdmirx_CORE_ISR_SDP_audio_client(hdmirx_route_handle_t *const pInpHdl)
{
  volatile U16 ulAudIrqStatus;

  ulAudIrqStatus = HDMI_READ_REG_WORD((U32)(pInpHdl->BaseAddress +
                                      HDRX_SDP_AUDIO_IRQ_STATUS));

  if (ulAudIrqStatus & HDRX_SDP_AUD_LAYOUT_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio Layout status changed\n"); */
    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_STATUS_IRQ_STS)
    {
      sthdmirx_handle_audio_channel_status_data(pInpHdl);
    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_PKT_PRSNT_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio packet present\n"); */
    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_MUTE_FLAG_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio mute flag changed\n"); */

    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_BUF_OVR_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio buffer over flow\n"); */
    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_BUFP_DIFA_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio buffer pointer dif A\n"); */
    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_BUFP_DIFB_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio buffer pointer dif B\n"); */
    }

  if (ulAudIrqStatus & HDRX_SDP_AUD_MUTE_IRQ_STS)
    {
      /*DBG_HDRX_AUDIO_INFO(" Audio mute changed\n"); */
    }

  HDMI_WRITE_REG_WORD((U32)
                      (pInpHdl->BaseAddress + HDRX_SDP_AUDIO_IRQ_STATUS),
                      ulAudIrqStatus);

}

/******************************************************************************
FUNCTION	:  	 sthdmirx_audio_channel_status_formatted_get

USAGE		:    Read the Channel Status & fill the formatted channel status to strcut

INPUT		:    Sp_Acs - pointer to buffer

OUTPUT		:    Size of ACS structure in bytes or zero if audio channel status
                        contains unsupported format

USED_REGS	:  	 HDRX_SDP_AUD_CH_STS_0 .. HDRX_SDP_AUD_CH_STS_2
******************************************************************************/
U8 sthdmirx_audio_channel_status_formatted_get(const hdmirx_handle_t Handle,
    hdrx_audio_channel_status_t *Sp_Acs)
{
  U32 ulAudChStatus[3];

  ulAudChStatus[0] = HDMI_READ_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                         HDRX_SDP_AUD_CH_STS_0));
  ulAudChStatus[1] = HDMI_READ_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                         HDRX_SDP_AUD_CH_STS_1));
  ulAudChStatus[2] = (U32)HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) +
                     HDRX_SDP_AUD_CH_STS_2));

  DBG_HDRX_AUDIO_INFO("ACS Byte[0-3] : 0x%x", ulAudChStatus[0]);
  DBG_HDRX_AUDIO_INFO("Byte[4-7] : 0x%x  ", ulAudChStatus[1]);
  DBG_HDRX_AUDIO_INFO("Byte[8] : 0x%x \n", (ulAudChStatus[2] & 0xff));

  Sp_Acs->Standard = (U16) (ulAudChStatus[0] & 0x03);
  Sp_Acs->Copyright = (U16) ((ulAudChStatus[0] >> 2) & 0x01);
  Sp_Acs->AdditionalInfo = (U16) ((ulAudChStatus[0] >> 3) & 0x07);
  Sp_Acs->Mode = (U16) ((ulAudChStatus[0] >> 6) & 0x03);
  Sp_Acs->CategoryCode = (U16) ((ulAudChStatus[0] >> 8) & 0xFF);
  Sp_Acs->SourceNumber = (U16) ((ulAudChStatus[0] >> 16) & 0x0F);
  Sp_Acs->ChannelNumber = (U16) ((ulAudChStatus[0] >> 20) & 0x0F);
  Sp_Acs->SamplingFrequency = (U16) ((ulAudChStatus[0] >> 24) & 0x0F);
  Sp_Acs->ClockAccuracy = (U16) ((ulAudChStatus[0] >> 28) & 0x03);
  Sp_Acs->Reserved0 = (U16) ((ulAudChStatus[0] >> 30) & 0x03);
  Sp_Acs->WordLength = (U16) (ulAudChStatus[1] & 0x0F);

  return sizeof(hdrx_audio_channel_status_t);
}

/******************************************************************************
FUNCTION	:  	 sthdmirx_audio_channel_status_get

USAGE		:    Read the Channel Status & fill the buffer

INPUT		:    pBuffer - pointer to buffer

OUTPUT		:    Data is buffered

USED_REGS	:  	 HDRX_SDP_AUD_CH_STS_0 .. HDRX_SDP_AUD_CH_STS_2
******************************************************************************/
BOOL sthdmirx_audio_channel_status_get(const hdmirx_handle_t Handle,
                                       U8 *pBuffer)
{
  U8 i;

  for (i = 0; i < 9; i++)
    {
      *pBuffer = (U8)HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                              HDRX_SDP_AUD_CH_STS_0 + i));
      pBuffer++;
    }

  return TRUE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_handle_audio_channel_status_data
 USAGE        :     Get the Audio channel status data & compares with the local stored copy for event notification
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_handle_audio_channel_status_data(
  hdmirx_route_handle_t *const pInpHandle)
{
  hdrx_audio_channel_status_t pstTempInfo;

  if (sthdmirx_audio_channel_status_formatted_get
      (pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsAcsAvbl = 1;

  if (HDMI_MEM_CMP
      (&pInpHandle->stInfoPacketData.stAudioChannelStatus, &pstTempInfo,
       sizeof(hdrx_audio_channel_status_t)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stAudioChannelStatus,
                   &pstTempInfo, sizeof(hdrx_audio_channel_status_t));
      pInpHandle->stNewDataFlags.bIsNewAcs = TRUE;
      DBG_HDRX_AUDIO_INFO("Audio Channel Status is changed!!\n");
    }

  return TRUE;

}

/******************************************************************************
 FUNCTION     :     sthdmirx_audio_type_selectHW
 USAGE        :     Get the Audio channel status data & compares with the local stored copy for event notification
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_audio_type_selectHW(hdmirx_handle_t Handle,
                                  stm_hdmirx_audio_stream_type_t estAudioStream)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  switch (estAudioStream)
    {
    case STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN:
    case STM_HDMIRX_AUDIO_STREAM_TYPE_IEC:
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                       (InpHandle_p->BaseAddress +
                                        HDRX_SDP_AUD_CTRL),
                                       HDRX_AU_TP_SEL,
                                       (0x00 <<
                                        HDRX_AU_TP_SEL_SHIFT));
      break;

    case STM_HDMIRX_AUDIO_STREAM_TYPE_DSD:
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                       (InpHandle_p->BaseAddress +
                                        HDRX_SDP_AUD_CTRL),
                                       HDRX_AU_TP_SEL,
                                       (0x01 <<
                                        HDRX_AU_TP_SEL_SHIFT));
      break;

    case STM_HDMIRX_AUDIO_STREAM_TYPE_DST:
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                       (InpHandle_p->BaseAddress +
                                        HDRX_SDP_AUD_CTRL),
                                       HDRX_AU_TP_SEL,
                                       (0x02 <<
                                        HDRX_AU_TP_SEL_SHIFT));
      break;

    case STM_HDMIRX_AUDIO_STREAM_TYPE_HBR:
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)
                                       (InpHandle_p->BaseAddress +
                                        HDRX_SDP_AUD_CTRL),
                                       HDRX_AU_TP_SEL,
                                       (0x03 <<
                                        HDRX_AU_TP_SEL_SHIFT));
      break;

    default:
      break;
    }

}
