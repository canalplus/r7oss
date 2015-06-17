/***********************************************************************
 *
 * File: linux/drivers/video/stmfbvar.c
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/fb.h>
#include <asm/div64.h>

#include <stmdisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"


/*****************************************************************************
 * Processing of framebuffer var structures to ST core driver information
 */
#define DEBUG_VAR 0

struct colour_desc_entry {
   SURF_FMT fmt;
   int      bitdepth;
   int      aoffset;
   int      alength;
   int      roffset;
   int      rlength;
   int      goffset;
   int      glength;
   int      boffset;
   int      blength;
   int      nonstd;
#if DEBUG_VAR == 1
   const char *name;
#endif
};


#if DEBUG_VAR == 1
#define E(f,d,ao,al,ro,rl,go,gl,bo,bl,n) \
  { f, d, ao, al, ro, rl, go, gl, bo, bl, n, #f }
#else
#define E(f,d,ao,al,ro,rl,go,gl,bo,bl,n) \
  { f, d, ao, al, ro, rl, go, gl, bo, bl, n }
#endif
static const struct colour_desc_entry colour_formats[] = {
  E(SURF_CLUT8,       8,  0, 0,  0, 0,  0, 0,  0, 0, 0),
  E(SURF_RGB565,     16,  0, 0, 11, 5,  5, 6,  0, 5, 0),
  E(SURF_ARGB1555,   16, 15, 1, 10, 5,  5, 5,  0, 5, 0),
  E(SURF_ARGB4444,   16, 12, 4,  8, 4,  4, 4,  0, 4, 0),
  E(SURF_RGB888,     24,  0, 0, 16, 8,  8, 8,  0, 8, 0),
  E(SURF_ARGB8565,   24, 16, 8, 11, 5,  5, 6,  0, 5, 0),
  E(SURF_ARGB8888,   32, 24, 8, 16, 8,  8, 8,  0, 8, 0),
  /* unusual formats */
  E(SURF_BGRA8888,   32,  0, 8,  8, 8, 16, 8, 24, 8, 0),
  E(SURF_YCBCR422R,  16,  0, 0,  0, 0,  0, 0,  0, 0, 1),
  E(SURF_CRYCB888,   24,  0, 0,  0, 0,  0, 0,  0, 0, 2),
  E(SURF_ACRYCB8888, 32, 24, 8,  0, 0,  0, 0,  0, 0, 0),
  E(SURF_ACLUT88,    16,  8, 8,  0, 0,  0, 0,  0, 0, 0),
};
#undef E


struct standard_list_entry
{
  stm_display_mode_t           mode;
  ULONG                        tvStandard;
  enum stmfbio_output_standard stmfbio_standard;
#if DEBUG_VAR == 1
  const char *name;
#endif
};

#if DEBUG_VAR == 1
#  define E(m,std,o,name)  { m, std, o, name }
#else
#  define E(m,std,o,name)  { m, std, o }
#endif
static const struct standard_list_entry stmfbio_standard_list[] = {
  E(STVTG_TIMING_MODE_480I59940_13500,       STM_OUTPUT_STD_NTSC_M,    STMFBIO_STD_NTSC_M,       "NTSC-M"       ),
  E(STVTG_TIMING_MODE_480I59940_13500,       STM_OUTPUT_STD_NTSC_J,    STMFBIO_STD_NTSC_M_JP,    "NTSC-J"       ),
  E(STVTG_TIMING_MODE_480I59940_13500,       STM_OUTPUT_STD_NTSC_443,  STMFBIO_STD_NTSC_443,     "NTSC-443"     ),
  E(STVTG_TIMING_MODE_480I59940_13500,       STM_OUTPUT_STD_PAL_M,     STMFBIO_STD_PAL_M,        "PAL-M"        ),
  E(STVTG_TIMING_MODE_480I59940_13500,       STM_OUTPUT_STD_PAL_60,    STMFBIO_STD_PAL_60,       "PAL-60"       ),
  E(STVTG_TIMING_MODE_576I50000_13500,       STM_OUTPUT_STD_PAL_BDGHI, STMFBIO_STD_PAL,          "PAL"          ),
  E(STVTG_TIMING_MODE_576I50000_13500,       STM_OUTPUT_STD_PAL_N,     STMFBIO_STD_PAL_N,        "PAL-N"        ),
  E(STVTG_TIMING_MODE_576I50000_13500,       STM_OUTPUT_STD_PAL_Nc,    STMFBIO_STD_PAL_Nc,       "PAL-Nc"       ),
  E(STVTG_TIMING_MODE_576I50000_13500,       STM_OUTPUT_STD_SECAM,     STMFBIO_STD_SECAM,        "SECAM"        ),
  E(STVTG_TIMING_MODE_480P60000_27027,       STM_OUTPUT_STD_SMPTE293M, STMFBIO_STD_480P_60,      "480p@60"      ),
  E(STVTG_TIMING_MODE_480P59940_27000,       STM_OUTPUT_STD_SMPTE293M, STMFBIO_STD_480P_59_94,   "480p@59.94"   ),
  E(STVTG_TIMING_MODE_576P50000_27000,       STM_OUTPUT_STD_SMPTE293M, STMFBIO_STD_576P_50,      "576p"         ),
  E(STVTG_TIMING_MODE_1080P60000_148500,     STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_60,     "1080p@60"     ),
  E(STVTG_TIMING_MODE_1080P59940_148352,     STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_59_94,  "1080p@59.94"  ),
  E(STVTG_TIMING_MODE_1080P50000_148500,     STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_50,     "1080p@50"     ),
  E(STVTG_TIMING_MODE_1080P30000_74250,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_30,     "1080p@30"     ),
  E(STVTG_TIMING_MODE_1080P29970_74176,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_29_97,  "1080p@29.97"  ),
  E(STVTG_TIMING_MODE_1080P25000_74250,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_25,     "1080p@25"     ),
  E(STVTG_TIMING_MODE_1080P24000_74250,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_24,     "1080p@24"     ),
  E(STVTG_TIMING_MODE_1080P23976_74176,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080P_23_976, "1080p@23.976" ),
  E(STVTG_TIMING_MODE_1080I60000_74250,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080I_60,     "1080i@60"     ),
  E(STVTG_TIMING_MODE_1080I59940_74176,      STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080I_59_94,  "1080i@59.94"  ),
  E(STVTG_TIMING_MODE_1080I50000_74250_274M, STM_OUTPUT_STD_SMPTE274M, STMFBIO_STD_1080I_50,     "1080i@50"     ),
  E(STVTG_TIMING_MODE_720P60000_74250,       STM_OUTPUT_STD_SMPTE296M, STMFBIO_STD_720P_60,      "720p@60"      ),
  E(STVTG_TIMING_MODE_720P59940_74176,       STM_OUTPUT_STD_SMPTE296M, STMFBIO_STD_720P_59_94,   "720p@59.94"   ),
  E(STVTG_TIMING_MODE_720P50000_74250,       STM_OUTPUT_STD_SMPTE296M, STMFBIO_STD_720P_50,      "720p@50"      ),
  E(STVTG_TIMING_MODE_480P60000_25200,       STM_OUTPUT_STD_VESA,      STMFBIO_STD_VGA_60,       "VGA@60"       ),
  E(STVTG_TIMING_MODE_480P59940_25180,       STM_OUTPUT_STD_VESA,      STMFBIO_STD_VGA_59_94,    "VGA@59.94"    ),
};
#undef E


#if DEBUG_VAR == 1

static void
stmfb_display_var (const char                     *prefix,
                   const struct fb_var_screeninfo *var)
{
	printk( "stmfb: ----%3s---- struct fb_var_screeninfo var = %p ---------------\n",
          prefix, var);

	printk( "stmfb: resolution: %ux%ux%u (virtual %ux%u+%u+%u)\n",
			var->xres, var->yres, var->bits_per_pixel,
			var->xres_virtual, var->yres_virtual,
			var->xoffset, var->yoffset);

	printk( "stmfb: color: %c %d R(%u,%u,%u), G(%u,%u,%u), B(%u,%u,%u), T(%u,%u,%u)\n",
			var->grayscale?'G':'C', var->nonstd,
			var->red.offset,    var->red.length,   var->red.msb_right,
			var->green.offset,  var->green.length, var->green.msb_right,
			var->blue.offset,   var->blue.length,  var->blue.msb_right,
			var->transp.offset, var->transp.length,var->transp.msb_right);

	printk( "stmfb: timings: %ups (%u,%u)-(%u,%u)+%u+%u\n",
		var->pixclock,
		var->left_margin, var->upper_margin, var->right_margin,
		var->lower_margin, var->hsync_len, var->vsync_len);

	printk(	"stmfb: activate %08x accel_flags %08x sync %08x vmode %08x\n",
		var->activate, var->accel_flags, var->sync, var->vmode);

	printk(	"stmfb: -------------------------------------------------------------------\n");
}


static void
stmfb_display_planeconfig (const char                        *prefix,
                           const struct stmfbio_plane_config *c)
{
  int         x;
  const char *name = "";

  for(x = 0; x < ARRAY_SIZE (colour_formats); ++x)
    if (colour_formats[x].fmt == c->format) {
      name = colour_formats[x].name;
      break;
    }

  printk ("stmfbio_plane_config %3s (@ %p) %ux%u (rescale to %u,%u-%ux%u) %u (%s) @ %lx pitch %u %dbpp\n",
          prefix, c,
          c->source.w, c->source.h, c->dest.x, c->dest.y, c->dest.dim.w,
          c->dest.dim.h, c->format, name, c->baseaddr, c->pitch,
          c->bitdepth);
}


static void
stmfb_display_outputstandard (const char                         *prefix,
                              const enum stmfbio_output_standard  s)
{
  int x;

  for (x = 0; x < ARRAY_SIZE (stmfbio_standard_list); ++x)
    if ((stmfbio_standard_list[x].stmfbio_standard & s) == s) {
      printk ("stmfbio_output_standard %3s %llx (%s)\n",
              prefix, s, stmfbio_standard_list[x].name);
      return;
    }

    printk ("stmfbio_output_standard %3s %llx unknown!\n", prefix, s);
}

#else

static inline void __attribute__((nonnull)) stmfb_display_var(const char *prefix, const struct fb_var_screeninfo* var) { }
static inline void __attribute__((nonnull)) stmfb_display_planeconfig(const char *prefix, const struct stmfbio_plane_config *c) { }
static inline void __attribute__((nonnull(1))) stmfb_display_outputstandard(const char *prefix, const enum stmfbio_output_standard s) { }

#endif


u32
stmfb_bitdepth_for_pixelformat (SURF_FMT format)
{
  int x;
  for (x = 0; x < ARRAY_SIZE (colour_formats); ++x)
    if (colour_formats[x].fmt == format)
      return colour_formats[x].bitdepth;
  return 0;
}

u32
stmfb_pitch_for_config (const struct stmfbio_plane_config *c)
{
  /* Potential problem with this - if we have to pad the display for some
     reason, this will be incorrect. */
  return c->source.w * (c->bitdepth >> 3);
}


int
stmfb_verify_baseaddress (const struct stmfb_info           *i,
                          const struct stmfbio_plane_config *c,
                          unsigned long                      plane_size,
                          unsigned long                      baseaddr)
{
  if (c == &i->current_planeconfig && !i->current_planeconfig_valid)
  {
    DPRINTK ("current plane config not valid\n");
    return -EAGAIN;
  }

  if (baseaddr < i->ulPFBBase
      /* fight hackers -> overflow? */
      || (baseaddr + plane_size) < i->ulPFBBase
      /* pitch in combination with plane height is too too big */
      || (baseaddr + plane_size) > (i->ulPFBBase + i->ulFBSize)) {
    DPRINTK ("Invalid base address: %lx (fb @ %lx %lu bytes (end @ %lx) for plane size %lu\n",
             baseaddr, i->ulPFBBase, i->ulFBSize,
             i->ulPFBBase +i->ulFBSize, plane_size);
    return -EINVAL;
  }

  return 0;
}

int
stmfb_verify_planeinfo (const struct stmfb_info        *i,
                        const struct stmfbio_planeinfo *plane)
{
  const struct stmfbio_plane_config * const c = &plane->config;
  stm_display_plane_t               * const pFBPlane = i->pFBPlane;
  const SURF_FMT                    *plane_formats;
  int                                n_plane_Formats;
  stm_plane_caps_t                   plane_caps;
  int                                x;

  if(!i)
    return -ENODEV;

  stmfb_display_planeconfig ("verify", &plane->config);

  if(!plane || plane->layerid != 0)
    return -EINVAL;

  /* check allowed maximum plane size supported by the hardware */
  x = stm_display_plane_get_capabilities (pFBPlane, &plane_caps);
  if (signal_pending (current))
    return -ERESTARTSYS;
  if (x < 0)
    return -EIO;
  if (c->source.w < plane_caps.ulMinWidth
      || c->source.w > plane_caps.ulMaxWidth
      || c->source.h < plane_caps.ulMinHeight
      || c->source.h > plane_caps.ulMaxHeight
      || c->dest.x < 0
      || c->dest.y < 0
     )
    /* FIXME: or should we just clamp here? */
    /* FIXME2: min/max width and height probably apply to dest, not source?!*/
    return -EINVAL;
#if 0
  if (c->dest.dim.w < plane_caps.ulMinWidth
      || c->dest.dim.w > plane_caps.ulMaxWidth
      || c->dest.dim.h < plane_caps.ulMinHeight
      || c->dest.dim.h > plane_caps.ulMaxHeight)
    /* FIXME: or should we just clamp here? */
    return -EINVAL;
#else
#  warning check rescale factors here
#endif

  /* check that the surface format is actually supported by the hardware. */
  n_plane_Formats = stm_display_plane_get_image_formats (pFBPlane,
                                                         &plane_formats);
  if (signal_pending (current))
    return -ERESTARTSYS;
  if(n_plane_Formats < 0)
    return -EIO;

  for (x = 0; x < n_plane_Formats; ++x)
    if (plane_formats[x] == c->format)
      break;
  if (x == n_plane_Formats)
    return -EINVAL;

  /* yes, it is, now verify pitch */
  if (!c->pitch)
    return -EINVAL;

  if (!c->bitdepth)
    return -EINVAL;
  if (c->pitch < c->source.w * (c->bitdepth >> 3))
    /* pitch too small */
    return -EINVAL;

  /* and now for the base address */
  /* If the video memory hasn't been set yet, we are trying to find a default
     mode from the module parameters line, so ignore this test. */
  if (!i->ulPFBBase)
    return 0;
  return stmfb_verify_baseaddress (i, c, c->pitch * c->source.h, c->baseaddr);
}

int
stmfb_outputinfo_to_videomode (const struct stmfb_info         * const i,
                               const struct stmfbio_outputinfo * const output,
                               struct stmfb_videomode          * const vm)
{
  int x;

  if(!i)
    return -ENODEV;

  stmfb_display_outputstandard ("decode in", output->standard);

  if(!output || output->outputid != STMFBIO_OUTPUTID_MAIN || !vm)
    return -EINVAL;

  vm->pModeLine = NULL;

  for (x = 0; x < ARRAY_SIZE (stmfbio_standard_list); ++x) {
    if ((stmfbio_standard_list[x].stmfbio_standard & output->standard) == output->standard) {
#if DEBUG_VAR == 1
      DPRINTK ("%s: found mode(%d): %u tvStandard: 0x%lx stmfbid: 0x%.16llx %s\n",
               __FUNCTION__, x, stmfbio_standard_list[x].mode,
               stmfbio_standard_list[x].tvStandard, output->standard,
               stmfbio_standard_list[x].name);
#else
      DPRINTK ("%s: found mode(%d): %u tvStandard: 0x%lx stmfbid: 0x%.16llx\n",
               __FUNCTION__, x, stmfbio_standard_list[x].mode,
               stmfbio_standard_list[x].tvStandard, output->standard);
#endif

      vm->pModeLine = stm_display_output_get_display_mode (i->pFBMainOutput,
                                                           stmfbio_standard_list[x].mode);
      break;
    }
  }

  if(signal_pending(current))
    return -ERESTARTSYS;

  if(!vm->pModeLine)
  {
    DPRINTK ("%s: stmfb_std %.16llx not supported\n", __FUNCTION__,
             output->standard);
    return -EINVAL;
  }

  vm->ulTVStandard = stmfbio_standard_list[x].tvStandard;

  return 0;
}

static int
stmfb_fbdev_screeninfo_to_planeconfig (const struct stmfb_info        *i,
                                       const struct fb_var_screeninfo * const v,
                                       struct stmfbio_plane_config    * const config)
{
  int c;

  /* we do not support panning in the X direction so the real and virtual
     sizes must be the same */
  if ((v->xres != v->xres_virtual) || (v->yres > v->yres_virtual)) {
    DPRINTK ("(virtual) resolution invalid: xres/_virt: %u/%u yres/_virt: %u/%u\n",
             v->xres, v->xres_virtual, v->yres, v->yres_virtual);
    return -EINVAL;
  }
  /* check that the panning offsets are usable */
  if(v->xoffset != 0 || (v->yoffset + v->yres) > v->yres_virtual) {
    DPRINTK ("requested offsets > virtual: x/yoffset: %u/%u, yres/_virt: %u/%u\n",
             v->xoffset, v->yoffset, v->yres, v->yres_virtual);
    return -EINVAL;
  }

  config->dest.x = 0;
  config->dest.y = 0;
  config->dest.dim.w = config->source.w = v->xres;
  config->dest.dim.h = config->source.h = v->yres;

  /* search for the exact pixel format, only look at the bit length of each
     component so we can use fbset -rgba to change mode */
  for (c = 0; c < ARRAY_SIZE (colour_formats); ++c)
  {
    if(v->transp.length == colour_formats[c].alength
       && v->red.length == colour_formats[c].rlength
       && v->green.length == colour_formats[c].glength
       && v->blue.length == colour_formats[c].blength
       && v->nonstd == colour_formats[c].nonstd) {

      if((v->transp.offset && v->transp.offset != colour_formats[c].aoffset)
         || (v->red.offset && v->red.offset != colour_formats[c].roffset)
         || (v->green.offset && v->green.offset != colour_formats[c].goffset)
         || (v->blue.offset && v->blue.offset != colour_formats[c].boffset))
        continue;

      /* the colour format matches */
      break;
    }
  }

  if(c < ARRAY_SIZE (colour_formats)
     && colour_formats[c].bitdepth == v->bits_per_pixel)
    config->format = colour_formats[c].fmt;
  else
  {
    /* The vars bitdepth and rgba specification is not consistent. Catching
       this allows us to just use fbset -depth to change quickly between the
       colour depths without having to specify -rgba as well or to use
       fb.modes settings which only pass the bitdepth (colour lengths come in
       as 0/0/0/0 in that case) */
    switch(v->bits_per_pixel)
    {
      case  8: config->format = SURF_CLUT8;    break;
      case 16: config->format = SURF_RGB565;   break;
      case 24: config->format = SURF_RGB888;   break;
      case 32: config->format = SURF_ARGB8888; break;
      default:
        return -EINVAL;
    }
  }

  /* now the pitch */
  config->bitdepth = stmfb_bitdepth_for_pixelformat (config->format);
  config->pitch = stmfb_pitch_for_config (config);

  config->baseaddr = i->ulPFBBase + (v->yoffset * config->pitch);

  return 0;
}

static int
stmfb_fbdev_screeninfo_to_videomode (const struct stmfb_info        * const i,
                                     const struct fb_var_screeninfo * const v,
                                     struct stmfb_videomode         * const vm)
{
  unsigned long long pixclock;
  unsigned long      totallines, totalpixels;
  stm_scan_type_t    scanType;
  unsigned long      outputStandards;

  if (!i)
    return -ENODEV;

  switch (v->vmode & FB_VMODE_MASK)
  {
    case FB_VMODE_NONINTERLACED:
      scanType = SCAN_P;
      break;
    case FB_VMODE_INTERLACED:
      scanType = SCAN_I;
      break;
    case FB_VMODE_DOUBLE:
    default:
        DPRINTK("Unsupported FB VMODE flags: %u\n", v->vmode);
        return -EINVAL;
  }

  /* Convert pixelclock from picoseconds to Hz.
     Note that on the sh4 we can do an unsigned 64bit/32bit divide. */
  pixclock = 1000000000000ULL;
  do_div (pixclock, v->pixclock);

  totallines  = v->yres + v->upper_margin + v->lower_margin + v->vsync_len;
  totalpixels = v->xres + v->left_margin + v->right_margin + v->hsync_len;

  vm->pModeLine = stm_display_output_find_display_mode (i->pFBMainOutput,
                                                        v->xres,
                                                        v->yres,
                                                        totallines,
                                                        totalpixels,
                                                        (unsigned long) pixclock,
                                                        scanType);
  if (signal_pending(current))
    return -ERESTARTSYS;

  if (!vm->pModeLine)
    return -EINVAL;

  /*
   * We have to make a sensible decision about the TV standard to use
   * if the timing mode can support more than one. The Linux framebuffer
   * setup gives us no help with this. If the configured or default encoding
   * isn't supported on the mode, fall back to the most sensible default.
   */
  outputStandards = vm->pModeLine->ModeParams.OutputStandards;

  if(outputStandards & i->main_config.sdtv_encoding)
    vm->ulTVStandard = i->main_config.sdtv_encoding;
  else if(outputStandards & i->default_sd_encoding)
    vm->ulTVStandard = i->default_sd_encoding;
  else if(outputStandards & STM_OUTPUT_STD_NTSC_M)
    vm->ulTVStandard = STM_OUTPUT_STD_NTSC_M;
  else if(outputStandards & STM_OUTPUT_STD_PAL_BDGHI)
    vm->ulTVStandard = STM_OUTPUT_STD_PAL_BDGHI;
  else if((outputStandards & STM_OUTPUT_STD_HD_MASK) != 0)
    vm->ulTVStandard = (outputStandards & STM_OUTPUT_STD_HD_MASK);
  else if(outputStandards & STM_OUTPUT_STD_VESA)
    vm->ulTVStandard = STM_OUTPUT_STD_VESA;
  else if(outputStandards & STM_OUTPUT_STD_SMPTE293M)
    vm->ulTVStandard = STM_OUTPUT_STD_SMPTE293M;
  else
    BUG();

  return 0;
}

/*
 * stmfb_decode_var
 * Get the video params out of 'var' and construct a hw specific video
 * mode record.
 */
int
stmfb_decode_var (const struct fb_var_screeninfo * const v,
                  struct stmfb_videomode         * const vm,
                  struct stmfbio_planeinfo       * const plane,
                  const struct stmfb_info        * const i)
{
  int ret = 0;

  stmfb_display_var("var in", v);

  if(vm)
    ret = stmfb_fbdev_screeninfo_to_videomode (i, v, vm);

  if(!ret)
  {
    plane->layerid = 0;

    plane->activate = 0;
    switch (v->activate & FB_ACTIVATE_MASK) {
      case FB_ACTIVATE_NOW:
        plane->activate = STMFBIO_ACTIVATE_IMMEDIATE;
        break;
      case FB_ACTIVATE_TEST:
        plane->activate = STMFBIO_ACTIVATE_TEST;
        break;
      default:
        break;
    }
    if (v->activate & FB_ACTIVATE_VBL)
      plane->activate |= STMFBIO_ACTIVATE_VBL;

    ret = stmfb_fbdev_screeninfo_to_planeconfig (i, v, &plane->config);
    if(!ret)
    {
      ret = stmfb_verify_planeinfo (i, plane);
      if (!ret)
      {
        /* If the video memory hasn't been set yet, we are trying to find a
           default mode from the module parameters line, so ignore this
           test. */
        if (i->ulFBSize)
        {
          ret = stmfb_verify_baseaddress (i, &plane->config,
                                          v->yres_virtual * plane->config.pitch,
                                          i->ulPFBBase);
          if (ret)
          {
            DPRINTK ("Insufficient memory for requested virtual %u x %u pitch: %u\n",
                     v->xres_virtual, v->yres_virtual, plane->config.pitch);
            DPRINTK ("            %lu available, %u requested\n",
                     i->ulFBSize, v->yres_virtual * plane->config.pitch);
          }
        }
      }
    }
  }

#if DEBUG_VAR == 1
  if (ret)
    printk ("Unable to match the above!\n");
#endif

  return ret;
}

int
stmfb_get_outputstandards (struct stmfb_info              * const i,
                           struct stmfbio_outputstandards * const stds)
{
  unsigned long caps;

  stds->all_standards = STMFBIO_STD_UNKNOWN;

  if (stm_display_output_get_capabilities (i->pFBMainOutput, &caps) < 0) {
    DPRINTK ("%s: Unable to get output capabilities\n", __FUNCTION__);
    return signal_pending (current) ? -ERESTARTSYS : -EIO;
  }

  DPRINTK ("%s: Output capabilities: 0x%lx\n", __FUNCTION__, caps);

  if (caps & STM_OUTPUT_CAPS_SD_ANALOGUE) {
    DPRINTK ("%s: adding SD standards\n", __FUNCTION__);
    stds->all_standards |= STMFBIO_STD_SD;
  }

  if (caps & STM_OUTPUT_CAPS_ED_ANALOGUE) {
    DPRINTK ("%s: adding ED standards\n", __FUNCTION__);
    stds->all_standards |= STMFBIO_STD_ED;
  }

  if (caps & STM_OUTPUT_CAPS_HD_ANALOGUE) {
    DPRINTK ("%s: adding HD standards\n", __FUNCTION__);
    stds->all_standards |= (STMFBIO_STD_VESA
                            | STMFBIO_STD_SMPTE296M
                            | STMFBIO_STD_1080P_23_976
                            | STMFBIO_STD_1080P_24
                            | STMFBIO_STD_1080P_25
                            | STMFBIO_STD_1080P_29_97
                            | STMFBIO_STD_1080P_30
                            | STMFBIO_STD_1080I_60
                            | STMFBIO_STD_1080I_59_94
                            | STMFBIO_STD_1080I_50);

    /* Test to see if the high framerate 1080p modes are available by trying
       to get the mode descriptor for one, this returns 0 if the hardware
       cannot support it.
       FIXME: returns 0, too, if the device lock couldn't be obtained! Should
       we retry in this case? */
    if (stm_display_output_get_display_mode (i->pFBMainOutput,
                                             STVTG_TIMING_MODE_1080P60000_148500) != 0) {
      DPRINTK ("%s: adding fast 1080p standards\n", __FUNCTION__);
      stds->all_standards |= (STMFBIO_STD_1080P_60
                              | STMFBIO_STD_1080P_59_94
                              | STMFBIO_STD_1080P_50);
    }
  }

  return 0;
}

int
stmfb_videomode_to_outputinfo (const struct stmfb_videomode * const vm,
                               struct stmfbio_outputinfo    * const output)
{
  unsigned long tvStandard = vm->ulTVStandard;
  int           x;

  output->outputid = STMFBIO_OUTPUTID_MAIN;
  output->activate = STMFBIO_ACTIVATE_IMMEDIATE;

  for (x = 0; x < ARRAY_SIZE (stmfbio_standard_list); ++x)
  {
    if (stmfbio_standard_list[x].mode == vm->pModeLine->Mode
        && stmfbio_standard_list[x].tvStandard == tvStandard)
    {
      DPRINTK ("%s: found mode(%d): %u tvStandard: 0x%lx stmfbid: 0x%.16llx\n",
               __FUNCTION__, x, vm->pModeLine->Mode, tvStandard,
               stmfbio_standard_list[x].stmfbio_standard);
      output->standard = stmfbio_standard_list[x].stmfbio_standard;

      return 0;
    }
  }

  return -EINVAL;
}

static void
stmfbio_planeconfig_to_fb_var_screeninfo (const struct stmfb_info           * const i,
                                          const struct stmfbio_plane_config * const config,
                                          struct fb_var_screeninfo          * const v)
{
  int c;

  stmfb_display_planeconfig ("encode in", config);

  v->xres           = config->source.w;
  v->xres_virtual   = config->source.w;
  v->xoffset        = 0;
  v->yres           = config->source.h;
  v->yres_virtual   = config->source.h;
  v->yoffset        = (config->baseaddr - i->ulPFBBase) / config->pitch;
  v->grayscale      = 0;

  for (c = 0; c < ARRAY_SIZE (colour_formats); ++c)
  {
    if (colour_formats[c].fmt == config->format)
    {
      v->bits_per_pixel = colour_formats[c].bitdepth;

      v->transp.offset = colour_formats[c].aoffset;
      v->transp.length = colour_formats[c].alength;
      v->red.offset    = colour_formats[c].roffset;
      v->red.length    = colour_formats[c].rlength;
      v->green.offset  = colour_formats[c].goffset;
      v->green.length  = colour_formats[c].glength;
      v->blue.offset   = colour_formats[c].boffset;
      v->blue.length   = colour_formats[c].blength;

      break;
    }
  }
  if (unlikely (c == ARRAY_SIZE (colour_formats)))
  {
    v->bits_per_pixel = 0;
    memset (&v->red,    0, sizeof (v->transp));
    memset (&v->green,  0, sizeof (v->green));
    memset (&v->blue,   0, sizeof (v->blue));
    memset (&v->transp, 0, sizeof (v->transp));
  }

  v->nonstd = colour_formats[c].nonstd;

  /* Height/Width of picture in mm */
  v->height = v->width = -1;
}

static void
stmfbio_videomode_to_fb_var_screeninfo (const struct stmfb_videomode * const vm,
                                        struct fb_var_screeninfo     * const v)
{
  u64 timingtmp;

  /* Convert pixel clock (Hz) to pico seconds, correctly rounding around 1/2 */
  timingtmp = 1000000000000ULL + (vm->pModeLine->TimingParams.ulPixelClock / 2);
  do_div (timingtmp, vm->pModeLine->TimingParams.ulPixelClock);
  v->pixclock     = (u32) timingtmp;

  v->xres         = vm->pModeLine->ModeParams.ActiveAreaWidth;
  v->hsync_len    = vm->pModeLine->TimingParams.HSyncPulseWidth;
  v->left_margin  = (vm->pModeLine->ModeParams.ActiveAreaXStart
                     - v->hsync_len);
  v->right_margin = (vm->pModeLine->TimingParams.PixelsPerLine
                     - vm->pModeLine->ModeParams.ActiveAreaXStart
                     - vm->pModeLine->ModeParams.ActiveAreaWidth);

  v->yres         = vm->pModeLine->ModeParams.ActiveAreaHeight;
  v->vsync_len    = vm->pModeLine->TimingParams.VSyncPulseWidth;
  v->upper_margin = (vm->pModeLine->ModeParams.FullVBIHeight
                     - v->vsync_len);
  v->lower_margin = (vm->pModeLine->TimingParams.LinesByFrame
                     - vm->pModeLine->ModeParams.FullVBIHeight
                     - vm->pModeLine->ModeParams.ActiveAreaHeight);

  v->sync = 0;
  if (vm->pModeLine->TimingParams.HSyncPolarity)
    v->sync |= FB_SYNC_HOR_HIGH_ACT;
  if (vm->pModeLine->TimingParams.VSyncPolarity)
    v->sync |= FB_SYNC_VERT_HIGH_ACT;

  v->vmode = ((vm->pModeLine->ModeParams.ScanType == SCAN_P)
              ? FB_VMODE_NONINTERLACED : FB_VMODE_INTERLACED);
}

/*
 *  stmfb_encode_var
 *  Fill a 'var' structure based on the values in 'vm' and maybe other
 *  values read out of the hardware. This is the reverse of stmfb_decode_var
 */
int
stmfb_encode_var (struct fb_var_screeninfo       * const v,
                  const struct stmfb_videomode   * const vm,
                  const struct stmfbio_planeinfo * const plane,
                  const struct stmfb_info        * const i)
{
  memset(v, 0, sizeof(struct fb_var_screeninfo));

  stmfbio_videomode_to_fb_var_screeninfo (vm, v);
  stmfbio_planeconfig_to_fb_var_screeninfo (i, &plane->config, v);

  stmfb_display_var("encode out", v);

  return 0;
}


/******************************************************************************
 * Extended var information to control ST specific plane properties
 */
int
stmfb_queuebuffer (struct stmfb_info * const i)
{
  spin_lock_irq(&(i->framebufferSpinLock));
  i->num_outstanding_updates++;
  spin_unlock_irq(&(i->framebufferSpinLock));

  DPRINTK("%s: pa sz: %lx/%lu, fmt: %d %lubpp pitch/lines: %lu/%lu\n"
          "   src  x/y/w/h: %4ld/%4ld/%4ld/%4ld alpha %lx flg %lx\n"
          "   dest x/y/w/h: %4ld/%4ld/%4ld/%4ld flg %lx ckey %lx/%lx\n",
          __FUNCTION__,
          i->current_buffer_setup.src.ulVideoBufferAddr,
          i->current_buffer_setup.src.ulVideoBufferSize,
          i->current_buffer_setup.src.ulColorFmt,
          i->current_buffer_setup.src.ulPixelDepth,
          i->current_buffer_setup.src.ulStride,
          i->current_buffer_setup.src.ulTotalLines,
          i->current_buffer_setup.src.Rect.x,
          i->current_buffer_setup.src.Rect.y,
          i->current_buffer_setup.src.Rect.width,
          i->current_buffer_setup.src.Rect.height,
          i->current_buffer_setup.src.ulConstAlpha,
          i->current_buffer_setup.src.ulFlags,
          i->current_buffer_setup.dst.Rect.x,
          i->current_buffer_setup.dst.Rect.y,
          i->current_buffer_setup.dst.Rect.width,
          i->current_buffer_setup.dst.Rect.height,
          i->current_buffer_setup.dst.ulFlags,
          i->current_buffer_setup.dst.ulColorKeyMin,
          i->current_buffer_setup.dst.ulColorKeyMax);

  if(stm_display_plane_queue_buffer(i->pFBPlane,&i->current_buffer_setup)<0)
  {
    spin_lock_irq(&(i->framebufferSpinLock));
    i->num_outstanding_updates--;
    spin_unlock_irq(&(i->framebufferSpinLock));

    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EINVAL;
  }

  return 0;
}


static int
stmfb_decode_var_ex (struct stmfbio_var_screeninfo_ex * const v,
                     stm_display_buffer_t             * const b,
                     struct stmfb_info                * const i)
{
  stm_display_plane_t *plane;
  int                  layerid = v->layerid;

  /*
   * Currently only support the framebuffer plane, but are leaving open the
   * possibility of accessing other planes in the future.
   */
  if(layerid != 0 || v->activate == STMFBIO_ACTIVATE_TEST)
    return -EINVAL;

  plane = i->pFBPlane;

  v->failed = 0;

  if(v->caps & STMFBIO_VAR_CAPS_COLOURKEY)
  {
    b->src.ColorKey.flags = SCKCF_ENABLE;
    if (v->colourKeyFlags & STMFBIO_COLOURKEY_FLAGS_ENABLE)
    {
      b->src.ColorKey.enable = 1;

      b->src.ColorKey.flags  |= SCKCF_FORMAT;
      b->src.ColorKey.format  = SCKCVF_RGB;

      b->src.ColorKey.flags  |= (SCKCF_MINVAL | SCKCF_MAXVAL);
      b->src.ColorKey.minval  = v->min_colour_key;
      b->src.ColorKey.minval  = v->max_colour_key;

      b->src.ColorKey.flags |= (SCKCF_R_INFO | SCKCF_G_INFO | SCKCF_B_INFO);
      if(v->colourKeyFlags & STMFBIO_COLOURKEY_FLAGS_INVERT)
      {
        DPRINTK("Inverse colour key.\n");
        b->src.ColorKey.r_info  = SCKCCM_INVERSE;
        b->src.ColorKey.g_info  = SCKCCM_INVERSE;
        b->src.ColorKey.b_info  = SCKCCM_INVERSE;
      }
      else
      {
        DPRINTK("Normal colour key.\n");
        b->src.ColorKey.r_info  = SCKCCM_ENABLED;
        b->src.ColorKey.g_info  = SCKCCM_ENABLED;
        b->src.ColorKey.b_info  = SCKCCM_ENABLED;
      }
    }
    else
    {
      b->src.ColorKey.enable = 0;
    }

    i->current_var_ex.min_colour_key = v->min_colour_key;
    i->current_var_ex.max_colour_key = v->max_colour_key;
    i->current_var_ex.colourKeyFlags = v->colourKeyFlags;
  }
  else
    b->src.ColorKey.flags = SCKCF_NONE;

  if(v->caps & STMFBIO_VAR_CAPS_FLICKER_FILTER)
  {
    if((b->src.ulPixelDepth == 8 || b->src.ulColorFmt == SURF_ACLUT88)
       && (v->ff_state != STMFBIO_FF_OFF))
      v->failed |= STMFBIO_VAR_CAPS_FLICKER_FILTER;

    switch(v->ff_state)
    {
      case STMFBIO_FF_OFF:
        DPRINTK("Flicker Filter Off.\n");
        b->dst.ulFlags &= ~(STM_PLANE_DST_FLICKER_FILTER | STM_PLANE_DST_ADAPTIVE_FF);
        break;
      case STMFBIO_FF_SIMPLE:
        DPRINTK("Flicker Filter Simple.\n");
        b->dst.ulFlags |= STM_PLANE_DST_FLICKER_FILTER;
        break;
      case STMFBIO_FF_ADAPTIVE:
        DPRINTK("Flicker Filter Adaptive.\n");
        b->dst.ulFlags |= (STM_PLANE_DST_FLICKER_FILTER | STM_PLANE_DST_ADAPTIVE_FF);
        break;
      default:
        DPRINTK("Flicker Filter state invalid.\n");
        v->failed |= STMFBIO_VAR_CAPS_FLICKER_FILTER;
        break;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_FLICKER_FILTER))
      i->current_var_ex.ff_state = v->ff_state;
  }

  if(v->caps & STMFBIO_VAR_CAPS_PREMULTIPLIED)
  {
    if(v->premultiplied_alpha)
    {
      DPRINTK("Premultiplied Alpha.\n");
      b->src.ulFlags |= STM_PLANE_SRC_PREMULTIPLIED_ALPHA;
    }
    else
    {
      DPRINTK("Non-Premultiplied Alpha.\n");
      b->src.ulFlags &= ~STM_PLANE_SRC_PREMULTIPLIED_ALPHA;
    }

    i->current_var_ex.premultiplied_alpha = v->premultiplied_alpha;
  }


  if(v->caps & STBFBIO_VAR_CAPS_RESCALE_COLOUR_TO_VIDEO_RANGE)
  {
    if(v->rescale_colour_to_video_range)
    {
      DPRINTK("Rescale colour to video output range.\n");
      b->dst.ulFlags |= STM_PLANE_DST_RESCALE_TO_VIDEO_RANGE;
    }
    else
    {
      DPRINTK("Full colour output range.\n");
      b->dst.ulFlags &= ~STM_PLANE_DST_RESCALE_TO_VIDEO_RANGE;
    }

    i->current_var_ex.rescale_colour_to_video_range = v->rescale_colour_to_video_range;
  }


  if(v->caps & STMFBIO_VAR_CAPS_OPACITY)
  {
    if (v->opacity <255)
    {
      b->src.ulFlags     |= STM_PLANE_SRC_CONST_ALPHA;
      b->src.ulConstAlpha = v->opacity;
    }
    else
    {
      b->src.ulFlags &= ~STM_PLANE_SRC_CONST_ALPHA;
    }

    DPRINTK("Opacity = %d.\n",v->opacity);

    i->current_var_ex.opacity = v->opacity;
  }

  if(v->caps & STMFBIO_VAR_CAPS_GAIN)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_control(plane, PLANE_CTRL_GAIN, v->gain)<0)
        v->failed |= STMFBIO_VAR_CAPS_GAIN;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_GAIN))
    {
      DPRINTK("Gain = %d.\n",v->gain);
      i->current_var_ex.gain = v->gain;
    }
  }

  if(v->caps & STMFBIO_VAR_CAPS_BRIGHTNESS)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_control(plane, PLANE_CTRL_PSI_BRIGHTNESS, v->brightness)<0)
        v->failed |= STMFBIO_VAR_CAPS_BRIGHTNESS;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_BRIGHTNESS))
    {
      DPRINTK("Brightness = %d.\n",v->brightness);
      i->current_var_ex.brightness = v->brightness;
    }
  }

  if(v->caps & STMFBIO_VAR_CAPS_SATURATION)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_control(plane, PLANE_CTRL_PSI_SATURATION, v->saturation)<0)
        v->failed |= STMFBIO_VAR_CAPS_SATURATION;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_SATURATION))
    {
      DPRINTK("Saturation = %d.\n",v->saturation);
      i->current_var_ex.saturation = v->saturation;
    }
  }

  if(v->caps & STMFBIO_VAR_CAPS_CONTRAST)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_control(plane, PLANE_CTRL_PSI_CONTRAST, v->contrast)<0)
        v->failed |= STMFBIO_VAR_CAPS_CONTRAST;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_CONTRAST))
    {
      DPRINTK("Contrast = %d.\n",v->contrast);
      i->current_var_ex.contrast = v->contrast;
    }
  }

  if(v->caps & STMFBIO_VAR_CAPS_TINT)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_control(plane, PLANE_CTRL_PSI_TINT, v->tint)<0)
        v->failed |= STMFBIO_VAR_CAPS_TINT;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_TINT))
    {
      DPRINTK("Tint = %d.\n",v->tint);
      i->current_var_ex.tint = v->tint;
    }
  }

  if(v->caps & STMFBIO_VAR_CAPS_ALPHA_RAMP)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_control(plane, PLANE_CTRL_ALPHA_RAMP, v->alpha_ramp[0] | (v->alpha_ramp[1]<<8))<0)
        v->failed |= STMFBIO_VAR_CAPS_ALPHA_RAMP;
    }

    if(!(v->failed & STMFBIO_VAR_CAPS_ALPHA_RAMP))
    {
      DPRINTK("Alpha Ramp = [%d,%d].\n",v->alpha_ramp[0],v->alpha_ramp[1]);
      i->current_var_ex.alpha_ramp[0] = v->alpha_ramp[0];
      i->current_var_ex.alpha_ramp[1] = v->alpha_ramp[1];
    }
  }

  if(v->caps & STMFBIO_VAR_CAPS_ZPOSITION)
  {
    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      if(stm_display_plane_set_depth(plane, i->pFBMainOutput, v->z_position, 1)<0)
      {
        DPRINTK("FAILED: Set Z Position = %d.\n",v->z_position);
        v->failed |= STMFBIO_VAR_CAPS_ZPOSITION;
      }
      else
      {
        if(stm_display_plane_get_depth(plane, i->pFBMainOutput, &i->current_var_ex.z_position)<0)
        {
          DPRINTK("FAILED: Get Z Position\n");
          v->failed |= STMFBIO_VAR_CAPS_ZPOSITION;
        }
        else
        {
          DPRINTK("Real Z Position = %d.\n",i->current_var_ex.z_position);
        }
      }
    }
    else
    {
      DPRINTK("Z Position = %d.\n",v->z_position);
      i->current_var_ex.z_position = v->z_position;
    }
  }

  if(v->failed != 0)
  {
    if(signal_pending(current))
      return -EINTR;
    else
      return -EINVAL;
  }

  return 0;
}


