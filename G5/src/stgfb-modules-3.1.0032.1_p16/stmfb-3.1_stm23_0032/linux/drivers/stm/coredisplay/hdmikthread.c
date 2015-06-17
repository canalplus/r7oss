/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/hdmikthread.c
 * Copyright (c) 2005-2009 STMicroelectronics Limited.
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
  #include <linux/freezer.h>
#endif
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


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
  #define set_freezable(x)
#endif

#define I2C_DRIVERID_STM_HDMI     128
#define I2C_CLIENT_NAME_STM_HDMI  "HDMI-EDID"
#define I2C_EDID_ADDR             0x50
#define I2C_EDDC_SEGMENT_REG_ADDR 0x30


/*
 * We have to force the detection of the I2C devices, because the SMBUS QUICK
 * probing mechanism corrupts the Quantum Data 882 EDID. We would prefer to
 * use the new i2c_new_device interface in the later 2.6 kernels, but need
 * to keep compatibility with 2.6.17 for the moment.
 */
static unsigned short forced_i2c_edid[] = { ANY_I2C_BUS, I2C_EDID_ADDR, ANY_I2C_BUS, I2C_EDDC_SEGMENT_REG_ADDR, I2C_CLIENT_END};
static unsigned short *forces_edid[]    = { forced_i2c_edid,NULL };

static struct i2c_driver driver = {
   .id             = I2C_DRIVERID_STM_HDMI,
   .attach_adapter = 0,
   .detach_client  = 0,
   .command        = 0,
   .driver         = {
       .owner          = THIS_MODULE,
       .name           = "i2c_hdmi_driver"
   }
};

static unsigned short ignore = I2C_CLIENT_END;

static struct i2c_client_address_data edid_addr_data = {
  .forces     = forces_edid,
  .normal_i2c = &ignore,
  .probe      = &ignore,
  .ignore     = &ignore
};

/***********************************************************************
 * I2C management routines for EDID devices
 */
static int stmhdmi_attach_edid(struct i2c_adapter *adap, int addr, int kind)
{
  struct i2c_client *client;

  if (!(client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)))
    return -ENOMEM;


  memset(client, 0, sizeof(struct i2c_client));

  client->adapter = adap;
  client->driver  = &driver;
  client->addr    = addr;

  strncpy(client->name, I2C_CLIENT_NAME_STM_HDMI, I2C_NAME_SIZE);

  i2c_attach_client(client);

  return 0;
}


static void stmhdmi_i2c_connect(struct stm_hdmi *hdmi)
{
  struct list_head *entry;

  DPRINTK("Probing for I2C clients\n");
  /*
   * Only probe for clients we don't already have, this neatly copes
   * with a "fake" hotplug event that is generated when the display
   * mode changes.
   */
  if(!hdmi->edid_client)
  {
    i2c_probe(hdmi->i2c_adapter, &edid_addr_data, stmhdmi_attach_edid);
  }

  DPRINTK("Searching for registered I2C client objects\n");

  mutex_lock(&hdmi->i2c_adapter->clist_lock);

  list_for_each(entry,&hdmi->i2c_adapter->clients)
  {
    struct i2c_client *client = list_entry(entry, struct i2c_client, list);

    if(client->driver->id == I2C_DRIVERID_STM_HDMI)
    {
      if(!strcmp(client->name,I2C_CLIENT_NAME_STM_HDMI))
      {
        switch(client->addr)
        {
          case I2C_EDDC_SEGMENT_REG_ADDR:
            DPRINTK("Found EDDC I2C Client\n");
            hdmi->eddc_segment_reg_client = client;
            break;
          case I2C_EDID_ADDR:
            DPRINTK("Found EDID I2C Client\n");
            hdmi->edid_client = client;
            break;
          default:
            break;
        }
      }
    }
  }

  mutex_unlock(&hdmi->i2c_adapter->clist_lock);
}


static void stmhdmi_i2c_disconnect(struct stm_hdmi *hdmi)
{
  if(hdmi->edid_client)
  {
    i2c_detach_client(hdmi->edid_client);
    kfree(hdmi->edid_client);
    hdmi->edid_client = 0;
  }
  if(hdmi->eddc_segment_reg_client)
  {
    i2c_detach_client(hdmi->eddc_segment_reg_client);
    kfree(hdmi->eddc_segment_reg_client);
    hdmi->eddc_segment_reg_client = 0;
  }
}


