/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_packet.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

#define USE_DIRECT_PACKET_MEMORY    1

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/
#include <hdmirx_drv.h>
#include <hdmirx_core.h>
#include <InternalTypes.h>
#include <hdmirx_utility.h>
#include <hdmirx_RegOffsets.h>
#include <stm_hdmirx_os.h>
#include "hdmirx_core_export.h"

/* Private Typedef -----------------------------------------------*/

typedef enum
{
  COMMON_PACKET_MEM_REGION_START_IDX = 0,
  VS_PACKET_MEM_REGION_START_IDX = 1,
  AVI_PACKET_MEM_REGION_START_IDX = 2,
  SPD_PACKET_MEM_REGION_START_IDX = 3,
  AUDIOINFO_PACKET_MEM_REGION_START_IDX = 4,
  MPEG_PACKET_MEM_REGION_START_IDX = 5,
  ACP_PACKET_MEM_REGION_START_IDX = 6,
  ISRC1_PACKET_MEM_REGION_START_IDX = 7,
  ISRC2_PACKET_MEM_REGION_START_IDX = 8,
  ACR_PACKET_MEM_REGION_START_IDX = 9,
  GC_PACKET_MEM_REGION_START_IDX = 10,
  META_PACKET_MEM_REGION_START_IDX = 11
} STHDMIRX_MemoryPacketIds_t;

/* Private Defines ------------------------------------------------*/
#define HDRX_PACKET_READ_TIMEOUT             20	/*20 ms */
#define MEMORY_SIZE_PER_PACKET_TYPE          32	/* 32 Bytes */
/* Private macro's ------------------------------------------------*/

#define HDMI_PACKET_HEADER_SIZE     3

#define HDMI_VENDOR_PACK_SIZE	    28
#define HDMI_AVI_PACK_SIZE		    14
#define HDMI_SDP_PACK_SIZE		    26
#define HDMI_AUDIO_PACK_SIZE	    6
#define HDMI_MPEG_PACK_SIZE		    11
#define HDMI_ACP_PACK_SIZE		    16
#define HDMI_ISR1_PACK_SIZE		    16
#define HDMI_ISR2_PACK_SIZE		    16
#define HDMI_ACR_PACK_SIZE		    7
#define HDMI_UNDEF_PACK_SIZE	    28
#define HDMI_GC_PACK_SIZE		    7
#define HDMI_GAMUT_PACK_SIZE	    28

#define STHDMIRX_READ_PACKETCHANNELID (adrs)     (*(U8 HDMI_MEM *)(Adrs))

#define STHDMIRX_PACKET_HEADER(adrs,i)     (U8 HDMI_MEM *)(adrs + 0x01 + (i))

#define STHDMIRX_PACKET_BODY(adrs,i)       (U8 HDMI_MEM *)(adrs + 0x04 + (i))

#define STHDMIRX_READ_PACKET_HEADER(adrs,i)            \
   (sthdmirx_read_byte_from_packet((U8 HDMI_MEM *)(adrs + 0x01 + i)))

#define STHDMIRX_READ_PACKET_BODY(adrs,i)            \
   (sthdmirx_read_byte_from_packet((U8 HDMI_MEM *)(adrs + 0x04 + i)))

/* Clear HDMI packet */
#define STHDMIRX_CLEAR_PACKET(adrs,PacketIndex)    \
    sthdmirx_write_word_to_packet((U16 HDMI_MEM *)(adrs + PacketIndex*32), 0xffff)

/* Private Variables -----------------------------------------------*/

static U8 const Ba_PacketSize[] =
{
  0,
  HDMI_VENDOR_PACK_SIZE,
  HDMI_AVI_PACK_SIZE,
  HDMI_SDP_PACK_SIZE,
  HDMI_AUDIO_PACK_SIZE,
  HDMI_MPEG_PACK_SIZE,
  HDMI_ACP_PACK_SIZE,
  HDMI_ISR1_PACK_SIZE,
  HDMI_ISR2_PACK_SIZE,
  HDMI_ACR_PACK_SIZE,
  HDMI_UNDEF_PACK_SIZE,
  HDMI_GC_PACK_SIZE,
  HDMI_GAMUT_PACK_SIZE
};

