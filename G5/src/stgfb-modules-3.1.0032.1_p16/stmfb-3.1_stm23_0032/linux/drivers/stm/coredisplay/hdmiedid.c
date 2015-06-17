/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/hdmiedid.c
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

/*
 * Standard EDID defines
 */
#define STM_EDID_MANUFACTURER        0x08
#define STM_EDID_PROD_ID             0x0A
#define STM_EDID_SERIAL_NR           0x0C
#define STM_EDID_PRODUCTION_WK       0x10
#define STM_EDID_PRODUCTION_YR       0x11
#define STM_EDID_VERSION             0x12
#define STM_EDID_REVISION            0x13
#define STM_EDID_MAX_HSIZE           0x15
#define STM_EDID_MAX_VSIZE           0x16
#define STM_EDID_FEATURES            0x18
#define STM_EDID_ESTABLISHED_TIMING1 0x23
#define STM_EDID_ESTABLISHED_TIMING2 0x24
#define STM_EDID_MANUFACTURER_TIMING 0x25
#define STM_EDID_STD_TIMING_START    0x26
#define STM_EDID_DEFAULT_TIMING      0x36
#define STM_EDID_INFO_1              0x48
#define STM_EDID_INFO_2              0x5A
#define STM_EDID_INFO_3              0x6C

#define STM_EDID_MD_RANGES           0xFD
#define STM_EDID_MD_TYPE_NAME        0xFC

#define STM_EDID_EXTENSION           0x7E

#define STM_EDID_VGA_TIMING_CODE     0x20

#define STM_EDID_FEATURES_GTF        (1<<0)
#define STM_EDID_FEATURES_PT         (1<<1)
#define STM_EDID_FEATURES_SRGB       (1<<1)

#define STM_EDID_DTD_SIZE            18

/*
 * CEA Extension defines
 */
#define STM_EXT_TYPE              0
#define STM_EXT_TYPE_CEA          0x2
#define STM_EXT_TYPE_EDID_20      0x20
#define STM_EXT_TYPE_COLOR_INFO_0 0x30
#define STM_EXT_TYPE_DI           0x40
#define STM_EXT_TYPE_LS           0x50
#define STM_EXT_TYPE_MI           0x60
#define STM_EXT_TYPE_BLOCK_MAP    0xF0
#define STM_EXT_TYPE_MANUFACTURER 0xFF
#define STM_EXT_VERSION           1
#define STM_EXT_TIMING_OFFSET     2
#define STM_EXT_FORMATS           3
#define STM_EXT_DATA_BLOCK        4

#define STM_EXT_FORMATS_YCBCR_422 (1<<4)
#define STM_EXT_FORMATS_YCBCR_444 (1<<5)
#define STM_EXT_FORMATS_AUDIO     (1<<6)
#define STM_EXT_FORMATS_UNDERSCAN (1<<7)

#define STM_CEA_BLOCK_TAG_SHIFT   5
#define STM_CEA_BLOCK_TAG_MASK    (0x7<<STM_CEA_BLOCK_TAG_SHIFT)
#define STM_CEA_BLOCK_LEN_MASK    0x1F

#define STM_CEA_BLOCK_TAG_AUDIO   1
#define STM_CEA_BLOCK_TAG_VIDEO   2
#define STM_CEA_BLOCK_TAG_VENDOR  3
#define STM_CEA_BLOCK_TAG_SPEAKER 4
#define STM_CEA_BLOCK_TAG_EXT     7

#define STM_CEA_BLOCK_AUDIO_FORMAT_MASK   0x78
#define STM_CEA_BLOCK_AUDIO_FORMAT_SHIFT  3
#define STM_CEA_BLOCK_AUDIO_CHANNELS_MASK 0x7

#define STM_CEA_BLOCK_EXT_TAG_VCDB        0x0
#define STM_CEA_BLOCK_EXT_TAG_COLORIMETRY 0x5

/******************************************************************************
 * A quick and dirty determination of is the TV aspect ratio
 * is 4:3 or 16:9, using integer maths.
 */
static stm_wss_t stmhdmi_find_aspect(unsigned int x,unsigned int y)
{
unsigned int ratio;

  if (0 == x || 0 == y)
    return STM_WSS_OFF;

  ratio = (x*1000)/y;
  if(ratio<1400)
    return STM_WSS_4_3;
  else
    return STM_WSS_16_9;
}

/******************************************************************************
 * Helper to add display modes, removing duplicates
 */
