/*
 * gam_utils.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * GAMMA Utilities (Loading and Saving GAM files for test purposes)
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/bpa2.h>

#include "gam_utils.h"

#include "state.h"
#include "strings.h"
#include "blit_debug.h"


static const unsigned int gamma_types[STM_BLITTER_SF_COUNT] = {
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_INVALID)] = BLITTER_GAMFILE_UNKNOWN,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_RGB565)] = BLITTER_GAMFILE_RGB565,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_RGB24)] = BLITTER_GAMFILE_RGB888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_BGR24)] = BLITTER_GAMFILE_UNKNOWN,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_RGB32)] = BLITTER_GAMFILE_xRGB8888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_ARGB1555)] = BLITTER_GAMFILE_ARGB1555,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_ARGB4444)] = BLITTER_GAMFILE_ARGB4444,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_ARGB8565)] = BLITTER_GAMFILE_ARGB8565,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_AlRGB8565)] = BLITTER_GAMFILE_ARGB8565,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_ARGB)] = BLITTER_GAMFILE_ARGB8888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_AlRGB)] = BLITTER_GAMFILE_ARGB8888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_BGRA)] = BLITTER_GAMFILE_UNKNOWN,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_BGRAl)] = BLITTER_GAMFILE_UNKNOWN,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_LUT8)] = BLITTER_GAMFILE_CLUT8,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_LUT4)] = BLITTER_GAMFILE_CLUT4,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_LUT2)] = BLITTER_GAMFILE_CLUT2,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_LUT1)] = BLITTER_GAMFILE_CLUT1,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_ALUT88)] = BLITTER_GAMFILE_ACLUT88,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_AlLUT88)] = BLITTER_GAMFILE_ACLUT88,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_ALUT44)] = BLITTER_GAMFILE_ACLUT44,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_A8)] = BLITTER_GAMFILE_A8,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_Al8)] = BLITTER_GAMFILE_A8,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_A1)] = BLITTER_GAMFILE_A1,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_AVYU)] = BLITTER_GAMFILE_AYCBCR8888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_AlVYU)] = BLITTER_GAMFILE_AYCBCR8888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_VYU)] = BLITTER_GAMFILE_YCBCR888,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YUY2)] = BLITTER_GAMFILE_UNKNOWN, /* BLITTER_422FILE_YCBCR422R endianess swapped */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_UYVY)] = BLITTER_422FILE_YCBCR422R,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YV12)] = BLITTER_GAMFILE_UNKNOWN, /* 420 planar */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_I420)] = BLITTER_GAMFILE_I420,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YV16)] = BLITTER_GAMFILE_UNKNOWN, /* 422 planar */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YV61)] = BLITTER_GAMFILE_UNKNOWN, /* 422 planar */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YCBCR444P)] = BLITTER_GAMFILE_UNKNOWN, /* 444 planar */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_NV12)] = BLITTER_420FILE_YCBCR420R2B,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_NV21)] = BLITTER_GAMFILE_UNKNOWN, /* BLITTER_420FILE_YCBCR420R2B chroma bytes swapped */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_NV16)] = BLITTER_422FILE_YCBCR422R2B,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_NV61)] = BLITTER_GAMFILE_UNKNOWN, /* BLITTER_422FILE_YCBCR422R2B chroma bytes swapped */
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YCBCR420MB)] = BLITTER_420FILE_YCBCR420MB,
  [STM_BLITTER_SF_MASK (STM_BLITTER_SF_YCBCR422MB)] = BLITTER_422FILE_YCBCR422MB,
};

