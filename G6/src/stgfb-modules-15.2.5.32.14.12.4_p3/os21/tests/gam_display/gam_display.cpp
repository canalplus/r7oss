/***********************************************************************
 *
 * File: os21/tests/gam_display/gam_display.cpp
 * Copyright (c) 2004, 2005, 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Display a GAM file on a video plane.
 *
\***********************************************************************/

#include <os21/stglib/application_helpers.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <malloc.h>     // For memalign


static semaphore_t *hotplug_sem     = 0;
static int silent_hotplug           = 0;

static stm_display_output_connection_status_t currentDisplayStatus = STM_DISPLAY_DISCONNECTED;

static stm_display_output_h hHDMI     = 0;
static stm_display_output_h hDVO      = 0;

static stm_display_mode_t  ModeLine = { STM_TIMING_MODE_RESERVED };
static stm_display_mode_t  HDMIModeLine = { STM_TIMING_MODE_RESERVED };
static uint32_t            hdmistandard = STM_OUTPUT_STD_CEA861;
static uint32_t            hdmiflags    = STM_MODE_FLAGS_NONE;

#if 1
    /* Default mode: HD 50 Hz */
    stm_display_mode_id_t  MODE     = STM_TIMING_MODE_1080I50000_74250_274M;
    unsigned long       STANDARD = STM_OUTPUT_STD_SMPTE274M;
#else
    /* Default mode: SD 50 Hz */
    stm_display_mode_id_t  MODE     = STM_TIMING_MODE_576I50000_13500;
    unsigned long       STANDARD = STM_OUTPUT_STD_PAL_BDGHI;
#endif

static interrupt_t *vsync_interrupt=0;


typedef struct GamPictureHeader_s
{
    unsigned short     HeaderSize;
    unsigned short     Signature;
    unsigned short     Type;
    unsigned short     Properties;
    unsigned int       PictureWidth;     /* With 4:2:0 R2B this is a OR between Pitch and Width */
    unsigned int       PictureHeight;
    unsigned int       LumaSize;
    unsigned int       ChromaSize;
} GamPictureHeader_t;




/******************************************/
static int vsync_isr(void *data)
{
  struct display_update_s *d = (struct display_update_s *)data;
  uint32_t     timingevent;
  stm_time64_t dummy;

  /* Clear for edge triggered ILC interrupts, this is a NOP on other controllers */
  interrupt_clear(vsync_interrupt);

  stm_display_output_handle_interrupts(d->output);

  stm_display_output_get_last_timing_event(d->output, &timingevent, &dummy);

  if(timingevent == STM_TIMING_EVENT_LINE || timingevent == STM_TIMING_EVENT_NONE)
    return OS21_SUCCESS;

  semaphore_signal(d->frameupdate_sem);

  if(hHDMI)
  {
    stm_display_output_connection_status_t oldDisplayStatus = currentDisplayStatus;

    stm_display_output_get_connection_status(hHDMI, &currentDisplayStatus);

    if(hotplug_poll_pio)
    {
      unsigned char hotplugstate = *hotplug_poll_pio & hotplug_pio_pin;
      if(currentDisplayStatus == STM_DISPLAY_DISCONNECTED)
      {
        /*
         * If the device has just been plugged in, flag that we now need to
         * start the output.
         */
        if(hotplugstate != 0)
        {
          currentDisplayStatus = STM_DISPLAY_NEEDS_RESTART;
          stm_display_output_set_connection_status(hHDMI, currentDisplayStatus);
        }
      }
      else
      {
        /*
         * We may either be waiting for the output to be started, or already
         * started, so only change the state if the device has now been
         * disconnected.
         */
        if(hotplugstate == 0)
        {
          currentDisplayStatus = STM_DISPLAY_DISCONNECTED;
          stm_display_output_set_connection_status(hHDMI, currentDisplayStatus);
        }
      }
    }

    if(oldDisplayStatus != currentDisplayStatus)
    {
      semaphore_signal(hotplug_sem);
    }
  }

  return OS21_SUCCESS;
}


