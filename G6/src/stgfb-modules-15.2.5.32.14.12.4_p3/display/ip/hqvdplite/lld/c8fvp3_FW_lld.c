#include "c8fvp3_FW_lld.h"
#include "hqvdp_mailbox_macros.h"

#define CEIL(X) ((X-(gvh_u32_t)(X)) > 0 ? (gvh_u32_t)(X+1) : (gvh_u32_t)(X))

void c8fvp3_SetVsyncMode(gvh_u32_t base_address, gvh_u32_t vsyncmode) {
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_SOFT_VSYNC_OFFSET, vsyncmode & mailbox_SOFT_VSYNC_VSYNC_MODE_MASK);
}

void c8fvp3_SetCmdAddress(gvh_u32_t base_address, gvh_u32_t cmd_address) {
  WriteRegister(base_address +  HQVDP_LITE_MBX_BASEADDR + mailbox_NEXT_CMD_OFFSET,  cmd_address);
}

void c8fvp3_GenSoftVsync(gvh_u32_t base_address) {
  gvh_u32_t vsync_mode;

  vsync_mode=ReadRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_SOFT_VSYNC_OFFSET);
  WriteRegister(base_address +  HQVDP_LITE_MBX_BASEADDR + mailbox_SOFT_VSYNC_OFFSET, mailbox_SOFT_VSYNC_SW_VSYNC_CTRL_MASK | vsync_mode);
  WriteRegister(base_address +  HQVDP_LITE_MBX_BASEADDR + mailbox_SOFT_VSYNC_OFFSET, vsync_mode);
}

gvh_bool_t c8fvp3_IsReady(gvh_u32_t base_address) {
  yield();
  return (((ReadRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_INFO_XP70_OFFSET) &
            mailbox_INFO_XP70_FW_READY_MASK) == mailbox_INFO_XP70_FW_READY_MASK)?1:0);
}

gvh_bool_t c8fvp3_IsCmdPending(gvh_u32_t base_address) {
  yield();
  return  (((ReadRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_SOFT_VSYNC_OFFSET) &
             mailbox_SOFT_VSYNC_CMD_PENDING_MASK) == mailbox_SOFT_VSYNC_CMD_PENDING_MASK)?1:0);
}

void c8fvp3_ClearIrqToHost(gvh_u32_t base_address) {
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_IRQ_TO_HOST_OFFSET, 0);
}

gvh_bool_t c8fvp3_GetEopStatus(gvh_u32_t base_address) {
  yield();
  return (ReadRegister (base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_INFO_HOST_OFFSET)
          & mailbox_INFO_HOST_FW_EOP_MASK) ? 1: 0;
}

void c8fvp3_ClearInfoToHost(gvh_u32_t base_address) {
  /* read modify write INFO to Host */
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_INFO_HOST_OFFSET,
                ReadRegister (base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_INFO_HOST_OFFSET)
                & ~mailbox_INFO_HOST_FW_EOP_MASK);
}

void c8fvp3_BootIp(gvh_u32_t base_address) {
  /* set softvsyncmode */
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_SOFT_VSYNC_OFFSET,
                mailbox_SOFT_VSYNC_VSYNC_MODE_MASK);

  /* authorize idle mode */
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_STARTUP_CTRL_1_OFFSET,
                mailbox_STARTUP_CTRL_1_AUTHORIZE_IDLE_MODE_MASK);

  /* soft reset */
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_SW_RESET_CTRL_OFFSET,
                mailbox_SW_RESET_CTRL_SW_RESET_CORE_MASK);

  /* fetch enable */
  WriteRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_STARTUP_CTRL_2_OFFSET,
                mailbox_STARTUP_CTRL_2_FETCH_EN_MASK);

}

gvh_bool_t c8fvp3_IsBootIpDone (gvh_u32_t base_address) {
  /* wait for reset done */
  yield();
  return (((ReadRegister(base_address + HQVDP_LITE_MBX_BASEADDR + mailbox_STARTUP_CTRL_1_OFFSET)
            & mailbox_STARTUP_CTRL_1_XP70_RESET_DONE_MASK) == mailbox_STARTUP_CTRL_1_XP70_RESET_DONE_MASK) ? 1: 0);
}

