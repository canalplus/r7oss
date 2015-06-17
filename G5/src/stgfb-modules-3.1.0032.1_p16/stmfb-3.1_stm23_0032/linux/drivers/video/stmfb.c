/***********************************************************************
 *
 * File: linux/drivers/video/stmfb.c
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>

#if defined(CONFIG_BPA2)
#include <linux/bpa2.h>
#else
#error Kernel must have the BPA2 memory allocator configured
#endif

#include <asm/irq.h>
#include <asm/semaphore.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
#error This driver version is for ST LDDE2.2 and kernel version >= 2.6.17
#endif

/*
 * Define the maximum number of framebuffers/display pipelines the
 * driver can support.
 */
#define NR_FRAMEBUFFERS 4

/* See stmfbinfo.h for the device structure */
static struct stmfb_info stmfb_info[NR_FRAMEBUFFERS];

static char *display0;
static char *display1;
static char *display2;
static char *display3;

module_param(display0, charp, 0444);
MODULE_PARM_DESC(display0, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

module_param(display1, charp, 0444);
MODULE_PARM_DESC(display1, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

module_param(display2, charp, 0444);
MODULE_PARM_DESC(display2, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

module_param(display3, charp, 0444);
MODULE_PARM_DESC(display3, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");


static int stmfb_probe_get_blitter(struct stmfb_info *i,
                                   const struct stmcore_display_pipeline_data * const pd);

/************************************************************************
 *  Initialization and cleanup code
 */
static int stmfb_parse_module_parameters(struct stmfb_info *i, int display)
{
  char *paramstring    = NULL;
  char *copy           = NULL;
  char *mode           = NULL;
  char *memsize        = NULL;
  char *auxmemsize     = NULL;
  char *tvstandard     = NULL;
  char *componentvideo = NULL;
  char *hdmivideo      = NULL;
  char *blit_api_ver;
  unsigned long fbsize = 0;
  int stride;


  /*
   * Note: slightly messy due to module parameter limitations and not wanting to
   * use parameter arrays.
   */
  switch(display)
  {
    case 0:
      paramstring = display0;
      break;
    case 1:
      paramstring = display1;
      break;
    case 2:
      paramstring = display2;
      break;
    case 3:
      paramstring = display3;
      break;
    default:
      break;
  }

  printk("stmfb: fb%d parameters = \"%s\"\n",display,(paramstring==NULL)?"<NULL>":paramstring);

  if(paramstring == NULL || *paramstring == '\0')
    return -ENODEV;

  copy = kstrdup(paramstring, GFP_KERNEL);
  if(!copy)
    return -ENOMEM;

  mode           = strsep(&copy,":,");
  memsize        = strsep(&copy,":,");
  auxmemsize     = strsep(&copy,":,");
  tvstandard     = strsep(&copy,":,");
  componentvideo = strsep(&copy,":,");
  hdmivideo      = strsep(&copy,":,");
  blit_api_ver   = strsep(&copy,":,");

  if(!mode || *mode == '\0')
  {
    DPRINTK("No mode string found\n");
    kfree(copy);
    return -ENODEV;
  }

  if(fb_find_mode(&i->info.var,&i->info,mode,i->videomodedb,ARRAY_SIZE(i->videomodedb),NULL,16) == 0)
  {
    DPRINTK("No suitable mode found (not even a default!)\n");
    kfree(copy);
    return -ENODEV;
  }

  if(memsize)
  {
    fbsize = (unsigned long)memparse(memsize,&memsize);
    DPRINTK("requested memory = %lu\n",fbsize);
  }

  stride = i->info.var.xres * (i->info.var.bits_per_pixel/8);
  i->info.var.yres_virtual  = fbsize / stride;

  if(i->info.var.yres_virtual < i->info.var.yres)
  {
    DPRINTK("Requested memory is too small for default video mode, fixing it\n");
    fbsize = i->info.var.yres * stride;
  }

  /*
   * Having determined size, set the virtual yres to equal yres for the
   * default mode.
   */
  i->info.var.yres_virtual = i->info.var.yres;

  i->ulFBSize = fbsize;

  if(auxmemsize)
  {
    i->AuxSize[0] = (unsigned long)memparse(auxmemsize,&auxmemsize);
    DPRINTK("requested %lu bytes of auxiliary memory\n",i->AuxSize[0]);
  }
  else
    i->AuxSize[0] = 0;

  i->default_sd_encoding = STM_OUTPUT_STD_NTSC_M;

  if (tvstandard)
  {
    switch(*tvstandard)
    {
      case 'N':
      case 'n':
      {
        if(!strcmp(tvstandard,"NTSC-J"))
        {
          DPRINTK("Selecting NTSC-J output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_NTSC_J;
        }
        else if(!strcmp(tvstandard,"NTSC-443"))
        {
          DPRINTK("Selecting NTSC-443 output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_NTSC_443;
        }
        else
        {
          DPRINTK("Selecting NTSC (US) output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_NTSC_M;
        }
        break;
      }
      case 'S':
      case 's':
      {
        DPRINTK("Selecting SECAM output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_SECAM;
        break;
      }
      case 'P':
      case 'p':
      {
        if(!strcmp(tvstandard,"PAL-M"))
        {
          DPRINTK("Selecting PAL-M output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_PAL_M;
        }
        else if(!strcmp(tvstandard,"PAL-N"))
        {
          DPRINTK("Selecting PAL-N output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_PAL_N;
        }
        else if(!strcmp(tvstandard,"PAL-Nc"))
        {
          DPRINTK("Selecting PAL-Nc output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_PAL_Nc;
        }
        else if(!strcmp(tvstandard, "PAL-60"))
        {
          DPRINTK("Selecting PAL-60 output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_PAL_60;
        }
        else
        {
          DPRINTK("Selecting PAL-BDGHI output\n");
          i->default_sd_encoding = STM_OUTPUT_STD_PAL_BDGHI;
        }

        break;
      }
      default:
      {
        DPRINTK("Invalid default SD output standard, falling back to NTSC\n");
      }
    }
  }

  if(i->main_config.caps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
    i->main_config.sdtv_encoding = i->default_sd_encoding;

  /*
   * Set the output format
   */
  if(componentvideo)
  {
    ULONG outputcaps;
    if(stm_display_output_get_capabilities(i->pFBMainOutput,&outputcaps)<0)
    {
      DPRINTK("Cannot get output capabilities\n");
      return -EIO;
    }

    switch(*componentvideo)
    {
      case 'C':
      case 'c':
        if(outputcaps & (STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
                         STM_OUTPUT_CAPS_SD_RGB_CVBS_YC    |
                         STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC))
        {
          i->main_config.analogue_config = STMFBIO_OUTPUT_ANALOGUE_CVBS;
          i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YC;
        }
        else
        {
          printk(KERN_WARNING "Cannot select CVBS/S-Video on display %d\n",display);
        }
        break;
      case 'R':
      case 'r':
        if(outputcaps & (STM_OUTPUT_CAPS_RGB_EXCLUSIVE |
                         STM_OUTPUT_CAPS_SD_RGB_CVBS_YC))
        {
          i->main_config.analogue_config = STMFBIO_OUTPUT_ANALOGUE_RGB;
          if(outputcaps & STM_OUTPUT_CAPS_SD_RGB_CVBS_YC)
            i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CVBS;
        }
        else
        {
          printk(KERN_WARNING "Cannot select RGB on display %d\n",display);
        }
        break;
      case 'Y':
      case 'y':
        if(outputcaps & (STM_OUTPUT_CAPS_YPrPb_EXCLUSIVE |
                         STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC))
        {
          i->main_config.analogue_config = STMFBIO_OUTPUT_ANALOGUE_YPrPb;

          if(outputcaps & STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC)
            i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CVBS;
        }
        else
        {
          printk(KERN_WARNING "Cannot select YPrPb on display %d\n",display);
        }
        break;
      default:
        printk(KERN_WARNING "Unknown analogue video type option - falling back to driver defaults\n");
        break;
    }
  }

  if(hdmivideo && i->hdmi)
  {
    switch(*hdmivideo)
    {
      case 'Y':
      case 'y':
        i->hdmi->video_type   = STM_VIDEO_OUT_YUV;
        i->main_config.hdmi_config |= STMFBIO_OUTPUT_HDMI_YUV;
        break;
      default:
        DPRINTK("Unknown hdmi video type option - falling back to RGB\n");
      case 'R':
      case 'r':
        i->hdmi->video_type   = STM_VIDEO_OUT_RGB;
        i->main_config.hdmi_config &= ~STMFBIO_OUTPUT_HDMI_YUV;
        break;
    }
  }

  i->blitter_api = 1;
  if (blit_api_ver)
  {
    if (blit_api_ver[0] >= '0' && blit_api_ver[0] <= '9'
        && blit_api_ver[1] == '\0')
      i->blitter_api = *blit_api_ver - '0';
    else
      printk(KERN_WARNING "Unknown Blitter API version specified, ignoring\n");
  }

  kfree(copy);

  return 0;
}


static void stmfb_encode_videomode(const stm_mode_line_t *mode, struct fb_videomode *vm)
{
  u64 timingtmp;

  /* TODO: this really ought to be merged into stmfb_encode_var() since the conversions
   *       absolutely must not disagree (otherwise we end up with spurious entried in
   *       the modelist).
   */

  vm->name = NULL;
  vm->refresh = mode->ModeParams.FrameRate / 1000;
  vm->xres = mode->ModeParams.ActiveAreaWidth;
  vm->yres = mode->ModeParams.ActiveAreaHeight;

  timingtmp = 1000000000000ULL;
  do_div(timingtmp, mode->TimingParams.ulPixelClock);
  vm->pixclock = (unsigned long) timingtmp;

  vm->left_margin = mode->ModeParams.ActiveAreaXStart - mode->TimingParams.HSyncPulseWidth;
  vm->right_margin = mode->TimingParams.PixelsPerLine - mode->ModeParams.ActiveAreaXStart -
                                                        mode->ModeParams.ActiveAreaWidth;
  vm->upper_margin = mode->ModeParams.FullVBIHeight - mode->TimingParams.VSyncPulseWidth;
  vm->lower_margin = mode->TimingParams.LinesByFrame - mode->ModeParams.FullVBIHeight -
                                                       mode->ModeParams.ActiveAreaHeight;
  vm->hsync_len = mode->TimingParams.HSyncPulseWidth;
  vm->vsync_len = mode->TimingParams.VSyncPulseWidth;

  /* set all the bitfields to zero and fill them in programatically */
  vm->sync = 0;
  vm->vmode = 0;
  vm->flag = 0;

  if (mode->TimingParams.HSyncPolarity)
    vm->sync|=FB_SYNC_HOR_HIGH_ACT;

  if (mode->TimingParams.VSyncPolarity)
    vm->sync|=FB_SYNC_VERT_HIGH_ACT;


  if (SCAN_I == mode->ModeParams.ScanType) {
    vm->vmode |= FB_VMODE_INTERLACED;
  } else {
    vm->vmode |= FB_VMODE_NONINTERLACED;
  }
  /* TODO: FB_VMODE_DOUBLE */

  if (mode->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] ||
      mode->ModeParams.HDMIVideoCodes[AR_INDEX_16_9]) {
    vm->flag |= FB_MODE_IS_STANDARD; /* meaning, in this case, CEA-861 */
  }
}


static void stmfb_enumerate_modes(struct stmfb_info *i)
{
  int n;
  const stm_mode_line_t *mode;
  struct fb_videomode vm;

  INIT_LIST_HEAD(&i->info.modelist);

  for (n=0; n<STVTG_TIMING_MODE_COUNT; n++) {
    if (!(mode = stm_display_output_get_display_mode(i->pFBMainOutput, n)))
      continue;

    stmfb_encode_videomode(mode, &vm);

    /*
     * Create a state mode database of just the supported modes for module
     * parameter parsing.
     */
    i->videomodedb[n] = vm;

    /*
     * Add to the framework's mode list for sysfs
     */
    fb_add_videomode(&vm, &i->info.modelist);
  }

  /* the modelist is automatically destroyed by unregister_framebuffer() so
   * we only have to worry about leaks on error paths before we are successfully
   * registered.
   */
}

/*
 *
 */
static void stmfb_destroy_auxmem(struct stmfb_info *i)
{
  unsigned int idx;

  for (idx = 0; idx < ARRAY_SIZE (i->AuxPart); ++idx)
  {
    if (i->AuxPart[idx])
    {
      bpa2_free_pages (i->AuxPart[idx], i->AuxBase[idx]);
      i->AuxPart[idx] = NULL;
      i->AuxBase[idx] = 0;
      i->AuxSize[idx] = 0;
    }
  }
}

/*
 * stmfb_destroyfb
 * Unregister a framebuffer, clear it off the display and release
 * its resources.
 */
static int __exit stmfb_destroyfb(struct stmfb_info *i)
{
  DPRINTK("\n");

  stmfb_cleanup_class_device(i);
  unregister_framebuffer(&i->info);

  if(i->pFBPlane && i->pFBMainOutput)
  {
    stm_display_plane_unlock(i->pFBPlane);
    stm_display_plane_disconnect_from_output(i->pFBPlane, i->pFBMainOutput);
    stm_display_output_stop(i->pFBMainOutput);

    if(i->hdmi)
    {
      /*
       * Force the HDMI manager state as we do on a mode change to force a
       * hotplug event if the display is started again, i.e. the framebuffer
       * module gets reloaded.
       */
      i->hdmi->status_changed = 0;
      i->hdmi->status         = STM_DISPLAY_DISCONNECTED;
    }

    stm_display_plane_release(i->pFBPlane);

    i->pFBPlane  = NULL;
  }

  if(i->ulPFBBase)
  {
    iounmap(i->info.screen_base);
    i->info.screen_base = NULL;

    bpa2_free_pages (i->FBPart, i->ulPFBBase);
    i->ulPFBBase = 0;
    i->FBPart = NULL;
  }

  fb_dealloc_cmap (&i->info.cmap);

  stmfb_destroy_auxmem (i);

  return 0;
}

/*
 *  stmfb_createfb
 *  Create framebuffer related state and register with upper layers
 */
static int stmfb_createfb(struct stmfb_info *i, int display, const char *name)
{
  int ret;
  unsigned long nPages;
  int fb_registered = 0;
  const struct stmcore_display_pipeline_data * const pd =
    *((struct stmcore_display_pipeline_data **) i->platformDevice->dev.platform_data);

  DPRINTK("\n");

  /*
   * Copy all available display modes into the modelist, before we parse
   * the module parameters to get the default mode.
   */
  stmfb_enumerate_modes(i);

  /*
   * Need to set the framebuffer operation table and get hold of the display
   * plane before parsing the module parameters so fb_find_mode can work.
   */
  i->info.fbops = &stmfb_ops;

  if((ret = stmfb_parse_module_parameters(i, display)) < 0)
    goto failed0;

  /* can create blitter only after module parameters have been parsed */
  if((ret = stmfb_probe_get_blitter(i,pd))<0)
    goto failed0;

  nPages = (i->ulFBSize + PAGE_SIZE - 1) / PAGE_SIZE;

  i->FBPart = bpa2_find_part ("bigphysarea");
  i->ulPFBBase = i->FBPart ? bpa2_alloc_pages (i->FBPart, nPages, 0, GFP_KERNEL)
                           : 0;
  if (!i->ulPFBBase)
  {
    printk(KERN_WARNING "Failed to allocate fb%d memory, requested size = %lu\n",display, i->ulFBSize);
    ret = -ENOMEM;
    goto failed0;
  }


  /* try to allocate memory from BPA2 as additional memory for graphics
     operations. Basically, this is not vital, but driver loading will still
     fail if an auxsize has been specified in the module parameters which can
     not be satisfied, either because no BPA2 partition 'gfx-memory' exists,
     or if it's not large enough for the given auxsize. */
  /* Please note that due to hardware limitations, this can actually be a bit
     complex (2 & 3):
     1) we look for partitions labelled 'gfx-memory-[0...x]', each of which
        should be 64MB in size and be aligned to a 64MB bank. This makes the
        process quite easy for us.
     2) Failing that, we try to use a partition labelled 'gfx-memory'. Now we
        have to make sure ourselves that each allocation does not cross a 64MB
        bank boundary.
     3) Failing that, too, or in case the 'gfx-memory' partition not being
        large enough to accomodate for the amount of gfx memory requested in
        the module options, we will do the same but this time use the
        'bigphysarea' partition. */
  /* So, either one can configure several 'gfx-memory-%d' partitions
     (preferred, to have maximum control over placement of gfx memory), just
     use one large 'gfx-memory' partition, or configure nothing at all and be
     limited to 'bigphysarea' memory.
     The combined memory of all the allocations will be made available to
     DirectFB. */
  /* FIXME: this code is way too complex! */
#define MEMORY64MB_SIZE      (1<<26)
  if (i->AuxSize[0])
  {
    /* aux memory was requested */
    unsigned int       idx;        /* index into i->AuxPart[] */
    /* order of partnames is important here! */
    static const char *partnames[] = { "gfx-memory-%d", "gfx-memory", "bigphysarea" };
    unsigned int       partnameidx /* index into partnames[] */,
                       partidx     /* index in case of 'gfx-memory-%d' */;
    char               partname[14]; /* real name in case of 'gfx-memory-%d' */
    struct bpa2_part  *part;
    unsigned long      still_needed = i->AuxSize[0];

    unsigned long      this_alloc;
    int                alignment;

    partidx = 0;
    /* find a partition suitable for us, in the preferred order, as outlined
       above */
    for (partnameidx = 0; partnameidx < ARRAY_SIZE (partnames); ++partnameidx)
    {
      sprintf (partname, partnames[partnameidx], 0);
      part = bpa2_find_part (partname);
      if (part)
        break;
    }

    if (!part)
    {
      printk (KERN_ERR "no BPA2 partitions found for auxmem!\n");
      goto failed_bigphys;
    }

    idx = 0;

restart:
    if (still_needed >= MEMORY64MB_SIZE)
    {
      /* we first try to satisfy from a 64MB aligned region */
      this_alloc = MEMORY64MB_SIZE;
      /* don't request a specific alignment if individual 'gfx-memory-%d'
         partitions are configured in the kernel */
      alignment  = (partnameidx == 0) ? 0 : MEMORY64MB_SIZE / PAGE_SIZE;
    }
    else
    {
      /* if requested size is < 64MB, we are optimistic and hope it will
         fit into one 64MB bank without alignment restrictions. This
         doesn't mean we will be happy with a region crossing that
         boundary, i.e. we will still make sure all restrictions are met.
         We are really being just optimistic here. */
      this_alloc = still_needed;
      alignment  = 0;
    }

    while (part && idx < ARRAY_SIZE (i->AuxPart) && still_needed)
    {
      int           this_pages;
      unsigned long base;

      printk (KERN_INFO "trying to alloc %lu bytes (align: %.4x, still needed:"
                        " %lu) from '%s' for GFX%d auxmem\n",
              this_alloc, alignment, still_needed, partname, display);

      this_pages = (this_alloc + PAGE_SIZE - 1) / PAGE_SIZE;
      base = bpa2_alloc_pages (part, this_pages, alignment, GFP_KERNEL);
      if (base)
      {
        /* make sure it doesn't cross a 64MB boundary. At some point we will
           be using the new BPA2 allocator API... */
        if ((base & ~(MEMORY64MB_SIZE - 1)) != ((base + this_alloc - 1) & ~(MEMORY64MB_SIZE - 1)))
        {
          unsigned long topAddress;

          printk ("%s: %8.lx + %lu (%d pages) crosses a 64MB boundary, retrying!\n",
                  __FUNCTION__, base, this_alloc, this_pages);

          /* free and try a new reservation with different attributes,
             hoping nobody requests bpa2 memory in between... */
          bpa2_free_pages (part, base);

          if (partnameidx == 0)
          {
            /* if we have 'gfx-memory-%d' partitions, try the next one */
            /* Getting here doesn't neccessarily mean there is an error in
               the BPA2 partition definition. This can happen if e.g. two
               framebuffers are configured and the auxmem size requested is
               larger but not a multiple of 64MB (since we are optimistic and
               try to re-use partially used partitions), so don't be tempted
               to put a WARN_ON() here! */
            sprintf (partname, partnames[partnameidx], ++partidx);
            part = bpa2_find_part (partname);
            continue;
          }

          if (still_needed == this_alloc && alignment == 0)
          {
            /* this can only happen on the last chunk of memory needed. So
               we first try to fit it into its own region.
               I.e. we first try to put the last chunk into a partly used
               bank. If we succeed, but the chunk now spans a boundary
               (which gets us here), we try to put it into its own
               bank. If that fails, too, we will come back again (this
               time with alignment == 1) and split the chunk into two. */
            alignment = MEMORY64MB_SIZE / PAGE_SIZE;
            continue;
          }

          /* standard case, allocate up to the end of current 64MB bank,
             effectively splitting the current chunk into two. */
#define _ALIGN_UP(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
          topAddress = _ALIGN_UP (base, MEMORY64MB_SIZE);
          this_alloc = topAddress - base;
          this_pages = (this_alloc + PAGE_SIZE - 1) / PAGE_SIZE;

          /* alignment should be 0 or 1 here */
          WARN_ON (alignment != 0
                   && alignment != 1);
          base = bpa2_alloc_pages (part, this_pages, alignment, GFP_KERNEL);
          if (!base)
          {
            /* shouldn't happen here, since we just moments ago suceeded in
               allocating even more memory than now, so we don't know what
               to do and kindof panic ...*/
            break;
          }
        }

        /* we now have found a more or less nice place to chill. */
        i->AuxPart[idx] = part;
        i->AuxBase[idx] = base;
        i->AuxSize[idx] = this_alloc;
        ++idx;

        still_needed -= this_alloc;

        printk (KERN_INFO "success: base: %.8lx, still needed: %lu\n",
                base, still_needed);

        if (still_needed >= MEMORY64MB_SIZE)
        {
          /* we first try to satisfy from a 64MB aligned region */
          this_alloc = MEMORY64MB_SIZE;
          alignment  = (partnameidx == 0) ? 0 : MEMORY64MB_SIZE / PAGE_SIZE;
        }
        else
        {
          /* if requested size is < 64MB we are on the last chunk of memory
             blocks. We are otimistic and hope it will fit into a possibly
             already partly used 64MB bank without alignment restrictions,
             i.e. we hope it will not span a 64MB boundary. If that's not
             the case, we will again use an alignment of 64MB, so as to
             reduce scattering of the auxmem chunks. */
          this_alloc = still_needed;
          alignment  = 0;

          /* if using 'gfx-memory-%d' partitions, try to share the last
             chunk with another chunk (in a possibly partly used
             partition). */
          if (partnameidx == 0)
            partidx = -1;
        }

        if (partnameidx == 0)
        {
          /* in case we are using 'gfx-memory-%d' as partition name, make
             sure we advance to the next partition */
          sprintf (partname, partnames[partnameidx], ++partidx);
          part = bpa2_find_part (partname);
        }
      }
      else
      {
        if (alignment > 1)
        {
          if (still_needed == this_alloc)
            /* make sure not to get into an endless loop which would be
               switching around the alignment all the time. */
            alignment = 1;
          else
            /* try again with different alignment rules */
            alignment = 0;
        }
        else
        {
          /* failed: couldn't allocate any memory at all, even without any
             alignment restrictions.
             1) In case there are 'gfx-memory-%d' partitions, we advance
                to the next one.
             2) In case we had been using 'gfx-memory' as partition name,
                we now try 'bigphysarea'*/
          if (partnameidx == 0)
          {
            /* use next 'gfx-memory-%d' partition */
            BUG_ON (strcmp (partnames[partnameidx], "gfx-memory-%d"));
            sprintf (partname, partnames[partnameidx], ++partidx);
            part = bpa2_find_part (partname);
          }
          else if (partnameidx == 1)
          {
            /* advance from 'gfx-memory' to 'bigphysarea' partition name */
            BUG_ON (strcmp (partnames[partnameidx], "gfx-memory"));
            sprintf (partname, partnames[++partnameidx], 0);
            BUG_ON (strcmp (partnames[partnameidx], "bigphysarea"));
            part = bpa2_find_part (partname);
            goto restart;
          }
          else
            /* that's it, there are no partitions left to check for auxmem */
            break;
        }
      }
    }

    if (still_needed)
    {
      /* not enough chill space configured for BPA2 */
      printk (KERN_WARNING "Failed to allocate enough fb%d auxmem "
                           "(still need %lu bytes)\n",
              display, still_needed);
      ret = -ENOMEM;
      goto failed_auxmem;
    }
  }

  if(stm_display_plane_connect_to_output(i->pFBPlane, i->pFBMainOutput) < 0)
  {
    printk(KERN_ERR "fb%d display cannot be connected to display output pipeline, this is very bad!\n",display);
    ret = -EIO;
    goto failed_auxmem;
  }

  if(stm_display_plane_lock(i->pFBPlane) < 0)
  {
    printk(KERN_WARNING "fb%d display plane is may already be in use by another driver\n",display);
    ret = -EBUSY;
    goto failed_auxmem;
  }

  i->main_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;
  if(stmfb_set_output_configuration(&i->main_config,i)<0)
  {
    printk(KERN_WARNING "fb%d main output configuration is unsupported\n",display);
    ret = -EINVAL;
    goto failed_auxmem;
  }

  /*
   * Get the framebuffer layer default extended var state and capabilities
   */
  i->current_var_ex.layerid = 0;

  if((ret = stmfb_encode_var_ex(&i->current_var_ex, i)) < 0)
  {
    printk(KERN_WARNING "fb%d failed to get display plane's extended capabilities\n",display);
    goto failed_auxmem;
  }

  /* Setup the framebuffer info for registration */
  i->info.screen_base = ioremap_nocache(i->ulPFBBase,i->ulFBSize);
  i->info.flags       = FBINFO_DEFAULT |
                        FBINFO_PARTIAL_PAN_OK |
                        FBINFO_HWACCEL_YPAN;

  strcpy(i->info.fix.id, name);	 /* identification string */

  i->info.fix.smem_start  = i->ulPFBBase;                /* Start of frame buffer mem (physical address)               */
  i->info.fix.smem_len    = i->ulFBSize;                 /* Length of frame buffer mem as the device sees it           */
  i->info.fix.mmio_start  = pd->mmio;                    /* memory mapped register access     */
  i->info.fix.mmio_len    = pd->mmio_len;                /* Length of Memory Mapped I/O       */
  i->info.fix.type        = FB_TYPE_PACKED_PIXELS;       /* see FB_TYPE_*                     */
  i->info.fix.type_aux    = 0;                           /* Interleave for interleaved Planes */
  i->info.fix.xpanstep    = 0;                           /* zero if no hardware panning       */
  i->info.fix.ypanstep    = 1;                           /* zero if no hardware panning       */
  i->info.fix.ywrapstep   = 0;                           /* zero if no hardware ywrap         */

  i->info.pseudo_palette = &i->pseudo_palette;

  if(fb_alloc_cmap(&i->info.cmap, 256, 1)<0)
  {
    printk(KERN_ERR "fb%d unable to allocate colour map\n",display);
    ret = -ENOMEM;
    goto failed_ioremap;
  }

  if(i->info.cmap.len != 256)
  {
    printk(KERN_ERR "fb%d WTF colour map length is wrong????\n",display);
  }

  if (register_framebuffer(&i->info) < 0)
  {
    printk(KERN_ERR "fb%d register =_framebuffer failed!\n",display);
    ret = -ENODEV;
    goto failed_cmap;
  }

  /*
   * If there was no console activated on the registered framebuffer, we have
   * to force the display mode be updated. This sequence is cribbed from the
   * matroxfb driver.
   */
  if(!i->current_videomode_valid)
  {
    i->info.var.activate |= FB_ACTIVATE_FORCE;
    if((ret = fb_set_var(&i->info, &i->info.var))<0)
    {
      printk(KERN_WARNING "fb%d failed to set default display mode, display pipeline may already be in use\n",display);
      goto failed_register;
    }
  }

  stmfb_init_class_device(i);

  DPRINTK("out\n");

  return 0;

failed_register:
  fb_registered = 1;
  unregister_framebuffer (&i->info);

failed_cmap:
  fb_dealloc_cmap (&i->info.cmap);

failed_ioremap:
  iounmap (i->info.screen_base);
  i->info.screen_base = NULL;

failed_auxmem:
  stmfb_destroy_auxmem (i);

failed_bigphys:
  bpa2_free_pages (i->FBPart, i->ulPFBBase);
  i->ulPFBBase = 0;
  i->FBPart = NULL;

failed0:
  if (!fb_registered)
    /* unregister_framebuffer() does fb_destroy_modelist() for us, so don't
       destroy it twice! */
    fb_destroy_modelist (&i->info.modelist);

  return ret;
}


static void stmfb_cleanup_info(struct stmfb_info *i)
{
  if(i->pFBMainOutput)
  {
    stm_display_output_release(i->pFBMainOutput);
    i->pFBMainOutput = NULL;
  }

  if(i->pFBDVO)
  {
    stm_display_output_release(i->pFBDVO);
    i->pFBDVO = NULL;
  }

  if(i->pBlitterCLUT)
  {
    dma_free_coherent(&i->platformDevice->dev, sizeof(unsigned long)*256, i->pBlitterCLUT, i->dmaBlitterCLUT);
    i->pBlitterCLUT = NULL;
  }

  if(i->pFBCLUT)
  {
    dma_free_coherent(&i->platformDevice->dev, sizeof(unsigned long)*256, i->pFBCLUT, i->dmaFBCLUT);
    i->pFBCLUT = NULL;
  }

  i->hdmi           = NULL; /* HDMI data is owned by the platform device */
  i->pBlitter       = NULL; /* Blitter is freed globally at the moment */
  i->platformDevice = NULL;
}


static int stmfb_probe_get_outputs(struct stmfb_info *i,struct stmcore_display_pipeline_data *pd)
{
  i->pFBMainOutput = stm_display_get_output(pd->device, pd->main_output_id);
  if(!i->pFBMainOutput)
    return -ENODEV;

  i->main_config.outputid = STMFBIO_OUTPUTID_MAIN;
  stmfb_initialise_output_config(i->pFBMainOutput,&i->main_config);

  if(pd->dvo_output_id != -1)
  {
    i->pFBDVO = stm_display_get_output(pd->device, pd->dvo_output_id);
    i->main_config.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
    i->main_config.dvo_config = (STMFBIO_OUTPUT_DVO_CLIP_VIDEORANGE |
                                 STMFBIO_OUTPUT_DVO_YUV_444_16BIT   |
                                 STMFBIO_OUTPUT_DVO_DISABLED);

    stm_display_output_set_control(i->pFBDVO, STM_CTRL_SIGNAL_RANGE, STM_SIGNAL_VIDEO_RANGE);
  }

  i->hdmi = pd->hdmi_data;
  if(i->hdmi)
  {
    i->main_config.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
    i->main_config.hdmi_config = (i->hdmi->disable==0)?STMFBIO_OUTPUT_HDMI_ENABLED:STMFBIO_OUTPUT_HDMI_DISABLED;
    i->main_config.hdmi_config |= (STMFBIO_OUTPUT_HDMI_RGB | STMFBIO_OUTPUT_HDMI_CLIP_VIDEORANGE);

    if(mutex_lock_interruptible(&i->hdmi->lock))
      return -EINTR;

    stm_display_output_set_control(i->hdmi->hdmi_output, STM_CTRL_SIGNAL_RANGE, STM_SIGNAL_VIDEO_RANGE);

    mutex_unlock(&i->hdmi->lock);
  }

  return 0;
}


static int stmfb_probe_get_plane(struct stmfb_info *i,struct stmcore_display_pipeline_data *pd)
{
  /*
   * Get framebuffer plane, we do this here instead of in stmfb_createfb
   * as we need the device handle.
   *
   * First check that the preferred plane (which we are going to use) can
   * use memory from the system partition. At the moment we are not
   * supporting framebuffer allocation from secondary LMI.
   */
  int n=0;
  while(pd->planes[n].id != pd->preferred_graphics_plane) n++;

  if(!(pd->planes[n].flags & STMCORE_PLANE_MEM_SYS))
    return -ENOMEM;

  i->pFBPlane  = stm_display_get_plane(pd->device, pd->preferred_graphics_plane);
  if(!i->pFBPlane)
    return -ENODEV;

  i->pFBCLUT = dma_alloc_coherent(&i->platformDevice->dev, sizeof(unsigned long)*256, &i->dmaFBCLUT, GFP_KERNEL);
  if(!i->pFBCLUT)
    return -ENOMEM;

  return 0;
}


static int stmfb_probe_get_blitter(struct stmfb_info *i,const struct stmcore_display_pipeline_data * const pd)
{
  i->pBlitter = pd->blitter;
  if(i->pBlitter)
  {
    i->pBlitterCLUT = dma_alloc_coherent(&i->platformDevice->dev, sizeof(unsigned long)*256, &i->dmaBlitterCLUT, GFP_KERNEL);
    switch(pd->blitter_type)
    {
      case STMCORE_BLITTER_GAMMA:
        DPRINTK("Blitter is a Gamma\n");
        i->info.fix.accel = FB_ACCEL_ST_GAMMA;
        break;
      case STMCORE_BLITTER_BDISPII:
        if (i->blitter_api == 1 && pd->mmio)
        {
          DPRINTK("Blitter is a BDISP and you want support for user space AQs\n");
          i->info.fix.accel = FB_ACCEL_ST_BDISP_USER;

          i->pVSharedAreaBase = stm_display_blitter_get_shared (i->pBlitter);
          if (i->pVSharedAreaBase)
            {
              /* shared area struct occupies one page infront of the actual node
                 list. */
              i->ulSharedAreaSize = PAGE_SIZE
                                    + i->pVSharedAreaBase->nodes_size;
              i->ulPSharedAreaBase = i->pVSharedAreaBase->nodes_phys
                                     - PAGE_SIZE;
            }
        }
        else
        {
          DPRINTK("Blitter is a BDISP\n");
          i->info.fix.accel = FB_ACCEL_ST_BDISP;
        }
        break;
    }
  }
  else
  {
    i->info.fix.accel = FB_ACCEL_ST_NONE;
  }

  return 0;
}


static int stmfb_probe(struct platform_device *pdev)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);
  struct stmcore_display_pipeline_data *pd;
  int ret;

  DPRINTK("\n");

  if(i != NULL)
  {
    /*
     * If the device already has registered driver data, don't replace it
     * with the framebuffer.
     */
    return -EBUSY;
  }

  if(pdev->id < 0 || pdev->id >= NR_FRAMEBUFFERS)
    return -ENODEV;

  i  = &stmfb_info[pdev->id];
  pd = *((struct stmcore_display_pipeline_data **)pdev->dev.platform_data);

  if(!pd)
    return -ENODEV;

  if(!try_module_get(pd->owner))
    return -ENODEV;

  /* Setup framebuffer structure */
  sema_init(&i->framebufferLock,1);
  spin_lock_init(&i->framebufferSpinLock);
  init_waitqueue_head(&i->framebuffer_updated_wait_queue);
  i->opens = 0;
  i->fbdev_api_suspended = 0;

  i->platformDevice = pdev; /* Needed for power management state in fbops */

  if((ret = stmfb_probe_get_outputs(i,pd))<0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);
    return ret;
  }

  if((ret = stmfb_probe_get_plane(i,pd))<0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);
    return ret;
  }

  if ((ret = stmfb_createfb(i, pdev->id, pd->name)) < 0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);

    /*
     * stmfb_createfb makes a lot of calls to the core driver which can be
     * interrupted by a signal, checking for this here keeps the code cleaner.
     */
    if(signal_pending(current))
      return -EINTR;
    else
      return ret;
  }

  /*
   * Hook info structure into the platform device's driver data.
   */
  platform_set_drvdata(pdev,i);

  return 0;
}


static int __exit stmfb_remove(struct platform_device *pdev)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);
  struct stmcore_display_pipeline_data *pd = *((struct stmcore_display_pipeline_data **)pdev->dev.platform_data);

  DPRINTK("\n");

  if(!i)
    return 0;

  if(i->pBlitter)
    stm_display_blitter_sync(i->pBlitter);

  stmfb_destroyfb(i);
  stmfb_cleanup_info(i);

  platform_set_drvdata(pdev,NULL);

  module_put(pd->owner);

  return 0;
}


static void stmfb_shutdown(struct platform_device *pdev)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);
  if(!i)
    return;

  return;
}


#ifdef CONFIG_PM
static int stmfb_suspend(struct platform_device *pdev, pm_message_t state)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);

  DPRINTK("\n");

  if(!i)
    return 0;

  if (state.event == pdev->dev.power.power_state.event)
    return 0;

  acquire_console_sem();

  if(down_interruptible(&i->framebufferLock))
  {
    release_console_sem();
    return -EINTR;
  }

  if(i->pBlitter)
  {
    if(stm_display_blitter_sync(i->pBlitter)<0)
    {
      release_console_sem();
      up(&i->framebufferLock);
      return -EINTR;
    }
  }

  /*
   * Suspend the main output first (there must be one!) which will stop the
   * timing generator, hence any more vsync interrupts.
   */
  if(i->pFBMainOutput)
    stm_display_output_suspend(i->pFBMainOutput);

  if(i->pFBDVO)
    stm_display_output_suspend(i->pFBDVO);

  /*
   * HDMI is different, we need to stop it completely, which will force
   * the display state to NEEDS_RESTART. Once we resume the main output and
   * get a new VSYNC interrupt the ISR will spot this state change and signal
   * the HDMI thread which will look to see if the connected device has changed
   * (i.e. someone plugged in a different TV) while we were suspended and bring
   * the HDMI output back up again (if required at all) in the correct state.
   */
  if(i->hdmi)
  {
    stm_display_output_stop(i->hdmi->hdmi_output);

    /* Disable HDMI IRQ */
    if(i->hdmi->irq != -1)
      disable_irq(i->hdmi->irq);
  }

  /*
   * We release the framebuffer lock here as the fb_set_suspend callbacks
   * might call back into the driver.
   */
  up(&i->framebufferLock);

  fb_set_suspend(&i->info, 1);

  release_console_sem();

  DPRINTK("finished suspending\n");
  return 0;
}


static int stmfb_resume(struct platform_device *pdev)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);

  DPRINTK("\n");

  if(!i)
    return 0;

  if(pdev->dev.power.power_state.event == PM_EVENT_ON)
    return 0;

  acquire_console_sem();

  if(down_interruptible(&i->framebufferLock))
  {
    release_console_sem();
    return -EINTR;
  }

  /* Enable HDMI IRQ */
  if(i->hdmi)
  {
    if(i->hdmi->irq != -1)
      enable_irq(i->hdmi->irq);
  }

  if(i->pFBMainOutput)
    stm_display_output_resume(i->pFBMainOutput);

  if(i->pFBDVO)
    stm_display_output_resume(i->pFBDVO);

  up(&i->framebufferLock);

  /*
   * Now the hardware is back, kick the framebuffer into life
   */
  fb_pan_display(&i->info, &i->info.var);
  fb_set_cmap(&i->info.cmap, &i->info);
  fb_set_suspend(&i->info, 0);

  release_console_sem();

  DPRINTK("resumed\n");

  return 0;
}
#endif


static struct platform_driver stmfb_driver = {
   .probe    = stmfb_probe,
#ifdef CONFIG_PM
   .suspend  = stmfb_suspend,
   .resume   = stmfb_resume,
#endif
   .shutdown = stmfb_shutdown,
   .remove   = __exit_p(stmfb_remove),
   .driver   = {
       .name     = "stmcore-display",
       .owner    = THIS_MODULE
   }
};


static void __exit stmfb_exit(void)
{
  platform_driver_unregister(&stmfb_driver);
}


static int __init stmfb_init(void)
{
  memset(stmfb_info, 0, sizeof(stmfb_info));
  platform_driver_register(&stmfb_driver);

  return 0;
}


/******************************************************************************
 *  Modularization
 */

module_init(stmfb_init);
module_exit(stmfb_exit);

MODULE_LICENSE("GPL");