static int stmhdmi_add_display_mode(struct stm_hdmi *hdmi, const stm_mode_line_t *display_mode)
{
  int n;

  if(display_mode == 0)
  {
    DPRINTK("Null display mode\n");
    return -EINVAL;
  }

  for(n=0;n<hdmi->edid_info.num_modes;n++)
  {
    if(hdmi->edid_info.display_modes[n] == display_mode)
    {
      DPRINTK("Duplicate display mode %lux%lu@%lu\n",display_mode->ModeParams.ActiveAreaWidth,display_mode->ModeParams.ActiveAreaHeight,display_mode->ModeParams.FrameRate);
      return 0;
    }
  }

  if(hdmi->edid_info.num_modes == STM_MAX_CEA_MODES)
  {
    DPRINTK("Device exported too many display modes\n");
    return -EINVAL;
  }

  DPRINTK("Adding display mode %lux%lu@%lu\n",display_mode->ModeParams.ActiveAreaWidth,display_mode->ModeParams.ActiveAreaHeight,display_mode->ModeParams.FrameRate);
  hdmi->edid_info.display_modes[hdmi->edid_info.num_modes] = display_mode;
  hdmi->edid_info.num_modes++;

  return 0;
}


/******************************************************************************
 * EDID and CEA-861 descriptor interpretation
 */
static void stmhdmi_interpret_timing_block(unsigned char *data,struct stm_hdmi *hdmi)
{
  const stm_mode_line_t *display_mode;
  unsigned long hsync_off,hsync_width;
  unsigned long vsync_off,vsync_width;
  unsigned long ulpixclock;
  unsigned long ulxres;
  unsigned long ultotalpixels;
  unsigned long ulyres;
  unsigned long ultotallines;
  stm_scan_type_t scanType;

  if((data[0] == 0) && (data[1] == 0))
    return;

  ulpixclock    = ((unsigned int) data[0] | ((unsigned int)(data[1] << 8))) * 10000;
  ulxres        = ((unsigned int) data[2] | ((unsigned int)(data[4]  & 0xF0) << 4));
  ultotalpixels = ulxres + ((unsigned int) data[3] | ((unsigned int)(data[4]  & 0x0F) << 8));
  ulyres        = ((unsigned int) data[5] | ((unsigned int)(data[7]  & 0xF0) << 4));
  ultotallines  = ulyres + ((unsigned int) data[6] | ((unsigned int)(data[7]  & 0x0F) << 8));
  scanType      = (data[17] >> 7)?SCAN_I:SCAN_P;

  if(scanType == SCAN_I)
  {
    /*
     * EDID information for interlaced modes has the number of vertical lines
     * per field, not per frame, so multiply the values up by 2 before querying
     * the display mode.
     */
    ulyres       *= 2;
    ultotallines  = (ultotallines*2) + 1;
  }

  hsync_off   = data[8] + ((data[11] & 0xC0) << 2);
  hsync_width = data[9] + ((data[11] & 0x30) << 4);
  vsync_off   = (data[10] >> 4)   + ((data[11] & 0x0C) << 2);
  vsync_width = (data[10] & 0x0F) + ((data[11] & 0x03) << 4);

  display_mode = stm_display_output_find_display_mode(hdmi->main_output,
                                                      ulxres,
                                                      ulyres,
                                                      ultotallines,
                                                      ultotalpixels,
                                                      ulpixclock,
                                                      scanType);

  if(display_mode != 0)
  {
    stmhdmi_add_display_mode(hdmi, display_mode);
  }
  else
  {
    DPRINTK("Mode not supported:\n");
    DPRINTK_NOPREFIX("Display Mode: %lux%lu%s (%luHz) total (%lux%lu)\n", ulxres,
                                                                          ulyres,
                                                                          (scanType == SCAN_I)?"i":"p",
                                                                          ulpixclock/(ultotalpixels*ultotallines),
                                                                          ultotalpixels,
                                                                          ultotallines);
  }

}


static void stmhdmi_interpret_data_block(unsigned char *data,struct stm_hdmi *hdmi)
{

  if(data[0] == 0 && data[1] == 0 && data[2] == 0 && data[4] == 0)
  {
    /*
     * This is a monitor descriptor
     */
    switch(data[3])
    {
      case STM_EDID_MD_RANGES:
      {
        DPRINTK("Found Monitor Descriptor Ranges\n");
        hdmi->edid_info.min_vfreq = data[5];
        hdmi->edid_info.max_vfreq = data[6];
        hdmi->edid_info.min_hfreq = data[7];
        hdmi->edid_info.max_hfreq = data[8];
        hdmi->edid_info.max_clock = data[9]*10;

        break;
      }
      case STM_EDID_MD_TYPE_NAME:
      {
        int n;
        DPRINTK("Found Monitor Descriptor Name\n");
        for(n=0;n<13;n++)
        {
          if((hdmi->edid_info.monitor_name[n] = data[n+5]) == 0x0A)
            break;
        }

        hdmi->edid_info.monitor_name[n] = '\0';

        break;
      }
    }
  }
  else
  {
    /* This is a timing mode descriptor. */
    stmhdmi_interpret_timing_block(data,hdmi);
  }
}


static inline int stmhdmi_edid_checksum(edid_block_t data)
{
  register int n;
  register unsigned char sum = 0;

  for(n=0;n<128;n++)
  {
    sum += data[n];
  }

  if(sum != 0)
    return -EINVAL;

  return 0;
}