void c8fvp3_SetHvsrcLut(c8fvp3_hqvdplite_api_struct_t *FD_HQVDP_Cmd_p,
                        gvh_u8_t filter_strength_yv,
                        gvh_u8_t filter_strength_cv,
                        gvh_u8_t filter_strength_yh,
                        gvh_u8_t filter_strength_ch) {

  gvh_u8_t input_chroma_sampling, output_chroma_sampling;
  gvh_u8_t memtotv;
  gvh_u32_t input_format, output_format;
  gvh_u32_t output_3d_format;
  gvh_u32_t input_3d_format;
  gvh_bool_t OuputIsSbs, OuputIsTab;
  gvh_bool_t InputIs444, OutputIs444;
  gvh_bool_t prog_not_int;
  gvh_bool_t dei_bypass;
  gvh_bool_t shift;
  gvh_u8_t StrengthYV, StrengthCV, StrengthYH, StrengthCH;
  gvh_u8_t shiftYH, shiftCH, shiftYV, shiftCV;
  gvh_u32_t shifth,shiftv;
  gvh_u32_t inputviewportori,inputviewportsize, outputsize;
  gvh_u32_t border_width;
  gvh_u32_t offset3d_left_width, offset3d_right_width;
  gvh_u32_t memformat;

#ifdef HQVDPLITE_API_FOR_STAPI
  memformat     = (FD_HQVDP_Cmd_p->Top.MemFormat);
  prog_not_int  = (FD_HQVDP_Cmd_p->Top.Config & TOP_CONFIG_PROGRESSIVE_MASK) >> TOP_CONFIG_PROGRESSIVE_SHIFT;
  dei_bypass    = (FD_HQVDP_Cmd_p->Csdi.Config & CSDI_CONFIG_MAIN_MODE_MASK) >> CSDI_CONFIG_MAIN_MODE_SHIFT;
#else
  memformat     = (FD_HQVDP_Cmd_p->TOP.MEM_FORMAT);
  prog_not_int  = (FD_HQVDP_Cmd_p->TOP.CONFIG & TOP_CONFIG_PROGRESSIVE_MASK) >> TOP_CONFIG_PROGRESSIVE_SHIFT;
  dei_bypass    = (FD_HQVDP_Cmd_p->CSDI.CONFIG & CSDI_CONFIG_MAIN_MODE_MASK) >> CSDI_CONFIG_MAIN_MODE_SHIFT;
#endif

  memtotv       = (memformat & TOP_MEM_FORMAT_MEM_TO_TV_MASK) >> TOP_MEM_FORMAT_MEM_TO_TV_SHIFT;
  input_format  = (memformat & TOP_MEM_FORMAT_INPUT_FORMAT_MASK) >> TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT;
  output_format = (memformat & TOP_MEM_FORMAT_OUTPUT_FORMAT_MASK) >> TOP_MEM_FORMAT_OUTPUT_FORMAT_SHIFT;

  output_3d_format = (memformat & TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_MASK) >> TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_SHIFT ;
  input_3d_format = (memformat & TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK) >> TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT ;

  OuputIsSbs = (output_3d_format == FORMAT_3DTV_SBS)?1:0;
  OuputIsTab = (output_3d_format == FORMAT_3DTV_TAB)?1:0;

  input_chroma_sampling  = (input_format & CHROMA_SAMPLING_MASK) >> CHROMA_SAMPLING_SHIFT;
  output_chroma_sampling = (output_format & CHROMA_SAMPLING_MASK) >> CHROMA_SAMPLING_SHIFT;

  InputIs444 = ((input_chroma_sampling == CHROMA_SAMPLING_444) || (input_chroma_sampling == CHROMA_SAMPLING_NO_ALPHA_444))?1:0;
  OutputIs444 = ((output_chroma_sampling == CHROMA_SAMPLING_444) || (memtotv))?1:0;

  StrengthYV = filter_strength_yv;
  StrengthYH = filter_strength_yh;
  StrengthCV = filter_strength_cv;
  StrengthCH = filter_strength_ch;

  if ((prog_not_int == 0) && (dei_bypass == CSDI_CONFIG_MAIN_MODE_BYPASS)) {
    shift = 1;
  } else {
    shift = 0;
  }


#ifdef HQVDPLITE_API_FOR_STAPI
  inputviewportori     = (FD_HQVDP_Cmd_p->Top.InputViewportOri);
  inputviewportsize    = (FD_HQVDP_Cmd_p->Top.InputViewportSize);
  offset3d_left_width  = (FD_HQVDP_Cmd_p->Top.LeftView3doffsetWidth);
  offset3d_right_width = (FD_HQVDP_Cmd_p->Top.RightViewBorderWidth);
  outputsize           = (FD_HQVDP_Cmd_p->Hvsrc.OutputPictureSize);
#else
  inputviewportori     = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI);
  inputviewportsize    = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_SIZE);
  offset3d_left_width  = (FD_HQVDP_Cmd_p->TOP.LEFT_VIEW_3DOFFSET_WIDTH);
  offset3d_right_width = (FD_HQVDP_Cmd_p->TOP.RIGHT_VIEW_3DOFFSET_WIDTH);
  outputsize           = (FD_HQVDP_Cmd_p->HVSRC.OUTPUT_PICTURE_SIZE);