static const stm_blitter_surface_format_t
__attribute_const__
gamtype_to_stmblitter (enum gamfile_type gamtype,
                       unsigned short    properties)
{
  switch (gamtype)
  {
    case BLITTER_GAMFILE_RGB565: return STM_BLITTER_SF_RGB565;
    case BLITTER_GAMFILE_RGB888: return STM_BLITTER_SF_RGB24;
    case BLITTER_GAMFILE_xRGB8888: return STM_BLITTER_SF_RGB32;

    case BLITTER_GAMFILE_ARGB8565:
      if (properties == BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA)
        return STM_BLITTER_SF_ARGB8565;
      return STM_BLITTER_SF_AlRGB8565;
    case BLITTER_GAMFILE_ARGB8888:
      if (properties == BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA)
        return STM_BLITTER_SF_ARGB;
      return STM_BLITTER_SF_AlRGB;

    case BLITTER_GAMFILE_ARGB1555: return STM_BLITTER_SF_ARGB1555;
    case BLITTER_GAMFILE_ARGB4444: return STM_BLITTER_SF_ARGB4444;
    case BLITTER_GAMFILE_CLUT1: return STM_BLITTER_SF_LUT1;
    case BLITTER_GAMFILE_CLUT2: return STM_BLITTER_SF_LUT2;
    case BLITTER_GAMFILE_CLUT4: return STM_BLITTER_SF_LUT4;
    case BLITTER_GAMFILE_CLUT8: return STM_BLITTER_SF_LUT8;
    case BLITTER_GAMFILE_ACLUT44: return STM_BLITTER_SF_ALUT44;

    case BLITTER_GAMFILE_ACLUT88:
      if (properties == BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA)
        return STM_BLITTER_SF_ALUT88;
      return STM_BLITTER_SF_AlLUT88;

    case BLITTER_GAMFILE_YCBCR888: return STM_BLITTER_SF_VYU;
    case BLITTER_422FILE_YCBCR422R: return STM_BLITTER_SF_UYVY;

    case BLITTER_GAMFILE_AYCBCR8888:
      if (properties == BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA)
        return STM_BLITTER_SF_AVYU;
      return STM_BLITTER_SF_AlVYU;

    case BLITTER_420FILE_YCBCR420MB: return STM_BLITTER_SF_YCBCR420MB;
    case BLITTER_422FILE_YCBCR422MB: return STM_BLITTER_SF_YCBCR422MB;
    case BLITTER_GAMFILE_A1: return STM_BLITTER_SF_A1;

    case BLITTER_GAMFILE_A8:
      if (properties == BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA)
        return STM_BLITTER_SF_A8;
      return STM_BLITTER_SF_Al8;

    case BLITTER_420FILE_YCBCR420R2B: return STM_BLITTER_SF_NV12;
    case BLITTER_422FILE_YCBCR422R2B: return STM_BLITTER_SF_NV16;

    case BLITTER_GAMFILE_I420: return STM_BLITTER_SF_I420;

    case BLITTER_GAMFILE_CLUT8_4444:
    case BLITTER_GAMFILE_YCBCR101010:
    case BLITTER_GAMFILE_RGB101010:
    case BLITTER_GAMFILE_XY:
    case BLITTER_GAMFILE_XYL:
    case BLITTER_GAMFILE_XYC:
    case BLITTER_GAMFILE_XYLC:

    case BLITTER_GAMFILE_UNKNOWN:
    default: break;
  }
  return STM_BLITTER_SF_INVALID;
}

static void
gam_print_bitmap_debug (const char *FileName, const bitmap_t *Bitmap_p)
{
  printk(KERN_INFO "GAM file_name: %s\n", FileName);
#ifdef DEBUG
  printk(KERN_INFO "Bitmap_p->format: %x (%s)\n", Bitmap_p->format,
         stm_blitter_get_surface_format_name (Bitmap_p->format));
#else
  printk(KERN_INFO "Bitmap_p->format: %x\n", Bitmap_p->format);
#endif
  printk(KERN_INFO "Bitmap_p->pitch: %llu\n",
         (unsigned long long) Bitmap_p->pitch);
  printk(KERN_INFO "Bitmap_p->size.w: %lld\n", (long long) Bitmap_p->size.w);
  printk(KERN_INFO "Bitmap_p->size.h: %lld\n", (long long) Bitmap_p->size.h);
  printk(KERN_INFO "Bitmap_p->buffer_size: %llu\n",
         (unsigned long long) Bitmap_p->buffer_size);
  printk(KERN_INFO "Bitmap_p->buffer_address.base: %llx\n",
         (unsigned long long) Bitmap_p->buffer_address.base);
}

