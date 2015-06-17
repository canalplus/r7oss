/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmi/hdmiedid.c
 * Copyright (c) 2008-2012 STMicroelectronics Limited.
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
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>
#include "stmhdmi.h"

struct stm_hdmi_extended_mode
{
  unsigned int ext_vic;
  stm_display_mode_id_t mode_id;
};

#define STM_HDMI_MAX_EXTENDED_MODES 6

static const struct stm_hdmi_extended_mode edid_4k_modes[] = {
  {1, STM_TIMING_MODE_4K2K29970_296703},      /*!< HDMI extended resolution VIC 1 */
  {1, STM_TIMING_MODE_4K2K30000_297000},      /*!< HDMI extended resolution VIC 1 */
  {2, STM_TIMING_MODE_4K2K25000_297000},      /*!< HDMI extended resolution VIC 2 */
  {3, STM_TIMING_MODE_4K2K23980_296703},      /*!< HDMI extended resolution VIC 3 */
  {3, STM_TIMING_MODE_4K2K24000_297000},      /*!< HDMI extended resolution VIC 3 */
  {4, STM_TIMING_MODE_4K2K24000_297000_WIDE}, /*!< HDMI extended resolution VIC 4 */
};

/******************************************************************************
 * A quick and dirty determination of is the TV aspect ratio
 * is 4:3 or 16:9, using integer maths.
 */
static stm_wss_aspect_ratio_t stmhdmi_find_aspect(unsigned int x,unsigned int y)
{
unsigned int ratio;

  if (0 == x || 0 == y)
    return STM_WSS_ASPECT_RATIO_UNKNOWN;

  ratio = (x*1000)/y;
  if(ratio<1400)
    return STM_WSS_ASPECT_RATIO_4_3;
  else if (ratio<1800)
    return STM_WSS_ASPECT_RATIO_16_9;
  else
    return STM_WSS_ASPECT_RATIO_GT_16_9;
}

/******************************************************************************
 * Helper to add display modes, removing duplicates
 */
static int stmhdmi_add_display_mode(struct stm_hdmi *hdmi, const stm_display_mode_t *display_mode)
{
  int n;

  for(n=0;n<hdmi->edid_info.num_modes;n++)
  {
    if(hdmi->edid_info.display_modes[n].mode_id == display_mode->mode_id)
    {
      DPRINTK("Duplicate display mode %ux%u@%u\n",display_mode->mode_params.active_area_width,display_mode->mode_params.active_area_height,display_mode->mode_params.vertical_refresh_rate);
      return 0;
    }
  }

  if(hdmi->edid_info.num_modes == STM_MAX_CEA_MODES)
  {
    DPRINTK("Device exported too many display modes\n");
    return -EINVAL;
  }

  DPRINTK("Adding display mode %ux%u@%u\n",display_mode->mode_params.active_area_width,display_mode->mode_params.active_area_height,display_mode->mode_params.vertical_refresh_rate);
  hdmi->edid_info.display_modes[hdmi->edid_info.num_modes] = *display_mode;
  hdmi->edid_info.num_modes++;

  return 0;
}


/******************************************************************************
 * EDID and CEA-861 descriptor interpretation
 */
static void stmhdmi_interpret_timing_block(unsigned char *data,struct stm_hdmi *hdmi)
{
  stm_display_mode_t display_mode;
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
  scanType      = (data[17] >> 7)?STM_INTERLACED_SCAN:STM_PROGRESSIVE_SCAN;

  if(scanType == STM_INTERLACED_SCAN)
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

  if(!stm_display_output_find_display_mode(hdmi->main_output,
                                           ulxres,
                                           ulyres,
                                           ultotallines,
                                           ultotalpixels,
                                           ulpixclock,
                                           scanType,
                                           &display_mode))
  {
    stmhdmi_add_display_mode(hdmi, &display_mode);
  }
  else
  {
    DPRINTK("Mode not supported:\n");
    DPRINTK_NOPREFIX("Display Mode: %lux%lu%s (%luHz) total (%lux%lu)\n", ulxres,
                                                                          ulyres,
                                                                          (scanType == STM_INTERLACED_SCAN)?"i":"p",
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
  hdmi->edid_info.cea_capabilities |= (rawedid[STM_EDID_FEATURES] & STM_EDID_FEATURES_RGB)?STM_CEA_CAPS_RGB:0;

  /*
   * At this point we assume the display is DVI, until we find a HDMI vendor tag
   */
  hdmi->edid_info.display_type = STM_DISPLAY_DVI;

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

  {
    stm_display_mode_t vga;
    if(!stm_display_output_get_display_mode(hdmi->main_output, STM_TIMING_MODE_480P59940_25180, &vga))
    {
      stmhdmi_add_display_mode(hdmi, &vga);
    }
  }

  return 0;
}


static int stmhdmi_cea_process_video_short_codes(struct stm_hdmi *hdmi, unsigned long cea_mode)
{
  stm_display_mode_id_t mode;
  stm_display_mode_t    display_mode;
  int ret = -EINVAL;

  for(mode = 0;mode<STM_TIMING_MODE_COUNT;mode++)
  {
    if(!stm_display_output_get_display_mode(hdmi->main_output, mode, &display_mode))
    {
      if(display_mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] == cea_mode ||
         display_mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] == cea_mode ||
         display_mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27] == cea_mode)
      {
        stmhdmi_add_display_mode(hdmi, &display_mode);
        ret = 0;
      }
    }
  }

  return ret;
}