void hotplug_task_fn(void *val)
{
  while(1)
  {
    semaphore_wait(hotplug_sem);
    switch(currentDisplayStatus)
    {
      case STM_DISPLAY_NEEDS_RESTART:
        if(!silent_hotplug) printf("HDMI TV Starting Link\n");
        if(stm_display_output_start(hHDMI, &HDMIModeLine)<0)
        {
          if(!silent_hotplug) printf("Unable to start HDMI\n");
        }
        break;
      case STM_DISPLAY_CONNECTED:
        if(!silent_hotplug) printf("HDMI TV Successfully Connected\n");
        break;
      case STM_DISPLAY_DISCONNECTED:
        if(!silent_hotplug) printf("HDMI TV Disconnected\n");
        stm_display_output_stop(hHDMI);
        break;
    }
  }
}


void usage(void)
{
    fprintf(stderr,"Usage: gam_display file_name [options]\n");
    fprintf(stderr,"\ti      - Force source as interlaced\n");
    fprintf(stderr,"\tb      - Show bottom field when source is interlaced\n");
    fprintf(stderr,"\tc      - Crop the source to {x=100, y=100, w=200, h=200} \n");
    fprintf(stderr,"\t    \n");
    fprintf(stderr,"\t    Examples: \n");
    fprintf(stderr,"\t    To display a Progressive picture: gam_display.out ftb_420_MB.gam\n");
    fprintf(stderr,"\t    To display an Interlaced picture: gam_display.out ftb_420_MB.gam i \n");

    exit(1);
}