static int stmhdmi_read_edid_block0(struct stm_hdmi *hdmi, edid_block_t rawedid)
{
  unsigned long header1;
  unsigned long header2;
  int           manufacturer_id;
  int           product_id;
  int           serial_nr;

  header1 = rawedid[0] | (rawedid[1]<<8) | (rawedid[2]<<16) | (rawedid[3]<<24);
  header2 = rawedid[4] | (rawedid[5]<<8) | (rawedid[6]<<16) | (rawedid[7]<<24);

  if((header1 != 0xffffff00) && (header2 != 0x00ffffff))
  {
    DPRINTK("Invalid EDID header 0x%08lx 0x%08lx\n",header1,header2);
    stmhdmi_invalidate_edid_info(hdmi);
    return -EINVAL;
  }

  manufacturer_id = rawedid[STM_EDID_MANUFACTURER] |
                   (rawedid[STM_EDID_MANUFACTURER+1] << 8);

  product_id      = rawedid[STM_EDID_PROD_ID]      |
                   (rawedid[STM_EDID_PROD_ID+1] << 8);

  serial_nr       = (rawedid[STM_EDID_SERIAL_NR]          |
                    (rawedid[STM_EDID_SERIAL_NR+1] << 8)  |
                    (rawedid[STM_EDID_SERIAL_NR+2] << 16) |
                    (rawedid[STM_EDID_SERIAL_NR+3] << 24));

  hdmi->edid_info.edid_version    = rawedid[STM_EDID_VERSION];
  hdmi->edid_info.edid_revision   = rawedid[STM_EDID_REVISION];

  if((hdmi->edid_info.edid_version != 1) || (hdmi->edid_info.edid_revision < 3))
  {
    DPRINTK("EDID v%d.%d not recognised\n",hdmi->edid_info.edid_version,hdmi->edid_info.edid_revision);
    stmhdmi_invalidate_edid_info(hdmi);
    return -ENODEV;
  }

  hdmi->edid_info.manufacturer_id = manufacturer_id;
  hdmi->edid_info.product_id      = product_id;
  hdmi->edid_info.serial_nr       = serial_nr;
  hdmi->edid_info.production_week = rawedid[STM_EDID_PRODUCTION_WK];
  hdmi->edid_info.production_year = rawedid[STM_EDID_PRODUCTION_YR]+1990;
  hdmi->edid_info.max_hsize       = rawedid[STM_EDID_MAX_HSIZE];
  hdmi->edid_info.max_vsize       = rawedid[STM_EDID_MAX_VSIZE];

  hdmi->edid_info.tv_aspect       = stmhdmi_find_aspect(hdmi->edid_info.max_hsize,
                                                     hdmi->edid_info.max_vsize);

  hdmi->edid_info.cea_capabilities |= (rawedid[STM_EDID_FEATURES] & STM_EDID_FEATURES_GTF)?STM_CEA_CAPS_GTF:0;
  hdmi->edid_info.cea_capabilities |= (rawedid[STM_EDID_FEATURES] & STM_EDID_FEATURES_SRGB)?STM_CEA_CAPS_SRGB:0;

  stmhdmi_interpret_timing_block(&rawedid[STM_EDID_DEFAULT_TIMING],hdmi);
  stmhdmi_interpret_data_block(&rawedid[STM_EDID_INFO_1],hdmi);
  stmhdmi_interpret_data_block(&rawedid[STM_EDID_INFO_2],hdmi);
  stmhdmi_interpret_data_block(&rawedid[STM_EDID_INFO_3],hdmi);

  /*
   * Add in VGA, the CEA-861 spec states this should be specified in
   * the established timings. However we have evidence of devices that do
   * not do this or specify VGA in the detailed timings either.
   */
  if((rawedid[STM_EDID_ESTABLISHED_TIMING1] == STM_EDID_VGA_TIMING_CODE) ||
     (rawedid[STM_EDID_ESTABLISHED_TIMING2] == STM_EDID_VGA_TIMING_CODE) ||
     (rawedid[STM_EDID_MANUFACTURER_TIMING] == STM_EDID_VGA_TIMING_CODE))
  {
    DPRINTK("Found VGA mode in established timings\n");
  }
  else
  {
    DPRINTK("Warning: VGA mode missing in established timings\n");
  }

  stmhdmi_add_display_mode(hdmi, stm_display_output_get_display_mode(hdmi->main_output, STVTG_TIMING_MODE_480P59940_25180));

  return 0;
}