#endif

  if (output_3d_format == FORMAT_3DTV_SBS) {
    border_width = (((offset3d_left_width  & TOP_LEFT_VIEW_BORDER_WIDTH_LEFT_MASK)   >> TOP_LEFT_VIEW_BORDER_WIDTH_LEFT_SHIFT)  +
                   ((offset3d_left_width  & TOP_LEFT_VIEW_BORDER_WIDTH_RIGHT_MASK)  >> TOP_LEFT_VIEW_BORDER_WIDTH_RIGHT_SHIFT)  +
                   ((offset3d_right_width & TOP_RIGHT_VIEW_BORDER_WIDTH_LEFT_MASK)  >> TOP_RIGHT_VIEW_BORDER_WIDTH_LEFT_SHIFT)  +
                   ((offset3d_right_width & TOP_RIGHT_VIEW_BORDER_WIDTH_RIGHT_MASK) >> TOP_RIGHT_VIEW_BORDER_WIDTH_RIGHT_SHIFT)) >> 1;
  } else if (output_3d_format == FORMAT_3DTV_TAB) {
    border_width = ((offset3d_left_width  & TOP_LEFT_VIEW_BORDER_WIDTH_LEFT_MASK)  >> TOP_LEFT_VIEW_BORDER_WIDTH_LEFT_SHIFT) +
                   ((offset3d_left_width  & TOP_LEFT_VIEW_BORDER_WIDTH_RIGHT_MASK) >> TOP_LEFT_VIEW_BORDER_WIDTH_RIGHT_SHIFT);
  } else {
    border_width = ((offset3d_left_width  & TOP_LEFT_VIEW_BORDER_WIDTH_LEFT_MASK)  >> TOP_LEFT_VIEW_BORDER_WIDTH_LEFT_SHIFT) +
                   ((offset3d_left_width  & TOP_LEFT_VIEW_BORDER_WIDTH_RIGHT_MASK) >> TOP_LEFT_VIEW_BORDER_WIDTH_RIGHT_SHIFT);
  }


  HVSRC_getFilterCoefHV(((inputviewportori & TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_MASK) >> TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_SHIFT),
                        (((inputviewportsize & TOP_INPUT_VIEWPORT_SIZE_WIDTH_MASK) >> TOP_INPUT_VIEWPORT_SIZE_WIDTH_SHIFT) << ((input_3d_format == DOLBY_BASE_ENHANCED_LAYER)?1:0)) + border_width,
                        ((outputsize & HVSRC_OUTPUT_PICTURE_SIZE_WIDTH_MASK) >> HVSRC_OUTPUT_PICTURE_SIZE_WIDTH_SHIFT),
                        (((inputviewportsize & TOP_INPUT_VIEWPORT_SIZE_HEIGHT_MASK) >> TOP_INPUT_VIEWPORT_SIZE_HEIGHT_SHIFT) >> shift),
                        ((outputsize & HVSRC_OUTPUT_PICTURE_SIZE_HEIGHT_MASK) >> HVSRC_OUTPUT_PICTURE_SIZE_HEIGHT_SHIFT),
                        StrengthYV,
                        StrengthCV,
                        StrengthYH,
                        StrengthCH,
                        InputIs444,
                        OutputIs444,
                        OuputIsSbs,
                        OuputIsTab,
#ifdef HQVDPLITE_API_FOR_STAPI
                        FD_HQVDP_Cmd_p->Hvsrc.YhCoef,
                        FD_HQVDP_Cmd_p->Hvsrc.ChCoef,
                        FD_HQVDP_Cmd_p->Hvsrc.YvCoef,
                        FD_HQVDP_Cmd_p->Hvsrc.CvCoef,
#else
                        FD_HQVDP_Cmd_p->HVSRC.YH_COEF,
                        FD_HQVDP_Cmd_p->HVSRC.CH_COEF,
                        FD_HQVDP_Cmd_p->HVSRC.YV_COEF,
                        FD_HQVDP_Cmd_p->HVSRC.CV_COEF,
#endif
                        &shiftYH,
                        &shiftCH,
                        &shiftYV,
                        &shiftCV);

  shifth = (gvh_u32_t) (shiftYH << HVSRC_HORI_SHIFT_Y_SHIFT) | (shiftCH << HVSRC_HORI_SHIFT_C_SHIFT);
  shiftv = (gvh_u32_t) (shiftYV << HVSRC_VERT_SHIFT_Y_SHIFT) | (shiftCV << HVSRC_VERT_SHIFT_C_SHIFT);

