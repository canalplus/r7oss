/***********************************************************************
 *
 * File: soc/sti7200/sti7200xvp.cpp
 * Copyright (c) 2007/2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7200flexvp.h"
#include "sti7200xvp.h"


#undef TNR_DO_MEMCPY_FALLBACK_ON_MODE_CHANGE


static const ULONG xvp_reg_WR[] =
{
  0x00600020, 0x00005a10, 0x000000e0, 0x000000c1, 0x00000002, 0x00000003, 0x00c00001, 0x00400010,
  0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x005fffe0
};

static const ULONG xvp_reg_RD[] =
{
  0x00600060, 0x00600140, 0x00600380, 0x00005a10, 0x00000120, 0x00000101, 0x00000002, 0x00000003,
  0x00c00001, 0x00400010, 0x000b0b11, 0x00000260, 0x00000201, 0x000000e2, 0x000000c3, 0x00000230,
  0x00200009, 0x00a00600, 0x00a03610, 0x00203605, 0x00400011, 0x00201606, 0x00400b00, 0x00201007,
  0x005ff300, 0x00201008, 0x005fef00, 0x00401000, 0x00058711, 0x00056712, 0x000005c0, 0x00000461,
  0x000001e2, 0x00000ea3, 0x00000550, 0x00200008, 0x00600500, 0x0020040d, 0x006005a0, 0x00600580,
  0x0020040d, 0x00600560, 0x00a00200, 0x00a00a00, 0x00a00200, 0x00a02a10, 0x00202a0a, 0x006006e0,
  0x00200a0b, 0x00401700, 0x00200009, 0x00400800, 0x0020010c, 0x005ff700, 0x005ffb00, 0x00201008,
  0x00400011, 0x00400012, 0x00000044, 0x00003705, 0x00001706, 0x00001107, 0x00001008, 0x00000809,
  0x00002b0a, 0x00000b0b, 0x0000010c, 0x0000040d, 0x005fffe0, 0x00ffffff, 0x00ffffff, 0x00ffffff,
  0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00600740
};


/* cluster */
#define XVP_CLUSTER_STREAMER_OFFSET       0x00000000
#  define XVP_CLUSTER_STREAMER_SIZE       0x00001000

#define XVP_CLUSTER_STBUS_WRPLUG_OFFSET   0x00002000
#  define XVP_CLUSTER_STBUS_WRPLUG_SIZE   0x00002000

#define XVP_CLUSTER_STBUS_RDPLUG_OFFSET   0x00004000
#  define XVP_CLUSTER_STBUS_RDPLUG_SIZE   0x00002000
   /* for both, RD and WR plugs */
#  define STBUS_PLUG_CONTROL           0x1000
#    define PLUG_Enable         (1 << 0)
#    define PLUG_Busy           (1 << 1)
#  define STBUS_PLUG_PAGE_SIZE         0x1004
#    define PLUG_Page_size_64   (0x00000000)
#    define PLUG_Page_size_128  (0x00000001)
#    define PLUG_Page_size_256  (0x00000002)
#    define PLUG_Page_size_512  (0x00000003)
#    define PLUG_Page_size_1k   (0x00000004)
#    define PLUG_Page_size_2k   (0x00000005)
#    define PLUG_Page_size_4k   (0x00000006)
#    define PLUG_Page_size_8k   (0x00000007)
#    define PLUG_Page_size_16k  (0x00000008)
#    define PLUG_Page_size_MASK (0x0000000f)
#  define STBUS_PLUG_MIN_OPC           0x1008
#  define STBUS_PLUG_MAX_OPC           0x100c
     /* for min & max */
#    define PLUG_opc_1          (0x00000000)
#    define PLUG_opc_2          (0x00000001)
#    define PLUG_opc_4          (0x00000002)
#    define PLUG_opc_8          (0x00000003)
#    define PLUG_opc_16         (0x00000004)
#    define PLUG_opc_32         (0x00000005)
#    define PLUG_opc_64         (0x00000006)
#  define STBUS_PLUG_MAX_CHK           0x1010
#    define PLUG_Max_chk_1      (0x00000000)
#    define PLUG_Max_chk_2      (0x00000001)
#    define PLUG_Max_chk_4      (0x00000002)
#    define PLUG_Max_chk_8      (0x00000003)
#    define PLUG_Max_chk_16     (0x00000004)
#    define PLUG_Max_chk_32     (0x00000005)
#    define PLUG_Max_chk_64     (0x00000006)
#    define PLUG_Max_chk_128    (0x00000007)
#  define STBUS_PLUG_MAX_MSG           0x1014
#    define PLUG_Max_msg_1      (0x00000000)
#    define PLUG_Max_msg_2      (0x00000001)
#    define PLUG_Max_msg_4      (0x00000002)
#    define PLUG_Max_msg_8      (0x00000003)
#    define PLUG_Max_msg_16     (0x00000004)
#    define PLUG_Max_msg_32     (0x00000005)
#    define PLUG_Max_msg_64     (0x00000006)
#    define PLUG_Max_msg_128    (0x00000007)
#  define STBUS_PLUG_MIN_SPACE         0x1018
#    define PLUG_Min_space_MASK (0x000003ff)

/* mailbox */
#define XVP_XP70_MAILBOX_OFFSET           0x00001000
#  define XVP_XP70_MAILBOX_SIZE           0x00001000
/* mailbox registers */
#  define MBOX_IRQ_TO_XP70        (XVP_XP70_MAILBOX_OFFSET + 0x00)
#    define MBOX_IToxP70_MASK            (1 << 0)
#  define MBOX_INFO_HOST          (XVP_XP70_MAILBOX_OFFSET + 0x04)
     /* no info to give to the firmware */
#    define MBOX_Nothing                 (0)
     /* this value is set by the host when it wants to change the firmware */
#    define MBOX_FirmwareSwitch          (1)
     /* this value is set by the host when it downloads a firmware.
        When the firmware will start running it will immediately decode
        the first host command. */
#    define MBOX_StartImmediately        (2)
     /* this value is set by the host when it downloads a firmware.
        When the firmware will start running it will go to the idle state
        and will decode the first host command only after the next VSync. */
#    define MBOX_StartAfterVsync         (3)
#    define MBOX_InfoRegisterHost_MASK   (0x0000ffff)
#  define MBOX_IRQ_TO_HOST        (XVP_XP70_MAILBOX_OFFSET + 0x08)
#    define MBOX_ITToHost_MASK           (1 << 0)
#  define MBOX_INFO_xP70          (XVP_XP70_MAILBOX_OFFSET + 0x0c)
#    define MBOX_InfoRegisterxP70_MASK   (0x0000ffff)
#  define MBOX_SW_RESET_CTRL      (XVP_XP70_MAILBOX_OFFSET + 0x10)
#    define MBOX_SWResetFullFlexVPE      (1 << 0)
#    define MBOX_SWResetStreamer         (1 << 1)
#    define MBOX_SWResetPlugs            (1 << 2)
#    define MBOX_SWResetQueues           (1 << 3)
#    define MBOX_SWResetCore             (1 << 4)
#  define MBOX_STARTUP_CTRL_1     (XVP_XP70_MAILBOX_OFFSET + 0x14)
#    define MBOX_xP70ResetDone           (1 << 0)
#    define MBOX_IdelReqCapture          (1 << 1)
#    define MBOX_AuthorizeIdleModes      (1 << 2)
#    define MBOX_IdleStatue_MASK         (0x00000078)
#    define MBOX_WakeUp                  (1 << 7)
#    define MBOX_EmulModeForDebug        (1 << 8)
#    define MBOX_EmulModeAck             (1 << 9)
#    define MBOX_PermanentQueueAuthorize (1 <<10)
#  define MBOX_STARTUP_CTRL_2     (XVP_XP70_MAILBOX_OFFSET + 0x18)
#    define MBOX_FetchEnAuto             (1 << 0)
#    define MBOX_FetchEn                 (1 << 1)
#  define MBOX_AUTHORIZE_QUEUES   (XVP_XP70_MAILBOX_OFFSET + 0x1c)
#    define MBOX_AuthorizeQueueNow       (1 << 0)
#  define MBOX_GP_STATUS          (XVP_XP70_MAILBOX_OFFSET + 0x20)
#    define MBOX_GPStatus_MASK           (0x000000ff)
#  define MBOX_NEXT_CMD           (XVP_XP70_MAILBOX_OFFSET + 0x24)
#    define MBOX_NextCommand_MASK        (0xffffffff)
#  define MBOX_CURRENT_CMD        (XVP_XP70_MAILBOX_OFFSET + 0x28)
#    define MBOX_CurrentCommand_MASK     (0xffffffff)