static int stmhdmi_cea_process_video_short_codes(struct stm_hdmi *hdmi, unsigned long cea_mode)
{
  stm_display_mode_t mode;
  const stm_mode_line_t *display_mode;
  int   ret = -EINVAL;
  for(mode = STVTG_TIMING_MODE_480I60000_13514;mode<STVTG_TIMING_MODE_COUNT;mode++)
  {
    if((display_mode = stm_display_output_get_display_mode(hdmi->main_output, mode)) != 0)
    {
      if(display_mode->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] == cea_mode ||
         display_mode->ModeParams.HDMIVideoCodes[AR_INDEX_16_9] == cea_mode)
      {
        stmhdmi_add_display_mode(hdmi, display_mode);
        ret = 0;
      }
    }
  }

  return ret;
}


static int stmhdmi_cea_process_blocks(struct stm_hdmi *hdmi, edid_block_t rawedid)
{
  int block_end = rawedid[STM_EXT_TIMING_OFFSET];
  int block_pos = 4;
  int offset;

  while(block_pos < block_end)
  {
    unsigned char tag    = (rawedid[block_pos]&STM_CEA_BLOCK_TAG_MASK)>>STM_CEA_BLOCK_TAG_SHIFT;
    unsigned char len    = (rawedid[block_pos]&STM_CEA_BLOCK_LEN_MASK);
    unsigned char exttag = 0;

    if(tag == STM_CEA_BLOCK_TAG_EXT)
    {
      exttag = rawedid[block_pos+1];
      block_pos +=2;
      len--;
    }
    else
    {
      block_pos +=1;
    }

    switch(tag)
    {
      case STM_CEA_BLOCK_TAG_VIDEO:
        DPRINTK_NOPREFIX("Found CEA Block Video Tag:\n");
        for(offset=0;offset<len;offset++)
        {
          unsigned cea_mode = (unsigned)(rawedid[block_pos+offset]&0x7f);
          int      native   = ((rawedid[block_pos+offset]&0x80) != 0);
          if(!stmhdmi_cea_process_video_short_codes(hdmi, cea_mode))
          {
            DPRINTK_NOPREFIX("Found CEA Video Mode:%u (%s)\n",cea_mode,native?"native":"non-native");
            hdmi->edid_info.cea_codes[cea_mode] = native?STM_CEA_VIDEO_CODE_NATIVE:STM_CEA_VIDEO_CODE_NON_NATIVE;
          }
          else
          {
            DPRINTK_NOPREFIX("Unsupported CEA Video Mode:%u\n",cea_mode);
          }
        }
        break;

      case STM_CEA_BLOCK_TAG_AUDIO:
        DPRINTK_NOPREFIX("Found CEA Block Audio Tag:\n");
        for(offset=0;offset<len;offset+=3)
        {
          unsigned formatcode  = (unsigned)(rawedid[block_pos+offset] & STM_CEA_BLOCK_AUDIO_FORMAT_MASK) >> STM_CEA_BLOCK_AUDIO_FORMAT_SHIFT;
          unsigned channels    = (unsigned)(rawedid[block_pos+offset] & STM_CEA_BLOCK_AUDIO_CHANNELS_MASK) + 1;
          unsigned frequencies = (unsigned)(rawedid[block_pos+offset+1]);
          unsigned other       = (unsigned)(rawedid[block_pos+offset+2]);
          DPRINTK_NOPREFIX("Found CEA Audio Mode:%u channels = %u frequencies = 0x%08x other = 0x%08x\n",formatcode,channels,frequencies,other);

          hdmi->edid_info.audio_modes[formatcode].format       = formatcode;
          hdmi->edid_info.audio_modes[formatcode].max_channels = channels;
          hdmi->edid_info.audio_modes[formatcode].sample_rates = frequencies;
          if(formatcode == STM_CEA_AUDIO_LPCM)
            hdmi->edid_info.audio_modes[formatcode].lpcm_bit_depths = other;
          else if(formatcode >= STM_CEA_AUDIO_AC3 && formatcode <= STM_CEA_AUDIO_ATRAC)
            hdmi->edid_info.audio_modes[formatcode].max_bit_rate = other*8;
          else if(formatcode == STM_CEA_AUDIO_WMA_PRO)
            hdmi->edid_info.audio_modes[formatcode].profile = other&0x7;
          else if(formatcode == STM_CEA_AUDIO_EXTENSION_FORMAT)
            hdmi->edid_info.audio_modes[formatcode].ex_format = (stm_cea_audio_ex_formats_t)(other>>3);
          else
            hdmi->edid_info.audio_modes[formatcode].format_data = other;
        }
        break;

      case STM_CEA_BLOCK_TAG_VENDOR:
        DPRINTK_NOPREFIX("Found CEA Block Vendor Tag:\n");
        if(len<5)
        {
          DPRINTK_NOPREFIX("CEA Block Vendor Tag: invalid length %d\n",len);
          break;
        }

        if((rawedid[block_pos] == 0x03) && (rawedid[block_pos+1] == 0x0c) && rawedid[block_pos+2] == 0x0)
        {
          DPRINTK_NOPREFIX("Found IEEE HDMI Identifier:\n");
          hdmi->edid_info.display_type = STM_DISPLAY_HDMI;
          hdmi->edid_info.cec_address[0] = rawedid[block_pos+3] >> 4;
          hdmi->edid_info.cec_address[1] = rawedid[block_pos+3] & 0xf;
          hdmi->edid_info.cec_address[2] = rawedid[block_pos+4] >> 4;
          hdmi->edid_info.cec_address[3] = rawedid[block_pos+4] & 0xf;
          if(len >= 6)
            hdmi->edid_info.hdmi_vsdb_flags = rawedid[block_pos+5];

          if(len >= 7)
            hdmi->edid_info.max_tmds_clock = rawedid[block_pos+6]*5000000;

          if(len >= 8)
          {
            if(rawedid[block_pos+7] & 0x80)
            {
              hdmi->edid_info.progressive_video_latency = rawedid[block_pos+8];
              hdmi->edid_info.progressive_audio_latency = rawedid[block_pos+9];
            }
            if(rawedid[block_pos+7] & 0x40)
            {
              hdmi->edid_info.interlaced_video_latency = rawedid[block_pos+10];
              hdmi->edid_info.interlaced_audio_latency = rawedid[block_pos+11];
            }
            else
            {
              hdmi->edid_info.interlaced_video_latency = hdmi->edid_info.progressive_video_latency;
              hdmi->edid_info.interlaced_audio_latency = hdmi->edid_info.progressive_audio_latency;
            }
          }
        }
        break;

      case STM_CEA_BLOCK_TAG_SPEAKER:
        DPRINTK_NOPREFIX("Found CEA Block Speaker Tag:\n");
        hdmi->edid_info.speaker_allocation = (unsigned int)rawedid[block_pos]|((unsigned int)rawedid[block_pos+1]<<8);
        DPRINTK_NOPREFIX("Speaker Allocation: 0x%08x\n",hdmi->edid_info.speaker_allocation);
        break;

      case STM_CEA_BLOCK_TAG_EXT:
        switch(exttag)
        {
          case STM_CEA_BLOCK_EXT_TAG_VCDB:
            DPRINTK_NOPREFIX("Found CEA Block Extended VCDB Tag flags = 0x%x\n",(unsigned)rawedid[block_pos]);
            hdmi->edid_info.cea_vcdb_flags = rawedid[block_pos];
            break;
          case STM_CEA_BLOCK_EXT_TAG_COLORIMETRY:
            DPRINTK_NOPREFIX("Found CEA Block Extended Colorimetry Tag\n");
            hdmi->edid_info.cea_coldb_flags = (unsigned)rawedid[block_pos]|((unsigned)rawedid[block_pos+1]<<8);
            break;
          default:
            DPRINTK_NOPREFIX("Found Unknown CEA Block Extended Tag: %d\n",(int)exttag);
            break;
        }
        break;

      default:
        break;
    }

    block_pos += (int)len;
  }

  return 0;
}