static int stmhdmi_get_nth_cea_code(struct stm_hdmi *hdmi, int edid_entry)
{
  int n;
  for(n=0;n<STM_MAX_CEA_MODES;n++)
  {
    switch(hdmi->edid_info.cea_codes[n].cea_code_descriptor)
    {
      case STM_CEA_VIDEO_CODE_UNSUPPORTED:
      default:
        break;
      case STM_CEA_VIDEO_CODE_NATIVE:
      case STM_CEA_VIDEO_CODE_NON_NATIVE:
        if ( hdmi->edid_info.cea_codes[n].edid_entry==edid_entry)
          return n;
    }
  }
  return -ENODEV;
}


static int stmhdmi_hdmi_ext_vic_block(struct stm_hdmi *hdmi, edid_block_t rawedid, int len, int block_pos)
{
  stm_display_mode_t    display_mode;
  unsigned int i,offset;
  unsigned char val;

  for(offset=0;offset<len;offset++)
  {
    val=rawedid[block_pos+offset];
    for (i=0; i<STM_HDMI_MAX_EXTENDED_MODES ; i++)
    {
      if (val == edid_4k_modes[i].ext_vic)
      {
        if(!stm_display_output_get_display_mode(hdmi->main_output, edid_4k_modes[i].mode_id, &display_mode))
        {
          stmhdmi_add_display_mode(hdmi, &display_mode);
        }
      }
    }
  }
  return 0;
}