static int fill_buffer_descriptor(stm_display_buffer_t * pBufferDesc, char *file_name, const stm_display_mode_t  *pMode, bool is_src_progressive, bool show_bottom_field, stm_rect_t *input_window)
{
    FILE *                  fp;
    GamPictureHeader_t      GamPictureHeader;
    int                     bytesread;
    char *                  fbuffer;
    unsigned int            FileSize, FrameBufferSize;
    void *                  fbufferphys;
    stm_pixel_format_t     ColorFmt;
    uint32_t                PixelDepth;
    unsigned int            Pitch;

    memset(pBufferDesc, 0, sizeof(stm_display_buffer_t) );

    fp = fopen(file_name, "rb");

    if (fp == NULL)
    {
        printf("Error! File not found!\n");
        return -1;
    }

    // Obtain file size
    fseek (fp , 0 , SEEK_END);
    FileSize = ftell (fp);
    fseek (fp , 0 , SEEK_SET);

    // Read Gam header
    bytesread = fread(&GamPictureHeader, 1, sizeof(GamPictureHeader_t), fp);
    if (bytesread != sizeof(GamPictureHeader_t) )
    {
        printf("Error while reading the GAM Header!\n");
        return -1;
    }

    /* To avoid memory crash, we should check data consistency:
    For MB pictures, we should have "FileSize = header size + Luma size + Chroma size"
    For Raster Progressive pictures, it seems to be more complex... We can find GAM Raster pictures with the following rules:
        "FileSize = header size + 2 * Luma size" (Chroma size = 0)
        "FileSize = header size + Luma size + Chroma size"
    */
    if ( (FileSize != (GamPictureHeader.LumaSize + GamPictureHeader.ChromaSize + sizeof(GamPictureHeader_t) ) ) &&
         (FileSize != (2 * GamPictureHeader.LumaSize + sizeof(GamPictureHeader_t) ) ) )
    if (bytesread != sizeof(GamPictureHeader_t) )
    {
        printf("Invalid GAM Header!\n");
        return -1;
    }

    FrameBufferSize = FileSize - sizeof(GamPictureHeader_t);

    // Allocate memory for the FrameBuffer (with alignment constraint)
    fbuffer = (char *) memalign(4096, FrameBufferSize);
    if(!fbuffer)
    {
        printf("Unable to allocate framebuffer\n");
        return -1;
    }

    memset(fbuffer, 0x00, FrameBufferSize);

    printf("Start reading GAM file...\n");

    // Read the file content
    fread(fbuffer, 1, FrameBufferSize, fp);

    printf("Read completed\n");

    cache_purge_data(fbuffer, FrameBufferSize);
    vmem_virt_to_phys(fbuffer, &fbufferphys);

    switch (GamPictureHeader.Type)
    {
        case 0x92:
            /* 0x92 = YcbCr 4:2:2 Raster */
            ColorFmt = SURF_YCBCR422R;
            PixelDepth = 16;
            Pitch = 2 * GamPictureHeader.PictureWidth;
            break;

        case 0xB0:
            /* 0xB0 = YcbCr 4:2:0 Raster Double Buffer */
            /* With 4:2:0 R2B, PictureWidth is a OR between Pitch and Width (each one on 16 bits) */
            Pitch = (GamPictureHeader.PictureWidth >> 16) & 0xFFFF;
            GamPictureHeader.PictureWidth = GamPictureHeader.PictureWidth & 0xFFFF;
            ColorFmt = SURF_YCbCr420R2B;
            PixelDepth = 8;
            break;

        case 0x94:
            /* 0x94 = OMEGA2 4:2:0 (= Macroblock) */
            ColorFmt = SURF_YCBCR420MB;
            PixelDepth = 8;
            Pitch = GamPictureHeader.PictureWidth;
            break;

        default:
            printf("Gam format not supported!\n");
            return -1;
    }


    pBufferDesc->src.primary_picture.video_buffer_addr = (uint32_t)fbufferphys;
    pBufferDesc->src.primary_picture.video_buffer_size = FrameBufferSize;
    pBufferDesc->src.primary_picture.chroma_buffer_offset = GamPictureHeader.LumaSize;
    pBufferDesc->src.primary_picture.color_fmt = ColorFmt;
    pBufferDesc->src.primary_picture.pixel_depth = PixelDepth;
    pBufferDesc->src.primary_picture.pitch = Pitch;
    pBufferDesc->src.primary_picture.width = GamPictureHeader.PictureWidth;
    pBufferDesc->src.primary_picture.height = GamPictureHeader.PictureHeight;

    pBufferDesc->src.visible_area.x = 0;
    pBufferDesc->src.visible_area.y = 0;
    pBufferDesc->src.visible_area.width = GamPictureHeader.PictureWidth;
    pBufferDesc->src.visible_area.height = GamPictureHeader.PictureHeight;

    input_window->x      = 0;
    input_window->y      = 0;
    input_window->width  = GamPictureHeader.PictureWidth;
    input_window->height = GamPictureHeader.PictureHeight;

    pBufferDesc->src.pixel_aspect_ratio.numerator = 1;
    pBufferDesc->src.pixel_aspect_ratio.denominator = 1;

    pBufferDesc->src.src_frame_rate.numerator = 1;
    pBufferDesc->src.src_frame_rate.denominator = 1;

    pBufferDesc->src.linear_center_percentage = 100;
    pBufferDesc->src.ulConstAlpha = 255;

    pBufferDesc->src.ColorKey.flags = SCKCF_NONE;
    pBufferDesc->src.ColorKey.enable = '\0';
    pBufferDesc->src.ColorKey.format = SCKCVF_RGB;
    pBufferDesc->src.ColorKey.r_info = SCKCCM_DISABLED;
    pBufferDesc->src.ColorKey.g_info = SCKCCM_DISABLED;
    pBufferDesc->src.ColorKey.b_info = SCKCCM_DISABLED;
    pBufferDesc->src.ColorKey.minval = 0;
    pBufferDesc->src.ColorKey.maxval = 0;

    pBufferDesc->src.clut_bus_address = 0;
    pBufferDesc->src.post_process_luma_type = 0;
    pBufferDesc->src.post_process_chroma_type = 0;
    if (is_src_progressive)
    {
        pBufferDesc->src.flags = 0;
    }
    else
    {
        /*
         * As we are queuing a single field from a single buffer; this is
         * basically like a "pause" trick mode. We need to indicate that we
         * prefer to use an interpolated version of the "other" field for
         * an interlaced display rather than the "real data" for that field in
         * the buffer using STM_BUFFER_SRC_INTERPOLATE_FIELDS.
         *
         * When the video pipeline has a de-interlacer which is
         * active this doesn't actually make a difference, we always use the
         * de-interlaced frame to generate both output fields. But when the
         * de-interlacer is switched off or doesn't exist this determines what
         * buffer data we will really use.
         */
        pBufferDesc->src.flags = STM_BUFFER_SRC_INTERLACED | STM_BUFFER_SRC_INTERPOLATE_FIELDS;
        pBufferDesc->src.flags |= show_bottom_field?STM_BUFFER_SRC_BOTTOM_FIELD_ONLY:STM_BUFFER_SRC_TOP_FIELD_ONLY;
    }

    pBufferDesc->info.ulFlags = STM_BUFFER_PRESENTATION_PERSISTENT;

    return 0;
}