/* Private functions prototypes ------------------------------------*/
BOOL sthdmirx_handle_VS_data(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_AV_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_SPD_data(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_audio_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_MPEG_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_ACP_Data(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_ISRC1_data(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_ISRC2_data(hdmirx_route_handle_t *const pInpHandle);;
BOOL sthdmirx_handle_GCP_data(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_gamutlmeta_packet_data(
  hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_mute_events_data(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_undefined_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_handle_ACR_data(hdmirx_route_handle_t *const pInpHandle);

BOOL sthdmirx_read_packet(const hdmirx_handle_t Handle,
                          sthdmirx_packet_id_t PacketIndex);
U8 sthdmirx_copy_packet(U8 HDMI_RAM *Bp_Dest, U8 HDMI_MEM *Bp_Source,
                        U8 B_Size);
U8 sthdmirx_read_packet_body(const hdmirx_handle_t Handle,
                             sthdmirx_packet_id_t PacketIndex, U8 HDMI_RAM *Bp_Buffer);
/* Interface procedures/functions ---------------------------------- */
/******************************************************************************
 FUNCTION     :  sthdmirx_read_packet_body
 USAGE        :  Read requested packes body to the buffer, does not perform field formatting
 INPUT        :  Packet index, pointer to buffer
 RETURN       :  Size of packet body
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_packet_body(const hdmirx_handle_t Handle,
                             sthdmirx_packet_id_t PacketIndex, U8 HDMI_RAM *Bp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  if (sthdmirx_read_packet(InpHandle_p, PacketIndex))
    {
      sthdmirx_copy_packet(Bp_Buffer, STHDMIRX_PACKET_BODY(
                             (U32)(InpHandle_p->PacketBaseAddress), 0),
                           Ba_PacketSize[PacketIndex]);
      return Ba_PacketSize[PacketIndex];
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_packet_header
 USAGE        :  Read requested packes header to the buffer, does not perform field
 INPUT        :  Packet index, pointer to buffer
 RETURN       :  Size of packet header
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_packet_header(const hdmirx_handle_t Handle,
                               sthdmirx_packet_id_t PacketIndex, U8 HDMI_RAM *Bp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  if (sthdmirx_read_packet(InpHandle_p, PacketIndex))
    {
      sthdmirx_copy_packet(Bp_Buffer, STHDMIRX_PACKET_HEADER(
                             (U32)(InpHandle_p->PacketBaseAddress), 0),
                           HDMI_PACKET_HEADER_SIZE);
      return HDMI_PACKET_HEADER_SIZE;
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_copy_packet
 USAGE        :  Read requested packes data to reserved chip packet memory
 INPUT        :  Bp_Dest - Destination, Bp_Source - Source, B_Size - Size
 RETURN       :  Size of read
 USED_REGS    :  None
******************************************************************************/
U8 sthdmirx_copy_packet(U8 HDMI_RAM *Bp_Dest, U8 HDMI_MEM *Bp_Source,
                        U8 B_Size)
{
  U8 B_BytesRead = 0;

  if (B_Size & 1)
    {
      if (HDMI_MEM_CPY(Bp_Dest, Bp_Source, sizeof(U8)) == FALSE)
        {
          return 0;
        }
      B_Size--;
      B_BytesRead++;
      Bp_Dest++;
      Bp_Source++;
    }
  for (; B_Size; B_Size -= 2, B_BytesRead += 2, Bp_Dest += 2, Bp_Source += 2)
    {
      if (HDMI_MEM_CPY(Bp_Dest, Bp_Source, sizeof(U32)) == FALSE)
        return B_BytesRead;
    }
  return B_BytesRead;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_cmp_packet
 USAGE        :  Compare Data
 INPUT        :  Bp_Source1 - Destination, Bp_Source2 - Source, B_Size - Size
 RETURN       :  True or False
 USED_REGS    :  None
******************************************************************************/
BOOL sthdmirx_cmp_packet(U8 HDMI_RAM *Bp_Source1, U8 HDMI_MEM *Bp_Source2,
                         U8 B_Size)
{
  if (B_Size & 1)
    {
      U8 B_CheckValue1, B_CheckValue2;

      if (HDMI_MEM_CPY(&B_CheckValue1, Bp_Source1, sizeof(U8)) == FALSE)
        {
          return FALSE;
        }

      if (HDMI_MEM_CPY(&B_CheckValue2, Bp_Source2, sizeof(U8)) == FALSE)
        {
          return FALSE;
        }

      if (B_CheckValue1 != B_CheckValue2)
        {
          return FALSE;
        }

      B_Size--;
      Bp_Source1++;
      Bp_Source2++;
    }

  for (; B_Size; B_Size -= 2, Bp_Source1 += 2, Bp_Source2 += 2)
    {
      U16 B_CheckValue1, B_CheckValue2;

      if (HDMI_MEM_CPY((U8 *)&B_CheckValue1, Bp_Source1, sizeof(U16)) == FALSE)
        {
          return FALSE;
        }

      if (HDMI_MEM_CPY((U8 *)&B_CheckValue2, Bp_Source2, sizeof(U16)) == FALSE)
        {
          return FALSE;
        }
      if (B_CheckValue1 != B_CheckValue2)
        {
          return FALSE;
        }
    }

  return TRUE;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_byte_from_packet
 USAGE        :  Read requested packes data to reserved chip packet memory
 INPUT        :  Bp_Source - Source
 RETURN       :  U8 value readed
 USED_REGS    :  None
******************************************************************************/
U8 sthdmirx_read_byte_from_packet(U8 HDMI_MEM *Bp_Source)
{
  U8 B_Value;

  HDMI_MEM_CPY(&B_Value, Bp_Source, sizeof(U8));

  return B_Value;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_word_from_packet
 USAGE        :  Read requested packes data to reserved chip packet memory
 INPUT        :  Bp_Source - Source
 RETURN       :  U32 value readed
 USED_REGS    :  None
******************************************************************************/
U16 sthdmirx_read_word_from_packet(U16 HDMI_MEM *Bp_Source)
{
  U16 W_Value;
  HDMI_MEM_CPY((U8 *) & W_Value, (U8 *) Bp_Source, sizeof(U16));
  return W_Value;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_write_word_to_packet
 USAGE        :  Write requested packes data to reserved chip packet memory
 INPUT        :  Bp_Source - Source
 RETURN       :  TRUE/FALSE
 USED_REGS    :  None
******************************************************************************/
BOOL sthdmirx_write_word_to_packet(U16 HDMI_MEM *Bp_Source, U16 W_Value)
{
  return (BOOL) (HDMI_MEM_CPY((U8 HDMI_RAM *) (Bp_Source),
                              (U8 HDMI_MEM *) & W_Value, sizeof(U16)));
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_packet
 USAGE        :  Read requested packes body to reserved chip packet memory
 INPUT        :  Packet index
 RETURN       :  If read is successful or not
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
BOOL sthdmirx_read_packet(const hdmirx_handle_t Handle,
                          sthdmirx_packet_id_t PacketIndex)
{

#if !USE_DIRECT_PACKET_MEMORY
  U32 DW_BeginTime;
#endif

  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  if (PacketIndex == HDRX_UNDEF_PACK)
    {
      HDMI_PRINT
      ("sthdmirx_read_packet: Error. undefined packet reading is not implemented\n");
      return FALSE;
    }
  else if (PacketIndex > HDRX_UNDEF_PACK)
    {
      PacketIndex--;
    }

  /*Check if packet avalible */
  if (!(HDMI_READ_REG_WORD((U32)(InpHandle_p->BaseAddress + HDRX_PKT_STATUS)) &
        (1 << PacketIndex)))
    {
      HDMI_PRINT
      ("sthdmirx_read_packet: Error. Packet %d is not avalible",
       PacketIndex);
      return FALSE;
    }
#if !USE_DIRECT_PACKET_MEMORY
  /*Select packet */
  HDMI_WRITE_REG_BYTE((U32) (InpHandle_p->BaseAddress + HDRX_PACKET_SEL),
                      PacketIndex);
  HDMI_SET_REG_BITS_BYTE((U32)(InpHandle_p->BaseAddress + HDRX_PACK_CTRL),
                         HDRX_PACKET_RD_REQ);
  HDMI_CLEAR_REG_BITS_BYTE((U32)
                           (InpHandle_p->BaseAddress + HDRX_PACK_CTRL),
                           HDRX_PACKET_RD_REQ);

  DW_BeginTime = stm_hdmirx_time_now();
  /*Wait util packet is copied */
  while (!(HDMI_READ_REG_WORD((U32)(InpHandle_p->BaseAddress +
                                    HDRX_PKT_STATUS)) & HDRX_PACKET_AVLBL))
    {
      if (stm_hdmirx_time_minus(stm_hdmirx_time_now(), DW_BeginTime) >=
          (M_NUM_TICKS_PER_MSEC(HDRX_PACKET_READ_TIMEOUT)))
        {
          HDMI_PRINT
          ("sthdmirx_read_packet: time out error while reading %d packet",
           PacketIndex);
          return FALSE;	/* if timeout, exit with error */
        }
    }
#endif

  return TRUE;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_AVI_info_frame
 USAGE        :  Format reading of Auxiliary Video information InfoFrame
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_AVI_INFOFRAME in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_AVI_info_frame(const hdmirx_handle_t Handle,
                                HDRX_AVI_INFOFRAME HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  PacketAddress = InpHandle_p->PacketBaseAddress;
#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (AVI_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  if (sthdmirx_read_packet(InpHandle_p, HDRX_AVI_PACK))
    {
      Sp_Buffer->CheckSum =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 0);
      Sp_Buffer->S =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x03;
      Sp_Buffer->B =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x0c) >> 2;
      Sp_Buffer->A =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x10) >> 4;
      Sp_Buffer->Y =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x60) >> 5;
      Sp_Buffer->PB1_Rsvd =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x80) >> 7;
      Sp_Buffer->R =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0x0f;
      Sp_Buffer->M =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0x30) >> 4;
      Sp_Buffer->C =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0xc0) >> 6;
      Sp_Buffer->SC =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 3) & 0x03;
      Sp_Buffer->Q =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 3) & 0x0c) >> 2;
      Sp_Buffer->EC =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 3) & 0x70) >> 4;
      Sp_Buffer->ITC =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 3) & 0x80) >> 7;
      Sp_Buffer->VIC =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 4) & 0x7f;
      Sp_Buffer->PB4_Rsvd =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 4) & 0x80) >> 7;
      Sp_Buffer->PR =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x0f;
      Sp_Buffer->CN =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x30) >> 4;
      Sp_Buffer->YQ =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0xC0) >> 6;