/* memory */
#define XVP_XP70_DMEM_OFFSET           0x00020000
#  define XVP_XP70_DMEM_SIZE           0x00010000lu
#define XVP_XP70_IMEM_OFFSET           0x00030000
#  define XVP_XP70_IMEM_SIZE           0x00004000lu



/* Status returned by the firmware in MBX_INFO_TO_HOST register. */
enum _XvpTnrError {
  /* The firmware has been sucessfull */
  TNR_NO_ERROR,
  /* The Vsync interrupt raised and pixels filtered were not fully
   sent to the external memory/VDP */
  TNR_FIRM_TOO_SLOW,
  /* In case of TNR
   A downgraded TNR was produced. It can occur in different cases:
   - a too big zoom factor was set => a downgraded TNR is produced in order
   to be real time.
   - the firmware was too slow during the previous Vsync. During the following
     Vsync, the firmware produces a downgraded TNR in order to be sure to be
     realtime (because the firmware starts late). */
  TNR_DOWNGRADED_GRAIN
};


/* Flags definition for struct _XvpTNRTransform::flags */
/* If set, this bit indicates that the TNR process is
   disabled => no processing :
   - The firmware just reads input video and outputs it as is to the VDP.
   - No field is written back to the external memory. */
#define TNR_FLAGS_DISABLE          (1 << 0)
/* If set, this bit indicates that the TNR process is bypassed =>
   - The firmware reads and writes the same buffers as when applying TNR,
     with the same reformatting and without applying any TNR.
   - Field is sent to the external memory/VDP. */
#define TNR_FLAGS_BYPASS           (1 << 1)
/* If set, this bit indicates that the noise to consider has been software
   generated. Otherwise, it means that the noise is 'classical RF' noise. */
#define TNR_FLAGS_SWNOISE_TYPE     (1 << 2)
/* If set, this bit indicates that the firmware has to send the processed
   video to the VDP. Otherwise, video is not sent to the VDP.
   Note : video is also sent to external memory in any case due to the
          recursivity of the algorithm. */
#define TNR_FLAGS_TOVDP            (1 << 3)
/* It set, this bit indicates that the DEI block is bypassed */
#define TNR_FLAGS_BYPASS_DEI       (1 << 4)
/* If set, this bit indicates that the current field is a Top (Odd) Field.
   Otherwise, it is a bottom field. */
#define TNR_FLAGS_TOP_NOTBOT       (1 << 5)
/* If set, this bit instructs the firmware to use custom Look Up Tables (LUT)
   to adapt to the NLE. Otherwise, the default LUT embedded in the firmware
   is used. */
#define TNR_FLAGS_CUSTOM_PROFILE   (1 << 6)
/* If set, this bit indicates that the characteristics of the Custom Profile
   have been updated. Otherwise, it means the firmware should keep using the
   previous profile (ie. LUT contents). */
#define TNR_FLAGS_PROFILE_UPDATED  (1 << 7)
/* If set, this bit indicates that internally computed Noise Level for the
   next field has to be overrriden by the NLE value supplied through
   parameter ExtNLE */
#define TNR_OVERRIDE_NLE           (1 << 8)
/* If set, this bit indicates that the current frame is the first of a new
   sequence, so bypass TNR algo for this frame. Otherwise, process TNR
   without exception. */
#define TNR_NEW_SEQUENCE           (1 << 9)


#define TNR_MAX_WIDTH   720
#define TNR_MAX_HEIGHT  576

/* for _XvpTNRTransform.UserZoomOutThresh */
#define TNR_THRESHOLD     0x09


/* Maximum number of elements in a LUT */
#define LUT_MAX 32

struct _XvpTnrLuts {
  /* Temporal interpolation of luma */
  ULONG LumaFactor[LUT_MAX];
  /* Temporal interpolation of chroma */
  ULONG ChromaFactor[LUT_MAX];
  /* Threshold value */
  ULONG LumaFader[LUT_MAX];
  /* This LUT defines which NLE values will enable the spatial filter */
  ULONG FilterEnable[LUT_MAX];
  /* This LUT selects between weak and strong filtering */
  ULONG FilterType[LUT_MAX];
} __attribute__((packed));


struct _XvpTNRTransform {
  /* Bitfield to pass different information to the firmware. */
  ULONG flags;
  /* Begin address (in external memory) of the current (index 'N') luma and
     chroma fields:
     - If input format is 4:2:2 single buffer, the same buffer contains both
       luma and chroma samples and is pointed to by CurrentLumaBaseAddr,
       therefore CurrentChromaBaseAddr is ignored in that case.
     - If input format is 4:2:0 omega2 MB format, the top field base address
       shall be aligned on a 512 bytes boundary (9 less significant bits
       equal to 0). */
  ULONG CurrentLumaBase;
  ULONG CurrentChromaBase;
  /* Begin addresses (in external memory) of buffers where the firmware will
     store luma and chroma fields generated by the firmware after TNR
     processing. Fields are always stored in 4:2:2 raster dual buffer (Y and
     CbCr) format.
     Only pixels belonging to the viewport are written in memory.
     It will be read back at next field iterations as previous and
     pre-previous fields. */
  ULONG DestLumaBase;
  ULONG DestChromaBase;
  /* Begin address (in external memory) of the previous (index 'N-1') luma
     and chroma fields, that were indicated by FilteredFieldNLumaBaseAddr and
     FilteredFieldNChrBaseAddr at the previous field iteration.
     These fields are previously TNR filtered fields and were stored in
     4:2:2 raster dual buffer format. */
  ULONG PreviousLumaBase;
  ULONG PreviousChromaBase;
  /* Begin address (in external memory) of the previous (index 'N-2') luma
     and chroma fields, that were indicated by FilteredFieldNLumaBaseAddr and
     FilteredFieldNChrBaseAddr at the pre-previous field iteration.
     These fields are previously TNR filtered fields and were stored in 4:2:2
     raster dual buffer format. */
  ULONG PrePreviousLumaBase;
  ULONG PrePreviousChromaBase;
  /* Input field pitch :
     - If input format is 4:2:2 raster single buffer:
       -> Number of bytes between start of 2 successive lines of a same field.
     - If input format is 4:2:0 Omega2 Macroblock:
       -> bits [10:8]: pitch in terms of number of lines in the source buffer.
       -> bits [7:0] : number of macroblocks per line. */
  ULONG FieldPitch;
  /* Size of the picture (in pixels):
     - bits 26 to 16 contain the height (>= 1). Shall be a multiple of
       16 pixels???
     - bits 10 to 0 contain the width (>= 1). Shall be a multiple of
       16 pixels??? */
  ULONG PictureSize;
  /* Top left corner coordinates of the viewport (in pixels):
     - bits 26 to 16 contains the Y coordinate (>= 0).
     - bits 10 to 0 contains the X coordinate (>= 0).
     It shall have the same value as in the DEI_VIEWPORT_ORIG register. */
  ULONG ViewPortOrigin;
  /* Viewport size (in pixels):
     - bits 26 to 16 contains the height (>= 2).
     - bits 10 to 0 contains the width (>= 2).
     It shall have the same value as in the DEI_VIEWPORT_SIZE register. */
  ULONG ViewPortSize;
  /* This field contains the vertical and horizontal luma sample rate
     converter state-machine increments, in 3.13 format.
     - bits 31 to 16 contains the vertical increment
       It shall have the same value as in the VHSRC_Y_VSRC register.
     - bits 15 to 0 contains the horizontal increment
       It shall have the same value as in the VHSRC_Y_HSRC register. */
  ULONG LumaZoom;
  /* This field contains the user zoom out in 2.3 format (bits 4 to 0). */
  ULONG UserZoomOut;
  /* This field contains the user zoom out thresholds, in 2.3 format:
     - bits 28 to 24 contains the second interlaced threshold
     - bits 20 to 16 contains the first interlaced threshold
     - bits 12 to 8 contains the second progressive threshold
     - bits 4 to 0 contains the first progressive threshold */
  ULONG UserZoomOutThresh;
  /* External NLE value to be used by the firmware, if TNR_OVERRIDE_NLE_BIT
     flag is set. Otherwise the ExtNLE parameters is ignored.
     TNR_OVERRIDE_NLE_BIT = 0 -> firmware uses NLE computed by internal TNR
                                 algorithm.
     TNR_OVERRIDE_NLE_BIT = 1 -> firmware uses NLE computed by external
                                 host. */
  ULONG ExtNLE;
  /* This field is used by the firmware only if the
     TNR_FLAGS_CUSTOM_PROFILE_BIT bit is set. It contains the address in
     external memory of a record containing the LUTs. */
  struct _XvpTnrLuts *CustomProfile;
} __attribute__((packed));