/******************************************************************************
 * Functions to check the EDID for a valid mode and enable the HDMI output
 */
static void stmhdmi_configure_hdmi_formatting(struct stm_hdmi *hdmi)
{
  ULONG video_type;
  ULONG colour_depth;
  ULONG quantization = 0;
  ULONG sink_supports_deepcolour;

  /*
   * We are connected to a CEA conformant HDMI device. In this case the spec
   * says we must do HDMI; if the device does not support YCrCb then force
   * RGB output.
   */
  if(hdmi->edid_info.cea_capabilities & (STM_CEA_CAPS_YUV|STM_CEA_CAPS_422))
  {
    video_type = hdmi->video_type & (STM_VIDEO_OUT_RGB |
                                     STM_VIDEO_OUT_YUV |
                                     STM_VIDEO_OUT_422);
  }
  else
  {
    video_type = STM_VIDEO_OUT_RGB;
  }

  DPRINTK("Setting HDMI output format %s\n",(video_type&STM_VIDEO_OUT_RGB)?"RGB":"YUV");

  sink_supports_deepcolour = (hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_MASK)?1:0;

  stm_display_output_set_control(hdmi->hdmi_output, STM_CTRL_HDMI_SINK_SUPPORTS_DEEPCOLOUR, sink_supports_deepcolour);

  /*
   * Filter the requested colour depth based on the EDID and colour format,
   * falling back to standard colour if there is any mismatch.
   */
  if(!sink_supports_deepcolour         ||
     (video_type & STM_VIDEO_OUT_422)  ||
     ((video_type & STM_VIDEO_OUT_YUV) && !(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_Y444)))
  {
    DPRINTK("Deepcolour not supported in requested colour format\n");
    colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT;
  }
  else
  {
    switch(hdmi->video_type & STM_VIDEO_OUT_HDMI_COLOURDEPTH_MASK)
    {
      case STM_VIDEO_OUT_HDMI_COLOURDEPTH_30BIT:
        if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_30BIT)
          colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_30BIT;
        else
          colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT;
        break;
      case STM_VIDEO_OUT_HDMI_COLOURDEPTH_36BIT:
        if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_36BIT)
          colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_36BIT;
        else
          colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT;
        break;
      case STM_VIDEO_OUT_HDMI_COLOURDEPTH_48BIT:
        if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_48BIT)
          colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_48BIT;
        else
          colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT;
        break;
      case STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT:
      default:
        colour_depth = STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT;
        break;
    }
  }

  switch(colour_depth)
  {
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT:
      DPRINTK("Setting 24bit colour\n");
      break;
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_30BIT:
      DPRINTK("Setting 30bit colour\n");
      break;
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_36BIT:
      DPRINTK("Setting 36bit colour\n");
      break;
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_48BIT:
      DPRINTK("Setting 48bit colour\n");
      break;
  }

  stm_display_output_set_control(hdmi->hdmi_output, STM_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_HDMI | video_type | colour_depth));

  if(hdmi->edid_info.cea_vcdb_flags & STM_CEA_VCDB_QS_RGB_QUANT)
    quantization |= (ULONG)STM_HDMI_AVI_QUANTIZATION_RGB;

  if(hdmi->edid_info.cea_vcdb_flags & STM_CEA_VCDB_QY_YCC_QUANT)
    quantization |= (ULONG)STM_HDMI_AVI_QUANTIZATION_YCC;

  stm_display_output_set_control(hdmi->hdmi_output, STM_CTRL_HDMI_AVI_QUANTIZATION, quantization);

  if(hdmi->cea_mode_from_edid)
  {
    ULONG tmp = (hdmi->edid_info.tv_aspect == STM_WSS_4_3)?STM_HDMI_CEA_MODE_4_3:STM_HDMI_CEA_MODE_16_9;
    stm_display_output_set_control(hdmi->hdmi_output, STM_CTRL_HDMI_CEA_MODE_SELECT, tmp);
  }
}


static int stmhdmi_determine_pixel_repetition(struct stm_hdmi        *hdmi,
                                              const stm_mode_line_t **mode,
                                              ULONG                  *tvStandard)