static int stmhdmi_read_cea_block(struct stm_hdmi *hdmi, edid_block_t rawedid)
{
  hdmi->edid_info.cea_version = rawedid[STM_EXT_VERSION];
  DPRINTK_NOPREFIX("CEA Extension Header v%d: this is a TV\n",hdmi->edid_info.cea_version);

  hdmi->edid_info.display_type = STM_DISPLAY_DVI;
  if(hdmi->edid_info.cea_version > 2)
    stmhdmi_cea_process_blocks(hdmi, rawedid);


  if(rawedid[STM_EXT_FORMATS] & STM_EXT_FORMATS_YCBCR_444)
    hdmi->edid_info.cea_capabilities |= STM_CEA_CAPS_YUV;

  if(rawedid[STM_EXT_FORMATS] & STM_EXT_FORMATS_YCBCR_422)
    hdmi->edid_info.cea_capabilities |= STM_CEA_CAPS_422;

  if(rawedid[STM_EXT_FORMATS] & STM_EXT_FORMATS_UNDERSCAN)
    hdmi->edid_info.cea_capabilities |= STM_CEA_CAPS_UNDERSCAN;;

  if(rawedid[STM_EXT_FORMATS] & STM_EXT_FORMATS_AUDIO)
    hdmi->edid_info.cea_capabilities |= STM_CEA_CAPS_BASIC_AUDIO;

  if(rawedid[STM_EXT_TIMING_OFFSET] != 0)
  {
    int timing_offset;

    /*
     * Detailed timing descriptors start at the value in STM_EXT_TIMING_OFFSET
     * and are then continuous to the end of the EDID block - the checksum at
     * byte 127 and at least one padding byte at offset 126.
     */
    for(timing_offset = rawedid[STM_EXT_TIMING_OFFSET];timing_offset<=(126-STM_EDID_DTD_SIZE); timing_offset += STM_EDID_DTD_SIZE)
    {
      DPRINTK("timing data offset = %d (0x%x)\n",timing_offset,timing_offset);
      stmhdmi_interpret_timing_block(&rawedid[timing_offset],hdmi);
    }
  }
  return 0;
}