CSTi7200xVP::CSTi7200xVP (const CSTi7200FlexVP * const flexvp,
                          ULONG                 baseAddr)
{
  DEBUGF2 (4, (FENTRY ", baseAddr: %lx, self: %p\n",
               __PRETTY_FUNCTION__, baseAddr, this));

  m_FlexVP     = flexvp;
  m_xVPAddress = baseAddr;

  m_eMode  = PCxVPC_BYPASS;
  m_eState = XVPSTATE_IDLE;

  m_CurrentTNRMailbox = 0;

  m_TNRFieldN = 0;
  for (ULONG i = 0; i < TNR_FRAME_COUNT; ++i)
    g_pIOS->ZeroMemory (&m_TNRFilteredFrame[i], sizeof (DMA_Area));
  g_pIOS->ZeroMemory (&m_TNRMailbox[0], sizeof (DMA_Area));
  g_pIOS->ZeroMemory (&m_TNRMailbox[1], sizeof (DMA_Area));

  m_bUseTNR = true;

  m_imem = 0;
  m_dmem = 0;

  m_NLE_Override = 15;
}



CSTi7200xVP::~CSTi7200xVP (void)
{
  DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));

  xp70deinit ();

  g_pIOS->FreeDMAArea (&m_TNRMailbox[0]);
  g_pIOS->FreeDMAArea (&m_TNRMailbox[1]);
  for (ULONG i = 0; i < TNR_FRAME_COUNT; ++i)
    g_pIOS->FreeDMAArea (&m_TNRFilteredFrame[i]);

  if (LIKELY (m_imem))
    g_pIOS->ReleaseFirmware (m_imem);
  if (LIKELY (m_dmem))
    g_pIOS->ReleaseFirmware (m_dmem);
}


void
CSTi7200xVP::Create (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  /* allocate some memory */
  if (m_bUseTNR)
    {
      for (ULONG i = 0; i < TNR_FRAME_COUNT; ++i)
        {
          g_pIOS->AllocateDMAArea (&m_TNRFilteredFrame[i],
                                   /* alloc enough for one complete frame
                                      at full PAL in SURF_YCBCR422R color-
                                      format */
                                   TNR_MAX_WIDTH * 2 * TNR_MAX_HEIGHT,
                                   512,
                                   /* FIXME: decide on which one to use! */
                                   SDAAF_NONE);
//                                   SDAAF_VIDEO_MEMORY);
          if (!m_TNRFilteredFrame[i].pMemory)
            {
              DEBUGF (("%s could not allocate DMA memory for TNR filtering\n:",
                       __PRETTY_FUNCTION__));
              m_bUseTNR = false;
              break;
            }
          DEBUGF (("%s: pMemory/ulPhysical TNR mem @ %p/%.8lx\n",
                   __PRETTY_FUNCTION__,
                   m_TNRFilteredFrame[i].pMemory,
                   m_TNRFilteredFrame[i].ulPhysical));
        }

      if (m_bUseTNR)
        {
          g_pIOS->AllocateDMAArea (&m_TNRMailbox[0],
                                   sizeof (struct _XvpTNRTransform)
                                   + sizeof (struct _XvpTnrLuts),
                                   16,
                                   SDAAF_NONE);
          g_pIOS->AllocateDMAArea (&m_TNRMailbox[1],
                                   sizeof (struct _XvpTNRTransform)
                                   + sizeof (struct _XvpTnrLuts),
                                   16,
                                   SDAAF_NONE);
          if (!m_TNRMailbox[0].pMemory
              || !m_TNRMailbox[1].pMemory)
            {
              DEBUGF (("%s could not allocate DMA memory for TNR!\n",
                       __PRETTY_FUNCTION__));
              m_bUseTNR = false;
            }
        }
    }

  cluster_init ();
  xp70init ();
}


ULONG
CSTi7200xVP::GetCapabilities (void) const
{
  DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));
  return PLANE_CTRL_CAPS_FILMGRAIN
         | (m_bUseTNR ? PLANE_CTRL_CAPS_TNR : 0);
}


bool
CSTi7200xVP::SetControl (stm_plane_ctrl_t control,
                         ULONG            value)
{
  bool retval;

  DEBUGF2 (8, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_XVP_CONFIG:
      {
      enum PlaneCtrlxVPConfiguration config =
        (enum PlaneCtrlxVPConfiguration) value;

      switch (config)
        {
        case PCxVPC_BYPASS:
          m_eMode  = PCxVPC_BYPASS;
          m_eState = XVPSTATE_IDLE;
          ReleaseFirmwares ();
          retval = true;
          break;

        case PCxVPC_FILMGRAIN:
          retval = xp70PrepareFirmwareUpload (config);
          m_eMode = retval ? config : PCxVPC_BYPASS;
          break;

        case PCxVPC_TNR:
        case PCxVPC_TNR_BYPASS:
        case PCxVPC_TNR_NLEOVER:
          if (m_eMode != PCxVPC_TNR
              && m_eMode != PCxVPC_TNR_BYPASS
              && m_eMode != PCxVPC_TNR_NLEOVER)
            {
              retval = m_bUseTNR ? xp70PrepareFirmwareUpload (config) : false;
              m_eMode = retval ? config : PCxVPC_BYPASS;
            }
          else
            {
              m_eMode = config;
              retval = true;
            }
          break;

        default:
          retval = false;
          break;
        }
      }
      break;

    case PLANE_CTRL_XVP_TNRNLE_OVERRIDE:
      m_NLE_Override = value & 0xff;
      retval = true;
      break;

    default:
      retval = false;
      break;
    }

  return retval;
}


bool
CSTi7200xVP::GetControl (stm_plane_ctrl_t  control,
                         ULONG            *value) const
{
  bool retval;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_XVP_CONFIG:
      switch (m_eMode)
        {
        case PCxVPC_BYPASS:
        case PCxVPC_FILMGRAIN:
        case PCxVPC_TNR:
        case PCxVPC_TNR_BYPASS:
        case PCxVPC_TNR_NLEOVER:
          *value = m_eMode;
          retval = true;
          break;

        default:
          ASSERTF (1 == 2, ("%s: shouldn't be reached!\n",
                            __PRETTY_FUNCTION__));
          retval = false;
          break;
        }
      break;

    case PLANE_CTRL_XVP_TNRNLE_OVERRIDE:
      *value = m_NLE_Override;
      retval = true;
      break;

    default:
      retval = false;
      break;
    }

  return retval;
}