static void
gam_print_palette_debug (const bitmap_t *Bitmap_p)
{
  if (!Bitmap_p->buffer_address.palette.clut_base)
    return;

  printk(KERN_INFO "Bitmap_p->buffer_address.palette.entries: %p\n",
         Bitmap_p->buffer_address.palette.entries);
  printk(KERN_INFO "Bitmap_p->buffer_address.palette.clut_base: %llx\n",
         (long long) Bitmap_p->buffer_address.palette.clut_base);
  printk(KERN_INFO "Bitmap_p->buffer_address.palette.num_entries: %d\n",
         Bitmap_p->buffer_address.palette.num_entries);
  printk(KERN_INFO "  (palette_size: %zu bytes)\n",
         (size_t) (Bitmap_p->buffer_address.palette.num_entries
                   * sizeof (stm_blitter_color_t)));
}

int
load_gam_file (const char *FileName,
               bitmap_t   *Bitmap_p)
{
  struct file *f;
  mm_segment_t old_fs;
  int res;
  GamPictureHeader_t header_file;
  struct bpa2_part *bpa2;

  memset (Bitmap_p, 0, sizeof (*Bitmap_p));

  if (!FileName)
    return -EBADF;
  if (!Bitmap_p)
    return -EINVAL;

  printk(KERN_DEBUG "%s: '%s'\n", __func__, FileName);

  bpa2 = bpa2_find_part("blitter");
  if (!bpa2) {
    printk (KERN_DEBUG "%s: no bpa2 partition 'blitter'\n", __func__);
    return -ENOENT;
  }

  old_fs = get_fs ();
  set_fs (KERNEL_DS);

  f = filp_open (FileName, O_RDONLY, 0);
  if (IS_ERR (f))
  {
    res = PTR_ERR (f);
    f = NULL;
    goto out_fs;
  }

  res = 0;

  printk (KERN_INFO "Reading GAM header file...\n");
  f->f_op->read (f, (void *) &header_file, sizeof (header_file), &f->f_pos);

  /* Setup picture info ... */
  Bitmap_p->format
    = gamtype_to_stmblitter ((enum gamfile_type) header_file.Type,
                             header_file.Properties);

  switch (Bitmap_p->format)
  {
    case STM_BLITTER_SF_ARGB:
    case STM_BLITTER_SF_AlRGB:
    case STM_BLITTER_SF_AVYU:
    case STM_BLITTER_SF_AlVYU:
      Bitmap_p->pitch += header_file.PictureWidth;
      /* fall through */
    case STM_BLITTER_SF_RGB24:
    case STM_BLITTER_SF_ARGB8565:
    case STM_BLITTER_SF_AlRGB8565:
    case STM_BLITTER_SF_VYU:
      Bitmap_p->pitch += header_file.PictureWidth;
      /* fall through */
    case STM_BLITTER_SF_RGB565:
    case STM_BLITTER_SF_ARGB1555:
    case STM_BLITTER_SF_ARGB4444:
      Bitmap_p->pitch += (2 * header_file.PictureWidth);
      header_file.LumaSize = Bitmap_p->pitch * header_file.PictureHeight;
      header_file.ChromaSize = 0;
      break;

    case STM_BLITTER_SF_I420:
      Bitmap_p->pitch = header_file.PictureWidth >> 16;
      header_file.PictureWidth = header_file.PictureWidth & 0xFFFF;
      if(Bitmap_p->pitch == 0)
        Bitmap_p->pitch = header_file.PictureWidth;
      /* Patch the chroma size to the real value */
      header_file.ChromaSize = header_file.LumaSize/2;
      Bitmap_p->buffer_address.cb_offset = header_file.LumaSize;
      Bitmap_p->buffer_address.cr_offset = header_file.LumaSize + header_file.ChromaSize/2;
      break;

    case STM_BLITTER_SF_YUY2:
    case STM_BLITTER_SF_UYVY:
      Bitmap_p->pitch = 2 * header_file.PictureWidth;
      if (header_file.ChromaSize == 0)
        /* We use only luma as chroma is interleaved so update the size
           accordingly */
        header_file.LumaSize *= 2;
      else
      {
        header_file.LumaSize += header_file.ChromaSize;
        header_file.ChromaSize = 0;
      }
      break;

    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
      /* With 4:2:x R2B, PictureWidth is a OR between Pitch and Width (each one
         on 16 bits) */
      Bitmap_p->pitch = header_file.PictureWidth >> 16;
      header_file.PictureWidth = header_file.PictureWidth & 0xFFFF;
      if(Bitmap_p->pitch == 0)
        Bitmap_p->pitch = header_file.PictureWidth;
      /* Patch the chroma size to the real value */
      header_file.ChromaSize = header_file.LumaSize;
      if (header_file.Type == BLITTER_420FILE_YCBCR420R2B)
        header_file.ChromaSize /= 2;
      Bitmap_p->buffer_address.cbcr_offset = header_file.LumaSize;
      break;

    case STM_BLITTER_SF_YCBCR420MB:
    case STM_BLITTER_SF_YCBCR422MB:
      Bitmap_p->pitch = ALIGN (header_file.PictureWidth, 16);
      Bitmap_p->buffer_address.cbcr_offset = header_file.LumaSize;
      break;

    case STM_BLITTER_SF_LUT8:
      Bitmap_p->buffer_address.palette.num_entries = 256;
      Bitmap_p->pitch = ALIGN (header_file.PictureWidth, 8);
      break;

    case STM_BLITTER_SF_INVALID:
    case STM_BLITTER_SF_RGB32:
    case STM_BLITTER_SF_BGR24:
    case STM_BLITTER_SF_BGRA:
    case STM_BLITTER_SF_BGRAl:
    case STM_BLITTER_SF_LUT4:
    case STM_BLITTER_SF_LUT2:
    case STM_BLITTER_SF_LUT1:
    case STM_BLITTER_SF_ALUT88:
    case STM_BLITTER_SF_AlLUT88:
    case STM_BLITTER_SF_ALUT44:
    case STM_BLITTER_SF_A8:
    case STM_BLITTER_SF_Al8:
    case STM_BLITTER_SF_A1:
    case STM_BLITTER_SF_YV12:
    case STM_BLITTER_SF_YV16:
    case STM_BLITTER_SF_YV61:
    case STM_BLITTER_SF_YCBCR444P:
    case STM_BLITTER_SF_COUNT:
    default:
      printk (KERN_ERR "stm-blitter format %.8x not supported (gam: %04x)!\n",
              Bitmap_p->format, header_file.Type);
      res = -EILSEQ;
      goto out_file;
  }
  Bitmap_p->colorspace = ((Bitmap_p->format & STM_BLITTER_SF_YCBCR)
                          ? STM_BLITTER_SCS_BT601
                          : STM_BLITTER_SCS_RGB);

  /* Allocate palette entires*/
  if (Bitmap_p->format & STM_BLITTER_SF_INDEXED)
  {
    unsigned long clut_base;
    size_t palette_size;
    int i;

    palette_size = (Bitmap_p->buffer_address.palette.num_entries
                    * sizeof (stm_blitter_color_t));

    /* Allocate memory for clut entries */
    Bitmap_p->buffer_address.palette.clut_base
      = clut_base
      = bpa2_alloc_pages(bpa2,
                         ((palette_size + PAGE_SIZE - 1) / PAGE_SIZE),
                         PAGE_SIZE, GFP_KERNEL);
    if (!clut_base) {
      printk(KERN_ERR
             "%s: could not allocate %zu bytes from bpa2 partition\n",
             __func__, palette_size);
      res = -ENOMEM;
      goto out_palette_bpa2;
    }

    Bitmap_p->buffer_address.palette.entries = ioremap(clut_base,
                                                       palette_size);
    if (!Bitmap_p->buffer_address.palette.entries) {
      printk(KERN_ERR "Failed to ioremap clut entries!\n");
      res = -ENOMEM;
      goto out_palette_ioremap;
    }

    printk (KERN_INFO "Reading palette data from file...");
    for (i = 0; i < Bitmap_p->buffer_address.palette.num_entries; ++i) {
      stm_blitter_color_t col;

      f->f_op->read (f, &col.b, 1, &f->f_pos);
      f->f_op->read (f, &col.g, 1, &f->f_pos);
      f->f_op->read (f, &col.r, 1, &f->f_pos);
      f->f_op->read (f, &col.a, 1, &f->f_pos);

      /* GAM files store the alpha value in limited range 0..0x80, so we need
         to scale it to 0..0xff, as this is what the driver expects. */
      col.a = ((uint16_t) (col.a) * 2) - 1;

      Bitmap_p->buffer_address.palette.entries[i] = col;
    }
  }

  Bitmap_p->size.w = header_file.PictureWidth;
  Bitmap_p->size.h = header_file.PictureHeight;
  Bitmap_p->buffer_size = header_file.LumaSize + header_file.ChromaSize;

  /* Allocate memory for surface buffer */
  Bitmap_p->buffer_address.base
    = bpa2_alloc_pages(bpa2,
                       ((Bitmap_p->buffer_size + PAGE_SIZE - 1) / PAGE_SIZE),
                       PAGE_SIZE, GFP_KERNEL);
  if (!Bitmap_p->buffer_address.base) {
    printk(KERN_ERR "%s: could not allocate %zu bytes from bpa2 partition\n",
           __func__, Bitmap_p->buffer_size);
    res = -ENOMEM;
    goto out_buffer_bpa2;
  }
  Bitmap_p->memory = ioremap(Bitmap_p->buffer_address.base,
                             Bitmap_p->buffer_size);
  if (!Bitmap_p->memory) {
    printk(KERN_ERR "Failed to ioremap memory!\n");
    res = -ENOMEM;
    goto out_buffer_ioremap;
  }

  gam_print_bitmap_debug (FileName, Bitmap_p);
  gam_print_palette_debug (Bitmap_p);

  printk (KERN_INFO "Reading Bitmap data from file...");
  if ((Bitmap_p->format == STM_BLITTER_SF_RGB24)
      || (Bitmap_p->format == STM_BLITTER_SF_ARGB8565)
      || (Bitmap_p->format == STM_BLITTER_SF_VYU))
  {
    /* 24bits bitmap types... */
    unsigned int i, j;
    char PixelsBuffer[4];

    for (i = 0, j = 0; i < Bitmap_p->size.h; ++i)
    {
      j = i * Bitmap_p->pitch;
      while (j < (Bitmap_p->pitch * (i + 1) - 1))
      {
        f->f_op->read (f, (void*) &PixelsBuffer, 4, &f->f_pos);
        if (Bitmap_p->format == STM_BLITTER_SF_VYU)
        {
          ((char *)Bitmap_p->memory)[j++] = (char)((PixelsBuffer[1]) & 0xff);
          ((char *)Bitmap_p->memory)[j++] = (char)((PixelsBuffer[0]) & 0xff);
          ((char *)Bitmap_p->memory)[j++] = (char)((PixelsBuffer[2]) & 0xff);
        }
        else
        {
          ((char *)Bitmap_p->memory)[j++] = (char)((PixelsBuffer[0]) & 0xff);
          ((char *)Bitmap_p->memory)[j++] = (char)((PixelsBuffer[1]) & 0xff);
          ((char *)Bitmap_p->memory)[j++] = (char)((PixelsBuffer[2]) & 0xff);
        }
      }
      /* Recompute pitch value */
      if (((Bitmap_p->size.w * 3) % 4) != 0)
        ((char *) Bitmap_p->memory)[j] = 0;
    }
  }
  else
    /* read entire file */
    f->f_op->read (f, (void *) Bitmap_p->memory, Bitmap_p->buffer_size,
                   &f->f_pos);


out_buffer_ioremap:
  if (res)
    bpa2_free_pages(bpa2, Bitmap_p->buffer_address.base);
out_buffer_bpa2:
  /* palette pointers only exist for indexed formats */
  if (res && Bitmap_p->buffer_address.palette.entries)
    iounmap (Bitmap_p->buffer_address.palette.entries);
out_palette_ioremap:
  if (res && Bitmap_p->buffer_address.palette.clut_base)
    bpa2_free_pages(bpa2, Bitmap_p->buffer_address.palette.clut_base);
out_palette_bpa2:
out_file:
  filp_close (f, NULL);
out_fs:
  set_fs (old_fs);


  if (!res)
    printk (KERN_INFO "GAM file '%s' loaded successfully\n", FileName);
  else
    printk (KERN_ERR "Error while loading the GAM file: %d!\n", res);

  return res;
}
EXPORT_SYMBOL(load_gam_file);