static stm_plane_feature_t get_associated_feature(stm_display_plane_tuning_data_control_t tuning_data_control)
{
    stm_plane_feature_t feature;

    switch(tuning_data_control)
    {
        case PLANE_CTRL_VIDEO_IQI_TUNING_DATA:
            feature=PLANE_FEAT_VIDEO_IQI;
            break;

        case PLANE_CTRL_VIDEO_MADI_TUNING_DATA:
            feature=PLANE_FEAT_VIDEO_MADI;
            break;

        case PLANE_CTRL_VIDEO_FMD_TUNING_DATA:
            feature=PLANE_FEAT_VIDEO_FMD;
            break;

        case PLANE_CTRL_VIDEO_XVP_TUNING_DATA:
            feature=PLANE_FEAT_VIDEO_XVP;
            break;

        default:
            feature = PLANE_FEAT_VIDEO_ACC; // Dummy feature not implemented yet
    }
    return feature;
}


static uint32_t get_control_payload_size(stm_display_plane_tuning_data_control_t tuning_data_control, uint32_t revision)
{
    uint32_t size;

    switch(tuning_data_control)
    {
        case PLANE_CTRL_VIDEO_IQI_TUNING_DATA:
            if(revision == 1)
                size=sizeof(stm_iqi_tuning_data_t);
            else
                size=0;
            break;

        case PLANE_CTRL_VIDEO_MADI_TUNING_DATA:
            if(revision == 1)
                size=sizeof(stm_madi_tuning_data_t);
            else
                size=0;
            break;

        case PLANE_CTRL_VIDEO_FMD_TUNING_DATA:
            if(revision == 1)
                size=sizeof(stm_fmd_tuning_data_t);
            else
                size=0;
            break;

        case PLANE_CTRL_VIDEO_XVP_TUNING_DATA:
            if(revision == 1)
                size=sizeof(stm_xvp_tuning_data_t);
            else
                size=0;
            break;

        default:
            size=0;
    }
    return size;
}


static stm_tuning_data_t * alloc_and_fill_tuning_data (stm_display_plane_h p, stm_display_plane_tuning_data_control_t tuning_data_control, void *data)
{
    uint32_t            total_size, revision;
    stm_tuning_data_t  *tuning_data =NULL;
    uint32_t            data_size;

    if(stm_display_plane_get_tuning_data_revision(p, tuning_data_control, &revision) == 0)
    {
        data_size = get_control_payload_size(tuning_data_control, revision);

        total_size = sizeof(stm_tuning_data_t)+ data_size;
        tuning_data = (stm_tuning_data_t *)malloc(total_size);

        if (tuning_data == 0)
        {
            printf("Failed to allocate the tuning_data!\n");
            return 0;
        }

        tuning_data->size = total_size;
        tuning_data->feature = get_associated_feature(tuning_data_control);
        tuning_data->revision = revision;
        /* Now write data at the payload position located just after the structure */
        if(data)
            memcpy(GET_PAYLOAD_POINTER(tuning_data), data,  data_size);
    }

    return tuning_data;
}