void
CSTi7200xVP::cluster_init (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  /* load RD and WR plugs with microcode */
  for (ULONG i = 0; i < N_ELEMENTS (xvp_reg_RD); ++i)
    WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + (i * 4),
                 xvp_reg_RD[i]);

  for (ULONG i = 0; i < N_ELEMENTS (xvp_reg_WR); ++i)
    WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + (i * 4),
                 xvp_reg_WR[i]);

  /* configure STBus RD PLUG registers */
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_PAGE_SIZE, PLUG_Page_size_16k);
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_MIN_OPC,   PLUG_opc_8);
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_MAX_OPC,   PLUG_opc_32);
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_MAX_CHK,   PLUG_Max_chk_2);
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_MAX_MSG,   PLUG_Max_msg_2);
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_MIN_SPACE, 0x00);
  WriteXVPReg (XVP_CLUSTER_STBUS_RDPLUG_OFFSET + STBUS_PLUG_CONTROL,   PLUG_Enable);

  /* configure STBus WR PLUG registers */
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_PAGE_SIZE, PLUG_Page_size_16k);
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_MIN_OPC,   PLUG_opc_8);
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_MAX_OPC,   PLUG_opc_32);
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_MAX_CHK,   PLUG_Max_chk_2);
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_MAX_MSG,   PLUG_Max_msg_2);
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_MIN_SPACE, 0x00);
  WriteXVPReg (XVP_CLUSTER_STBUS_WRPLUG_OFFSET + STBUS_PLUG_CONTROL,   PLUG_Enable);

  /* initialize the streamer */
  /* STBRD */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x000, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x004, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x008, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x00c, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x010, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x014, 0x00000000);
  /* STBWR */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x200, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x204, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x208, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x20C, 0x00000000);
  /* WRQ0 */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x600, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x604, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x608, 0x00000000);
  /* WRQ1 */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x640, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x644, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x648, 0x00000000);
  /* ITCTX */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x800, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x804, 0x00000000);
  /* ISE */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x87c, 0x00000000);
  /* SFR */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x880, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x884, 0x00000000);
  /* PRBHCS */
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x890, 0x00000000);
  WriteXVPReg (XVP_CLUSTER_STREAMER_OFFSET + 0x894, 0x00000000);
}


void
CSTi7200xVP::xp70init (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  /* soft reset, bits are self-clearing */
  WriteXVPReg (MBOX_SW_RESET_CTRL, MBOX_SWResetFullFlexVPE
                                   | MBOX_SWResetStreamer
                                   | MBOX_SWResetPlugs
                                   | MBOX_SWResetQueues
                                   | MBOX_SWResetCore);

  /* init mailbox, IMEM & DMEM */
  g_pIOS->ZeroMemory ((UCHAR *) m_xVPAddress + XVP_XP70_MAILBOX_OFFSET,
                      0x28 * 4);
  g_pIOS->ZeroMemory ((UCHAR *) m_xVPAddress + XVP_XP70_IMEM_OFFSET,
                      XVP_XP70_IMEM_SIZE);
  g_pIOS->ZeroMemory ((UCHAR *) m_xVPAddress + XVP_XP70_DMEM_OFFSET,
                      XVP_XP70_DMEM_SIZE);

  /* wait for xVP to signal ReadyForDownload */
  while ((ReadXVPReg (MBOX_STARTUP_CTRL_1) & MBOX_xP70ResetDone) != MBOX_xP70ResetDone)
    ;

  /* configure mailbox registers */
  WriteXVPReg (MBOX_IRQ_TO_XP70,      0x00);
  WriteXVPReg (MBOX_IRQ_TO_HOST,      0x00);
  WriteXVPReg (MBOX_INFO_xP70,        0x00);
  WriteXVPReg (MBOX_STARTUP_CTRL_1,   MBOX_PermanentQueueAuthorize
                                      | MBOX_AuthorizeIdleModes);
  WriteXVPReg (MBOX_AUTHORIZE_QUEUES, 0x00);
  WriteXVPReg (MBOX_GP_STATUS,        0x00);

  WriteXVPReg (MBOX_NEXT_CMD,         0x00000000);
}


void
CSTi7200xVP::ReleaseFirmwares (void)
{
  DEBUGF2 (4, (FENTRY ": imem/dmem: %p/%p\n",
               __PRETTY_FUNCTION__, m_imem, m_dmem));

  if (m_dmem)
    {
      g_pIOS->ReleaseFirmware (m_dmem);
      m_dmem = 0;
    }
  if (m_imem)
    {
      g_pIOS->ReleaseFirmware (m_imem);
      m_imem = 0;
    }
}


bool
CSTi7200xVP::xp70PrepareFirmwareUpload (enum PlaneCtrlxVPConfiguration newmode)
{
  const char *imem_name, *dmem_name;

  DEBUGF2 (4, (FENTRY ": %d\n", __PRETTY_FUNCTION__, newmode));

  /* firmware will stop running upon next MBOX_IRQ_TO_XP70 only, we do this
     in the UpdateHW() handler. So while waiting for the vsync we prepare
     some other stuff, i.e. fetch the actual firmware. */
  m_eState = XVPSTATE_FWUPLOAD_PREP;

  WriteXVPReg (MBOX_INFO_HOST, MBOX_FirmwareSwitch);
  WriteXVPReg (MBOX_NEXT_CMD, 0x00000000);

  ReleaseFirmwares ();

  switch (newmode)
    {
    case PCxVPC_FILMGRAIN:
      imem_name = "FGT_VideoTransformer_pmem.bin";
      dmem_name = 0;
      break;

    case PCxVPC_TNR:
    case PCxVPC_TNR_BYPASS:
    case PCxVPC_TNR_NLEOVER:
      ASSERTF (m_bUseTNR == true,
               ("%s: shouldn't be reached\n", __PRETTY_FUNCTION__));
      imem_name = "TNR_VideoTransformer_pmem.bin";
      dmem_name = "TNR_VideoTransformer_dmem.bin";
      break;

    default:
      DEBUGF (("%s: mode %d unknown!\n", __PRETTY_FUNCTION__, newmode));
      goto error0;
    }

  if (LIKELY (!g_pIOS->RequestFirmware (&m_imem, imem_name)))
    if (dmem_name)
      g_pIOS->RequestFirmware (&m_dmem, dmem_name);

  if (m_imem
      && ((dmem_name && m_dmem)
          || !dmem_name))
    {
      /* success, now check that the FW data is reasonably sized */
      ASSERTF (m_imem->ulDataSize <= XVP_XP70_IMEM_SIZE,
               ("fw_size of %d (%lu) > XVP_XP70_IMEM_SIZE (%lu)?!\n",
                newmode, m_imem->ulDataSize, XVP_XP70_IMEM_SIZE));

      if (UNLIKELY (m_imem->ulDataSize > XVP_XP70_IMEM_SIZE))
        {
          DEBUGF (("%s: Bilbo firmware for %d too large (%lu > %lu)\n",
                   __PRETTY_FUNCTION__, newmode, m_imem->ulDataSize,
                   XVP_XP70_IMEM_SIZE));
          goto error0;
        }

      if (m_dmem)
        {
          ASSERTF (m_dmem->ulDataSize <= XVP_XP70_DMEM_SIZE,
                   ("fw_size of %d (%lu) > XVP_XP70_DMEM_SIZE (%lu)?!\n",
                    newmode, m_dmem->ulDataSize, XVP_XP70_DMEM_SIZE));

          if (UNLIKELY (m_dmem->ulDataSize > XVP_XP70_DMEM_SIZE))
            {
              DEBUGF (("%s: Bilbo firmware for %d too large (%lu > %lu)\n",
                       __PRETTY_FUNCTION__, newmode, m_dmem->ulDataSize,
                       XVP_XP70_DMEM_SIZE));
              goto error0;
            }
        }

      /* wait until firmware really stopped running */
      ULONG reg, count = 0;
      do
        {
          reg = ReadXVPReg (MBOX_STARTUP_CTRL_2);
          ++count;
          if (count > 10000)
            {
              DEBUGF (("%s: BAD: timeout waiting for xVP firmware to stop running\n",
                       __PRETTY_FUNCTION__));
              goto error0;
            }
        }
      while (reg & MBOX_FetchEn);
      if (count - 1)
        DEBUGF (("%s: had to wait %lu cycles for xVP firmware to stop running\n",
                 __PRETTY_FUNCTION__, count - 1));

      /* now it's safe to upload during VSync */
      m_eState = XVPSTATE_FWUPLOAD;
   }
  else
    goto error0;

  return true;


error0:

  m_eState = XVPSTATE_IDLE;

  ReleaseFirmwares ();

  return false;
}


bool
CSTi7200xVP::CanHandleBuffer (const stm_display_buffer_t * const pFrame,
                              struct XVPSetup            * const setup) const
{
  bool retval = false;

  setup->eMode = PCxVPC_BYPASS;