#ifdef HQVDPLITE_API_FOR_STAPI
  FD_HQVDP_Cmd_p->Hvsrc.HoriShift = shifth;
  FD_HQVDP_Cmd_p->Hvsrc.VertShift = shiftv;
#else
  FD_HQVDP_Cmd_p->HVSRC.HORI_SHIFT = shifth;
  FD_HQVDP_Cmd_p->HVSRC.VERT_SHIFT = shiftv;
#endif

}

gvh_u32_t c8fvp3_Set3DViewPort(c8fvp3_hqvdplite_api_struct_t *FD_HQVDP_Cmd_p, gvh_s16_t offset3D) {

/*  gvh_bool_t right_not_left;
  gvh_u32_t input_3d_format;*/
  gvh_u32_t offset_left_Lx, offset_left_Rx;
  gvh_u32_t offset_right_Lx, offset_right_Rx;
  gvh_u32_t VpLx, VpRx, VpLy, VpRy;
  gvh_u32_t VpW, VpH;
  gvh_u16_t wL;
  gvh_s32_t tmp;

#ifdef HQVDPLITE_API_FOR_STAPI
/*  input_3d_format = (FD_HQVDP_Cmd_p->Top.MemFormat & TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK) >> TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT ;
  right_not_left  = (FD_HQVDP_Cmd_p->Top.MemFormat & TOP_MEM_FORMAT_RIGHTNOTLEFT_MASK) >> TOP_MEM_FORMAT_RIGHTNOTLEFT_SHIFT; */
  VpLx    = (FD_HQVDP_Cmd_p->Top.InputViewportOri & TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_MASK) >> TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_SHIFT;
  VpRx    = (FD_HQVDP_Cmd_p->Top.InputViewportOriRight & TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTX_MASK) >> TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTX_SHIFT;
  VpLy    = (FD_HQVDP_Cmd_p->Top.InputViewportOri & TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_MASK) >> TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_SHIFT;
  VpRy    = (FD_HQVDP_Cmd_p->Top.InputViewportOriRight & TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTY_MASK) >> TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTY_SHIFT;
  VpW     = (FD_HQVDP_Cmd_p->Top.InputViewportSize & TOP_INPUT_VIEWPORT_SIZE_WIDTH_MASK) >> TOP_INPUT_VIEWPORT_SIZE_WIDTH_SHIFT;
  VpH     = (FD_HQVDP_Cmd_p->Top.InputViewportSize & TOP_INPUT_VIEWPORT_SIZE_HEIGHT_MASK) >> TOP_INPUT_VIEWPORT_SIZE_HEIGHT_SHIFT;
  wL      = (FD_HQVDP_Cmd_p->Top.InputFrameSize & TOP_INPUT_FRAME_SIZE_WIDTH_MASK) >> TOP_INPUT_FRAME_SIZE_WIDTH_SHIFT;
#else
/*  input_3d_format = (FD_HQVDP_Cmd_p->TOP.MEM_FORMAT & TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK) >>  TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT;
  right_not_left  = (FD_HQVDP_Cmd_p->TOP.MEM_FORMAT & TOP_MEM_FORMAT_RIGHTNOTLEFT_MASK) >> TOP_MEM_FORMAT_RIGHTNOTLEFT_SHIFT; */
  VpLx    = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI & TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_MASK) >> TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_SHIFT;
  VpRx    = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI_RIGHT & TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTX_MASK) >> TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTX_SHIFT;
  VpLy    = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI & TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_MASK) >> TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_SHIFT;
  VpRy    = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI_RIGHT & TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTY_MASK) >> TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTY_SHIFT;
  VpW     = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_SIZE & TOP_INPUT_VIEWPORT_SIZE_WIDTH_MASK) >> TOP_INPUT_VIEWPORT_SIZE_WIDTH_SHIFT;
  VpH     = (FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_SIZE & TOP_INPUT_VIEWPORT_SIZE_HEIGHT_MASK) >> TOP_INPUT_VIEWPORT_SIZE_HEIGHT_SHIFT;
  wL      = (FD_HQVDP_Cmd_p->TOP.INPUT_FRAME_SIZE & TOP_INPUT_FRAME_SIZE_WIDTH_MASK) >> TOP_INPUT_FRAME_SIZE_WIDTH_SHIFT;