static int stmhdmi_hdmi_3d_block(struct stm_hdmi *hdmi, edid_block_t rawedid, unsigned char video_block_flags, int len, int block_pos)
{
  int n,cea_code;
  uint32_t all_3d_flags = 0;
  int all_sbshalf_subsampling = 0;
  int offset = 0;

  /*
   * Add mandatory 3D video formats
   * - 1920*1080p23*98/24Hz ( Frame packing, Top-and-Bottom)
   * - 1280*720p59.94/60/50Hz (Frame packing, Top-and-Bottom)
   * - 1920*1080i59.94/60/50Hz (side-by-side (Half))
   */
  for(n=0;n<hdmi->edid_info.num_modes;n++)
  {
    if ((hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_1080P24000_74250)  ||
        (hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_1080P23976_74176)  ||
        (hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_720P60000_74250)   ||
        (hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_720P59940_74176)   ||
        (hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_720P50000_74250))
    {
      cea_code = hdmi->edid_info.display_modes[n].mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9];
      hdmi->edid_info.cea_codes[cea_code].supported_3d_flags = (STM_MODE_FLAGS_3D_TOP_BOTTOM |STM_MODE_FLAGS_3D_FRAME_PACKED);
      DPRINTK_NOPREFIX("Mandatory 3D formats for %ux%up-%u (VIC %d):0x%x\n",
          hdmi->edid_info.display_modes[n].mode_params.active_area_width,
          hdmi->edid_info.display_modes[n].mode_params.active_area_height,
          hdmi->edid_info.display_modes[n].mode_params.vertical_refresh_rate / 1000,
          cea_code,
          hdmi->edid_info.cea_codes[cea_code].supported_3d_flags);
    }
    else if ((hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_1080I60000_74250)||
             (hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_1080I59940_74176)||
             (hdmi->edid_info.display_modes[n].mode_id == STM_TIMING_MODE_1080I50000_74250_274M))
    {
      cea_code = hdmi->edid_info.display_modes[n].mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9];
      hdmi->edid_info.cea_codes[cea_code].supported_3d_flags = STM_MODE_FLAGS_3D_SBS_HALF;
      DPRINTK_NOPREFIX("Mandatory 3D formats for %ux%ui-%u (VIC %d):0x%x\n",
          hdmi->edid_info.display_modes[n].mode_params.active_area_width,
          hdmi->edid_info.display_modes[n].mode_params.active_area_height,
          hdmi->edid_info.display_modes[n].mode_params.vertical_refresh_rate / 1000,
          cea_code,
          hdmi->edid_info.cea_codes[cea_code].supported_3d_flags);
    }
  }

  if (((video_block_flags & STM_HDMI_VSDB_MULTI_MASK) == STM_HDMI_VSDB_MULTI_STRUCTURE_ALL) ||
      ((video_block_flags & STM_HDMI_VSDB_MULTI_MASK) == STM_HDMI_VSDB_MULTI_STRUCTURE_AND_MASK))
  {
    unsigned char val = rawedid[block_pos+offset];
    DPRINTK_NOPREFIX("Multi 3D structure is present at pos (%d)\nStructure ALL = 0x%x 0x%x\n",block_pos, rawedid[block_pos+offset],rawedid[block_pos+offset+1]);
    /*
     * Sink supports "side by side half with Horizontal sub-sampling 3D formats
     * Sink supports "side by side half with all Quincunx sub-sampling 3D formats
     */
    all_3d_flags |= ((val & 0x1)||(val & 0x80))?STM_MODE_FLAGS_3D_SBS_HALF:STM_MODE_FLAGS_NONE;
    switch(val & 0x81)
    {
      case 0:
        break;
      case 0x1:
        all_sbshalf_subsampling = STM_3D_DETAIL_H_SUBSAMPLING;
        break;
      case 0x80:
        all_sbshalf_subsampling = STM_3D_DETAIL_All_QUINCUNX;
        break;
      case 0x81:
        all_sbshalf_subsampling = STM_3D_DETAIL_ALL;
        break;
    }

    offset++;

    val = rawedid[block_pos+offset];

    /*
     * TODO: Check the master output to see if the hardware supports
     *       frame sequential and field alternative before we indicate
     *       support for these formats. Remember we are constructing a
     *       subset of the features supported by both the display _and_ the
     *       SoC.
     *
     * Sink supports Frame Packing 3D format
     */
    all_3d_flags |= (val & 0x1)?STM_MODE_FLAGS_3D_FRAME_PACKED:STM_MODE_FLAGS_NONE;
     /*
     * Sink supports Field alternative 3D format for interlaced video formats
     */
    all_3d_flags |= (val & 0x2)?STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE:STM_MODE_FLAGS_NONE;
    /*
     * Sink supports Top and Bottom 3D formats
     */
    all_3d_flags |= (val & 0x40)?STM_MODE_FLAGS_3D_TOP_BOTTOM:STM_MODE_FLAGS_NONE;
    offset ++;

    if (video_block_flags & STM_HDMI_VSDB_MULTI_STRUCTURE_ALL)
    {
      DPRINTK_NOPREFIX("All first 16 VIC codes in EDID support 3D structure flags = %x\n", all_3d_flags);
      /*
       * In this case, the HDMI sink supports the corresponding 3D structure
       * for all of the VICs listed in the first 16 entries in the EDID
       * Fetch the corresponding VICs in display_mode array
       */
      for (n=0; n<16; n++)
      {
        cea_code = stmhdmi_get_nth_cea_code(hdmi,n);
        if(cea_code > 0)
        {
          hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= all_3d_flags;
          if(all_3d_flags & STM_MODE_FLAGS_3D_SBS_HALF)
            hdmi->edid_info.cea_codes[cea_code].sbs_half_detail = all_sbshalf_subsampling;
        }
      }
    }
    else
    {
      DPRINTK_NOPREFIX("3D Mask = 0x%x 0x%x\n", rawedid[block_pos+offset],rawedid[block_pos+offset+1]);
      /*
       * In this case, the HDMI sink supports the corresponding 3D structure
       * for some selected VICs listed in the first 16 entries in the EDID
       *
       * Update the corresponding VICs based on bits set in the two mask bytes.
       * The first byte is the mask for entries 8..15, the second byte for
       * entries 0..7.
       */
      for( n=0; n<8; n++)
      {
        if (rawedid[block_pos+offset]& (0x1<<n))
        {
          cea_code = stmhdmi_get_nth_cea_code(hdmi,n+8);
          if(cea_code > 0)
          {
            DPRINTK_NOPREFIX("VIC entry %d (CEA code: %d) supports 3D structure flags = %x\n", n+8, cea_code, all_3d_flags);
            hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= all_3d_flags;
            if(all_3d_flags & STM_MODE_FLAGS_3D_SBS_HALF)
              hdmi->edid_info.cea_codes[cea_code].sbs_half_detail = all_sbshalf_subsampling;
          }
        }
      }
      offset++;
      for ( n=0; n<8; n++)
      {
        if (rawedid[block_pos+offset]& (0x1<<n))
        {
          cea_code = stmhdmi_get_nth_cea_code(hdmi,n);
          if(cea_code > 0)
          {
            DPRINTK_NOPREFIX("VIC entry: %d (CEA code: %d) supports 3D structure flags = %x\n", n, cea_code, all_3d_flags);
            hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= all_3d_flags;
            if(all_3d_flags & STM_MODE_FLAGS_3D_SBS_HALF)
              hdmi->edid_info.cea_codes[cea_code].sbs_half_detail = all_sbshalf_subsampling;
          }
        }
      }
      offset++;
    }
  }

  DPRINTK_NOPREFIX("Parsing individual 3D structures start offset = %d, block len = %d\n", offset, len);

  while(offset < len)
  {
    int EDID_Entry = (rawedid[block_pos+offset]>>4);
    cea_code = stmhdmi_get_nth_cea_code(hdmi,EDID_Entry);
    if(cea_code < 0)
    {
      DPRINTK_NOPREFIX("Individual 3D structure for VIC entry: %d has no matching CEA code recorded\n", EDID_Entry);
    }
    else
    {
      DPRINTK_NOPREFIX("Individual 3D structure for VIC entry: %d (CEA code: %d)", EDID_Entry, cea_code);
      if ((rawedid[block_pos+offset]&0x0F)==0x0)
      {
        hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= STM_MODE_FLAGS_3D_FRAME_PACKED;
        DPRINTK_NOPREFIX(": Frame Sequential\n");
      }
      if ((rawedid[block_pos+offset]&0x0F)==0x1)
      {
        hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE;
        DPRINTK_NOPREFIX(": Field Alternative\n");
      }
      if ((rawedid[block_pos+offset]&0x0F)==0x6)
      {
        hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= STM_MODE_FLAGS_3D_TOP_BOTTOM;
        DPRINTK_NOPREFIX(": Top Bottom\n");
      }

      /*
       * NOTE: The detail field is defined to be present for all structure types
       * between b1000~b1111, however all values other than b1000 are currently
       * reserved.
       */
      if ((rawedid[block_pos+offset]&0x0F)==0x8)
      {
        hdmi->edid_info.cea_codes[cea_code].supported_3d_flags |= STM_MODE_FLAGS_3D_SBS_HALF;
        offset++;
        hdmi->edid_info.cea_codes[cea_code].sbs_half_detail = (rawedid[block_pos+offset]>>4);
        DPRINTK_NOPREFIX(": SbS Half, Detail = 0x%x\n",hdmi->edid_info.cea_codes[cea_code].sbs_half_detail);
      }
    }
    offset++;
  }

  return 0;
}