  switch (m_eMode)
    {
    case PCxVPC_TNR:
    case PCxVPC_TNR_BYPASS:
    case PCxVPC_TNR_NLEOVER:
      if (!(pFrame->src.ulFlags & STM_PLANE_SRC_INTERLACED)
          || pFrame->src.ulColorFmt != SURF_YCBCR422R
          || pFrame->src.Rect.width > 720)
        break;

      setup->flags = 0;
      setup->eMode = m_eMode;
      setup->topField.bComplete
        = setup->botField.bComplete
        = setup->altTopField.bComplete
        = setup->altBotField.bComplete
        = false;
      retval = true;
      break;

    case PCxVPC_FILMGRAIN:
    case PCxVPC_BYPASS:
    default:
      break;
    }

  return retval;
}

bool
CSTi7200xVP::QueueBuffer1 (stm_display_buffer_t * const pFrame,
                           struct XVPSetup      * const setup)
{
  bool retval = false;

  switch (setup->eMode)
    {
    case PCxVPC_TNR:
    case PCxVPC_TNR_BYPASS:
    case PCxVPC_TNR_NLEOVER:
      DEBUGF2 (9, ("    %s: pFrame->src.Rect/stride: %lx/%lx/%lx/%lx/%lu\n",
                   __PRETTY_FUNCTION__, pFrame->src.Rect.x, pFrame->src.Rect.y,
                   pFrame->src.Rect.width, pFrame->src.Rect.height,
                   pFrame->src.ulStride));

      ASSERTF ((pFrame->src.Rect.width % 2) == 0,
               ("%s: width shall be even!\n", __PRETTY_FUNCTION__));
      ASSERTF ((pFrame->src.Rect.height % 2) == 0,
               ("%s: height shall be even!\n", __PRETTY_FUNCTION__));

      ASSERTF ((pFrame->src.ulFlags & STM_PLANE_SRC_INTERLACED) != 0,
               ("%s: non interlaced?!\n", __PRETTY_FUNCTION__));

      DEBUGF2 (7, ("    %s orig %.8lx\n",
                   __PRETTY_FUNCTION__,
                   pFrame->src.ulVideoBufferAddr));

      setup->stride = pFrame->src.ulStride;
      setup->pictureSize = ((pFrame->src.Rect.height & 0x07ff) << 16)
                           | (pFrame->src.Rect.width & 0x07ff);

      pFrame->src.ulFlags |= STM_PLANE_SRC_INTERPOLATE_FIELDS;


      setup->topField.TNRflags = setup->altBotField.TNRflags = TNR_FLAGS_TOP_NOTBOT;
      setup->botField.TNRflags = setup->altTopField.TNRflags = 0;

      /* FIXME remove */
      {
      for (ULONG fld = TNR_FRAME_DEST; fld < TNR_FRAME_COUNT; ++fld)
        {
          setup->topField.tnr_field[fld]
            = setup->botField.tnr_field[fld]
            = 0xdeadbeef;
          setup->altTopField.tnr_field[fld]
            = setup->altBotField.tnr_field[fld]
            = 0xcafebabe;
        }
      setup->topField.origVideoBuffer
        = setup->botField.origVideoBuffer
        = 0xbadc0fee;
      setup->altTopField.origVideoBuffer
        = setup->altBotField.origVideoBuffer
        = 0xc0ffee00;
      }

      if (pFrame->src.ulFlags & (STM_PLANE_SRC_TOP_FIELD_ONLY | STM_PLANE_SRC_BOTTOM_FIELD_ONLY))
        {
          /* only first field of buffer, which is either top or bottom  */
          struct XVPFieldSetup * const field
            = (pFrame->src.ulFlags & STM_PLANE_SRC_TOP_FIELD_ONLY)
               ? &setup->topField
               : &setup->botField;
          struct XVPFieldSetup * const altfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_TOP_FIELD_ONLY)
               ? &setup->altBotField
               : &setup->altTopField;

          ULONG dest_idx = m_TNRFieldN / 2;
          ULONG addr = m_TNRFilteredFrame[dest_idx].ulPhysical;

          field->tnr_field[TNR_FRAME_DEST]
            = altfield->tnr_field[TNR_FRAME_DEST]
            = addr + ((pFrame->src.ulFlags
                       & STM_PLANE_SRC_TOP_FIELD_ONLY) ? 0
                                                       : setup->stride);

          field->tnr_field[TNR_FRAME_PREV]
            = field->tnr_field[TNR_FRAME_PREPREV]
            = 0;
          altfield->tnr_field[TNR_FRAME_PREV]
            = altfield->tnr_field[TNR_FRAME_PREPREV]
            = 0;

          /* We change the address in here to the TNR's destination address,
             so DEI/VDP will work off this memory. */
          field->origVideoBuffer
            = altfield->origVideoBuffer
            = pFrame->src.ulVideoBufferAddr;

          pFrame->src.ulVideoBufferAddr = addr;

          m_TNRFieldN = (m_TNRFieldN + 1) % (TNR_FRAME_COUNT * 2);

          /* This is basically needed for the startup case only:
             E.g. we receive one 'bottom field first' buffer with
             displaytime = 0, i.e. infinite. Therefore the top/altbottom
             field addresses are not handled by the code above, and left
             undefined (which would be OK by itself...)
             Later we we receive a new bottom field only buffer with
             displaytime != 0. In this case, we later try to use references
             from the previous buffer's top field, which we don't have.
             An invalid address not only causes the image to be distorted,
             but most often a clash on the STBus or so.  */
          struct XVPFieldSetup * const unusedfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_TOP_FIELD_ONLY)
               ? &setup->botField
               : &setup->topField;
          struct XVPFieldSetup * const unusedaltfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_TOP_FIELD_ONLY)
               ? &setup->altTopField
               : &setup->altBotField;
          unusedfield->tnr_field[TNR_FRAME_DEST]
            = unusedaltfield->tnr_field[TNR_FRAME_DEST]
            = addr + ((pFrame->src.ulFlags
                       & STM_PLANE_SRC_TOP_FIELD_ONLY) ? setup->stride
                                                       : 0);
          unusedfield->tnr_field[TNR_FRAME_PREV]
            = unusedfield->tnr_field[TNR_FRAME_PREPREV]
            = 0;
          unusedaltfield->tnr_field[TNR_FRAME_PREV]
            = unusedaltfield->tnr_field[TNR_FRAME_PREPREV]
            = 0;
          unusedfield->origVideoBuffer
            = unusedaltfield->origVideoBuffer
            = pFrame->src.ulVideoBufferAddr;

          DEBUGF2 (7, ("    %s: %s d/c/p TNR: %.8lx/%.8lx/%.8lx\n",
                       __PRETTY_FUNCTION__,
                       (pFrame->src.ulFlags & STM_PLANE_SRC_TOP_FIELD_ONLY)
                       ? "top" : "bot",
                       field->tnr_field[TNR_FRAME_DEST],
                       field->tnr_field[TNR_FRAME_PREV],
                       field->tnr_field[TNR_FRAME_PREPREV]));
        }
      else
        {
          /* both fields of buffer, which is either top first or
             bottom first  */
          struct XVPFieldSetup * const firstfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST)
               ? &setup->botField
               : &setup->topField;
          struct XVPFieldSetup * const firstaltfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST)
               ? &setup->altTopField
               : &setup->altBotField;
          struct XVPFieldSetup * const secondfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST)
               ? &setup->topField
               : &setup->botField;
          struct XVPFieldSetup * const secondaltfield
            = (pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST)
               ? &setup->altBotField
               : &setup->altTopField;

          ULONG dest_idx1 = m_TNRFieldN / 2;
          ULONG addr1 = m_TNRFilteredFrame[dest_idx1].ulPhysical;
          m_TNRFieldN = (m_TNRFieldN + 1) % (TNR_FRAME_COUNT * 2);

          firstfield->tnr_field[TNR_FRAME_DEST]
            = firstaltfield->tnr_field[TNR_FRAME_DEST]
            = addr1 + ((pFrame->src.ulFlags
                        & STM_PLANE_SRC_BOTTOM_FIELD_FIRST) ? setup->stride
                                                            : 0);

          ULONG dest_idx2 = m_TNRFieldN / 2;
          ULONG addr2 = m_TNRFilteredFrame[dest_idx2].ulPhysical;
          m_TNRFieldN = (m_TNRFieldN + 1) % (TNR_FRAME_COUNT * 2);

          secondfield->tnr_field[TNR_FRAME_DEST]
            = secondaltfield->tnr_field[TNR_FRAME_DEST]
            = addr2 + ((pFrame->src.ulFlags
                        & STM_PLANE_SRC_BOTTOM_FIELD_FIRST) ? 0
                                                            : setup->stride);

          firstfield->tnr_field[TNR_FRAME_PREV]
            = firstfield->tnr_field[TNR_FRAME_PREPREV]
            = 0;
          firstaltfield->tnr_field[TNR_FRAME_PREV]
            = firstaltfield->tnr_field[TNR_FRAME_PREPREV]
            = 0;

          secondfield->tnr_field[TNR_FRAME_PREV] = firstfield->tnr_field[TNR_FRAME_DEST];
          secondaltfield->tnr_field[TNR_FRAME_PREV] = firstaltfield->tnr_field[TNR_FRAME_DEST];
          secondfield->tnr_field[TNR_FRAME_PREPREV]
            = secondaltfield->tnr_field[TNR_FRAME_PREPREV]
            = 0;

          /* We change the address in here to the TNR's destination address,
             so DEI/VDP will work off this memory. */
          firstfield->origVideoBuffer
            = firstaltfield->origVideoBuffer
            = pFrame->src.ulVideoBufferAddr