{
  int repeat = 4;

  DPRINTK("\n");

  /*
   * Find the maximum pixel repeat we can use for SD/ED modes; this is so we
   * always have the maximum audio bandwidth available.
   */
  while(repeat>1)
  {
    const stm_mode_line_t *m;
    DPRINTK("Trying pixel repeat = %d\n",repeat);
    m = stm_display_output_find_display_mode(hdmi->main_output,
                                               (*mode)->ModeParams.ActiveAreaWidth*repeat,
                                               (*mode)->ModeParams.ActiveAreaHeight,
                                               (*mode)->TimingParams.LinesByFrame,
                                               (*mode)->TimingParams.PixelsPerLine*repeat,
                                               (*mode)->TimingParams.ulPixelClock*repeat,
                                               (*mode)->ModeParams.ScanType);
    if(m)
    {
      int n;
      DPRINTK("Found SoC timing mode for pixel repeated mode %d @ %p\n",m->Mode,m);
      for(n=0;n<hdmi->edid_info.num_modes;n++)
      {
        const stm_mode_line_t *tmp = hdmi->edid_info.display_modes[n];

        if(tmp->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] == m->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] &&
           tmp->ModeParams.HDMIVideoCodes[AR_INDEX_16_9] == m->ModeParams.HDMIVideoCodes[AR_INDEX_16_9])
        {
          DPRINTK("Matched pixel repeated mode %d @ %p with display EDID supported modes\n",tmp->Mode,tmp);
          *mode = m;
          break;
        }
      }

      if(n<hdmi->edid_info.num_modes)
        break;
    }

    repeat /= 2;
  }

  switch(repeat)
  {
    case 4:
      *tvStandard |= STM_OUTPUT_STD_TMDS_PIXELREP_4X;
      break;
    case 2:
      *tvStandard |= STM_OUTPUT_STD_TMDS_PIXELREP_2X;
      break;
    default:
      if((*mode)->ModeParams.OutputStandards & STM_OUTPUT_STD_SD_MASK)
      {
        const stm_mode_line_t *m;
        repeat = 2;
        DPRINTK("No supported pixel repeated mode found for SD interlaced timing\n");
        DPRINTK("Trying to find 2x pixel repeat for non-strict EDID semantics\n");
        m = stm_display_output_find_display_mode(hdmi->main_output,
                                                   (*mode)->ModeParams.ActiveAreaWidth*repeat,
                                                   (*mode)->ModeParams.ActiveAreaHeight,
                                                   (*mode)->TimingParams.LinesByFrame,
                                                   (*mode)->TimingParams.PixelsPerLine*repeat,
                                                   (*mode)->TimingParams.ulPixelClock*repeat,
                                                   (*mode)->ModeParams.ScanType);
        if(m)
        {
          *mode = m;
          *tvStandard |= STM_OUTPUT_STD_TMDS_PIXELREP_2X;
        }
        else
        {
          DPRINTK("No really no pixel repeated mode supported by the hardware, errr that's odd\n");
          return -EINVAL;
        }
      }
      break;
  }

  return 0;
}


static int stmhdmi_output_start(struct stm_hdmi       *hdmi,
                                const stm_mode_line_t *mode,
                                ULONG                  tvStandard)
{
  const stm_mode_line_t *current_hdmi_mode;
  ULONG current_tv_standard;

  DPRINTK("\n");

  /*
   * Find out if the HDMI output is still running, we may be handling a deferred
   * disconnection hotplug event.
   */
  current_hdmi_mode = stm_display_output_get_current_display_mode(hdmi->hdmi_output);
  stm_display_output_get_current_tv_standard(hdmi->hdmi_output, &current_tv_standard);

  if(hdmi->deferred_disconnect > 0)
  {
    /*
     * You might think, hold on how can the mode have changed in this case,
     * but if the EDID changed what pixel repeated modes are supported then
     * that might happen. Therefore we return an error and the output will
     * be disabled and the application notified that it needs to do something.
     */
    if((current_hdmi_mode == mode) && (tvStandard == current_tv_standard))
      return 0;
    else
      return -EINVAL;
  }

  /*
   * If we got a request to restart the output, without a disconnect first,
   * then really do a restart if the mode is different by stopping the
   * output first. If the mode is exactly the same just call start which
   * will reset the output's connection state to connected.
   */
  if((current_hdmi_mode != mode) || (tvStandard != current_tv_standard))
    stm_display_output_stop(hdmi->hdmi_output);

  DPRINTK("Starting Video Mode %lux%lu%s\n",mode->ModeParams.ActiveAreaWidth,
                                            mode->ModeParams.ActiveAreaHeight,
                                            (mode->ModeParams.ScanType == SCAN_I)?"i":"p");

  if(stm_display_output_start(hdmi->hdmi_output, mode, tvStandard)<0)
    return -EINVAL;

  return 0;
}


