/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_hdcp.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/

#include <hdmirx_drv.h>
#include <hdmirx_core.h>
#include <InternalTypes.h>
#include <hdmirx_RegOffsets.h>
#include <linux/ktime.h>
#include <stm_display.h>
#include <linux/kthread.h>
#include <stm_hdmirx.h>
#include <linux/freezer.h>

/* Private Typedef -----------------------------------------------*/

/* Private Defines ------------------------------------------------*/

/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/

/* Interface procedures/functions ----------------------------------*/
/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_init
 USAGE        :     Hdcp Initialization
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_CORE_HDCP_init(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *Handle_p;
  U32 Read_reg;
  Handle_p = (hdmirx_route_handle_t *) Handle;
  /**************************************************************************
      1. Clock Switch to TCLK should be there
      2. Key must be preloaded to Key Ram by secure processor
  ****************************************************************************/
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDRX_KEYHOST_CONTROL), 0x10);
  STHDMIRX_DELAY_1ms(100);
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDRX_KEYHOST_CONTROL), 0x01);
  /*HDCP key loading*/
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDCP_T1_INIT_CTRL), 0x01);
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDCP_KEY_RD_CTRL), 0x01);

  STHDMIRX_DELAY_1ms(1000);
  /*Loading of the keys*/

  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDCP_KEY_RD_STATUS), 0xFF);
  STHDMIRX_DELAY_1ms(200);

  Read_reg= HDMI_READ_REG_WORD((U32)(Handle_p->BaseAddress + HDCP_KEY_RD_STATUS));
  #if 0 /*for debug purpose*/
  if(Read_reg==2)
    HDMI_PRINT("HDMI_HDCP: Error in loading the keys !!!!!\n");
  #endif
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDCP_T1_INIT_CTRL), 0x0);
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDCP_KEY_RD_CTRL), 0x0);

  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDRX_KEYHOST_CONTROL), 0x2);
  if ((Handle_p->HdmiRxMode != STM_HDMIRX_ROUTE_OP_MODE_DVI))
    {
      /* HDCP 1.1 without options and Enable HDCP with encrypted keys */
      HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_HDCP_CONTROL),
                          (HDRX_HDCP_HDMI_CAPABLE | HDRX_HDCP_AVMUTE_AUTO_EN |
                           HDRX_HDCP_AVMUTE_IGNORE));
    }
  /* Timebase is computed for a TCLK f 30MHz */
  HDMI_WRITE_REG_DWORD((U32) (Handle_p->BaseAddress + HDRX_HDCP_TIMEBASE ), 0x002DC6C0);

  /* Watchdog is computed to wait 5 second for the downstreams to be ready */
  HDMI_WRITE_REG_WORD((U32) (Handle_p->BaseAddress + HDRX_HDCP_TIMEOUT ), 0x320);

  /* Set register AKSV to be monitored to generate authencition detected event*/
  HDMI_WRITE_REG_WORD((U32) (Handle_p->BaseAddress + HDRX_HDCP_TBL_IRQ_INDEX ), 0x10);

  /* Disable I2C communication */
  HDMI_WRITE_REG_BYTE((U32) (Handle_p->BaseAddress + HDRX_HDCP_ADDR), 0x00);

  /* Enable HDCP */
  HDMI_SET_REG_BITS_WORD((U32)(Handle_p->BaseAddress + HDRX_HDCP_CONTROL),
                         (HDRX_HDCP_FAST_REAUTHENT_EN | HDRX_HDCP_EN));

  /* Clear authentication */
  HDMI_WRITE_REG_BYTE((U32)(Handle_p->BaseAddress + HDRX_HDCP_STATUS_EXT),
                      HDRX_HDCP_RE_AUTHENTICATION);

  /* Enable key loading */
  HDMI_SET_REG_BITS_WORD((U32)(Handle_p->BaseAddress + HDRX_HDCP_CONTROL),
                         HDRX_HDCP_BKSV_LOAD_EN);

  /* Delay should be here for 1 ms */
  STHDMIRX_DELAY_1ms(1);

  /* Clear authentication */
  HDMI_WRITE_REG_BYTE((U32)(Handle_p->BaseAddress + HDRX_HDCP_STATUS_EXT),
                      HDRX_HDCP_RE_AUTHENTICATION);

  /*Disable HDCP */
  HDMI_CLEAR_REG_BITS_WORD((U32)(Handle_p->BaseAddress + HDRX_HDCP_CONTROL),
                           HDRX_HDCP_EN);

  Handle_p->DownstreamKsvSize = 0;
  Handle_p->repeater_fn_support =true;
  Handle_p->RepeaterReady =false;
  memset(&Handle_p->DownstreamKsvList[0], 0, 127*5);


}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_enable
 USAGE        :     Enable/Disable the hdcp blocks
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_CORE_HDCP_enable(const hdmirx_handle_t Handle, BOOL B_Enable)
{

  if (B_Enable)
    {
       HDMI_CLEAR_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL),
                                        (HDRX_HDCP_DOWNSTREAM_READY | HDRX_HDCP_KSV_LIST_READY)) ;

       HDMI_SET_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_BSTATUS),
                                        0x0);
       HDMI_SET_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                    HDRX_HDCP_CONTROL), (HDRX_HDCP_EN | HDRX_HDCP_DECRYPT_EN |
                                                         HDRX_HDCP_BKSV_LOAD_EN));

    }
  else
    {
      HDMI_CLEAR_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                     HDRX_HDCP_CONTROL), HDRX_HDCP_EN);
    }

}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_set_HDCP_TWSaddrs
 USAGE        :     Set The I2C slave Address
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_CORE_set_HDCP_TWSaddrs(const hdmirx_handle_t Handle, U8 Addrs)
{
  U8 tWSAddrs;
  tWSAddrs = HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                 HDRX_HDCP_ADDR)) & 0x80;
  tWSAddrs |= (Addrs & 0x7f);
  tWSAddrs |=0x80;
  HDMI_WRITE_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_ADDR),
                      tWSAddrs);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_is_encryption_enabled
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_HDCP_is_encryption_enabled(const hdmirx_handle_t Handle)
{
  if (HDMI_READ_REG_BYTE((GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_STATUS))
      & HDRX_HDCP_DEC_STATUS)
    {
      return TRUE;
    }

  return FALSE;
}
/******************************************************************************
  FUNCTION     :     sthdmirx_CORE_HDCP_is_repeater_enabled
  USAGE        :
  INPUT        :
  RETURN       :
  USED_REGS    :
 ******************************************************************************/