//              + ((pFrame->src.ulFlags
//                  & STM_PLANE_SRC_BOTTOM_FIELD_FIRST) ? setup->stride
//                                                      : 0)
              ;
          secondfield->origVideoBuffer
            = secondaltfield->origVideoBuffer
            = pFrame->src.ulVideoBufferAddr
//              + ((pFrame->src.ulFlags
//                  & STM_PLANE_SRC_BOTTOM_FIELD_FIRST) ? 0
//                                                      : setup->stride)
              ;

          /* basically, we just assume addr1 == addr2! */
          pFrame->src.ulVideoBufferAddr = addr1;

          DEBUGF2 (7, ("    %s: %s d/c/p TNR: %.8lx/%.8lx/%.8lx\n",
                       __PRETTY_FUNCTION__,
                       (pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST)
                       ? "bot" : "top",
                       firstfield->tnr_field[TNR_FRAME_DEST],
                       firstfield->tnr_field[TNR_FRAME_PREV],
                       firstfield->tnr_field[TNR_FRAME_PREPREV]));
          DEBUGF2 (7, ("    %s: %s d/c/p TNR: %.8lx/%.8lx/%.8lx\n",
                       __PRETTY_FUNCTION__,
                       (pFrame->src.ulFlags & STM_PLANE_SRC_BOTTOM_FIELD_FIRST)
                       ? "top" : "bot",
                       secondfield->tnr_field[TNR_FRAME_DEST],
                       secondfield->tnr_field[TNR_FRAME_PREV],
                       secondfield->tnr_field[TNR_FRAME_PREPREV]));
        }

      retval = true;
      break;

    case PCxVPC_FILMGRAIN:
    case PCxVPC_BYPASS:
    default:
      break;
    }

  return retval;
}


bool
CSTi7200xVP::QueueBuffer2 (const stm_display_buffer_t         * const pFrame,
                           struct XVPSetup                    * const setup,
                           const struct XVP_QUEUE_BUFFER_INFO * const qbi) const
{
  bool retval = false;

  switch (setup->eMode)
    {
    case PCxVPC_BYPASS:
      break;

    case PCxVPC_FILMGRAIN:
      break;

    case PCxVPC_TNR:
    case PCxVPC_TNR_BYPASS:
    case PCxVPC_TNR_NLEOVER:
      /* these should have been checked above in CanHandleBuffer () */
      ASSERTF ((pFrame->src.ulFlags & STM_PLANE_SRC_INTERLACED) == STM_PLANE_SRC_INTERLACED,
               ("%s: BUG\n", __PRETTY_FUNCTION__));
      ASSERTF (pFrame->src.ulColorFmt == SURF_YCBCR422R,
               ("%s: BUG\n", __PRETTY_FUNCTION__));
      ASSERTF (pFrame->src.Rect.width <= 720,
               ("%s: BUG\n", __PRETTY_FUNCTION__));

      setup->topField.vpo    = qbi->tViewPortOrigin;
      setup->botField.vpo    = qbi->bViewPortOrigin;
      setup->altTopField.vpo = qbi->atViewPortOrigin;
      setup->altBotField.vpo = qbi->abViewPortOrigin;

      setup->topField.vps    = qbi->tViewPortSize;
      setup->botField.vps    = qbi->bViewPortSize;
      setup->altTopField.vps = qbi->atViewPortSize;
      setup->altBotField.vps = qbi->abViewPortSize;

      setup->topField.lumaZoom    = (qbi->tVHSRC_LUMA_VSRC << 16) | qbi->cVHSRC_LUMA_HSRC;
      setup->botField.lumaZoom    = (qbi->bVHSRC_LUMA_VSRC << 16) | qbi->cVHSRC_LUMA_HSRC;
      setup->altTopField.lumaZoom = (qbi->atVHSRC_LUMA_VSRC << 16) | qbi->cVHSRC_LUMA_HSRC;
      setup->altBotField.lumaZoom = (qbi->abVHSRC_LUMA_VSRC << 16) | qbi->cVHSRC_LUMA_HSRC;

      retval = true;
      break;

    default:
      ASSERTF (1 == 2, ("%s: shouldn't be reached!\n", __PRETTY_FUNCTION__));
      break;
    }

  return retval;
}


static inline bool
streamChanged (const XVPSetup * const node1,
               const XVPSetup * const node2)
{
  if (node1->eMode != node2->eMode
      || node1->pictureSize != node2->pictureSize
      || node1->stride != node2->stride)
    {
      DEBUGF (("      xvp %s() from mode/picsize/stride %d/%.8lx/%.8lx -> %d/%.8lx/%.8lx\n",
               __FUNCTION__,
               node1->eMode, node1->pictureSize, node1->stride,
               node2->eMode, node2->pictureSize, node2->stride));
      return true;
    }

  return false;
}


void
CSTi7200xVP::PreviousTNRFieldSetup (struct XVPSetup * const setup,
                                    bool             isTopNode) const
{
  struct XVPFieldSetup * const field = isTopNode ? &setup->topField
                                                 : &setup->botField;
  struct XVPFieldSetup * const altfield = isTopNode ? &setup->altBotField
                                                    : &setup->altTopField;

  if (!field->tnr_field[TNR_FRAME_PREV])
    {
      /* m_currentNode hasn't been updated yet! Neither has readerpos, i.e.
         readerpos == setup and m_currentNode is the one which just _was_
         being handled */
      bool                   streamChanging = true;
      const stm_plane_node  * const curNode = m_FlexVP->GetCurrentNode ();
      const struct XVPSetup * const xvp = (struct XVPSetup *) curNode->dma_area.pData;

      if (curNode->isValid)
        streamChanging = streamChanged (xvp, setup);

      if (curNode->isValid && !streamChanging)
        {
          const XVPFieldSetup * const tmp =
            isTopNode ? &((struct XVPSetup *) curNode->dma_area.pData)->botField
                      : &((struct XVPSetup *) curNode->dma_area.pData)->topField;

          field->tnr_field[TNR_FRAME_PREV]
            = altfield->tnr_field[TNR_FRAME_PREV]
            = tmp->tnr_field[TNR_FRAME_DEST];
        }
      else
        {
          field->TNRflags |= TNR_NEW_SEQUENCE;
          altfield->TNRflags |= TNR_NEW_SEQUENCE;

          /* this shouldn't be needed, but the TNR firmware seems to lock
             the bus or so if the address is invalid */
          field->tnr_field[TNR_FRAME_PREV]
            = altfield->tnr_field[TNR_FRAME_PREV]
            = field->origVideoBuffer + (isTopNode ? 0 : setup->stride);
        }
    }
}