static int stmhdmi_enable_mode_by_timing_limits(struct stm_hdmi       *hdmi,
                                                const stm_mode_line_t *mode,
                                                ULONG                  tvStandard)
{
  unsigned long hfreq,vfreq;

  DPRINTK("Falling back refresh range limits for DVI monitor\n");

  if(mode->TimingParams.ulPixelClock > (hdmi->edid_info.max_clock*1000000))
  {
    DPRINTK("Pixel clock (%luHz) out of range\n",mode->TimingParams.ulPixelClock);
    return -EINVAL;
  }

  /*
   * Check H & V Refresh instead
   */
  hfreq = mode->TimingParams.ulPixelClock / mode->TimingParams.PixelsPerLine;
  vfreq = mode->ModeParams.FrameRate / 1000;

  if((vfreq < hdmi->edid_info.min_vfreq) || (vfreq > hdmi->edid_info.max_vfreq))
  {
    DPRINTK("Vertical refresh (%luHz) out of range\n",vfreq);
    return -EINVAL;
  }

  if((hfreq < (hdmi->edid_info.min_hfreq*1000)) || (hfreq > (hdmi->edid_info.max_hfreq*1000)))
  {
    DPRINTK("Horizontal refresh (%luKHz) out of range\n",hfreq/1000);
    return -EINVAL;
  }

  DPRINTK("Starting DVI Video Mode %lux%lu hfreq = %luKHz vfreq = %luHz\n",mode->ModeParams.ActiveAreaWidth,
                                                                           mode->ModeParams.ActiveAreaHeight,
                                                                           hfreq/1000,
                                                                           vfreq);

  return stmhdmi_output_start(hdmi, mode, tvStandard);
}


static int stmhdmi_enable_link(struct stm_hdmi *hdmi)
{
  int n;
  const stm_mode_line_t *mode;
  ULONG tvStandard = STM_OUTPUT_STD_CEA861C;

  if(hdmi->edid_info.display_type == STM_DISPLAY_INVALID)
  {
    DPRINTK("Invalid EDID: Not enabling link\n");
    return -ENODEV;
  }

  /*
   * Get the master output's mode, this is what we want to set
   */
  mode = stm_display_output_get_current_display_mode(hdmi->main_output);
  if(!mode)
    return -EINVAL;

  if(hdmi->edid_info.display_type == STM_DISPLAY_DVI)
  {
    /*
     * We believe we now have a computer monitor or a TV with a DVI input not
     * a HDMI input. So set the output format to DVI and force RGB.
     */
    DPRINTK("Setting DVI/RGB output\n");
    stm_display_output_set_control(hdmi->hdmi_output, STM_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_RGB));
  }
  else
  {
    if(stmhdmi_determine_pixel_repetition(hdmi,&mode,&tvStandard)!=0)
      return -EINVAL;

    stmhdmi_configure_hdmi_formatting(hdmi);
  }


  /*
   * If we have been asked not to check against the EDID, just start
   * the mode.
   */
  if(hdmi->non_strict_edid_semantics)
    return stmhdmi_output_start(hdmi, mode, tvStandard);

  for(n=0;n<hdmi->edid_info.num_modes;n++)
  {
    const stm_mode_line_t *tmp = hdmi->edid_info.display_modes[n];

    /*
     * We match up the analogue display mode with the EDID mode by using the
     * HDMI video codes.
     */
    if(tmp->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] == mode->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] &&
       tmp->ModeParams.HDMIVideoCodes[AR_INDEX_16_9] == mode->ModeParams.HDMIVideoCodes[AR_INDEX_16_9])
    {
      return stmhdmi_output_start(hdmi, mode, tvStandard);
    }
  }

  DPRINTK("Current video mode not reported as supported by display\n");

  /*
   * We are still trying to only set modes indicated as valid in the EDID. So
   * for DVI devices allow modes that are inside the timing limits.
   */
  if(hdmi->edid_info.display_type == STM_DISPLAY_DVI)
    return stmhdmi_enable_mode_by_timing_limits(hdmi, mode, tvStandard);

  return -EINVAL;
}