void
free_gam_file (bitmap_t *Bitmap_p)
{
  if (Bitmap_p->memory)
  {
    struct bpa2_part *bpa2;

    printk (KERN_INFO "Freeing Buffer @ %p\n", Bitmap_p->memory);

    bpa2 = bpa2_find_part("blitter");
    if (!bpa2) {
      printk (KERN_WARNING "%s: no bpa2 partition 'blitter'\n", __func__);
      return;
    }

    /* palette pointers only exist for indexed formats */
    if (Bitmap_p->buffer_address.palette.entries)
      iounmap (Bitmap_p->buffer_address.palette.entries);
    if (Bitmap_p->buffer_address.palette.clut_base)
      bpa2_free_pages(bpa2, Bitmap_p->buffer_address.palette.clut_base);
    Bitmap_p->buffer_address.palette.entries = NULL;
    Bitmap_p->buffer_address.palette.clut_base = 0;

    iounmap(Bitmap_p->memory);
    bpa2_free_pages(bpa2, Bitmap_p->buffer_address.base);
    Bitmap_p->memory = NULL;
    Bitmap_p->buffer_address.base = 0;
  }
}
EXPORT_SYMBOL(free_gam_file);

int
save_gam_file (const char     *FileName,
               const bitmap_t *Bitmap_p)
{
  struct file *f;
  GamPictureHeader_t header_file;
  mm_segment_t old_fs;
  int res;
  size_t file_size;

  if (!FileName)
    return -EBADF;
  if (!Bitmap_p)
    return -EINVAL;

  old_fs = get_fs ();
  set_fs (KERNEL_DS);

  gam_print_bitmap_debug (FileName, Bitmap_p);

  f = filp_open (FileName, O_RDWR | O_CREAT, 0);
  if (IS_ERR (f))
  {
    res = PTR_ERR (f);
    f = NULL;
    goto out_fs;
  }

  memset (&header_file, 0, sizeof (header_file));
  file_size = 0;
  res = 0;

  header_file.PictureWidth  = Bitmap_p->size.w;
  header_file.PictureHeight = Bitmap_p->size.h;
  header_file.HeaderSize    = BLITTER_GAMFILE_HEADER_SIZE;
  header_file.Signature     = BLITTER_GAMFILE_SIGNATURE;
  header_file.Properties    = BLITTER_GAMFILE_PROPERTIES;
  header_file.Type          = gamma_types[STM_BLITTER_SF_MASK(Bitmap_p->format)];
  if ((Bitmap_p->format & STM_BLITTER_SF_ALPHA_LIMITED)
      != STM_BLITTER_SF_ALPHA_LIMITED)
    header_file.Properties = BLITTER_GAMFILE_PROPERTIES_FULL_ALPHA;
  header_file.LumaSize      = Bitmap_p->size.w * Bitmap_p->size.h;
  header_file.ChromaSize    = 0;

  /* Setup header file ... */
  switch ((enum gamfile_type) header_file.Type)
  {
    case BLITTER_422FILE_YCBCR422R:
      header_file.LumaSize    = Bitmap_p->buffer_size / 2;
      header_file.ChromaSize  = 0;
      header_file.Signature   = BLITTER_422FILE_SIGNATURE;
      header_file.Properties  = BLITTER_422FILE_PROPERTIES;
      /* fall through */
    case BLITTER_GAMFILE_RGB565:
    case BLITTER_GAMFILE_RGB888:
    case BLITTER_GAMFILE_xRGB8888:
    case BLITTER_GAMFILE_ARGB8565:
    case BLITTER_GAMFILE_ARGB8888:
    case BLITTER_GAMFILE_ARGB1555:
    case BLITTER_GAMFILE_ARGB4444:
    case BLITTER_GAMFILE_YCBCR888:
    case BLITTER_GAMFILE_AYCBCR8888:
      file_size = Bitmap_p->buffer_size;
      break;

    case BLITTER_420FILE_YCBCR420R2B:
      header_file.PictureWidth = ((Bitmap_p->pitch << 16) | Bitmap_p->size.w);

    case BLITTER_420FILE_YCBCR420MB:
      header_file.LumaSize   = Bitmap_p->buffer_size * 4;
      header_file.LumaSize  /= 6;
      header_file.ChromaSize = header_file.LumaSize / 2;
      header_file.Signature = BLITTER_420FILE_SIGNATURE;
      header_file.Properties = BLITTER_420FILE_PROPERTIES;
      file_size = header_file.LumaSize + header_file.ChromaSize;
      break;

    case BLITTER_GAMFILE_I420:
      header_file.LumaSize   = Bitmap_p->buffer_size * 4;
      header_file.LumaSize  /= 6;
      header_file.ChromaSize = header_file.LumaSize / 4;
      header_file.Signature = BLITTER_420FILE_SIGNATURE;
      header_file.Properties = BLITTER_420FILE_PROPERTIES;
      file_size = header_file.LumaSize + 2*header_file.ChromaSize;
      break;

    case BLITTER_422FILE_YCBCR422R2B:
    case BLITTER_422FILE_YCBCR422MB:
      header_file.LumaSize
        = header_file.ChromaSize
        = Bitmap_p->buffer_size / 2;
      header_file.Signature = BLITTER_422FILE_SIGNATURE;
      header_file.Properties = BLITTER_422FILE_PROPERTIES;
      file_size = header_file.LumaSize + header_file.ChromaSize;
      break;

    case BLITTER_GAMFILE_CLUT8:
      /* this is not supported, as palette handling is missing */
      file_size = Bitmap_p->buffer_size;
      /* fall through */
    case BLITTER_GAMFILE_UNKNOWN:
    case BLITTER_GAMFILE_CLUT1:
    case BLITTER_GAMFILE_CLUT2:
    case BLITTER_GAMFILE_CLUT4:
    case BLITTER_GAMFILE_ACLUT44:
    case BLITTER_GAMFILE_ACLUT88:
    case BLITTER_GAMFILE_CLUT8_4444:
    case BLITTER_GAMFILE_YCBCR101010:
    case BLITTER_GAMFILE_A1:
    case BLITTER_GAMFILE_A8:
    case BLITTER_GAMFILE_XY:
    case BLITTER_GAMFILE_XYL:
    case BLITTER_GAMFILE_XYC:
    case BLITTER_GAMFILE_XYLC:
    case BLITTER_GAMFILE_RGB101010:
    default:
      printk(KERN_ERR "GAM format %.8x not supported!\n",
             (enum gamfile_type) header_file.Type);
      res = -EPERM;
      goto out_file;
  }

  printk (KERN_INFO "Writing GAM header file...\n");
  f->f_op->write (f, (void *) &header_file, sizeof (header_file),
                  &f->f_pos);

  printk (KERN_INFO "Writing Bitmap data to file...\n");
  if ((header_file.Type == BLITTER_GAMFILE_RGB888)
      || (header_file.Type == BLITTER_GAMFILE_ARGB8565)
      || (header_file.Type == BLITTER_GAMFILE_YCBCR888))
  {
    /* 24bits bitmap types... */
    unsigned int i, j;
    char PixelsBuffer[4] = { 0x0, 0x0, 0x0, 0x80 };

    for (i = 0, j = 0; i < Bitmap_p->size.h; ++i)
    {
      j = i * Bitmap_p->pitch;
      while (j <  (Bitmap_p->pitch * (i + 1) - 1))
      {
        if (header_file.Type == BLITTER_GAMFILE_YCBCR888)
        {
          PixelsBuffer[1] = ((((char *)Bitmap_p->memory)[j++]) & 0xff);
          PixelsBuffer[0] = ((((char *)Bitmap_p->memory)[j++]) & 0xff);
          PixelsBuffer[2] = ((((char *)Bitmap_p->memory)[j++]) & 0xff);
        }
        else
        {
          PixelsBuffer[0] = ((((char *)Bitmap_p->memory)[j++]) & 0xff);
          PixelsBuffer[1] = ((((char *)Bitmap_p->memory)[j++]) & 0xff);
          PixelsBuffer[2] = ((((char *)Bitmap_p->memory)[j++]) & 0xff);
        }
        f->f_op->write (f, (void *) &PixelsBuffer, 4, &f->f_pos);
      }
      /* Recompute pitch value */
      if (((Bitmap_p->size.w * 3) % 4) != 0)
        ((char *) Bitmap_p->memory)[j] = 0;
    }
  }
  else
    /* write entire buffer */
    f->f_op->write (f, Bitmap_p->memory, file_size, &f->f_pos);

out_file:
  filp_close (f, NULL);

out_fs:
  set_fs (old_fs);

  if (!res)
    printk (KERN_INFO "GAM file '%s' saved successfully\n", FileName);
  else
    printk(KERN_ERR "Error while saving the GAM file!\n");

  return res;
}
EXPORT_SYMBOL(save_gam_file);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akram BEN BELGACEM <akram.ben-belgacem@st.com>");
MODULE_DESCRIPTION("GAM File library routines");