void
CSTi7200xVP::PrePreviousTNRFieldSetup (struct XVPSetup * const setup,
                                       bool             isTopNode) const
{
  struct XVPFieldSetup * const field = isTopNode ? &setup->topField
                                                 : &setup->botField;
  struct XVPFieldSetup * const altfield = isTopNode ? &setup->altBotField
                                                    : &setup->altTopField;

  if (!field->tnr_field[TNR_FRAME_PREPREV])
    {
      if (field->TNRflags & TNR_NEW_SEQUENCE)
        {
          /* stream is changing between this setup and last one. So it makes
             no sense to assign the pre previous. */
          field->tnr_field[TNR_FRAME_PREPREV]
            = altfield->tnr_field[TNR_FRAME_PREPREV]
            = field->origVideoBuffer + (isTopNode ? 0 : setup->stride);
        }
      else
        {
          bool                  wasStreamChange = true;

          const stm_plane_node * const prevNode = m_FlexVP->GetPreviousNode ();
          const stm_plane_node * const curNode  = m_FlexVP->GetCurrentNode ();

          if (prevNode->isValid && curNode->isValid)
            wasStreamChange = streamChanged ((const XVPSetup *) prevNode->dma_area.pData,
                                             (const XVPSetup *) curNode->dma_area.pData);

          if (prevNode->isValid && !wasStreamChange)
            {
              const XVPFieldSetup * const tmp =
                isTopNode ? &((struct XVPSetup *) prevNode->dma_area.pData)->topField
                          : &((struct XVPSetup *) prevNode->dma_area.pData)->botField;

              field->tnr_field[TNR_FRAME_PREPREV]
                = altfield->tnr_field[TNR_FRAME_PREPREV]
                = tmp->tnr_field[TNR_FRAME_DEST];
            }
          else
            {
              /*
               * Just pick the previous field as the pre previous, which is easy
               * as we just spent a load of logic working out the previous field.
               */
              field->tnr_field[TNR_FRAME_PREPREV]
                = altfield->tnr_field[TNR_FRAME_PREPREV]
                = field->tnr_field[TNR_FRAME_PREV];
            }
        }
    }
}


void
CSTi7200xVP::UpdateTNRFields (struct XVPSetup      * const setup,
                              struct XVPFieldSetup * const field) const
{
  ASSERTF ((setup->eMode == PCxVPC_TNR)
           || (setup->eMode == PCxVPC_TNR_BYPASS)
           || (setup->eMode == PCxVPC_TNR_NLEOVER),
           ("%s: BUG\n", __PRETTY_FUNCTION__));

  if (field->bComplete)
    return;

  PreviousTNRFieldSetup    (setup, field->TNRflags & TNR_FLAGS_TOP_NOTBOT);
  PrePreviousTNRFieldSetup (setup, field->TNRflags & TNR_FLAGS_TOP_NOTBOT);

  field->bComplete = true;
}


static inline ULONG __attribute__((const))
___host_to_le32(ULONG x)
{
#ifdef __BIG_ENDIAN__
  return x << 24 | x >> 24
         | (x & 0x0000ff00UL) << 8
         | (x & 0x00ff0000UL) >> 8;
#else
  return x;
#endif
}


void
CSTi7200xVP::UpdateHWTNRHandler (struct XVPSetup      * const setup,
                                 struct XVPFieldSetup * const field,
                                 ULONG                 extra_flags)
{
  enum PlaneCtrlxVPConfiguration setup_mode, own_mode;

  if (m_eMode == PCxVPC_TNR
      || m_eMode == PCxVPC_TNR_BYPASS
      || m_eMode == PCxVPC_TNR_NLEOVER)
    own_mode = PCxVPC_TNR;
  else
    own_mode = PCxVPC_FIRST;

  if (setup->eMode == PCxVPC_TNR
      || setup->eMode == PCxVPC_TNR_BYPASS
      || setup->eMode == PCxVPC_TNR_NLEOVER)
    setup_mode = PCxVPC_TNR;
  else
    setup_mode = PCxVPC_COUNT;

  if (setup_mode != own_mode)
    {
      /* we are expected to output some data at
         field->tnr_field[TNR_FRAME_DEST]. Maybe we should do this manually
         using some memcpy/DMA to have smooth switching from TNR to non-TNR? */
#ifdef TNR_DO_MEMCPY_FALLBACK_ON_MODE_CHANGE

      if (setup->eMode == PCxVPC_TNR
          || setup->eMode == PCxVPC_TNR_BYPASS
          || setup->eMode == PCxVPC_TNR_NLEOVER
          )
        {
          ULONG physical = field->tnr_field[TNR_FRAME_DEST];
          ULONG offset   = 0;
          if (!(field->TNRflags & TNR_FLAGS_TOP_NOTBOT))
            offset = setup->stride;

          for (int i = 0; i < TNR_FRAME_COUNT; i++)
            {
              /* find DMA_Area first */
              if (m_TNRFilteredFrame[i].ulPhysical == (physical - offset))
                {
                  ULONG width, height;

                  DEBUGF (("    xvp copy from %.8lx to %p/%.8x\n",
                           field->origVideoBuffer, m_TNRFilteredFrame[i].pMemory,
                           m_TNRFilteredFrame[i].ulPhysical));

                  width  = (setup->pictureSize >>  0) & 0xffff;
                  height = (setup->pictureSize >> 16) & 0xffff;
                  for (ULONG t = offset; t < width * height * 2; t += (2*setup->stride))
                    g_pIOS->MemcpyToDMAArea (&m_TNRFilteredFrame[i], t,
                                             (unsigned char *) (field->origVideoBuffer) + t,
                                             setup->stride);
                }
            }
        }
#endif
      return;
    }

  UpdateTNRFields (setup, field);

  /* First, read the old NLE value for debug */
  ULONG NLE = ReadXVPReg (XVP_XP70_DMEM_OFFSET + 0x10a4);
  DEBUGF (("NLE %.8lx\n", NLE));
  NLE_DUMP[m_CurrentNleIdx++] = NLE & 0x00ff0000;

  struct _XvpTNRTransform * const transform
    = (struct _XvpTNRTransform *) (m_TNRMailbox[m_CurrentTNRMailbox].pData);
  transform->flags = (0
                      | field->TNRflags
                      | extra_flags
                      | ((setup->eMode == PCxVPC_TNR_BYPASS) ? TNR_FLAGS_BYPASS
                                                             : 0)
                      | ((setup->eMode == PCxVPC_TNR_NLEOVER) ? TNR_OVERRIDE_NLE
                                                              : 0)
                     );

  transform->PrePreviousLumaBase = field->tnr_field[TNR_FRAME_PREPREV];
  transform->PreviousLumaBase    = field->tnr_field[TNR_FRAME_PREV];
  transform->DestLumaBase        = field->tnr_field[TNR_FRAME_DEST];
  transform->CurrentLumaBase     = field->origVideoBuffer;
  if (!(transform->flags & TNR_FLAGS_TOP_NOTBOT))
    transform->CurrentLumaBase += setup->stride;

  transform->CurrentChromaBase     = transform->CurrentLumaBase;
  transform->DestChromaBase        = transform->DestLumaBase;
  transform->PreviousChromaBase    = transform->PreviousLumaBase;
  transform->PrePreviousChromaBase = transform->PrePreviousLumaBase;

  /* x2 so we skip every other line because of interlaced */
  transform->FieldPitch  = setup->stride * 2;
  transform->PictureSize = setup->pictureSize;
  transform->ViewPortOrigin = field->vpo;
  transform->ViewPortSize   = field->vps;
  transform->LumaZoom = field->lumaZoom;

  /* FIXME? */
  transform->UserZoomOut = 1 << 3;
  transform->UserZoomOutThresh = TNR_THRESHOLD;

  transform->ExtNLE = (setup->eMode == PCxVPC_TNR_NLEOVER) ? m_NLE_Override : 0;
  transform->CustomProfile = 0;

  /* flush */
  g_pIOS->FlushCache (transform, sizeof (struct _XvpTNRTransform));

  /* fire */
  WriteXVPReg (MBOX_NEXT_CMD, m_TNRMailbox[m_CurrentTNRMailbox].ulPhysical);
  WriteXVPReg (MBOX_IRQ_TO_XP70, 0x00000001);

  ++m_CurrentTNRMailbox;
  m_CurrentTNRMailbox &= 1;

#if DEBUG_LEVEL > 7
  ULONG indx;
  if (field == &setup->topField) indx = 0;
  else if (field == &setup->botField) indx = 1;
  else if (field == &setup->altTopField) indx = 2;
  else if (field == &setup->altBotField) indx = 3;
  else indx = 4;
  DEBUGF (("    xvp: setup: %stop  d/c/p: %.8lx/%.8lx/%.8lx  orig: %.8lx\n"
           "    xvp: setup: %sbot  d/c/p: %.8lx/%.8lx/%.8lx\n"
           "    xvp: setup: %satop d/c/p: %.8lx/%.8lx/%.8lx\n"
           "    xvp: setup: %sabot d/c/p: %.8lx/%.8lx/%.8lx\n",
           (indx == 0) ? "*" : " ", setup->topField.tnr_field[TNR_FRAME_DEST], setup->topField.tnr_field[TNR_FRAME_PREV], setup->topField.tnr_field[TNR_FRAME_PREPREV],
           field->origVideoBuffer,
           (indx == 1) ? "*" : " ", setup->botField.tnr_field[TNR_FRAME_DEST], setup->botField.tnr_field[TNR_FRAME_PREV], setup->botField.tnr_field[TNR_FRAME_PREPREV],
           (indx == 2) ? "*" : " ", setup->altTopField.tnr_field[TNR_FRAME_DEST], setup->altTopField.tnr_field[TNR_FRAME_PREV], setup->altTopField.tnr_field[TNR_FRAME_PREPREV],
           (indx == 3) ? "*" : " ", setup->altBotField.tnr_field[TNR_FRAME_DEST], setup->altBotField.tnr_field[TNR_FRAME_PREV], setup->altBotField.tnr_field[TNR_FRAME_PREPREV]
         ));
  DEBUGF (("    xvp: using flg/cur/dest/prev/preprev: %.8lx/%.8lx/%.8lx/%.8lx/%.8lx\n",
           transform->flags, transform->CurrentLumaBase, transform->DestLumaBase,
           transform->PreviousLumaBase, transform->PrePreviousLumaBase));
#endif
}