/*******************************************************************************
 * Kernel management thread state machine helper functions.
 */
static void stmhdmi_handle_wait_queue_timeout(struct stm_hdmi *hdmi)
{
  switch(hdmi->status)
  {
    case STM_DISPLAY_CONNECTED:
    {
      stm_meta_data_result_t res;

      /*
       * We are deliberately ignoring any errors when queueing SPD data as
       * (a) they really shouldn't happen and (b) there is nothing we can
       * do about it anyway.
       */
      (void)stm_display_output_queue_metadata(hdmi->hdmi_output, hdmi->spd_metadata, &res);
      break;
    }
    case STM_DISPLAY_DISCONNECTED:
    {
      if(hdmi->deferred_disconnect > 0)
      {
        hdmi->deferred_disconnect--;

        if(hdmi->deferred_disconnect == 0)
        {
          DPRINTK("Doing deferred output disable due to HPD de-assert timeout\n");
          stm_display_output_stop(hdmi->hdmi_output);
          hdmi->disable = true;
          sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
        }
      }
      break;
    }
    default:
      break;
  }
}


static void stmhdmi_restart_display(struct stm_hdmi *hdmi)
{
  DPRINTK("Starting HDMI Output hdmi = %p\n",hdmi);

  /*
   * We always re-read the EDID, as we might have received a hotplug
   * event just because the user changed the TV setup and the EDID
   * information has now changed.
   */
  if(stmhdmi_read_edid(hdmi) != 0)
  {
    DPRINTK("EDID Read Error, setup safe EDID\n");
    stmhdmi_safe_edid(hdmi);
  }

  stmhdmi_dump_edid(hdmi);

  if(!hdmi->disable)
  {
    if(stmhdmi_enable_link(hdmi) != 0)
    {
      /*
       * If we had an error starting the output, do a stop just in case it was
       * already running. This might happen for instance if we got a restart
       * due to a change in the EDID mode semantics and the current mode is
       * no longer allowed on the display.
       */
      stm_display_output_stop(hdmi->hdmi_output);

      if(hdmi->deferred_disconnect > 0)
      {
        /*
         * We were waiting to see if we could ignore the HPD pulse,
         * but it turns out the new EDID is not compatible with the
         * existing display mode so we now disable the output and signal
         * the hotplug attribute so applications can check the EDID change.
         */
        DPRINTK("Doing deferred output disable due to incompatible EDID\n");
        hdmi->disable = true;
        sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
      }
    }
  }
  else
  {
    /*
     * We are not starting the display because we are disabled, so the sysfs
     * hotplug notification that is normally done when we enter the connected
     * state will not happen. Hence we signal it now so applications know
     * to check the new EDID and to possibly re-enable the HDMI output.
     */
    sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
  }

  hdmi->deferred_disconnect = 0;

}


static void stmhdmi_disconnect_display(struct stm_hdmi *hdmi)
{
  stmhdmi_invalidate_edid_info(hdmi);
  if(hdmi->hotplug_mode == STMHDMI_HPD_STOP_IF_NECESSARY)
  {
    DPRINTK("Deferring disconnect hdmi = %p\n",hdmi);
    /*
     * Wait for 3x 1/2 second timeouts to see if HPD gets re-asserted again
     * before really stopping the HDMI output. This is based on the time
     * a Yamaha RX-V1800 keeps hotplug de-asserted when a single TV connected
     * to it asserts its hotplug (approx 1s) plus some margin for us to respond.
     *
     * It is possible that AV receivers acting as repeaters will keep HPD
     * de-asserted while they authenticate all downstream devices, which can
     * take up to 4.2seconds (see HDCP specification) for the maximum repeater
     * depth. Do we want to wait that long?
     *
     */
    hdmi->deferred_disconnect = 3;
  }
  else
  {
    DPRINTK("Stopping HDMI Output hdmi = %p\n",hdmi);
    stm_display_output_stop(hdmi->hdmi_output);
    if(hdmi->hotplug_mode == STMHDMI_HPD_DISABLE_OUTPUT)
    {
      DPRINTK("Disabling HDMI Output hdmi = %p\n",hdmi);
      hdmi->disable = true;
    }

    sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
  }
}