BOOL sthdmirx_CORE_HDCP_is_repeater_enabled(const hdmirx_handle_t Handle)
{
  if (HDMI_READ_REG_BYTE(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL)
      & HDRX_HDCP_REPEATER)
   {
     return TRUE;
   }
  return FALSE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_is_repeater_ready
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_HDCP_is_repeater_ready(const hdmirx_handle_t Handle)
{
  if (HDMI_READ_REG_BYTE(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_STATUS)
         & HDRX_HDCP_STATE_WAIT_DWNSTRM_STS)
    {
      return TRUE;
    }
  return FALSE;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_CORE_HDCP_status_get
 USAGE        :   Get HDCP authentication status from Hdmi Hardware
 INPUT        :   None
 RETURN       :
 USED_REGS    :   HDRX_SDP_STATUS, HDRX_SDP_STATUS
******************************************************************************/
stm_hdmirx_route_hdcp_status_t sthdmirx_CORE_HDCP_status_get(const
    hdmirx_route_handle_t *pInpHandle)
{

  stm_hdmirx_route_hdcp_status_t Status;
  if (HDMI_READ_REG_BYTE((GET_CORE_BASE_ADDRS(pInpHandle) +
                          HDRX_HDCP_STATUS)) & HDRX_HDCP_AUTHENTICATION)
    {
      Status = STM_HDMIRX_ROUTE_HDCP_STATUS_AUTHENTICATION_DETECTED;

      if (HDMI_READ_REG_BYTE((U32) (GET_CORE_BASE_ADDRS(pInpHandle) +
                                    HDRX_SDP_STATUS)) & HDRX_NOISE_DETECTED)
        {
          Status = STM_HDMIRX_ROUTE_HDCP_STATUS_NOISE_DETECTED;
        }
    }
  else
    {
      Status = STM_HDMIRX_ROUTE_HDCP_STATUS_NOT_AUTHENTICATED;
    }

  return Status;
}
/******************************************************************************
 FUNCTION     :   sthdmirx_CORE_HDCP_Authentication_Detection
 USAGE        :   Get HDCP Authentication Detection
 INPUT        :   None
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_HDCP_Authentication_Detection(
     hdmirx_route_handle_t *const pInpHandle)
{

  if (HDMI_READ_REG_BYTE((GET_CORE_BASE_ADDRS(pInpHandle) +
      HDRX_HDCP_STATUS_EXT )) & HDRX_HDCP_RE_AUTHENTICATION)
    {

      HDMI_CLEAR_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(pInpHandle) + HDRX_HDCP_UPSTREAM_CTRL),
                               (HDRX_HDCP_DOWNSTREAM_READY | HDRX_HDCP_KSV_LIST_READY)) ;
      HDMI_SET_REG_BITS_BYTE((U32)(GET_CORE_BASE_ADDRS(pInpHandle) + HDRX_HDCP_UPSTREAM_CTRL),
                               HDRX_HDCP_KSV_FIFO_RESET );

      HDMI_SET_REG_BITS_BYTE((U32)(GET_CORE_BASE_ADDRS(pInpHandle) + HDRX_HDCP_KSV_FIFO_CTRL),
                              HDRX_HDCP_KSV_FIFO_SCL_STRCH_EN);

      stm_hdmirx_delay_us(1);

      HDMI_SET_REG_BITS_BYTE( (U32)(GET_CORE_BASE_ADDRS(pInpHandle) + HDRX_HDCP_STATUS_EXT), HDRX_HDCP_RE_AUTHENTICATION);

      return TRUE;
    }

  return FALSE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_set_repeater_mode_enable
 USAGE        :     Enable/Disable the hdcp repeater mode
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_CORE_HDCP_set_repeater_mode_enable(const hdmirx_handle_t Handle, BOOL B_Enable)
{
  hdmirx_dev_handle_t *dHandle_p;
  dHandle_p = (hdmirx_dev_handle_t *) Handle;

  if (B_Enable)
    {
      HDMI_SET_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL),
                                HDRX_HDCP_REPEATER);
      HDMI_SET_REG_BITS_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL),
                                HDRX_HDCP_KSV_FIFO_RESET );
    }
    else {
      HDMI_CLEAR_REG_BITS_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL),
                                HDRX_HDCP_REPEATER) ;
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_set_repeater_downstream_status
 USAGE        :     Set Downstream info in status register
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_CORE_HDCP_set_repeater_downstream_status(const hdmirx_handle_t Handle,
                                                       stm_hdmirx_hdcp_downstream_status_t *status)
{
   U32 reg =
   (status->max_cascade_exceeded?HDRX_HDCP_MAX_CASCADE_EXCEEDED:0x0) |
   ((status->depth & 0x07) << 8) |
   (status->max_devs_exceeded?HDRX_HDCP_MAX_DEV_EXCEEDED:0x0) |
   (status->device_count & 0x7F);
    HDMI_WRITE_REG_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_BSTATUS),
                             reg);
}

/**
*      SHA_Ft - Computing Function used in SHA Algorithm.
*      @t: round number
*      @B,C,D: Variable to hash
*
*      RETURNS:
*      the result of the hash

*/

static uint32_t SHA_Ft(U8 t ,U32 B,U32 C,U32 D)
{
  if(t<20)
    {
      return (B & C) | ((0xFFFFFFFF-B) & D);
    }
  else if (t<40)
    {
      return (B ^ C ^ D);
    }
  else if (t<60)
    {
      return (B & C) | (B & D) | (C & D) ;
    }
  else/*t<80*/
    {
      return (B ^ C ^ D);
    }
}

/**
*      SHA_K - Rounding Constant used in SHA-1 Algorithm.
*      @t: round number
*
*      RETURNS:
*      the result of the SHA-1
*/
static uint32_t SHA_K(U8 t)
{
  if(t<20)
    {
      return 0x5a827999;
    }
  else if (t<40)
    {
      return 0x6ed9eba1;
    }
  else if (t<60)
    {
      return 0x8f1bbcdc;
    }
  else/*t<80*/
    {
      return 0xca62c1d6;
    }
}

/**
*      SHA_Shift - Rolling to the left by n bits shifted back from right
*              Shift_n(X) = (X << n) OR (X >> 32-n);
*      @t: round number
*
*      RETURNS:
*      the result of the SHA shift
*/
static U32 SHA_Shift(U8 n, U32 X)
{
  return ( (X << n) | (X >> (32-n)) );
}
/**
*      SHA_1 -
*      @ArrayKSV_p: Pointer  to the table containing the KSV, BStatus and Mi value
*      @ByteNumber: Size of the structure ArrayKSV_p in byte
*      @Result_p: pointer to the memory to store the result of the SHA-1 algorithm
*/
static void SHA_1(U8* ArrayKSV_p, U16 ByteNumber, U32* Result_p)
{
  /*const K = [0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6]; generated an error here*/
  U16 N_512_pad,i,t,j;
  U32 Msg_bit_Length;/*Limitation to the SHA, only 32 not 64 bit total size*/
  U32 A,B,C,D,E,H0,H1,H2,H3,H4,W[80],TEMP;


  /*-------------------------------------------------------------------------------
Step 1 : Padding
-------------------------------------------------------------------------------*/
  N_512_pad = ( (ByteNumber+8) / 64)+1; /*for 3 bytes-> 1 block, 55 -> 1, 56 ? ->, 57 -> 2*/
  ArrayKSV_p[ByteNumber] = 0x80; /*1 and 000 0000 are padded */

  for(i=ByteNumber+1;i<N_512_pad*64-4;i++)
    {
      ArrayKSV_p[i] = 0;
    }
  Msg_bit_Length = ByteNumber * 8;
  ArrayKSV_p[N_512_pad*64-4] = Msg_bit_Length>>24;
  ArrayKSV_p[N_512_pad*64-3] = Msg_bit_Length>>16;
  ArrayKSV_p[N_512_pad*64-2] = Msg_bit_Length>>8;
  ArrayKSV_p[N_512_pad*64-1] = Msg_bit_Length;

  /*-------------------------------------------------------------------------------
Padding is OK till this point
Step 2 :
   * Computing Hash by 512 bit blocks for(i)
-------------------------------------------------------------------------------*/

  H0 = 0x67452301;
  H1 = 0xEFCDAB89;
  H2 = 0x98BADCFE;
  H3 = 0x10325476;
  H4 = 0xC3D2E1F0;

  for(i=0;i<N_512_pad;i++) /* (N_512_pad) blocks will be computed */
    {
      A = H0; B = H1; C = H2; D = H3; E = H4;
      for (j=0;j<16;j++) /* 16 loop, 4 each => 64 byte, 512 bit block treated*/
        {
          W[j] = ArrayKSV_p[(i*64)+4*j] * 16777216 + ArrayKSV_p[(i*64)+4*j+1]*65536 + ArrayKSV_p[(i*64)+4*j+2] *256 + ArrayKSV_p[(i*64)+4*j+3];

        }
      for (t=16;t<80;t++)
        {
          W[t] = SHA_Shift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
        }
      for (t=0;t<80;t++)
        {
          TEMP = SHA_Shift(5,(A)) + SHA_Ft(t,B,C,D) + E + W[t] + SHA_K(t);
          E = D; D = C; C = SHA_Shift(30,(B)); B = A; A = TEMP;
        }
      H0 = H0 + A; H1 = H1 + B; H2 = H2 + C; H3 = H3 + D; H4 = H4 + E;
    }

  Result_p[0] = H0;
  Result_p[1] = H1;
  Result_p[2] = H2;
  Result_p[3] = H3;
  Result_p[4] = H4;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_set_downstream_ksv_list
 USAGE        :     Set Downstream info in status register
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_CORE_HDCP_set_downstream_ksv_list(hdmirx_route_handle_t * Handle,
                                                U8 *ksv_list,
                                                U32 size)
{
  int i;
  U32 hash_result[5];
  U8 value[sizeof(U8)*11*64];
  U32 m0[2];
  U32 bstatus = 0;
  memset( &value[0], 0, sizeof(U8)*64*11);

  memcpy(&Handle->DownstreamKsvList[0], &ksv_list[0], sizeof(U8)*127*5);
  Handle->DownstreamKsvSize = size;
  HDMI_SET_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL),
      HDRX_HDCP_DOWNSTREAM_READY);


  for (i=0 ; i<Handle->DownstreamKsvSize ; i++)
    {
      value[i] = Handle->DownstreamKsvList[i];
      HDMI_PRINT("From HDMIRX driver KSV[%d] =0x%x\n", i, value[i]);
    }

  HDMI_WRITE_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_TBL_RD_INDEX ), 0x41) ;
  bstatus = HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_TBL_RD_DATA )) ;

  HDMI_WRITE_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_TBL_RD_INDEX ), 0x42) ;
  bstatus |= (HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_TBL_RD_DATA )) << 8) ;
  HDMI_PRINT("Bstatus value is : 0x%04x\n",bstatus);
  value[i++] = bstatus & 0xff;
  value[i++] = (bstatus >> 8) & 0xff;
  m0[0] = HDMI_READ_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_M0_0 ));
  HDMI_PRINT("M0 value of HDRX_HDCP_M0_0 register : 0x%08x\n",m0[0]);
  m0[1] = HDMI_READ_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_M0_1 ));
  HDMI_PRINT("M0 value of HDRX_HDCP_M0_1 register : 0x%08x\n",m0[1]);


  value[i++] = (m0[0] >> 0 ) & 0xff;
  value[i++] = (m0[0] >> 8 ) & 0xff;
  value[i++] = (m0[0] >> 16) & 0xff;
  value[i++] = (m0[0] >> 24) & 0xff;
  value[i++] = (m0[1] >> 0 ) & 0xff;
  value[i++] = (m0[1] >> 8 ) & 0xff;
  value[i++] = (m0[1] >> 16) & 0xff;
  value[i++] = (m0[1] >> 24) & 0xff;

  SHA_1(&(value[0]), i, &(hash_result[0]));

  HDMI_PRINT("Write V' into register : 0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",hash_result[0],hash_result[1],hash_result[2],hash_result[3],hash_result[4]);

  HDMI_WRITE_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_V0_0 ), hash_result[0]);
  HDMI_WRITE_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_V0_1 ), hash_result[1]);
  HDMI_WRITE_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_V0_2 ), hash_result[2]);
  HDMI_WRITE_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_V0_3 ), hash_result[3]);
  HDMI_WRITE_REG_DWORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_V0_4 ), hash_result[4]);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDCP_set_repeater_status_ready
 USAGE        :     Provide Fifo Size, Depth, V' and KSV Fifo list to the Upstream
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_CORE_HDCP_set_repeater_status_ready(hdmirx_route_handle_t * Handle, BOOL B_Ready)
{
  int i,j;
  U8 ksv_list[127*5];
  U32 size;
  U8 status;
  unsigned long time_elapsed;
  unsigned long time_now;
  hdmirx_route_handle_t * Handle_p = (hdmirx_route_handle_t *) Handle;

  Handle_p->RepeaterReady = B_Ready;

  if (Handle_p->RepeaterReady)
    {
      size = Handle_p->DownstreamKsvSize;
      memcpy(&(ksv_list[0]), &(Handle_p->DownstreamKsvList[0]), 127*5);
      i = 0;

      // Write maximum 10 bytes into the fifo
      for (j = 0 ; (i<size) && (j<10) ; j++)
        {
          HDMI_PRINT("%s HDRX_HDCP_KSV_FIFO ksv_fifo[%d] =0x%x\n",__FUNCTION__,i,ksv_list[i]);
          HDMI_WRITE_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_KSV_FIFO),
                    ksv_list[i++]);
          stm_hdmirx_delay_us(1);
        }

      i=0;

      HDMI_SET_REG_BITS_WORD((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_UPSTREAM_CTRL),
      HDRX_HDCP_KSV_LIST_READY);

      if ( size <= 5)
        {
          for (j = 0 ; (i<size) && (j<10) ; j++)
            {
              HDMI_PRINT("%s HDRX_HDCP_KSV_FIFO ksv_fifo[%d] =0x%x\n",__FUNCTION__,i,ksv_list[i]);
              HDMI_WRITE_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_KSV_FIFO),
                   ksv_list[i++]);
              stm_hdmirx_delay_us(1);
            }
          i=0;
        }
        time_elapsed = stm_hdmirx_convert_ms_to_jiffies(100)+ stm_hdmirx_time_now();
        time_now = stm_hdmirx_time_now();

        while (time_now < time_elapsed)
          {
            if (HDMI_READ_REG_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                HDRX_HDCP_STATUS_EXT )) & HDRX_HDCP_RE_AUTHENTICATION)
              {
                break;
              }
            time_now = stm_hdmirx_time_now();
            status = HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_KSV_FIFO_STS ));
            HDMI_PRINT("%s waiting to set again KSV FiFO with status 0x%x\n", __FUNCTION__,status );
            if((status & HDRX_HDCP_KSV_FIFO_E)|| (status & HDRX_HDCP_KSV_FIFO_ERROR))
              {
                for (j = 0 ; (i<size) && (j<10) ; j++)
                  {
                    HDMI_PRINT("%s HDRX_HDCP_KSV_FIFO ksv_fifo[%d] =0x%x\n",__FUNCTION__,i,ksv_list[i]);
                    HDMI_WRITE_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_KSV_FIFO),
                    ksv_list[i++]);
                    stm_hdmirx_delay_us(1);
                  }
                break;
              }
          }
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) + HDRX_HDCP_KSV_FIFO_CTRL),
                 HDRX_HDCP_KSV_FIFO_SCL_STRCH_EN);
    }
}
/* End of file */