void
CSTi7200xVP::UpdateHW (struct XVPSetup      * const setup,
                       struct XVPFieldSetup * const field)
{
  ULONG reg;
  ULONG flags = 0;

  DEBUGF2 (9, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (m_eState)
    {
    case XVPSTATE_IDLE:
      /* make sure bilbo does nothing */
      WriteXVPReg (MBOX_NEXT_CMD, 0x00000000);
      break;

    case XVPSTATE_FWUPLOAD_PREP:
      /* make sure bilbo does nothing */
      WriteXVPReg (MBOX_NEXT_CMD, 0x00000000);
      WriteXVPReg (MBOX_IRQ_TO_XP70, 0x00000001);
      break;

    case XVPSTATE_FWUPLOAD:
      DEBUGF2 (3, ("    xvp uploading (in VSync)\n"));
      /* we should have already waited earlier for firmware to have stopped
         running... */
      ASSERTF ((ReadXVPReg (MBOX_STARTUP_CTRL_2) & MBOX_FetchEn) == 0,
               ("%s: xVP firmware still running?!\n", __PRETTY_FUNCTION__));

      WriteXVPReg (MBOX_INFO_HOST, MBOX_StartAfterVsync);
      WriteXVPReg (MBOX_SW_RESET_CTRL, MBOX_SWResetCore);

      /* upload here. error checking has been done earlier */
      for (ULONG i = 0; i < m_imem->ulDataSize; i += 4)
        {
          reg = ___host_to_le32 (* ((unsigned long *) (&m_imem->pData[i])));
          WriteXVPReg (XVP_XP70_IMEM_OFFSET + i, reg);
        }
      if (m_dmem)
        for (ULONG i = 0; i < m_dmem->ulDataSize; i += 4)
          {
            reg = ___host_to_le32 (* ((unsigned long *) (&m_dmem->pData[i])));
            WriteXVPReg (XVP_XP70_DMEM_OFFSET + i, reg);
          }

      WriteXVPReg (MBOX_IRQ_TO_XP70, 0x00000000);
      WriteXVPReg (MBOX_STARTUP_CTRL_2, MBOX_FetchEnAuto
                                        /* explicitly allow xP70 to fetch
                                           instructions from IMEM.
                                           FetchEnAuto alone seems not
                                           to work. AFAIK, the firmware
                                           should set FetchEn by itself if
                                           FetchEnAuto is set, but doesn't
                                           seem to do so. */
                                        | MBOX_FetchEn);

      /* force TNR_NEW_SEQUENCE if firmware is just starting up */
      if (m_eMode == PCxVPC_TNR
          || m_eMode == PCxVPC_TNR_BYPASS
          || m_eMode == PCxVPC_TNR_NLEOVER
         )
        flags = TNR_NEW_SEQUENCE;

      m_eState = XVPSTATE_RUNNING;

      m_CurrentNleIdx = 0;
      g_pIOS->ZeroMemory (NLE_DUMP, sizeof (NLE_DUMP));

      /* fall through */
      DEBUGF2 (2, ("fall through, state/mode/setup->mode: %d/%d/%d\n",
                   m_eState, m_eMode, setup->eMode));

    case XVPSTATE_RUNNING:
      switch (m_eMode)
        {
        case PCxVPC_BYPASS:
        case PCxVPC_FILMGRAIN:
          break;

        case PCxVPC_TNR:
        case PCxVPC_TNR_BYPASS:
        case PCxVPC_TNR_NLEOVER:
          UpdateHWTNRHandler (setup, field, flags);
          break;

        default:
          ASSERTF (1 == 2, ("%s: shouldn't be reached!\n", __PRETTY_FUNCTION__));
          break;
        }

      /* Read status, but there's nothing else to do for us. The FW is
         intelligent enough on its own. */
      reg = ReadXVPReg (MBOX_INFO_xP70);
      if (reg)
        {
          DEBUGF (("%s: xP70 status: %.8lx\n", __PRETTY_FUNCTION__, reg));
          WriteXVPReg (MBOX_INFO_xP70, 0x00000000);
        }
      break;
    }
}


XVPFieldSetup *
CSTi7200xVP::SelectFieldSetup (bool                 isTopFieldOnDisplay,
                               struct XVPSetup     * const setup,
                               bool                 bIsPaused,
                               stm_plane_node_type  nodeType,
                               bool                 bUseAltFields) const
{
  DEBUGF2 (9, ("    %s for display/type/alt: %s/%s/%s\n",
               __PRETTY_FUNCTION__,
               isTopFieldOnDisplay ? "top" : "bot",
               (nodeType == GNODE_TOP_FIELD)
                 ? "top"
                 : (nodeType == GNODE_BOTTOM_FIELD) ? "bot" : "prg",
               bUseAltFields ? "true" : "false"));

  if (!isTopFieldOnDisplay)
    {
      /* bottom field on display */
      if (bIsPaused || bUseAltFields)
        {
          if (nodeType == GNODE_TOP_FIELD)
            DEBUGF2 (9, ("      using TOP\n"));
          else
            DEBUGF2 (9, ("      using ALTTOP\n"));

          return (nodeType == GNODE_TOP_FIELD)
                 ? &setup->topField
                 : &setup->altTopField;
        }

      DEBUGF2 (9, ("      using TOP\n"));
      return &setup->topField;
    }

  if (bIsPaused || bUseAltFields)
    {
      if (nodeType == GNODE_BOTTOM_FIELD)
        DEBUGF2 (9, ("      using BOT\n"));
      else
        DEBUGF2 (9, ("      using ALTBOT\n"));

      return (nodeType == GNODE_BOTTOM_FIELD)
             ? &setup->botField
             : &setup->altBotField;
    }

  DEBUGF2 (9, ("      using BOT\n"));
  return &setup->botField;
}


void
CSTi7200xVP::xp70deinit (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  /* reset IMEM memory */
  g_pIOS->ZeroMemory ((UCHAR *) m_xVPAddress + XVP_XP70_IMEM_OFFSET,
                      XVP_XP70_IMEM_SIZE);
  /* stop running process and reset BILBO */
  WriteXVPReg (MBOX_SW_RESET_CTRL, MBOX_SWResetFullFlexVPE
                                   | MBOX_SWResetStreamer
                                   | MBOX_SWResetPlugs
                                   | MBOX_SWResetQueues
                                   | MBOX_SWResetCore);
}