int
stmfb_set_var_ex (struct stmfbio_var_screeninfo_ex * const v,
                  struct stmfb_info                * const i)
{
  stm_display_plane_t *plane;
  int                  layerid = v->layerid;
  int                  ret;

  /*
   * Currently only support the framebuffer plane, but are leaving open the
   * possibility of accessing other planes in the future.
   */
  if(layerid != 0)
    return -EINVAL;

  plane = i->pFBPlane;

  v->failed = v->caps & ~i->current_var_ex.caps;
  if(v->failed != 0)
    return -EINVAL;

  if(v->activate == STMFBIO_ACTIVATE_TEST)
  {
    /*
     * Test is if a Z position change is valid.
     */
    if(v->caps & STMFBIO_VAR_CAPS_ZPOSITION)
    {
      ret = stm_display_plane_set_depth(plane, i->pFBMainOutput, v->z_position, 0);
      if(ret<0)
      {
        v->failed |= STMFBIO_VAR_CAPS_ZPOSITION;
        return ret;
      }
    }

    /*
     * Check that flicker filtering is not being switched on when the
     * framebuffer is in 8bit CLUT mode, flicker filtering is only usable
     * in true RGB modes.
     */
    if(v->caps & STMFBIO_VAR_CAPS_FLICKER_FILTER)
    {
      if((i->info.var.bits_per_pixel == 8) && (v->ff_state != STMFBIO_FF_OFF))
      {
        DPRINTK("Cannot enable flicker filter in 8bit modes\n");
      	v->failed |= STMFBIO_VAR_CAPS_FLICKER_FILTER;
      	return -EINVAL;
      }
    }

  }
  else
  {
    if((ret = stmfb_decode_var_ex(v, &i->current_buffer_setup, i))<0)
      return ret;

    if(v->activate == STMFBIO_ACTIVATE_IMMEDIATE)
    {
      BUG_ON (!i->current_planeconfig_valid);

      ret = stmfb_queuebuffer(i);
      if(ret<0)
      {
        v->failed |= v->caps & (STMFBIO_VAR_CAPS_COLOURKEY                     |
                                STMFBIO_VAR_CAPS_FLICKER_FILTER                |
                                STMFBIO_VAR_CAPS_PREMULTIPLIED                 |
                                STBFBIO_VAR_CAPS_RESCALE_COLOUR_TO_VIDEO_RANGE |
                                STMFBIO_VAR_CAPS_OPACITY);
        return ret;
      }
      else
      {
      	/*
      	 * Don't let too many updates back up on the plane, but on the other
      	 * hand don't block by default.
      	 */
        wait_event(i->framebuffer_updated_wait_queue, i->num_outstanding_updates < 4);
      }
    }
  }