//        Sp_Buffer->PB5_Rsvd         = (STHDMIRX_READ_PACKET_BODY(PacketAddress,5) & 0xf0) >> 4;
      Sp_Buffer->EndOfTopBar =
        MAKE_WORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 6),
                  STHDMIRX_READ_PACKET_BODY(PacketAddress, 7));
      Sp_Buffer->StartOfBottomBar =
        MAKE_WORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 8),
                  STHDMIRX_READ_PACKET_BODY(PacketAddress, 9));
      Sp_Buffer->EndOfLeftBar =
        MAKE_WORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 10),
                  STHDMIRX_READ_PACKET_BODY(PacketAddress, 11));
      Sp_Buffer->StartOfRightBar =
        MAKE_WORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 12),
                  STHDMIRX_READ_PACKET_BODY(PacketAddress, 13));
      return sizeof(HDRX_AVI_INFOFRAME);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_audio_info_frame
 USAGE        :  Format reading of Audio InfoFrame
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_AUDIO_INFOFRAME in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_audio_info_frame(const hdmirx_handle_t Handle,
                                  HDRX_AUDIO_INFOFRAME HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;
#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (AUDIOINFO_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif
  if (sthdmirx_read_packet(InpHandle_p, HDRX_AUDIO_INFO_PACK))
    {
      Sp_Buffer->CheckSum =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 0);
      Sp_Buffer->CC =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x07;
      Sp_Buffer->PB1_Rsvd =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x08) >> 3;
      Sp_Buffer->CT =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0xf0) >> 4;
      Sp_Buffer->SS =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0x03;
      Sp_Buffer->SF =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0x1c) >> 2;
      Sp_Buffer->PB2_Rsvd =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0xe0) >> 5;
      Sp_Buffer->FormatCodingType =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 3);
      Sp_Buffer->CA = STHDMIRX_READ_PACKET_BODY(PacketAddress, 4);
      Sp_Buffer->LFEP =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x03;
      Sp_Buffer->PB3_Rsvd =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x04) >> 2;
      Sp_Buffer->LSV =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x78) >> 3;
      Sp_Buffer->DM_INH =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x80) >> 7;

      return sizeof(HDRX_AUDIO_INFOFRAME);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_gamut_packet
 USAGE        :  Format reading Gamut Metadata packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_GAMUT_PACKET in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_gamut_packet(const hdmirx_handle_t Handle,
                              HDRX_GAMUT_PACKET HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;
#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (META_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif
  /*Minus one because gmt_HDMI_PACKET does not correspond to bits mapping */
  if (sthdmirx_read_packet(InpHandle_p, HDRX_GAMUT_PACK))
    {
      HDMI_MEM_CPY(Sp_Buffer->GBD, STHDMIRX_PACKET_BODY(PacketAddress, 0),
                   Ba_PacketSize[HDRX_GAMUT_PACK]);
      return sizeof(HDRX_GAMUT_PACKET);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_Gc_packet
 USAGE        :  Format reading of General Control packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_GC_PACKET in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_Gc_packet(const hdmirx_handle_t Handle,
                           HDRX_GC_PACKET HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (GC_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  /*Minus one because gmt_HDMI_PACKET does not correspond to bits mapping */
  if (sthdmirx_read_packet(Handle, HDRX_GC_PACK))
    {
      Sp_Buffer->Set_AVMUTE =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 0) & 0x01;
      Sp_Buffer->GCP_Rsrv1 =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 0) & 0x0e) >> 1;
      Sp_Buffer->Clear_AVMUTE =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 0) & 0x10) >> 4;
      Sp_Buffer->GCP_Rsrv2 =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 0) & 0xe0) >> 5;
      Sp_Buffer->CD =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x0f;
      Sp_Buffer->PP =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0xf0) >> 4;
      Sp_Buffer->Default_Phase =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0x01;
      Sp_Buffer->GCP_Rsrv3 =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 2) & 0xfe) >> 1;

      HDMI_MEM_CPY(Sp_Buffer->GCP_Rsrv4,
                   STHDMIRX_PACKET_BODY(PacketAddress, 3), 4);
      /*HDMI_PRINT("Sp_Buffer->CD:%d\n",Sp_Buffer->CD); */
      return sizeof(HDRX_GC_PACKET);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_ACR_packet
 USAGE        :  Format reading of Audio Clock Regeneration packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_ACR_PACKET in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_ACR_packet(const hdmirx_handle_t Handle,
                            HDRX_ACR_PACKET HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;
#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (ACR_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif
  if (sthdmirx_read_packet(InpHandle_p, HDRX_ACR_PACK))
    {
      Sp_Buffer->CTS = MAKE_DWORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 3),
                                  STHDMIRX_READ_PACKET_BODY(PacketAddress, 2),
                                  STHDMIRX_READ_PACKET_BODY(PacketAddress, 1) & 0x0f, 0);
      Sp_Buffer->N = MAKE_DWORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 6),
                                STHDMIRX_READ_PACKET_BODY(PacketAddress, 5),
                                STHDMIRX_READ_PACKET_BODY(PacketAddress, 4) & 0x0f, 0);

      return sizeof(HDRX_ACR_PACKET);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_ACP_packet
 USAGE        :  Format reading of Audio Content Protection packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_ACP_PACKET in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_ACP_packet(const hdmirx_handle_t Handle,
                            HDRX_ACP_PACKET HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (ACP_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  if (sthdmirx_read_packet(InpHandle_p, HDRX_ACP_PACK))
    {
      Sp_Buffer->ACPType = STHDMIRX_READ_PACKET_HEADER(PacketAddress, 1);
      HDMI_MEM_CPY(Sp_Buffer->ACPData, STHDMIRX_PACKET_BODY(PacketAddress, 0),
                   Ba_PacketSize[HDRX_ACP_PACK]);
      return sizeof(HDRX_ACP_PACKET);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_vendor_info_frame
 USAGE        :  Format reading of Vendor packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_VENDOR_INFOFRAME in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_vendor_info_frame(const hdmirx_handle_t Handle,
                                   HDRX_VENDOR_INFOFRAME HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (VS_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  if (sthdmirx_read_packet(InpHandle_p, HDRX_VENDOR_PACK))
    {
      Sp_Buffer->Length =
        STHDMIRX_READ_PACKET_HEADER(PacketAddress, 2) & 0x1F;
      Sp_Buffer->CheckSum =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 0);
      Sp_Buffer->IeeeId =
        MAKE_DWORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 1),
                   STHDMIRX_READ_PACKET_BODY(PacketAddress, 2),
                   STHDMIRX_READ_PACKET_BODY(PacketAddress, 3), 0);
      Sp_Buffer->Hdmi_Video_Format =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 4) & 0xE0) >> 5;
      Sp_Buffer->TDMetaPresent =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x08) >> 3;
      Sp_Buffer->structure3D =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0xF0) >> 4;

      if (Sp_Buffer->Length > 24)
        {
          Sp_Buffer->Length = 24;
        }
      HDMI_MEM_CPY(Sp_Buffer->Payload, STHDMIRX_PACKET_BODY(PacketAddress, 4),
                   Sp_Buffer->Length);
      return sizeof(HDRX_VENDOR_INFOFRAME);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_SPD_info_frame
 USAGE        :  Format reading of Source Product Descriptor InfoFrame
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_SPD_INFOFRAME in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_SPD_info_frame(const hdmirx_handle_t Handle,
                                HDRX_SPD_INFOFRAME HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;
#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (SPD_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif
  if (sthdmirx_read_packet(InpHandle_p, HDRX_SPD_PACK))
    {
      Sp_Buffer->CheckSum = STHDMIRX_READ_PACKET_BODY(PacketAddress, 0);
      HDMI_MEM_CPY(Sp_Buffer->VendorName,
                   STHDMIRX_PACKET_BODY(PacketAddress, 1), 8);
      HDMI_MEM_CPY(Sp_Buffer->ProductName,
                   STHDMIRX_PACKET_BODY(PacketAddress, 9), 16);
      Sp_Buffer->SDI = STHDMIRX_READ_PACKET_BODY(PacketAddress, 25);

      return sizeof(HDRX_SPD_INFOFRAME);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_MPEG_info_frame
 USAGE        :  ormat reading of MPEG InfoFrame packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_MPEG_INFOFRAME in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_MPEG_info_frame(const hdmirx_handle_t Handle,
                                 HDRX_MPEG_INFOFRAME HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (MPEG_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  if (sthdmirx_read_packet(InpHandle_p, HDRX_MPEG_PACK))
    {
      Sp_Buffer->CheckSum = STHDMIRX_READ_PACKET_BODY(PacketAddress, 0);
      Sp_Buffer->MpegBitRate =
        MAKE_DWORD(STHDMIRX_READ_PACKET_BODY(PacketAddress, 1),
                   STHDMIRX_READ_PACKET_BODY(PacketAddress, 2),
                   STHDMIRX_READ_PACKET_BODY(PacketAddress, 3),
                   STHDMIRX_READ_PACKET_BODY(PacketAddress, 4));

      Sp_Buffer->MF =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x03;
      Sp_Buffer->F52_53 =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x0c) >> 2;
      Sp_Buffer->FR0 =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0x10) >> 4;
      Sp_Buffer->F55_57 =
        (STHDMIRX_READ_PACKET_BODY(PacketAddress, 5) & 0xe0) >> 5;
      Sp_Buffer->F60_67 = STHDMIRX_READ_PACKET_BODY(PacketAddress, 6);
      Sp_Buffer->F70_77 = STHDMIRX_READ_PACKET_BODY(PacketAddress, 7);
      Sp_Buffer->F80_87 = STHDMIRX_READ_PACKET_BODY(PacketAddress, 8);
      Sp_Buffer->F90_97 = STHDMIRX_READ_PACKET_BODY(PacketAddress, 9);
      Sp_Buffer->F100_107 =
        STHDMIRX_READ_PACKET_BODY(PacketAddress, 10);

      return sizeof(HDRX_MPEG_INFOFRAME);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_ISRC1_packet
 USAGE        :  Format reading of International Standard Recording Code 1 packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_ISRC1_PACKET in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/

U8 sthdmirx_read_ISRC1_packet(const hdmirx_handle_t Handle,
                              HDRX_ISRC1_PACKET HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (ISRC1_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  if (sthdmirx_read_packet(InpHandle_p, HDRX_ISRC1_PACK))
    {
      HDMI_MEM_CPY(Sp_Buffer->UPC_EAN_ISRC,
                   STHDMIRX_PACKET_BODY(PacketAddress, 0),
                   Ba_PacketSize[HDRX_ISRC1_PACK]);
      return sizeof(HDRX_ISRC1_PACKET);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_ISRC2_packet
 USAGE        :  Format reading of International Standard Recording Code 2 packet
 INPUT        :  Bp_Buffer - Pointer to the structure the packet is copied to
 RETURN       :  Size of HDRX_ISRC2_PACKET in case of successful read or 0 if read is failed
 USED_REGS    :  HDMI_PACKET_SEL, HDMI_PACK_CTRL
******************************************************************************/
U8 sthdmirx_read_ISRC2_packet(const hdmirx_handle_t Handle,
                              HDRX_ISRC2_PACKET HDMI_RAM *Sp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  U32 PacketAddress;

  InpHandle_p = (hdmirx_route_handle_t *) Handle;
  PacketAddress = InpHandle_p->PacketBaseAddress;

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (ISRC2_PACKET_MEM_REGION_START_IDX *
                          MEMORY_SIZE_PER_PACKET_TYPE);
#endif

  if (sthdmirx_read_packet(InpHandle_p, HDRX_ISRC2_PACK))
    {
      HDMI_MEM_CPY(Sp_Buffer->UPC_EAN_ISRC,
                   STHDMIRX_PACKET_BODY(PacketAddress, 0),
                   Ba_PacketSize[HDRX_ISRC2_PACK]);
      return sizeof(HDRX_ISRC2_PACKET);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     :  sthdmirx_read_packet_formatted
 USAGE        :  Read requested packes and perform type cast from U8 buffer
                 to packet structure. Type casting is done platform (compiler)
                 independent way by byte by byte, bit by bit coping from byte array to structure fields
 INPUT        :  Packet index, pointer to buffer
 RETURN       :  Size of packet structure
 USED_REGS    :  None
******************************************************************************/
U8 sthdmirx_read_packet_formatted(const hdmirx_handle_t Handle,
                                  sthdmirx_packet_id_t PacketIndex, U8 HDMI_RAM *Bp_Buffer)
{
  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  switch (PacketIndex)
    {
    case HDRX_VENDOR_PACK:
      return sthdmirx_read_vendor_info_frame(InpHandle_p,
                                             (HDRX_VENDOR_INFOFRAME HDMI_RAM *) Bp_Buffer);

    case HDRX_AVI_PACK:
      return sthdmirx_read_AVI_info_frame(InpHandle_p,
                                          (HDRX_AVI_INFOFRAME HDMI_RAM *) Bp_Buffer);

    case HDRX_SPD_PACK:
      return sthdmirx_read_SPD_info_frame(InpHandle_p,
                                          (HDRX_SPD_INFOFRAME HDMI_RAM *) Bp_Buffer);

    case HDRX_AUDIO_INFO_PACK:
      return sthdmirx_read_audio_info_frame(InpHandle_p,
                                            (HDRX_AUDIO_INFOFRAME HDMI_RAM *) Bp_Buffer);

    case HDRX_MPEG_PACK:
      return sthdmirx_read_MPEG_info_frame(InpHandle_p,
                                           (HDRX_MPEG_INFOFRAME HDMI_RAM *) Bp_Buffer);

    case HDRX_ACP_PACK:
      return sthdmirx_read_ACP_packet(InpHandle_p,
                                      (HDRX_ACP_PACKET HDMI_RAM *) Bp_Buffer);

    case HDRX_ISRC1_PACK:
      return sthdmirx_read_ISRC1_packet(InpHandle_p,
                                        (HDRX_ISRC1_PACKET HDMI_RAM *) Bp_Buffer);

    case HDRX_ISRC2_PACK:
      return sthdmirx_read_ISRC2_packet(InpHandle_p,
                                        (HDRX_ISRC2_PACKET HDMI_RAM *) Bp_Buffer);

    case HDRX_ACR_PACK:
      return sthdmirx_read_ACR_packet(InpHandle_p,
                                      (HDRX_ACR_PACKET HDMI_RAM *) Bp_Buffer);

    case HDRX_GC_PACK:
      return sthdmirx_read_Gc_packet(InpHandle_p,
                                     (HDRX_GC_PACKET HDMI_RAM *) Bp_Buffer);

    case HDRX_GAMUT_PACK:
      return sthdmirx_read_gamut_packet(InpHandle_p,
                                        (HDRX_GAMUT_PACKET HDMI_RAM *) Bp_Buffer);

    default:
      return 0;
    }
}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_SW_clear_packet_data_memory(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *pInpHdl;
  pInpHdl = (hdmirx_route_handle_t *) Handle;

  HDMI_MEM_SET(&pInpHdl->stInfoPacketData, 0xff,
               sizeof(sthdmirx_Sdata_pkt_acquition_t));

  pInpHdl->stInfoPacketData.eColorDepth = HDRX_GCP_CD_NONE;
  pInpHdl->stNewDataFlags.bIsNewAviInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewAudioInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewAcs = FALSE;
  pInpHdl->stNewDataFlags.bIsNewSpdInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewVsInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewAcpInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewAcrInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewMpegInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewIsrc1Info = FALSE;
  pInpHdl->stNewDataFlags.bIsNewIsrc2Info = FALSE;
  pInpHdl->stNewDataFlags.bIsNewGcpInfo = FALSE;
  pInpHdl->stNewDataFlags.bIsNewMetaDataInfo = FALSE;

  HDMI_MEM_SET(&pInpHdl->stDataAvblFlags, 0,
               sizeof(sthdmirx_pkt_data_available_flags_t));

  return TRUE;

}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_clear_packet_memory(const hdmirx_handle_t Handle)
{

  hdmirx_route_handle_t *pInpHdl;

  pInpHdl = (hdmirx_route_handle_t *) Handle;
  if ((pInpHdl->HdmiRxMode == STM_HDMIRX_ROUTE_OP_MODE_HDMI)
      || (pInpHdl->HdmiRxMode == STM_HDMIRX_ROUTE_OP_MODE_AUTO))
    {
      HDMI_MEM_SET((U8 HDMI_MEM *) pInpHdl->PacketBaseAddress, 0xff,
                   32 * (HDRX_PACK_END - 1));
      HDMI_SET_REG_BITS_DWORD(((U32) pInpHdl->BaseAddress + HDRX_PACK_CTRL),
                              HDRX_CLEAR_ALL_PACKS);
      CPU_DELAY(100);
      HDMI_CLEAR_REG_BITS_DWORD(((U32) pInpHdl->BaseAddress + HDRX_PACK_CTRL),
                                HDRX_CLEAR_ALL_PACKS);
      /* Clear all software variables related to packet memory */
      sthdmirx_CORE_SW_clear_packet_data_memory(Handle);
    }
  return TRUE;
}

/******************************************************************************
 FUNCTION     :   This is defined for FPB1 Board.
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_clear_packet(const hdmirx_handle_t Handle,
                                sthdmirx_packet_id_t stPacketId)
{
  hdmirx_route_handle_t *pInpHdl;
  U32 PacketAddress;

  pInpHdl = (hdmirx_route_handle_t *) Handle;

  PacketAddress = pInpHdl->PacketBaseAddress;

  if (stPacketId == HDRX_UNDEF_PACK)
    {
      HDMI_PRINT(" Undefined packet Type\n");
      return FALSE;
    }

  if (stPacketId > HDRX_ACR_PACK)
    {
      stPacketId--;
    }

#ifdef USE_DIRECT_PACKET_MEMORY
  PacketAddress += (U32) (stPacketId * MEMORY_SIZE_PER_PACKET_TYPE);
#endif
  STHDMIRX_CLEAR_PACKET((U32) (pInpHdl->PacketBaseAddress), stPacketId);
  return TRUE;
}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_ISR_SDP_client(hdmirx_route_handle_t *const pInpHdl)
{
  volatile U16 ulSpdIrqStatus;

  ulSpdIrqStatus = HDMI_READ_REG_WORD(
                     (U32)(pInpHdl->BaseAddress + HDRX_SDP_IRQ_STATUS));

  if (ulSpdIrqStatus & HDRX_IF_VS_IRQ_STS)
    {
      sthdmirx_handle_VS_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_IF_AVI_IRQ_STS)
    {
      sthdmirx_handle_AV_info_frame_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_IF_SPD_IRQ_STS)
    {
      sthdmirx_handle_SPD_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_IF_AUDIO_IRQ_STS)
    {
      sthdmirx_handle_audio_info_frame_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_IF_MPEG_IRQ_STS)
    {
      sthdmirx_handle_MPEG_info_frame_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_ACP_IRQ_STS)
    {
      sthdmirx_handle_ACP_Data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_ISRC1_IRQ_STS)
    {
      sthdmirx_handle_ISRC1_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_ISRC2_IRQ_STS)
    {
      sthdmirx_handle_ISRC2_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_ACR_IRQ_STS)
    {
      sthdmirx_handle_ACR_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_GCP_IRQ_STS)
    {
      sthdmirx_handle_GCP_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_GMETA_IRQ_STS)
    {
      sthdmirx_handle_gamutlmeta_packet_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_MUTE_EVENT_IRQ_STS)
    {
      sthdmirx_handle_mute_events_data(pInpHdl);
    }

  if (ulSpdIrqStatus & HDRX_IF_UNDEF_IRQ_STS)
    {
      sthdmirx_handle_undefined_info_frame_data(pInpHdl);
    }

  /* clear the status after reading the Irq */
  HDMI_WRITE_REG_WORD((U32) (pInpHdl->BaseAddress + HDRX_SDP_IRQ_STATUS),
                      ulSpdIrqStatus);

  return TRUE;
}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_VS_data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_VENDOR_INFOFRAME pstTempInfo;

  if (sthdmirx_read_vendor_info_frame(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsVsInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stVSInfo, &pstTempInfo,
                   sizeof(HDRX_VENDOR_INFOFRAME)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stVSInfo,
                   &pstTempInfo, sizeof(HDRX_VENDOR_INFOFRAME));
      pInpHandle->stNewDataFlags.bIsNewVsInfo = TRUE;
    }

  return TRUE;
}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_AV_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle)
{

  HDRX_AVI_INFOFRAME pstTempInfo;

  if (sthdmirx_read_AVI_info_frame(pInpHandle, &pstTempInfo) == 0)
    {

      return FALSE;
    }
  HDMI_PRINT("AVI InfoFrame got changed\n");
  pInpHandle->stDataAvblFlags.bIsAviInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stAviInfo, &pstTempInfo,
                   sizeof(HDRX_AVI_INFOFRAME)))
    {
      sthdmirx_CORE_HDMI_signal_process_handler(pInpHandle);
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stAviInfo,
                   &pstTempInfo, sizeof(HDRX_AVI_INFOFRAME));
      //pInpHandle->stNewDataFlags.bIsNewAviInfo = TRUE;
    }

  return TRUE;

}

/******************************************************************************
FUNCTION     :
USAGE        :
INPUT        :
RETURN       :
USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_SPD_data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_SPD_INFOFRAME pstTempInfo;

  if (sthdmirx_read_SPD_info_frame(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsSpdInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stSpdInfo, &pstTempInfo,
                   sizeof(HDRX_SPD_INFOFRAME)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stSpdInfo,
                   &pstTempInfo, sizeof(HDRX_SPD_INFOFRAME));
      pInpHandle->stNewDataFlags.bIsNewSpdInfo = TRUE;
    }

  return TRUE;

}

/******************************************************************************
FUNCTION     :
USAGE        :
INPUT        :
RETURN       :
USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_audio_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_AUDIO_INFOFRAME pstTempInfo;

  if (sthdmirx_read_audio_info_frame(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsAudioInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stAudioInfo, &pstTempInfo,
                   sizeof(HDRX_AUDIO_INFOFRAME)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stAudioInfo,
                   &pstTempInfo, sizeof(HDRX_AUDIO_INFOFRAME));
      pInpHandle->stNewDataFlags.bIsNewAudioInfo = TRUE;
    }
  //HDMI_PRINT("Audio Info Frame Data capturing\n");
  return TRUE;

}

/******************************************************************************
FUNCTION     :
USAGE        :
INPUT        :
RETURN       :
USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_MPEG_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_MPEG_INFOFRAME pstTempInfo;

  if (sthdmirx_read_MPEG_info_frame(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }
  pInpHandle->stDataAvblFlags.bIsMpegInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stMpegInfo, &pstTempInfo,
                   sizeof(HDRX_MPEG_INFOFRAME)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stMpegInfo,
                   &pstTempInfo, sizeof(HDRX_MPEG_INFOFRAME));
      pInpHandle->stNewDataFlags.bIsNewMpegInfo = TRUE;
    }

  return TRUE;

}

/******************************************************************************
FUNCTION     :
USAGE        :
INPUT        :
RETURN       :
USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_ACP_Data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_ACP_PACKET pstTempInfo;

  if (sthdmirx_read_ACP_packet(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsAcpInfoAvbl = 1;

  //HDMI_PRINT(" ACP Type :%d\n",pstTempInfo.ACPType );

  if ((pstTempInfo.ACPType == 0) || (pstTempInfo.ACPType > 3))
    {
      pInpHandle->stAudioMngr.bAcpPacketPresent = FALSE;
    }
  else
    {
      /*Acp Packet is present & load the timeout */
      pInpHandle->stAudioMngr.bAcpPacketPresent = TRUE;
    }

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stAcpInfo, &pstTempInfo,
                   sizeof(HDRX_ACP_PACKET)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stAcpInfo,
                   &pstTempInfo, sizeof(HDRX_ACP_PACKET));
      pInpHandle->stNewDataFlags.bIsNewAcpInfo = TRUE;
    }

  /*To generate the interrupt again */
  sthdmirx_CORE_clear_packet(pInpHandle, HDRX_ACP_PACK);
  return TRUE;

}