static int stmhdmi_cea_hdmi_vendor_block(struct stm_hdmi *hdmi, edid_block_t rawedid, int len, int block_pos)
{
  unsigned char block_feature_flags,video_block_flags;
  int video_block_offset;
  int hdmi_vic_len, hdmi_3d_len;

  /*
   * block_pos points to byte 0 of the VSDB so that the offsets match the
   * specification. Note that len does not include this byte.
   */
  hdmi->edid_info.display_type = STM_DISPLAY_HDMI;
  hdmi->edid_info.cec_address[0] = rawedid[block_pos+4] >> 4;
  hdmi->edid_info.cec_address[1] = rawedid[block_pos+4] & 0xf;
  hdmi->edid_info.cec_address[2] = rawedid[block_pos+5] >> 4;
  hdmi->edid_info.cec_address[3] = rawedid[block_pos+5] & 0xf;

  if(len > 6)
    hdmi->edid_info.hdmi_vsdb_flags = rawedid[block_pos+6];

  if(len > 7)
    hdmi->edid_info.max_tmds_clock = rawedid[block_pos+7]*5000000;

  if(len > 8)
  {
    block_feature_flags = rawedid[block_pos+8];
    hdmi->edid_info.hdmi_cnc_flags = (block_feature_flags & STM_HDMI_VSDB_CNC_MASK);
    DPRINTK("VSDB block features (byte 7) = 0x%x\n",(int)block_feature_flags);
  }
  else
  {
    DPRINTK("No content, latency or video block information in this EDID, nothing more to do\n");
    return 0;
  }

  video_block_offset = 9;
  if(block_feature_flags & (STM_HDMI_VSDB_LATENCY_PRESENT | STM_HDMI_VSDB_I_LATENCY_PRESENT))
  {
    if(len < (video_block_offset+1))
    {
      DPRINTK("HDMI VSDB length doesn't match content features (latency)\n");
      return -EINVAL;
    }
    hdmi->edid_info.progressive_video_latency = rawedid[block_pos+video_block_offset];
    hdmi->edid_info.progressive_audio_latency = rawedid[block_pos+video_block_offset+1];
    video_block_offset +=2;
  }

  if(block_feature_flags & STM_HDMI_VSDB_I_LATENCY_PRESENT)
  {
    if(len < (video_block_offset+1))
    {
      DPRINTK("HDMI VSDB length doesn't match content features (I latency)\n");
      return -EINVAL;
    }
    hdmi->edid_info.interlaced_video_latency = rawedid[block_pos+video_block_offset];
    hdmi->edid_info.interlaced_audio_latency = rawedid[block_pos+video_block_offset+1];
    video_block_offset +=2;
  }
  else
  {
    hdmi->edid_info.interlaced_video_latency = hdmi->edid_info.progressive_video_latency;
    hdmi->edid_info.interlaced_audio_latency = hdmi->edid_info.progressive_audio_latency;
  }

  if ((block_feature_flags & STM_HDMI_VSDB_VIDEO_PRESENT) == 0)
  {
    DPRINTK("No video block in this EDID (EXT VIC or 3D), nothing more to do\n");
    return 0;
  }

  if (len < (video_block_offset+1))
  {
    DPRINTK("HDMI VSDB length doesn't match content features (video)\n");
    return -EINVAL;
  }

  video_block_flags = rawedid[block_pos+video_block_offset];
  DPRINTK("VSDB video_block_flags = 0x%x HDMI len byte = 0x%x\n",(int)video_block_flags,(int)(rawedid[block_pos+video_block_offset+1]));

  hdmi_vic_len      = (int)(rawedid[block_pos+video_block_offset+1] >> 5);
  hdmi_3d_len       = (int)(rawedid[block_pos+video_block_offset+1] & 0x1F);
  video_block_offset += 2;

  DPRINTK("HDMI VSDB length (%d) video block offset (%d) HDMI VIC len (%d) HDMI 3D length (%d)\n",len,video_block_offset,hdmi_vic_len,hdmi_3d_len);

  if (len < (video_block_offset+hdmi_vic_len+hdmi_3d_len-1))
  {
    DPRINTK("HDMI VSDB length does not match VIC and 3D blocks\n");
    return -EINVAL;
  }

  if(hdmi_vic_len > 0)
  {
    int ret;

    DPRINTK_NOPREFIX("Found HDMI Ext VIC block length %d\n", hdmi_3d_len);
    ret = stmhdmi_hdmi_ext_vic_block(hdmi, rawedid, hdmi_vic_len, block_pos+video_block_offset);
    if(ret < 0)
      return ret;

    video_block_offset += hdmi_vic_len;
  }

  if(video_block_flags & STM_HDMI_VSDB_3D_PRESENT)
  {
    int ret;

    DPRINTK_NOPREFIX("Found 3D structure %x block length %d\n", (int)video_block_flags, hdmi_3d_len);
    ret = stmhdmi_hdmi_3d_block(hdmi, rawedid, (video_block_flags & STM_HDMI_VSDB_3D_FLAGS_MASK), hdmi_3d_len, block_pos+video_block_offset);
    if(ret < 0)
      return ret;
  }

  return 0;
}

