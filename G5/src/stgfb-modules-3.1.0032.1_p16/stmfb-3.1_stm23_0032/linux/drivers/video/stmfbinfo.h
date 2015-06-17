/***********************************************************************
 *
 * File: linux/drivers/video/stmfbinfo.h
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

#include <linux/stm/stmcorehdmi.h>

struct stmfb_videomode {
  ULONG                  ulTVStandard;
  const stm_mode_line_t *pModeLine;
};


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

  STMFBBDispSharedAreaPriv *pVSharedAreaBase;  /* virtual address of shared area, if available             */
  unsigned long             ulPSharedAreaBase; /* physical address of shared area, if available            */
  unsigned long             ulSharedAreaSize;  /* total size of shared area                                */

  int                    blitter_api;
  stm_display_plane_t   *pFBPlane;             /* pointer to the framebuffer plane                         */
  stm_display_output_t  *pFBMainOutput;        /* pointer to the main video output                         */
  stm_display_output_t  *pFBDVO;               /* pointer to the DVO output (optional)                     */
  stm_display_blitter_t *pBlitter;             /* pointer to the blitter (optional)                        */
  struct stm_hdmi       *hdmi;                 /* pointer to HDMI context associated with                  */
                                               /* the main output (optional)                               */
  
  unsigned long         *pBlitterCLUT;         /* dma coherent pointer to CLUT for blitter ops             */
  dma_addr_t             dmaBlitterCLUT;       /* device physical pointer to CLUT for blitter ops          */
 
  unsigned long          default_sd_encoding;

  /* current video mode and valid flag */
  int                    opens;
  int                    fbdev_api_suspended;
  struct stmfb_videomode current_videomode;
  volatile int           current_videomode_valid;
  /* current plane config and valid flag */
  struct stmfbio_plane_config current_planeconfig;
  volatile int                current_planeconfig_valid; /* 0: no, 1: yes, 2: yes but needs to be requeued, */
                                                         /* e.g. because the output configuration changed   */

  /* current extended var info for the framebuffer plane */
  struct stmfbio_var_screeninfo_ex current_var_ex;

  /* current output configuration information */
  struct stmfbio_output_configuration main_config;
  
  /* framebuffer plane buffer descriptor for the current display setup */
  stm_display_buffer_t   current_buffer_setup;

  /*
   * Linux framebuffer stuff
   */
  struct fb_videomode    videomodedb[STVTG_TIMING_MODE_COUNT];
  unsigned long         *pFBCLUT;             /* dma coherent pointer to CLUT for framebuffer plane    */
  dma_addr_t             dmaFBCLUT;           /* device physical pointer to CLUT for framebuffer plane */
  unsigned long          pseudo_palette[16];
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
u32 stmfb_bitdepth_for_pixelformat (SURF_FMT format);
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
                                   struct stmfb_videomode          * const vm);
int stmfb_videomode_to_outputinfo (const struct stmfb_videomode * const vm,
                                   struct stmfbio_outputinfo    * const output);

int stmfb_set_videomode (enum stmfbio_output_id        output,
                         const struct stmfb_videomode * const vm,
                         struct stmfb_info            * const i);

int stmfb_decode_var (const struct fb_var_screeninfo * const v,
                      struct stmfb_videomode         * const vm,
                      struct stmfbio_planeinfo       * const plane,
                      const struct stmfb_info        * const i) __attribute__((nonnull(1,3,4)));
int stmfb_encode_var (struct fb_var_screeninfo       * const v,
                      const struct stmfb_videomode   * const vm,
                      const struct stmfbio_planeinfo * const plane,
                      const struct stmfb_info        * const i);

int stmfb_queuebuffer (struct stmfb_info * const i);
int stmfb_set_var_ex (struct stmfbio_var_screeninfo_ex * const v,
                      struct stmfb_info                * const i);
int stmfb_encode_var_ex (struct stmfbio_var_screeninfo_ex * const v,
                         const struct stmfb_info          * const i);

/*
 * Output configuration
 */
void stmfb_initialise_output_config (stm_display_output_t                * const out,
                                     struct stmfbio_output_configuration * const config);
int stmfb_set_output_configuration (struct stmfbio_output_configuration * const c,
                                    struct stmfb_info                   * const i);
int stmfb_get_output_configuration (struct stmfbio_output_configuration * const c,
                                    struct stmfb_info                   * const i);

/*
 * Sysfs implementation can be found in stmfbsysfs.c
 */
void stmfb_init_class_device    (struct stmfb_info * const fb_info);
void stmfb_cleanup_class_device (struct stmfb_info * const fb_info);

#undef DPRINTK
#ifdef DEBUG
#define DPRINTK(a,b...)	printk("stgfb: %s: " a, __FUNCTION__ ,##b)
#else
#define DPRINTK(a,b...)
#endif 

#endif /* _STMFBINFO_H */