static int stmhdmi_read_ext_edid_block(struct stm_hdmi *hdmi, edid_block_t rawedid)
{
  switch(rawedid[STM_EXT_TYPE])
  {
    case STM_EXT_TYPE_CEA:
      return stmhdmi_read_cea_block(hdmi, rawedid);

    case STM_EXT_TYPE_BLOCK_MAP:
    case STM_EXT_TYPE_MANUFACTURER:
    case STM_EXT_TYPE_EDID_20:
    case STM_EXT_TYPE_COLOR_INFO_0:
    case STM_EXT_TYPE_LS:
    case STM_EXT_TYPE_MI:
      /*
       * We don't care about these, an application can read the raw EDID
       * information from sysfs if it is interested.
       */
      break;

    case STM_EXT_TYPE_DI:
      DPRINTK_NOPREFIX("VESA DI Extension Header: this is a computer monitor\n");
      hdmi->edid_info.display_type = STM_DISPLAY_DVI;
      break;

    default:
      DPRINTK_NOPREFIX("Unknown Extension Header: 0x%x\n",(unsigned int)rawedid[STM_EXT_TYPE]);
      break;
  }

  return 0;
}


static int stmhdmi_read_extension_blocks(struct stm_hdmi *hdmi)
{
  int res,retry;
  struct i2c_msg msgs[3];
  int blockpairs = hdmi->edid_info.raw[0][STM_EDID_EXTENSION]/2;
  int blockmap = 1;
  unsigned char startreg = 0;
  unsigned char segment;

  if(hdmi->edid_info.raw[blockmap][STM_EXT_TYPE] != STM_EXT_TYPE_BLOCK_MAP)
  {
    DPRINTK("No Blockmap found in block1, cannot parse E-DDC addressed extensions\n");
    stmhdmi_invalidate_edid_info(hdmi);
    return -EINVAL;
  }

  msgs[0].addr  = hdmi->eddc_segment_reg_client->addr;
  msgs[0].flags = hdmi->eddc_segment_reg_client->flags & I2C_M_TEN;
  msgs[0].len   = 1;
  msgs[0].buf   = &segment;
  msgs[1].addr  = hdmi->edid_client->addr;
  msgs[1].flags = hdmi->edid_client->flags & I2C_M_TEN;
  msgs[1].len   = 1;
  msgs[1].buf   = &startreg;
  msgs[2].addr  = hdmi->edid_client->addr;
  msgs[2].flags = (hdmi->edid_client->flags & I2C_M_TEN) | I2C_M_RD;
  msgs[2].len   = sizeof(edid_block_t)*2;

  for(segment=1;segment<=blockpairs;segment++)
  {
    int blockmapaddress;
    int firstblocknr = segment*2;

    if(firstblocknr < 128)
      blockmapaddress = firstblocknr-1;
    else
      blockmapaddress = firstblocknr-128;


    msgs[2].buf = &(hdmi->edid_info.raw[firstblocknr][0]);

    for(retry=0;retry<5;retry++)
    {
      res = 0;

      /*
       * If we are retrying, wait and relax a while to give the TV a chance to
       * recover from any error.
       */
      if(retry>0)
      {
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(HZ/10);
      }

      if(hdmi->edid_info.raw[blockmap][blockmapaddress] == 0 &&
         hdmi->edid_info.raw[blockmap][blockmapaddress+1] == 0)
      {
        DPRINTK("Blockmap says segment %d is empty, the extension count was proably wrong\n",segment);
        /*
         * We have to be resilient against some EDIDs having the wrong
         * block count. This is not treated as an error, just stop reading.
         */
        break;
      }

      if(i2c_transfer(hdmi->edid_client->adapter, msgs, 3) != 3)
      {
        DPRINTK("I2C Transaction Error res=%d\n",res);
        res = -EIO;
        continue;
      }

      /*
       * Process the first of the block pair.
       */
      if((res=stmhdmi_edid_checksum(hdmi->edid_info.raw[firstblocknr]))<0)
      {
        /*
         * Retry read to see if we can get a good checksum
         */
        DPRINTK("Bad Checksum for extension block %d\n",firstblocknr);
        continue;
      }


      if(blockmapaddress == 0)
      {
        /*
         * This must be block 128 which should be another blockmap for
         * the next 127 blocks.
         */
        if(hdmi->edid_info.raw[firstblocknr][STM_EXT_TYPE] != STM_EXT_TYPE_BLOCK_MAP)
        {
          DPRINTK("Missing block map at block 128\n");
          res = -EINVAL;
          break;
        }

        /*
         * Change the blockmap block ready for the second block in this
         * pair.
         */
        blockmap = 128;
      }
      else
      {
        if(hdmi->edid_info.raw[blockmap][blockmapaddress] != hdmi->edid_info.raw[firstblocknr][STM_EXT_TYPE])
        {
          DPRINTK("Extension type doesn't match blockmap entry for block %d\n",firstblocknr);
          res = -EINVAL;
          break;
        }

        stmhdmi_read_ext_edid_block(hdmi,hdmi->edid_info.raw[firstblocknr]);
      }


      /*
       * Process the second of the block pair.
       */
      if((res=stmhdmi_edid_checksum(hdmi->edid_info.raw[firstblocknr+1]))<0)
      {
        /*
         * Retry read to see if we can get a good checksum
         */
        DPRINTK("Bad Checksum for extension block %d\n",firstblocknr+1);
        continue;
      }


      if(hdmi->edid_info.raw[blockmap][blockmapaddress+1] != hdmi->edid_info.raw[firstblocknr+1][STM_EXT_TYPE])
      {
        DPRINTK("Extension type doesn't match blockmap entry for block %d\n",firstblocknr+1);
        res = -EINVAL;
        break;
      }

      stmhdmi_read_ext_edid_block(hdmi,hdmi->edid_info.raw[firstblocknr+1]);

      /*
       * Success, break out of the retry loop.
       */
      break;
    }

    if(res <0)
    {
      DPRINTK("Extenstion blocks read error, invalidating EDID\n");
      stmhdmi_invalidate_edid_info(hdmi);
      return res;
    }

  }

  return 0;
}

