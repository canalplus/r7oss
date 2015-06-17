/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfbinfo.h
 * Copyright (c) 2005-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
//#define DEBUG
//#define DEBUG_UPDATES
#define DEBUG_CLUT 0

#ifndef _STMFBINFO_H
#define _STMFBINFO_H

#include <linux/suspend.h>
#include <linux/stm/stmcorehdmi.h>


struct stmfb_info {
  struct fb_info         info;

  struct platform_device *platformDevice;

  struct semaphore       framebufferLock;
  spinlock_t             framebufferSpinLock;

  wait_queue_head_t      framebuffer_updated_wait_queue;
  volatile int           num_outstanding_updates;

  struct bpa2_part      *FBPart;               /* framebuffer memory partition                             */
  unsigned long          ulPFBBase;            /* device physical pointer to framebuffer memory            */
  unsigned long          ulFBSize;             /* size of framebuffer memory                               */

  struct bpa2_part      *AuxPart[STMFBGP_GFX_LAST]; /* aux memory partition                                */
  unsigned long          AuxBase[STMFBGP_GFX_LAST]; /* device physical pointer to aux memory               */
  unsigned long          AuxSize[STMFBGP_GFX_LAST]; /* size of aux memory                                  */

  int                         display_device_id;    /* display device id                                             */
  stm_display_device_h        display_device;       /* handle to the display device                                  */
  stm_display_plane_h         hFBPlane;             /* handle to the framebuffer plane                               */
  stm_display_source_h        hFBSource;            /* handle to the framebuffer source                              */
  stm_display_source_queue_h  hQueueInterface;      /* handle to the queue interface                                 */
  stm_display_output_h        hFBMainOutput;        /* handle to the display output used to set the FB video timings */
  stm_display_output_h        hFBDVO;               /* handle to the DVO output (optional)                      */
  stm_display_output_h        hFBHDMI;              /* handle to the HDMI output (optional)                     */

  unsigned long          default_sd_encoding;

  /* current video mode and valid flag */
  int                    opens;
  int                    fbdev_api_suspended;
  stm_display_mode_t     current_videomode;
  volatile int           current_videomode_valid;
  /* current plane config and valid flag */
  struct stmfbio_plane_config current_planeconfig;
  volatile int                current_planeconfig_valid; /* 0: no, 1: yes, 2: yes but needs to be requeued, */
                                                         /* e.g. because the output configuration changed   */

  /* current extended var info for the framebuffer plane */
  struct stmfbio_var_screeninfo_ex2 current_var_ex;

  /* current output configuration information */
  struct stmfbio_output_configuration main_config;

  /* current 3D configuration */
  struct stmfbio_3d_configuration current_3d_config;

  /* framebuffer plane buffer descriptor for the current display setup */
  stm_display_buffer_t   current_buffer_setup;

  /*
   * Linux framebuffer stuff
   */
  struct fb_videomode    videomodedb[STM_TIMING_MODE_COUNT];
  unsigned long         *pFBCLUT;             /* dma coherent pointer to CLUT for framebuffer plane    */
  dma_addr_t             dmaFBCLUT;           /* device physical pointer to CLUT for framebuffer plane */
  unsigned long          pseudo_palette[16];

  /*
   * HDMI device reference.
   */
  struct file hdmi_dev;
  const struct file_operations *hdmi_fops;

};


/*
 * Main file ops structure used to register a framebuffer can be found in
 * stmfbops.c
 */
extern struct fb_ops stmfb_ops;

/*
 * Framebuffer IOCTL implementation can be found in stmfbioctl.c
 */
int stmfb_ioctl(struct fb_info* fb, u_int cmd, u_long arg);

/*
 * Framebuffer VAR handling and general display plane configuration can
 * be found in stmfbvar.c
 */
u32 stmfb_bitdepth_for_pixelformat (stm_pixel_format_t format);
u32 stmfb_pitch_for_config (const struct stmfbio_plane_config * const c);

int stmfb_verify_baseaddress (const struct stmfb_info           * const i,
                              const struct stmfbio_plane_config * const c,
                              unsigned long                      plane_size,
                              unsigned long                      baseaddr);
/* this also updates the pitch */
int stmfb_verify_planeinfo (const struct stmfb_info        * const i,
                            const struct stmfbio_planeinfo * const plane);

int stmfb_set_plane_pan (const struct stmfbio_plane_pan * const pan,
                         struct stmfb_info              * const i);
int stmfb_set_planemode (const struct stmfbio_planeinfo *plane,
                         struct stmfb_info              *i);

int stmfb_get_outputstandards (struct stmfb_info              * const i,
                               struct stmfbio_outputstandards * const stds);

int stmfb_outputinfo_to_videomode (const struct stmfb_info         * const i,
                                   const struct stmfbio_outputinfo * const output,
                                   stm_display_mode_t              * const vm);

int stmfb_videomode_to_outputinfo (const stm_display_mode_t  * const vm,
                                   struct stmfbio_outputinfo * const output);

int stmfb_set_videomode (enum stmfbio_output_id        output,
                         const stm_display_mode_t    * const vm,
                         struct stmfb_info           * const i);

int stmfb_decode_var (const struct fb_var_screeninfo * const v,
                      stm_display_mode_t             * const vm,
                      struct stmfbio_planeinfo       * const plane,
                      const struct stmfb_info        * const i) __attribute__((nonnull(1,3,4)));

int stmfb_encode_var (struct fb_var_screeninfo       * const v,
                      const stm_display_mode_t       * const vm,
                      const struct stmfbio_planeinfo * const plane,
                      const struct stmfb_info        * const i);

int stmfb_queuebuffer (struct stmfb_info * const i);
int stmfb_set_var_ex (struct stmfbio_var_screeninfo_ex2 * const v,
                      struct stmfb_info                 * const i);
int stmfb_encode_var_ex (struct stmfbio_var_screeninfo_ex2 * const v,
                         const struct stmfb_info           * const i);
int stmfb_set_panelmode (enum stmfbio_output_id        output,
                         const stm_display_mode_t     * const vm,
                         struct stmfb_info            * const i);

int stmfb_encode_mode (const stm_display_mode_t     * const vm,
                       struct stmfb_info            * const i);

/*
 * Output configuration
 */
void stmfb_initialise_output_config (stm_display_output_h                  out,
                                     struct stmfbio_output_configuration * const config);
int stmfb_set_output_configuration (struct stmfbio_output_configuration * const c,
                                    struct stmfb_info                   * const i);
int stmfb_get_output_configuration (struct stmfbio_output_configuration * const c,
                                    struct stmfb_info                   * const i);

int stmfb_set_3d_configuration(struct stmfb_info * const i,
                               const struct stmfbio_3d_configuration * const c);

void stmfb_set_videomode_3d_flags(struct stmfb_info  * const i,
                                  stm_display_mode_t * const vm);

void stmfb_sync_fb_with_output (struct stmfb_info * const i);
#if defined(SDK2_ENABLE_STMFB_ATTRIBUTES)
/*
 * Sysfs implementation can be found in stmfbsysfs.c
 */
void stmfb_init_class_device    (struct stmfb_info * const fb_info);
void stmfb_cleanup_class_device (struct stmfb_info * const fb_info);
#endif

#undef DPRINTK
#ifdef DEBUG
#define DPRINTK(a,b...) printk("stgfb: %s: " a, __FUNCTION__ ,##b)
#else
#define DPRINTK(a,b...)
#endif

#endif /* _STMFBINFO_H */