static void stmhdmi_connect_display(struct stm_hdmi *hdmi)
{
  if(!hdmi->disable)
  {
    stm_meta_data_result_t res;

    DPRINTK("HDMI Output connected, signalling for authentication hdmi = %p queue = %p\n",hdmi,&(hdmi->auth_wait_queue));
    hdmi->auth = 0;
    wake_up(&(hdmi->auth_wait_queue));
    /*
     * Schedule the sending of the product identifier HDMI info frame.
     */
    (void)stm_display_output_queue_metadata(hdmi->hdmi_output, hdmi->spd_metadata, &res);
    /*
     * Signal the hotplug sysfs attribute now the output is alive.
     */
    sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
  }
  else
  {
    DPRINTK("HDMI Output disabled\n");
    stm_display_output_stop(hdmi->hdmi_output);
  }
}


/******************************************************************************
 * The kernel thread that does the management of HDMI hotplugged devices
 */
int stmhdmi_manager(void *data)
{
struct stm_hdmi *hdmi = (struct stm_hdmi *)data;
unsigned long saveFlags;
int res;

  DPRINTK("Starting HDMI Thread for info = %p\n",hdmi);

  if((res = i2c_add_driver(&driver)))
  {
    DPRINTK("Registering I2C driver failed\n");
    return 0;
  }

  /*
   * This does not probe the required I2C addresses to avoid confusing devices
   * with the SMBUS QUICK style transaction. Instead it creates I2C clients
   * assuming they exist, which as we only use them when a hotplug is detected
   * should be the case. Therefore we can do this once instead of connecting
   * and disconnecting on hotplug events.
   */
  stmhdmi_i2c_connect(hdmi);

  DPRINTK("Entering hotplug event loop\n");

  set_freezable();

  while(1)
  {
    stm_display_status_t current_status;

    if(wait_event_interruptible_timeout(hdmi->status_wait_queue,
                                        ((hdmi->status_changed != 0) || kthread_should_stop()),HZ/2))
    {
      if (try_to_freeze())
      {
        /*
         * Back around the loop, any pending work or kthread stop will get
         * picked up again immediately.
         */
        continue;
      }
    }

    mutex_lock(&(hdmi->lock));

    if(kthread_should_stop())
    {
      DPRINTK("HDMI Thread terminating for info = %p\n",hdmi);

      stm_display_output_stop(hdmi->hdmi_output);
      stmhdmi_i2c_disconnect(hdmi);
      i2c_del_driver(&driver);

      mutex_unlock(&(hdmi->lock));

      return 0;
    }

    spin_lock_irqsave(&(hdmi->spinlock), saveFlags);

    /*
     * Handle the 1/2 second timeout to re-send the SPD info frame and
     * handle deferred disconnection after a HPD de-assert.
     */
    if(likely(hdmi->status_changed == 0))
    {
      spin_unlock_irqrestore(&(hdmi->spinlock), saveFlags);
      stmhdmi_handle_wait_queue_timeout(hdmi);
      mutex_unlock(&(hdmi->lock));
      continue;
    }

    /*
     * Handle a real HDMI state change
     */
    current_status = hdmi->status;
    hdmi->status_changed = 0;
    spin_unlock_irqrestore(&(hdmi->spinlock), saveFlags);

    DPRINTK("Handling HDMI State Change current_status = %d\n",current_status);

    switch(current_status)
    {
      case STM_DISPLAY_NEEDS_RESTART:
        stmhdmi_restart_display(hdmi);
        break;
      case STM_DISPLAY_DISCONNECTED:
        stmhdmi_disconnect_display(hdmi);
        break;
      case STM_DISPLAY_CONNECTED:
        stmhdmi_connect_display(hdmi);
        break;
    }

    DPRINTK("HDMI management loop finished\n");
    mutex_unlock(&(hdmi->lock));
  }
}