static int set_tuning_data_control_value(stm_display_plane_h p, stm_display_plane_tuning_data_control_t ctrl, void *newVal)
{
    int ret= -1;
    stm_tuning_data_t *ctrltuningdata=NULL;

    ctrltuningdata = alloc_and_fill_tuning_data(p, ctrl, newVal);
    if(ctrltuningdata)
    {
      ret = stm_display_plane_set_tuning_data_control(p, ctrl, ctrltuningdata);
      free(ctrltuningdata);
    }

    return ret;
}



int main(int argc, char **argv)
{
struct display_update_s *pDispUpdate = 0;
stm_display_device_h    hDev      = 0;
stm_display_output_h    hOutput   = 0;
stm_display_plane_h     hPlane    = 0;
stm_display_source_h    hSource   = 0;
stm_display_source_queue_h queue_interface=0;
stm_display_buffer_t    buffer_setup;
interrupt_t            *hdmi_interrupt=0;
int                     err;
int                     seconds;
int                     clip;
int                     rgb;
int                     dvi;
int                     cvbstrapfilter;
int                     device;
int                     error_ppm;
osclock_t               lasttime;
char                    file_name[50];
bool                    is_src_progressive = true;
bool                    show_bottom_field = false;
bool                    crop_src = false;
stm_display_source_interface_params_t iface_params;
stm_rect_t              input_window, output_window;

  kernel_initialize(NULL);
  kernel_start();
  kernel_timeslice(OS21_TRUE);

  hotplug_sem        = semaphore_create_fifo(0);

  if(argc<2)
    usage();

    // Skip the first argument which contain the exe name
  argc--;
  argv++;

  seconds = 600;
  rgb     = 0;
  dvi     = 0;
  cvbstrapfilter = 0;
  clip    = 0;
  device  = 0;
  error_ppm = 0;


    // The next argument contains the name of the GAM file to load
    if (argv[0] != NULL)
    {
        strncpy(file_name, argv[0], sizeof(file_name) );
        argc--;
        argv++;
    }

    // The following arguments contain the options
    while(argc>0)
    {
        switch(**argv)
        {
            case 'i':
            {
                printf("Force source as interlaced\n");
                is_src_progressive = false;
                break;
            }
            case 'b':
            {
                printf("Show the bottom field of an interlaced source\n");
                show_bottom_field = true;
                break;
            }

            case 'c':
            {
                printf("Crop the source to {x=100, y=100, w=200, h=200}\n");
                crop_src = true;
                break;
            }

        }

        argc--;
        argv++;
    }

  /****************************************/

  /* OS abstraction is now moved out of stm_display_open_device() */
  if(vibe_os_init()<0)
    return -1;

  if(vibe_debug_init()<0)
    return -1;

  if(stm_display_open_device(device,&hDev)!=0)
  {
    fprintf(stderr,"Display device not found\n");
    exit(1);
  }

  vsync_interrupt = 0;

  setup_soc();

  if(!vsync_interrupt)
  {
    printf("Cannot find VSYNC interrupt handler\n");
    return 1;
  }

  hOutput = get_analog_output(hDev, TRUE);
  if(!hOutput)
  {
    printf("Unable to get output\n");
    return 1;
  }

  if((pDispUpdate = start_display_update_thread(hDev, hOutput)) == 0)
  {
    printf("Unable to start display update thread\n");
    return 1;
  }

  err  = interrupt_install(vsync_interrupt, vsync_isr, pDispUpdate);
  err += interrupt_enable(vsync_interrupt);
  if(err>0)
  {
    printf("Unable to install and enable VSYNC interrupt\n");
    return 1;
  }

  setup_analogue_voltages(hOutput);
  stm_display_output_set_control(hOutput, OUTPUT_CTRL_CVBS_TRAP_FILTER, cvbstrapfilter);
  stm_display_output_set_control(hOutput, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

  stm_display_output_set_clock_reference(hOutput, STM_CLOCK_REF_30MHZ, error_ppm);

  hDVO = get_dvo_output(hDev);

  hHDMI = get_hdmi_output(hDev);

  if(hHDMI)
  {
    /*
     * Now we have a HDMI output pointer to handle hotplug interrupts,
     * we can enable the interrupt handlers.
     */
    hdmi_interrupt = get_hdmi_interrupt();
    if(hdmi_interrupt)
    {
      err  = interrupt_install(hdmi_interrupt, hdmi_isr, hHDMI);
      err += interrupt_enable(hdmi_interrupt);
    }

    if(err>0)
    {
      printf("Unable to install and enable hdmi interrupts\n");
      return 1;
    }

    stm_display_output_set_control(hHDMI, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, clip?STM_SIGNAL_VIDEO_RANGE:STM_SIGNAL_FILTER_SAV_EAV);

  }
  else
  {
    printf("No HDMI output available\n");
  }

  if(stm_display_output_get_display_mode(hOutput, MODE, &ModeLine)<0)
  {
    printf("Unable to use requested display mode\n");
    return 1;
  }

  if(hHDMI && (STANDARD & STM_OUTPUT_STD_SD_MASK))
  {
    hdmiflags |= STM_MODE_FLAGS_HDMI_PIXELREP_2X;
    if(stm_display_output_find_display_mode(hOutput,
                                       ModeLine.mode_params.active_area_width*2,
                                       ModeLine.mode_params.active_area_height,
                                       ModeLine.mode_timing.lines_per_frame,
                                       ModeLine.mode_timing.pixels_per_line*2,
                                       ModeLine.mode_timing.pixel_clock_freq*2,
                                       ModeLine.mode_params.scan_type,
                                       &HDMIModeLine)<0)
    {
      printf("Unable to find HDMI pixel doubled mode for SD mode\n");
      return 1;
    }
  }
  else
  {
    HDMIModeLine = ModeLine;
  }

  HDMIModeLine.mode_params.output_standards = hdmistandard;
  HDMIModeLine.mode_params.flags = hdmiflags;

  hPlane = get_and_connect_video_plane_to_output(hDev, hOutput);
  if(!hPlane)
  {
    printf("Unable to get graphics plane\n");
    return 1;
  }

  /*
   * If the plane has a de-interlacer, put it into median mode for this test,
   * the default 3D motion mode is not very useful if we are only queuing
   * one field.
   */
#if 1
  {
    stm_madi_tuning_data_t dei_data;
    dei_data.config  = PCDEIC_MEDIAN;

    err = set_tuning_data_control_value(hPlane, PLANE_CTRL_VIDEO_MADI_TUNING_DATA, &dei_data);
  }
#endif


  /*
   * Get a Source and try to connect it on created plane
   */
  hSource = get_and_connect_source_to_plane(hDev ,hPlane);
  if (!hSource)
  {
    printf("Unable to get a source for plane %p\n", hPlane);
    return 1;
  }

  /*
   * Lock the QueueBuffer Interface for our exclusive queue_buffer usage
   */
  iface_params.interface_type = STM_SOURCE_QUEUE_IFACE;
  if(stm_display_source_get_interface(hSource, iface_params, (void**)&queue_interface) != 0)
  {
    printf("No interface registred\n");
    return 1;
  }

  if(stm_display_source_queue_lock(queue_interface, false)<0)
  {
    printf("Cannot lock queue\n");
    return 1;
  }

  if(fill_buffer_descriptor(&buffer_setup, file_name, &ModeLine, is_src_progressive, show_bottom_field, &input_window) < 0)
  {
    printf("Unable to fill buffer_setup\n");
    return 1;
  }

  if (crop_src)
  {
    /* Try to read only a small part of the source */
    input_window.x = 100;
    input_window.y = 100;
    input_window.width = 200;
    input_window.height = 200;
  }

  printf("Clock is running at %ld ticks per second\n",(long)time_ticks_per_sec());

  uint32_t format = 0;
  if(STANDARD & STM_OUTPUT_STD_SD_MASK)
  {
    format = (STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS);
    format |= rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
  }
  else
  {
    format = rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
  }

  stm_display_output_set_control(hOutput, OUTPUT_CTRL_VIDEO_OUT_SELECT, format);

  if(hHDMI)
  {
    uint32_t format = 0;

    format |= rgb?STM_VIDEO_OUT_RGB:STM_VIDEO_OUT_YUV;
    format |= dvi?STM_VIDEO_OUT_DVI:STM_VIDEO_OUT_HDMI;
    format |= STM_VIDEO_OUT_24BIT;
    stm_display_output_set_control(hHDMI, OUTPUT_CTRL_VIDEO_OUT_SELECT, format);
  }


  ModeLine.mode_params.output_standards = STANDARD;
  ModeLine.mode_params.flags = STM_MODE_FLAGS_NONE;

  if(stm_display_output_start(hOutput, &ModeLine)<0)
  {
    printf("Unable to start display\n");
    return 1;
  }

  if(hDVO)
  {
    printf("Info: Attempting to start DVO\n");
    if(stm_display_output_start(hDVO, &ModeLine)<0)
    {
      printf("Info: Unable to start DVO\n");
    }
  }

  if(stm_display_plane_set_compound_control(hPlane, PLANE_CTRL_INPUT_WINDOW_VALUE, &input_window)<0)
  {
    printf("Unable to set Input window rect\n");
    return 1;
  }
  if(stm_display_plane_set_control(hPlane, PLANE_CTRL_INPUT_WINDOW_MODE, MANUAL_MODE)<0)
  {
    printf("Unable to set Input window mode\n");
    return 1;
  }

  if(stm_display_plane_set_control(hPlane, PLANE_CTRL_OUTPUT_WINDOW_MODE, AUTO_MODE)<0)
  {
    printf("Unable to set Output window mode\n");
    return 1;
  }


  lasttime = time_now(); // VTG Start time (approx)

  if(stm_display_source_queue_buffer(queue_interface,&buffer_setup)<0)
  {
    printf("Unable to queue framebuffer for display on graphics source\n");
    return 1;
  }

  task_create(hotplug_task_fn, 0, OS21_DEF_MIN_STACK_SIZE, MIN_USER_PRIORITY, "hotplug", 0);

  if(seconds == 0)
  {
    task_delay(time_ticks_per_sec()*5);
    task_priority_set(NULL,MIN_USER_PRIORITY);
    silent_hotplug = 1;
    err = get_yesno();
  }
  else
  {
    task_delay(time_ticks_per_sec()*seconds);
  }

  stm_display_plane_disconnect_from_output(hPlane, hOutput);
  stm_display_output_stop(hOutput);

  interrupt_disable(vsync_interrupt);
  interrupt_disable(hdmi_interrupt);

  if(hSource)
  {
    // Unlock this queue for our exclusive usage
    stm_display_source_queue_unlock(queue_interface);
    stm_display_source_queue_release(queue_interface);

    stm_display_plane_disconnect_from_source(hPlane, hSource);
    stm_display_source_close(hSource);
  }

  stm_display_plane_close(hPlane);

  stm_display_output_close(hDVO);
  stm_display_output_close(hHDMI);
  stm_display_output_close(hOutput);

  stm_display_device_close(hDev);

  /* vibe_os destruction is now moved out of stm_display_device_close() */
  vibe_debug_release();
  vibe_os_release();

  return err;
}