#endif

  offset_left_Lx  = 0;
  offset_left_Rx  = 0;
  offset_right_Lx = 0;
  offset_right_Rx = 0;

  if (offset3D < 0) {
    tmp = VpRx + VpW + (0 - offset3D) - wL;
    if (tmp > 0) {
      offset_right_Rx = (gvh_u16_t) (tmp);
    }
    // Define the right border width
    tmp = VpLx - (0 - offset3D);
    if (tmp < 0) {
      offset_left_Lx = (gvh_u16_t)(0 - tmp);
    }
    // Choice the max border width
    if (offset_right_Rx < offset_left_Lx) {
      offset_right_Rx = offset_left_Lx;
    } else {
      offset_left_Lx = offset_right_Rx;
    }
    VpW  -= offset_right_Rx;
    VpRx += (0 - offset3D);
    VpLx -= ((0 - offset3D) - offset_right_Rx);
  } else if (offset3D > 0) {
    // Define the left border width
    tmp = VpLx + VpW + offset3D - wL;
    if (tmp > 0) {
      offset_left_Rx = (gvh_u16_t) (tmp);
    }
    // Define the right border width
    tmp = VpRx - offset3D;
    if (tmp < 0) {
      offset_right_Lx = (gvh_u16_t)(0 - tmp);
    }
    // Choice the max border width
    if (offset_left_Rx < offset_right_Lx) {
      offset_left_Rx = offset_right_Lx;
    } else {
      offset_right_Lx = offset_left_Rx;
    }
    VpW  -= offset_left_Rx;
    VpLx += offset3D;
    VpRx -= (offset3D - offset_left_Rx);
    ///offset_right_Lx = 0;
  }

#ifdef HQVDPLITE_API_FOR_STAPI
  FD_HQVDP_Cmd_p->Top.InputViewportOri       = (VpLy << TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_SHIFT) | (VpLx << TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_SHIFT);
  FD_HQVDP_Cmd_p->Top.InputViewportOriRight  = (VpRy << TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTY_SHIFT) | (VpRx << TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTX_SHIFT);
  FD_HQVDP_Cmd_p->Top.InputViewportSize      = (VpH << TOP_INPUT_VIEWPORT_SIZE_HEIGHT_SHIFT) | (VpW << TOP_INPUT_VIEWPORT_SIZE_WIDTH_SHIFT);
  FD_HQVDP_Cmd_p->Top.LeftView3doffsetWidth  = (gvh_u32_t)((offset_left_Lx << TOP_LEFT_VIEW_3DOFFSET_WIDTH_LEFT_SHIFT) | (offset_left_Rx << TOP_LEFT_VIEW_3DOFFSET_WIDTH_RIGHT_SHIFT));
  FD_HQVDP_Cmd_p->Top.RightView3doffsetWidth = (gvh_u32_t)((offset_right_Lx << TOP_RIGHT_VIEW_3DOFFSET_WIDTH_LEFT_SHIFT) | (offset_right_Rx << TOP_RIGHT_VIEW_3DOFFSET_WIDTH_RIGHT_SHIFT));
#else
  FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI        = (VpLy << TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_SHIFT) | (VpLx << TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_SHIFT);
  FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_ORI_RIGHT  = (VpRy << TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTY_SHIFT) | (VpRx << TOP_INPUT_VIEWPORT_ORI_RIGHT_VIEWPORTX_SHIFT);
  FD_HQVDP_Cmd_p->TOP.INPUT_VIEWPORT_SIZE       = (VpH << TOP_INPUT_VIEWPORT_SIZE_HEIGHT_SHIFT) | (VpW << TOP_INPUT_VIEWPORT_SIZE_WIDTH_SHIFT);
  FD_HQVDP_Cmd_p->TOP.LEFT_VIEW_3DOFFSET_WIDTH  = (gvh_u32_t)((offset_left_Lx << TOP_LEFT_VIEW_3DOFFSET_WIDTH_LEFT_SHIFT) | (offset_left_Rx << TOP_LEFT_VIEW_3DOFFSET_WIDTH_RIGHT_SHIFT));
  FD_HQVDP_Cmd_p->TOP.RIGHT_VIEW_3DOFFSET_WIDTH = (gvh_u32_t)((offset_right_Lx << TOP_RIGHT_VIEW_3DOFFSET_WIDTH_LEFT_SHIFT) | (offset_right_Rx << TOP_RIGHT_VIEW_3DOFFSET_WIDTH_RIGHT_SHIFT));
#endif

  return HQVDP_NO_ERROR;
}