/******************************************************************************
 * Public interfaces called by the HDMI management kernel thread.
 */

int stmhdmi_read_edid(struct stm_hdmi *hdmi)
{
  int res,retry;
  unsigned char startreg = 0;
  struct i2c_msg msgs[2];

  /* this invalidation is good for all error paths before the call to
   * stmhdmi_read_edid_block0()
   */
  stmhdmi_invalidate_edid_info(hdmi);

  if(!hdmi->edid_client)
  {
    DPRINTK("No EDID client found\n");
    return -ENODEV;
  }

  msgs[0].addr  = hdmi->edid_client->addr;
  msgs[0].flags = hdmi->edid_client->flags & I2C_M_TEN;
  msgs[0].len   = 1;
  msgs[0].buf   = &startreg;
  msgs[1].addr  = hdmi->edid_client->addr;
  msgs[1].flags = (hdmi->edid_client->flags & I2C_M_TEN) | I2C_M_RD;
  msgs[1].len   = sizeof(edid_block_t)*2;
  msgs[1].buf = &(hdmi->edid_info.raw[0][0]);

  for(retry=0;retry<5;retry++)
  {
    res = 0;

    /*
     * If we are retrying, wait and relax a while to give the TV a chance to
     * recover from any error.
     */
    if(retry>0)
    {
      set_current_state(TASK_UNINTERRUPTIBLE);
      schedule_timeout(HZ/10);
    }

    if(i2c_transfer(hdmi->edid_client->adapter, &msgs[0], 2) != 2)
    {
      res = -EIO;
      continue;
    }

    if(hdmi->edid_info.raw[0][0] != 0)
    {
      printk(KERN_WARNING "first EDID byte (%d) is corrupt, attempting to fix..\n",hdmi->edid_info.raw[0][0]);
      hdmi->edid_info.raw[0][0] = 0;
    }

    if(stmhdmi_edid_checksum(hdmi->edid_info.raw[0]) < 0)
    {
      DPRINTK("Invalid extension header checksum block0\n");
#if defined(DEBUG)
      print_hex_dump_bytes("",DUMP_PREFIX_OFFSET,&(hdmi->edid_info.raw[0][0]),128);
#endif
      res = -EINVAL;
      continue;
    }

    if((hdmi->edid_info.raw[0][STM_EDID_EXTENSION] > 0) &&
       (stmhdmi_edid_checksum(hdmi->edid_info.raw[1]) < 0))
    {
      DPRINTK("Invalid extension header checksum block1\n");
#if defined(DEBUG)
      print_hex_dump_bytes("",DUMP_PREFIX_OFFSET,&(hdmi->edid_info.raw[1][0]),128);
#endif
      res = -EINVAL;
      continue;
    }

    /*
     * Success, break out of the retry loop.
     */
    break;
  }

  if(res<0)
    return res;

  if((res = stmhdmi_read_edid_block0(hdmi,hdmi->edid_info.raw[0])) < 0)
  {
#if defined(DEBUG)
    print_hex_dump_bytes("",DUMP_PREFIX_OFFSET,&(hdmi->edid_info.raw[0][0]),128);
#endif
    return res;
  }

  if(hdmi->edid_info.raw[0][STM_EDID_EXTENSION] == 0)
  {
    DPRINTK_NOPREFIX("No extension header:\n");
    hdmi->edid_info.display_type = STM_DISPLAY_DVI;
    return 0;
  }

  if((res = stmhdmi_read_ext_edid_block(hdmi,hdmi->edid_info.raw[1])) < 0)
  {
    stmhdmi_invalidate_edid_info(hdmi);
    return res;
  }

  if((hdmi->edid_info.raw[0][STM_EDID_EXTENSION] > 1) && (hdmi->eddc_segment_reg_client != 0))
    return stmhdmi_read_extension_blocks(hdmi);

  return 0;
}