/******************************************************************************
FUNCTION     :
USAGE        :
INPUT        :
RETURN       :
USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_ISRC1_data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_ISRC1_PACKET pstTempInfo;

  if (sthdmirx_read_ISRC1_packet(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsIsrc1InfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stIsrc1Info, &pstTempInfo,
                   sizeof(HDRX_ISRC1_PACKET)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stIsrc1Info,
                   &pstTempInfo, sizeof(HDRX_ISRC1_PACKET));
      pInpHandle->stNewDataFlags.bIsNewIsrc1Info = TRUE;
    }

  return TRUE;

}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_ISRC2_data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_ISRC2_PACKET pstTempInfo;

  if (sthdmirx_read_ISRC2_packet(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }
  pInpHandle->stDataAvblFlags.bIsIsrc2InfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stIsrc2Info, &pstTempInfo,
                   sizeof(HDRX_ISRC2_PACKET)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stIsrc2Info,
                   &pstTempInfo, sizeof(HDRX_ISRC2_PACKET));
      pInpHandle->stNewDataFlags.bIsNewIsrc2Info = TRUE;
    }

  return TRUE;

}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_ACR_data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_ACR_PACKET pstTempInfo;

  if (sthdmirx_read_ACR_packet(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }
  pInpHandle->stDataAvblFlags.bIsAcrInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stAcrInfo, &pstTempInfo,
                   sizeof(HDRX_ACR_PACKET)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stAcrInfo,
                   &pstTempInfo, sizeof(HDRX_ACR_PACKET));
      pInpHandle->stNewDataFlags.bIsNewAcrInfo = TRUE;
    }

  return TRUE;

}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_GCP_data(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_GC_PACKET pstTempInfo;

  if (sthdmirx_read_Gc_packet(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsGcpInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stGcpInfo, &pstTempInfo,
                   sizeof(HDRX_GC_PACKET)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stGcpInfo,
                   &pstTempInfo, sizeof(HDRX_GC_PACKET));
      pInpHandle->stNewDataFlags.bIsNewGcpInfo = TRUE;
    }

  return TRUE;

}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_gamutlmeta_packet_data(
  hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_GAMUT_PACKET pstTempInfo;

  if (sthdmirx_read_gamut_packet(pInpHandle, &pstTempInfo) == 0)
    {
      return FALSE;
    }

  pInpHandle->stDataAvblFlags.bIsMetaDataInfoAvbl = 1;

  if (HDMI_MEM_CMP(&pInpHandle->stInfoPacketData.stMetaDataInfo, &pstTempInfo,
                   sizeof(HDRX_GAMUT_PACKET)))
    {
      HDMI_MEM_CPY(&pInpHandle->stInfoPacketData.stMetaDataInfo,
                   &pstTempInfo, sizeof(HDRX_GAMUT_PACKET));
      pInpHandle->stNewDataFlags.bIsNewMetaDataInfo = TRUE;
    }

  return TRUE;

}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_mute_events_data(hdmirx_route_handle_t *const pInpHandle)
{
  UNUSED_PARAMETER(pInpHandle);
  return FALSE;
}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_handle_undefined_info_frame_data(
  hdmirx_route_handle_t *const pInpHandle)
{
  UNUSED_PARAMETER(pInpHandle);
  return FALSE;
}

/* End of file */