  return 0;
}


int
stmfb_encode_var_ex (struct stmfbio_var_screeninfo_ex * const v,
                     const struct stmfb_info          * const i)
{
  stm_display_plane_t *plane;
  int                  layerid = v->layerid;
  stm_plane_caps_t     caps;

  /*
   * Currently only support the framebuffer plane, but are leaving open the
   * possibility of accessing other planes in the future.
   */
  if(layerid != 0)
    return -EINVAL;

  plane = i->pFBPlane;

  memset(v, 0, sizeof(struct stmfbio_var_screeninfo_ex));
  v->layerid = layerid;

  if(stm_display_plane_get_capabilities(plane,&caps)<0)
    return -ERESTARTSYS;

  DPRINTK("Plane caps = 0x%08x\n",(unsigned)caps.ulCaps);

  if(caps.ulCaps & PLANE_CAPS_SRC_COLOR_KEY)
  {
    DPRINTK("Plane has Color Keying\n");
    v->caps |= STMFBIO_VAR_CAPS_COLOURKEY;
    v->colourKeyFlags = 0;
  }

  if(caps.ulCaps & PLANE_CAPS_FLICKER_FILTER)
  {
    DPRINTK("Plane has Flicker Filter\n");
    v->caps |= STMFBIO_VAR_CAPS_FLICKER_FILTER;
    v->ff_state = STMFBIO_FF_OFF;
  }

  if(caps.ulCaps & PLANE_CAPS_PREMULTIPLED_ALPHA)
  {
    DPRINTK("Plane supports premultipled alpha\n");
    v->caps |= STMFBIO_VAR_CAPS_PREMULTIPLIED;
    v->premultiplied_alpha = 1;
  }

  if(caps.ulCaps & PLANE_CAPS_RESCALE_TO_VIDEO_RANGE)
  {
    DPRINTK("Plane can rescale to video range\n");
    v->caps |= STBFBIO_VAR_CAPS_RESCALE_COLOUR_TO_VIDEO_RANGE;
    v->rescale_colour_to_video_range = 0;
  }

  if(caps.ulCaps & PLANE_CAPS_GLOBAL_ALPHA)
  {
    DPRINTK("Plane has global alpha\n");
    v->caps |= STMFBIO_VAR_CAPS_OPACITY;
    v->opacity = 255; /* fully opaque */
  }

  if(caps.ulControls & PLANE_CTRL_CAPS_GAIN)
  {
    DPRINTK("Plane has gain control\n");
    v->caps |= STMFBIO_VAR_CAPS_GAIN;
    v->gain = 255; /* 100% */
  }

  if(caps.ulControls & PLANE_CTRL_CAPS_PSI_CONTROLS)
  {
    DPRINTK("Plane has PSI control\n");
    v->caps |= STMFBIO_VAR_CAPS_BRIGHTNESS |
               STMFBIO_VAR_CAPS_SATURATION |
               STMFBIO_VAR_CAPS_CONTRAST   |
               STMFBIO_VAR_CAPS_TINT;

    v->brightness = 128;
    v->saturation = 128;
    v->contrast   = 128;
    v->tint       = 128;
  }

  if(caps.ulControls & PLANE_CTRL_CAPS_ALPHA_RAMP)
  {
    v->caps |= STMFBIO_VAR_CAPS_ALPHA_RAMP;
    v->alpha_ramp[0] = 0;
    v->alpha_ramp[1] = 255;
  }

  v->caps |= STMFBIO_VAR_CAPS_ZPOSITION;
  stm_display_plane_get_depth(plane, i->pFBMainOutput, &v->z_position);

  return 0;
}