static int stmhdmi_cea_hdmi_hf_vendor_block(struct stm_hdmi *hdmi, edid_block_t rawedid, int len, int block_pos)
{
  unsigned char block_feature_flags;

  /*
   * block_pos points to byte 0 of the HF-VSDB so that the offsets match the
   * specification. Note that len does not include this byte.
   */
  hdmi->edid_info.hf_version = rawedid[block_pos+4];

  if(len > 5)
    hdmi->edid_info.max_tmds_character_rate = rawedid[block_pos+5];

  if(len > 6)
    hdmi->edid_info.hdmi_hf_vsdb_flags = rawedid[block_pos+6];

  if(len > 7)
  {
    block_feature_flags = rawedid[block_pos+7];
    hdmi->edid_info.hdmi_hf_dc_flags = (block_feature_flags & STM_HDMI_HF_VSDB_DC_MASK);
    DPRINTK("HF-VSDB block features (byte 7) = 0x%x\n",(int)block_feature_flags);
  }
  else
  {
    DPRINTK("No content or video block information in this EDID, nothing more to do\n");
  }

  return 0;
}
static int stmhdmi_cea_process_blocks(struct stm_hdmi *hdmi, edid_block_t rawedid)
{
  int block_end = rawedid[STM_EXT_TIMING_OFFSET];
  int block_pos = 4;
  unsigned offset;
  unsigned cea_mode = 0;
  int      native = 0;

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
          cea_mode = (unsigned)(rawedid[block_pos+offset]&0x7f);
          if(cea_mode <= STM_MAX_CEA_MODES_HDMI_14B)
          {
            native   = ((rawedid[block_pos+offset]&0x80) != 0);
          }
          else
          {
            cea_mode = (unsigned)(rawedid[block_pos+offset]);
          }
          if(!stmhdmi_cea_process_video_short_codes(hdmi, cea_mode))
          {
            if(cea_mode <= STM_MAX_CEA_MODES_HDMI_14B)
            {
              DPRINTK_NOPREFIX("Found CEA Video Mode:%u (%s)\n",cea_mode,native?"native":"non-native");
              hdmi->edid_info.cea_codes[cea_mode].cea_code_descriptor = native?STM_CEA_VIDEO_CODE_NATIVE:STM_CEA_VIDEO_CODE_NON_NATIVE;
            }
            hdmi->edid_info.cea_codes[cea_mode].edid_entry = hdmi->edid_info.next_cea_video_code_index;
          }
          else
          {
            DPRINTK_NOPREFIX("Unsupported CEA Video Mode:%u\n",cea_mode);
          }
          hdmi->edid_info.next_cea_video_code_index++;
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
          int ret;
          DPRINTK_NOPREFIX("Found IEEE HDMI Identifier:\n");
          ret = stmhdmi_cea_hdmi_vendor_block(hdmi, rawedid, len, block_pos-1);
          if(ret < 0)
            return ret;
        }

        if((rawedid[block_pos] == 0xc4) && (rawedid[block_pos+1] == 0x5d) && rawedid[block_pos+2] == 0xd8)
        {
          int ret;
          DPRINTK_NOPREFIX("Found IEEE OUI Identifier:\n");
          ret = stmhdmi_cea_hdmi_hf_vendor_block(hdmi, rawedid, len, block_pos-1);
          if(ret < 0)
            return ret;
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
          case STM_CEA_BLOCK_EXT_TAG_YCBCR_420_VDB:
            DPRINTK_NOPREFIX("Found CEA Block Extended YCbCr 4:2:0 VDB Tag\n");
            hdmi->edid_info.cea_capabilities |= STM_CEA_CAPS_420;
            for(offset=0;offset<len-1;offset++)
            {
              hdmi->edid_info.svd[offset] = (unsigned char)rawedid[block_pos+offset];
            }
            break;
          case STM_CEA_BLOCK_EXT_TAG_YCBCR_420_CMDB:
            DPRINTK_NOPREFIX("Found CEA Block Extended YCbCr 4:2:0 Capability Map Data Block Tag\n");
            hdmi->edid_info.cea_capabilities |= STM_CEA_CAPS_420;
            for(offset=0;offset<len-1;offset++)
            {
              hdmi->edid_info.cmdb[offset] = (unsigned char)rawedid[block_pos+offset];
            }
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


static int stmhdmi_read_extension_blocks(struct stm_hdmi *hdmi, unsigned int i)
{
  int res,retry;
  int blockpairs = hdmi->edid_info.base_raw[STM_EDID_EXTENSION]/2;
  int blockmap = 1;
  unsigned char segment;

  if(hdmi->edid_info.extension_raw[blockmap-1][STM_EXT_TYPE] != STM_EXT_TYPE_BLOCK_MAP)
  {
    DPRINTK("No Blockmap found in block1, cannot parse E-DDC addressed extensions\n");
    stmhdmi_invalidate_edid_info(hdmi);
    return -EINVAL;
  }

  if((res = stm_display_link_edid_read(hdmi->link, i, hdmi->edid_info.extension_raw[i-1])) < 0)
  {
    DPRINTK("Extenstion blocks read error, invalidating EDID\n");
    return res;
  }

  for(segment=1;segment<=blockpairs;segment++)
  {
    int blockmapaddress;
    int firstblocknr = segment*2;

    if(firstblocknr < 128)
      blockmapaddress = firstblocknr-1;
    else
      blockmapaddress = firstblocknr-128;

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

      if(hdmi->edid_info.extension_raw[blockmap-1][blockmapaddress] == 0 &&
         hdmi->edid_info.extension_raw[blockmap-1][blockmapaddress+1] == 0)
      {
        DPRINTK("Blockmap says segment %d is empty, the extension count was proably wrong\n",segment);
        /*
         * We have to be resilient against some EDIDs having the wrong
         * block count. This is not treated as an error, just stop reading.
         */
        break;
      }


      if ((i%2)==0)
      {
        /*
         * Process the first of the block pair.
         */
        if((res=stmhdmi_edid_checksum(hdmi->edid_info.extension_raw[firstblocknr-1]))<0)
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
          if(hdmi->edid_info.extension_raw[firstblocknr-1][STM_EXT_TYPE] != STM_EXT_TYPE_BLOCK_MAP)
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
          if(hdmi->edid_info.extension_raw[blockmap-1][blockmapaddress] != hdmi->edid_info.extension_raw[firstblocknr-1][STM_EXT_TYPE])
          {
            DPRINTK("Extension type doesn't match blockmap entry for block %d\n",firstblocknr);
            res = -EINVAL;
            break;
          }

          stmhdmi_read_ext_edid_block(hdmi,hdmi->edid_info.extension_raw[firstblocknr-1]);
        }
      }
      else
      {
        /*
         * Process the second of the block pair.
         */
        if((res=stmhdmi_edid_checksum(hdmi->edid_info.extension_raw[firstblocknr]))<0)
        {
          /*
           * Retry read to see if we can get a good checksum
           */
          DPRINTK("Bad Checksum for extension block %d\n",firstblocknr+1);
          continue;
        }

        if(hdmi->edid_info.extension_raw[blockmap-1][blockmapaddress+1] != hdmi->edid_info.extension_raw[firstblocknr][STM_EXT_TYPE])
        {
          DPRINTK("Extension type doesn't match blockmap entry for block %d\n",firstblocknr+1);
          res = -EINVAL;
          break;
        }

        stmhdmi_read_ext_edid_block(hdmi,hdmi->edid_info.extension_raw[firstblocknr]);
      }
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

  int i, res ;
  /* this invalidation is good for all error paths before the call to
   * stmhdmi_read_edid_block0()
   */
  stmhdmi_invalidate_edid_info(hdmi);

  if((res = stm_display_link_edid_read(hdmi->link, 0, hdmi->edid_info.base_raw)) < 0)
  {
    return res;
  }

  if(stmhdmi_edid_checksum(hdmi->edid_info.base_raw) < 0)
  {
    DPRINTK("Invalid basic EDID checksum \n");
#if defined(DEBUG)
    print_hex_dump_bytes("",DUMP_PREFIX_OFFSET,&(hdmi->edid_info.base_raw[0]),128);
#endif
    return -EINVAL;
  }

  if((res = stmhdmi_read_edid_block0(hdmi,hdmi->edid_info.base_raw)) < 0)
  {
#if defined(DEBUG)
    print_hex_dump_bytes("",DUMP_PREFIX_OFFSET,&(hdmi->edid_info.base_raw[0]),128);
#endif
    return res;
  }

  if(hdmi->edid_info.base_raw[STM_EDID_EXTENSION] == 0)
  {
    DPRINTK_NOPREFIX("No extension header:\n");
    hdmi->edid_info.display_type = STM_DISPLAY_DVI;
    return 0;
  }
  else
  {
    if((hdmi->edid_info.extension_raw = kzalloc(sizeof(edid_block_t)*(hdmi->edid_info.base_raw[STM_EDID_EXTENSION]), GFP_KERNEL)) == 0)
      return -ENOMEM;
  }

  if((res = stm_display_link_edid_read(hdmi->link, 1, hdmi->edid_info.extension_raw[0])) < 0)
  {
    return res;
  }
  if(stmhdmi_edid_checksum(hdmi->edid_info.extension_raw[0]) < 0)
  {
    DPRINTK("Invalid extension block checksum \n");
#if defined(DEBUG)
    print_hex_dump_bytes("",DUMP_PREFIX_OFFSET,&(hdmi->edid_info.extension_raw[0][0]),128);
#endif
    return -EINVAL;
  }

  if((res = stmhdmi_read_ext_edid_block(hdmi,hdmi->edid_info.extension_raw[0])) < 0)
  {
    stmhdmi_invalidate_edid_info(hdmi);
    return res;
  }

  for (i=2; i<= hdmi->edid_info.base_raw[STM_EDID_EXTENSION]; i++)
  {
    stmhdmi_read_extension_blocks(hdmi,i);
  }
  hdmi->edid_updated = true;
  return 0;
}

int stmhdmi_safe_edid(struct stm_hdmi *hdmi)
{
  DPRINTK("Setting Safe EDID\n");
  hdmi->edid_info.display_type     = hdmi->hdmi_safe_mode?STM_DISPLAY_HDMI:STM_DISPLAY_DVI;
  strcpy(hdmi->edid_info.monitor_name,"SAFEMODE");
  hdmi->edid_info.tv_aspect        = STM_WSS_ASPECT_RATIO_4_3;
  hdmi->edid_info.cea_capabilities = STM_CEA_CAPS_RGB;
  hdmi->edid_info.max_clock        = 26;
  hdmi->edid_info.max_tmds_clock   = 25200000;
  stmhdmi_cea_process_video_short_codes(hdmi, 1);
  hdmi->edid_info.cea_codes[1].cea_code_descriptor = STM_CEA_VIDEO_CODE_NON_NATIVE;

  return 0;
}


void stmhdmi_invalidate_edid_info(struct stm_hdmi *hdmi)
{
  if (hdmi->edid_info.extension_raw!=NULL)
  {
    kfree(hdmi->edid_info.extension_raw);
    hdmi->edid_info.extension_raw = NULL;
  }
  memset(&hdmi->edid_info,0,sizeof(struct stm_edid));
  hdmi->edid_updated = false;
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

  if(hdmi->edid_info.tv_aspect != STM_WSS_ASPECT_RATIO_UNKNOWN)
  {
    DPRINTK_NOPREFIX("                : max size image %ux%ucm (%s)\n",hdmi->edid_info.max_hsize,
                                                                       hdmi->edid_info.max_vsize,
                                                                       (hdmi->edid_info.tv_aspect == STM_WSS_ASPECT_RATIO_4_3)?"4:3":
                                                                       ((hdmi->edid_info.tv_aspect == STM_WSS_ASPECT_RATIO_16_9)?"16:9":"64_27"));
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

  DPRINTK_NOPREFIX("Video Codes     :\n");

  for(n=0;n<STM_MAX_CEA_MODES;n++)
  {
    switch(hdmi->edid_info.cea_codes[n].cea_code_descriptor)
    {
      case STM_CEA_VIDEO_CODE_UNSUPPORTED:
      default:
        break;
      case STM_CEA_VIDEO_CODE_NON_NATIVE:
      case STM_CEA_VIDEO_CODE_NATIVE:
        DPRINTK_NOPREFIX("    %d%c:EDID index %d:%s", n,
            hdmi->edid_info.cea_codes[n].cea_code_descriptor==STM_CEA_VIDEO_CODE_NATIVE?'*':' ',
            hdmi->edid_info.cea_codes[n].edid_entry,
            hdmi->edid_info.cea_codes[n].supported_3d_flags?"3D":"2D" );
        if(hdmi->edid_info.cea_codes[n].supported_3d_flags)
          DPRINTK_NOPREFIX(":Flags 0x%x",hdmi->edid_info.cea_codes[n].supported_3d_flags);
        if(hdmi->edid_info.cea_codes[n].supported_3d_flags & STM_MODE_FLAGS_3D_SBS_HALF)
          DPRINTK_NOPREFIX(":SbSH 0x%x",hdmi->edid_info.cea_codes[n].sbs_half_detail);

        DPRINTK_NOPREFIX("\n");
        break;
    }
  }

}