int stmhdmi_safe_edid(struct stm_hdmi *hdmi)
{
  DPRINTK("Setting Safe EDID\n");
  hdmi->edid_info.display_type     = hdmi->hdmi_safe_mode?STM_DISPLAY_HDMI:STM_DISPLAY_DVI;
  strcpy(hdmi->edid_info.monitor_name,"SAFEMODE");
  hdmi->edid_info.tv_aspect        = STM_WSS_4_3;
  hdmi->edid_info.cea_capabilities = STM_CEA_CAPS_SRGB;
  hdmi->edid_info.max_clock        = 26;
  hdmi->edid_info.max_tmds_clock   = 25200000;
  stmhdmi_cea_process_video_short_codes(hdmi, 1);
  hdmi->edid_info.cea_codes[1]     = STM_CEA_VIDEO_CODE_NON_NATIVE;

  return 0;
}


void stmhdmi_invalidate_edid_info(struct stm_hdmi *hdmi)
{
  memset(&hdmi->edid_info,0,sizeof(struct stm_edid));
  hdmi->edid_info.display_type = STM_DISPLAY_INVALID;
}


/******************************************************************************
 * Debugging
 */
void stmhdmi_dump_edid(struct stm_hdmi *hdmi)
{
  int n;

  if(hdmi->edid_info.display_type == STM_DISPLAY_INVALID)
  {
    DPRINTK_NOPREFIX("Invalid Monitor Description\n");
    return;
  }

  if(hdmi->edid_info.monitor_name[0] != 0)
    DPRINTK_NOPREFIX("Monitor Name    : %s\n",hdmi->edid_info.monitor_name);
  else
    DPRINTK_NOPREFIX("Monitor Name    : Unknown\n");

  DPRINTK_NOPREFIX("Monitor Type    : %s\n",(hdmi->edid_info.display_type==STM_DISPLAY_DVI)?"DVI":"HDMI");

  DPRINTK_NOPREFIX("EDID v%d.%d       : manufacturer 0x%08x, product id 0x%08x \n",hdmi->edid_info.edid_version,
                                                                                   hdmi->edid_info.edid_revision,
                                                                                   hdmi->edid_info.manufacturer_id,
                                                                                   hdmi->edid_info.product_id);

  DPRINTK_NOPREFIX("                : serial no 0x%08x, manufactured wk %d %d\n",hdmi->edid_info.serial_nr,
                                                                                 hdmi->edid_info.production_week,
                                                                                 hdmi->edid_info.production_year);

  if(hdmi->edid_info.tv_aspect != STM_WSS_OFF)
  {
    DPRINTK_NOPREFIX("                : max size image %ux%ucm (%s)\n",hdmi->edid_info.max_hsize,
                                                                       hdmi->edid_info.max_vsize,
                                                                       (hdmi->edid_info.tv_aspect == STM_WSS_4_3)?"4:3":"16:9");
  }

  DPRINTK_NOPREFIX("Frequency Ranges: Vmin = %uHz Vmax = %uHz\n",  hdmi->edid_info.min_vfreq, hdmi->edid_info.max_vfreq);
  DPRINTK_NOPREFIX("                  Hmin = %uKHz Hmax = %uKHz\n",hdmi->edid_info.min_hfreq, hdmi->edid_info.max_hfreq);
  DPRINTK_NOPREFIX("                  Max PixClock = %uMHz\n",     hdmi->edid_info.max_clock);

  DPRINTK_NOPREFIX("GTF Support     : %s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_GTF)?"Yes":"No");
  DPRINTK_NOPREFIX("sRGB Support    : %s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_SRGB)?"Yes":"No");
  DPRINTK_NOPREFIX("YUV Support     : %s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_YUV)?"Yes":"No");
  DPRINTK_NOPREFIX("422 Support     : %s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_422)?"Yes":"No");
  DPRINTK_NOPREFIX("Audio Support   : %s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_BASIC_AUDIO)?"Yes":"No");
  DPRINTK_NOPREFIX("Underscan       : %s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_UNDERSCAN)?"Yes":"No");

  DPRINTK_NOPREFIX("Video Codes     : ");

  for(n=0;n<STM_MAX_CEA_MODES;n++)
  {
    switch(hdmi->edid_info.cea_codes[n])
    {
      case STM_CEA_VIDEO_CODE_UNSUPPORTED:
      default:
        break;
      case STM_CEA_VIDEO_CODE_NATIVE:
        DPRINTK_NOPREFIX("%d*,", n);
        break;
      case STM_CEA_VIDEO_CODE_NON_NATIVE:
        DPRINTK_NOPREFIX("%d,", n);
        break;
    }
  }

  DPRINTK_NOPREFIX("\n");
}
