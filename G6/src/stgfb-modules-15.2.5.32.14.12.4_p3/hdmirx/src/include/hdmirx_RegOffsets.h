/***********************************************************************
 *
 * File: hdmirx/src/include/hdmirx_RegOffsets.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __REGS_H__
#define __REGS_H__

// Date: 101130

/****************************************************************************/
/*  R E G I S T E R S   D E F I N I T I O N S                               */
/****************************************************************************/

/*          REGISTER NAME                       ADDRESS    TYPE  POD    UPDATES */
/*          ____________________________________________________________________*/

//    #define REGBASE                                 0x8100

#define DVIB_OFFSET                             0x700

#define CPHY_AUX_PHY_CTRL                              0x0000	//33536 (CPHY_PHYA)
#define CPHY_PHY_CTRL                                  0x000C	//33548 (CPHY_PHYA)
#define CPHY_L0_CTRL_0                                 0x0010	//33552 (CPHY_PHYA)
#define CPHY_L0_CTRL_4                                 0x0014	//33556 (CPHY_PHYA)
#define CPHY_L0_CPTRIM                                 0x0018	//33560 (CPHY_PHYA)
#define CPHY_L1_CTRL_0                                 0x001C	//33564 (CPHY_PHYA)
#define CPHY_L1_CTRL_4                                 0x0020	//33568 (CPHY_PHYA)
#define CPHY_L1_CPTRIM                                 0x0024	//33572 (CPHY_PHYA)
#define CPHY_L2_CTRL_0                                 0x0028	//33576 (CPHY_PHYA)
#define CPHY_L2_CTRL_4                                 0x002C	//33580 (CPHY_PHYA)
#define CPHY_L2_CPTRIM                                 0x0030	//33584 (CPHY_PHYA)
#define CPHY_L3_CTRL_0                                 0x0034	//33588 (CPHY_PHYA)
#define CPHY_L3_CTRL_4                                 0x0038	//33592 (CPHY_PHYA)
#define CPHY_L3_CPTRIM                                 0x003C	//33596 (CPHY_PHYA)
#define CPHY_RCAL_RESULT                               0x0040	//33600 (CPHY_PHYA)
#define CPHY_ALT_FREQ_MEAS_CTRL                        0x0044	//33604 (CPHY_PHYA)
#define CPHY_ALT_FREQ_MEAS_RESULT                      0x0048	//33608 (CPHY_PHYA)
#define CPHY_TEST_CTRL                                 0x004C	//33612 (CPHY_PHYA)
#define CPHY_BIST_CONTROL                              0x0050	//33616 (CPHY_PHYA)
#define CPHY_BIST_PATTERN0                             0x0054	//33620 (CPHY_PHYA)
#define CPHY_BIST_PATTERN1                             0x0058	//33624 (CPHY_PHYA)
#define CPHY_BIST_PATTERN2                             0x005C	//33628 (CPHY_PHYA)
#define CPHY_BIST_PATTERN3                             0x0060	//33632 (CPHY_PHYA)
#define CPHY_BIST_PATTERN4                             0x0064	//33636 (CPHY_PHYA)
#define CPHY_BIST_STATUS                               0x0068	//33640 (CPHY_PHYA)
#define CPHY_RSVRBITS                                  0x006C	//33644 (CPHY_PHYA)
#define CPHY_BIST_TMDS_CRC_UPDATE                      0x0070	//33648 (CPHY_PHYA)
#define CPHY_JE_CTRL                                   0x0080	//33664 (CPHY_PHY_JE)
#define CPHY_JE_CONFIG                                 0x0084	//33668 (CPHY_PHY_JE)
#define CPHY_JE                                        0x0088	//33672 (CPHY_PHY_JE)
#define CPHY_JE_TST_THRESHOLD                          0x008C	//33676 (CPHY_PHY_JE)
#define CPHY_JE_STATUS                                 0x0090	//33680 (CPHY_PHY_JE)
#if 0
#define CPHY1_AUX_PHY_CTRL                              0x0100	//33792 (CPHY1_PHYA)
#define CPHY1_PHY_CTRL                                  0x010C	//33804 (CPHY1_PHYA)
#define CPHY1_L0_CTRL_0                                 0x0110	//33808 (CPHY1_PHYA)
#define CPHY1_L0_CTRL_4                                 0x0114	//33812 (CPHY1_PHYA)
#define CPHY1_L0_CPTRIM                                 0x0118	//33816 (CPHY1_PHYA)
#define CPHY1_L1_CTRL_0                                 0x011C	//33820 (CPHY1_PHYA)
#define CPHY1_L1_CTRL_4                                 0x0120	//33824 (CPHY1_PHYA)
#define CPHY1_L1_CPTRIM                                 0x0124	//33828 (CPHY1_PHYA)
#define CPHY1_L2_CTRL_0                                 0x0128	//33832 (CPHY1_PHYA)
#define CPHY1_L2_CTRL_4                                 0x012C	//33836 (CPHY1_PHYA)
#define CPHY1_L2_CPTRIM                                 0x0130	//33840 (CPHY1_PHYA)
#define CPHY1_L3_CTRL_0                                 0x0134	//33844 (CPHY1_PHYA)
#define CPHY1_L3_CTRL_4                                 0x0138	//33848 (CPHY1_PHYA)
#define CPHY1_L3_CPTRIM                                 0x013C	//33852 (CPHY1_PHYA)
#define CPHY1_RCAL_RESULT                               0x0140	//33856 (CPHY1_PHYA)
#define CPHY1_ALT_FREQ_MEAS_CTRL                        0x0144	//33860 (CPHY1_PHYA)
#define CPHY1_ALT_FREQ_MEAS_RESULT                      0x0148	//33864 (CPHY1_PHYA)
#define CPHY1_TEST_CTRL                                 0x014C	//33868 (CPHY1_PHYA)
#define CPHY1_BIST_CONTROL                              0x0150	//33872 (CPHY1_PHYA)
#define CPHY1_BIST_PATTERN0                             0x0154	//33876 (CPHY1_PHYA)
#define CPHY1_BIST_PATTERN1                             0x0158	//33880 (CPHY1_PHYA)
#define CPHY1_BIST_PATTERN2                             0x015C	//33884 (CPHY1_PHYA)
#define CPHY1_BIST_PATTERN3                             0x0160	//33888 (CPHY1_PHYA)
#define CPHY1_BIST_PATTERN4                             0x0164	//33892 (CPHY1_PHYA)
#define CPHY1_BIST_STATUS                               0x0168	//33896 (CPHY1_PHYA)
#define CPHY1_RSVRBITS                                  0x016C	//33900 (CPHY1_PHYA)
#define CPHY1_BIST_TMDS_CRC_UPDATE                      0x0170	//33904 (CPHY1_PHYA)
#define CPHY1_JE_CTRL                                   0x0180	//33920 (CPHY1_PHY_JE)
#define CPHY1_JE_CONFIG                                 0x0184	//33924 (CPHY1_PHY_JE)
#define CPHY1_JE                                        0x0188	//33928 (CPHY1_PHY_JE)
#define CPHY1_JE_TST_THRESHOLD                          0x018C	//33932 (CPHY1_PHY_JE)
#define CPHY1_JE_STATUS                                 0x0190	//33936 (CPHY1_PHY_JE)
#define CPHY2_AUX_PHY_CTRL                              0x0200	//34048 (CPHY2_PHYA)
#define CPHY2_PHY_CTRL                                  0x020C	//34060 (CPHY2_PHYA)
#define CPHY2_L0_CTRL_0                                 0x0210	//34064 (CPHY2_PHYA)
#define CPHY2_L0_CTRL_4                                 0x0214	//34068 (CPHY2_PHYA)
#define CPHY2_L0_CPTRIM                                 0x0218	//34072 (CPHY2_PHYA)
#define CPHY2_L1_CTRL_0                                 0x021C	//34076 (CPHY2_PHYA)
#define CPHY2_L1_CTRL_4                                 0x0220	//34080 (CPHY2_PHYA)
#define CPHY2_L1_CPTRIM                                 0x0224	//34084 (CPHY2_PHYA)
#define CPHY2_L2_CTRL_0                                 0x0228	//34088 (CPHY2_PHYA)
#define CPHY2_L2_CTRL_4                                 0x022C	//34092 (CPHY2_PHYA)
#define CPHY2_L2_CPTRIM                                 0x0230	//34096 (CPHY2_PHYA)
#define CPHY2_L3_CTRL_0                                 0x0234	//34100 (CPHY2_PHYA)
#define CPHY2_L3_CTRL_4                                 0x0238	//34104 (CPHY2_PHYA)
#define CPHY2_L3_CPTRIM                                 0x023C	//34108 (CPHY2_PHYA)
#define CPHY2_RCAL_RESULT                               0x0240	//34112 (CPHY2_PHYA)
#define CPHY2_ALT_FREQ_MEAS_CTRL                        0x0244	//34116 (CPHY2_PHYA)
#define CPHY2_ALT_FREQ_MEAS_RESULT                      0x0248	//34120 (CPHY2_PHYA)
#define CPHY2_TEST_CTRL                                 0x024C	//34124 (CPHY2_PHYA)
#define CPHY2_BIST_CONTROL                              0x0250	//34128 (CPHY2_PHYA)
#define CPHY2_BIST_PATTERN0                             0x0254	//34132 (CPHY2_PHYA)
#define CPHY2_BIST_PATTERN1                             0x0258	//34136 (CPHY2_PHYA)
#define CPHY2_BIST_PATTERN2                             0x025C	//34140 (CPHY2_PHYA)
#define CPHY2_BIST_PATTERN3                             0x0260	//34144 (CPHY2_PHYA)
#define CPHY2_BIST_PATTERN4                             0x0264	//34148 (CPHY2_PHYA)
#define CPHY2_BIST_STATUS                               0x0268	//34152 (CPHY2_PHYA)
#define CPHY2_RSVRBITS                                  0x026C	//34156 (CPHY2_PHYA)
#define CPHY2_BIST_TMDS_CRC_UPDATE                      0x0270	//34160 (CPHY2_PHYA)
#define CPHY2_JE_CTRL                                   0x0280	//34176 (CPHY2_PHY_JE)
#define CPHY2_JE_CONFIG                                 0x0284	//34180 (CPHY2_PHY_JE)
#define CPHY2_JE                                        0x0288	//34184 (CPHY2_PHY_JE)
#define CPHY2_JE_TST_THRESHOLD                          0x028C	//34188 (CPHY2_PHY_JE)
#define CPHY2_JE_STATUS                                 0x0290	//34192 (CPHY2_PHY_JE)
#endif
//SWENG_0524 (7): Temporary changed to 0x0C00 in order to align with CPHY_1 for Athena EV1 board debugging.
//Manoranjani_0524 -- Changed to absolute values
#define HDMI_RX_PHY_RES_CTRL                            0x0000	//36864 (C9HDRX_HDMI_RX_PHY_TOP)
#define HDMI_RX_PHY_CLK_SEL                             0x0004	//36868 (C9HDRX_HDMI_RX_PHY_TOP)
#define HDMI_RX_PHY_CLK_MS_CTRL                         0x0008	//36872 (C9HDRX_HDMI_RX_PHY_TOP)
#define HDMI_RX_PHY_IP_VERSION                          0x000C	//36876 (C9HDRX_HDMI_RX_PHY_TOP)
#define HDMI_RX_PHY_CTRL                                0x0048	//36936 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_STATUS                              0x004C	//36940 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_PHS_PICK                            0x0050	//36944 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_SIGQUAL                             0x0054	//36948 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_PHS_A_SCR                           0x0058	//36952 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_PHS_B_SCR                           0x005C	//36956 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_PHS_C_SCR                           0x0060	//36960 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_IRQ_EN                              0x0064	//36964 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_IRQ_STS                             0x0068	//36968 (C9HDRX_HDMI_RX_PHY_DG)
#define HDMI_RX_PHY_TST_CTRL                            0x0098	//37016 ()

// Core base address 0x9000
#define HDRX_TCLK_SEL                                   0x0100	//37120 ()
#define HDRX_LNK_CLK_SEL                                0x0104	//37124 ()
#define HDRX_VCLK_SEL                                   0x0108	//37128 ()
#define HDRX_OUT_PIX_CLK_SEL                            0x010C	//37132 ()
#define HDRX_AUCLK_SEL                                  0x0110	//37136 ()
#define HDRX_RESET_CTRL                                 0x0114	//37140 ()
#define HDRX_CLK_FST_MS_CTRL                            0x0118	//37144 ()
#define HDRX_CLK_MS_FST_STATUS                          0x011C	//37148 ()
#define HDRX_CLK_RSRV                                   0x012C	//37164 ()
#define HDRX_IRQ_STATUS                                 0x0130	//37168 ()
#define HDRX_HPD_CTRL                                   0x0134	//37172 ()
#define HDRX_HPD_STATUS                                 0x0138	//37176 ()
#define HDRX_HPD_IRQ_EN                                 0x013C	//37180 ()
#define HDRX_HPD_IRQ_STS                                0x0140	//37184 ()
#define HDRX_MUTE_TOP_STATUS                            0x0144	//37188 ()
#define HDRX_MUTE_TOP_IRQ_EN                            0x0148	//37192 ()
#define HDRX_MUTE_TOP_IRQ_STS                           0x014C	//37196 ()
#define HDMI_RX_DIG_IP_VERSION                          0x0150	//37200 ()
#define HDRX_TOP_RSRV                                   0x0168	//37224 ()
#define HDRX_TOP_TST_CTRL                               0x016C	//37228 ()
#define HDRX_MAIN_LINK_STATUS                           0x0170	//37232 ()
#define HDRX_INPUT_SEL                                  0x0174	//37236 ()
#define HDRX_LINK_CONFIG                                0x0178	//37240 ()
#define HDRX_LINK_ERR_CTRL                              0x017C	//37244 ()
#define HDRX_PHY_DEC_ERR_STATUS                         0x0180	//37248 ()
#define HDRX_SIGNL_DET_CTRL                             0x0184	//37252 ()
#define HDRX_FREQ_MEAS_PERIOD                           0x0188	//37256 ()
#define HDRX_FREQ_TH                                    0x018C	//37260 (C9HDRX_HDMI_RX_CORE_LINK)
#define HDRX_FREQ_MEAS_CT                               0x0190	//37264 ()
#define HDRX_CLK_STBL_ZN                                0x0194	//37268 ()
#define HDRX_MEAS_RESULT                                0x0198	//37272 ()
#define HDRX_MEAS_IRQ_EN                                0x019C	//37276 ()
#define HDRX_MEAS_IRQ_STS                               0x01A0	//37280 ()
#define HDRX_TMDS_RX_CTL                                0x01A4	//37284 ()
#define HDRX_TMDS_RX_BIST_STB_CTRL                      0x01A8	//37288 ()
#define HDRX_TMDS_RX_BIST_CTRL                          0x01AC	//37292 ()
#define HDRX_TMDS_RX_BIST_CRC_CTRL                      0x01B0
#define HDRX_TMDS_RX_BIST_STATUS                        0x01B0	//37296 ()
#define HDRX_TMDS_RX_BIST_PRBS_ERROR                    0x01B8
#define HDRX_TMDS_RX_BIST_CRC_ERROR                     0x01BC
#define HDRX_TMDS_RX_BIST_SIGNATURE0                    0x01C0
#define HDRX_TMDS_RX_BIST_SIGNATURE1                    0x01C4
#define HDRX_PHY_LNK_CRC_TEST_STS                       0x01D4	//37332 ()
#define HDRX_LINK_RSRV                                  0x01D8	//37336 (C9HDRX_HDMI_RX_CORE_LINK)
#define HDRX_LINK_TST_CTRL                              0x01DC	//37340 ()
#define HDRX_SUB_SAMPLER                                0x01E0	//37344 ()
#define HDRX_VID_CONV_CTRL                              0x01E4	//37348 ()
#define HDRX_VID_BUF_OFFSET                             0x01E8	//37352 ()
#define HDRX_MD_CHNG_MT_DEL                             0x01EC	//37356 ()
#define HDRX_VID_BUF_DELAY                              0x01F0	//37360 ()
#define HDRX_VID_BUF_FILT_ZONE                          0x01F4	//37364 ()
#define HDRX_COLOR_DEPH_STATUS                          0x01F8	//37368 ()
#define HDRX_VIDEO_BUF_IRQ_EN                           0x01FC	//37372 ()
#define HDRX_VIDEO_BUF_IRQ_STS                          0x0200	//37376 ()
#define HDRX_VID_LNK_RSRV                               0x0218	//37400 ()
#define HDRX_VID_LNK_TST_CTRL                           0x021C	//37404 ()
#define HDRX_VIDEO_CTRL                                 0x0220	//37408 ()
#define HDRX_VID_REGEN_CTRL                             0x0224	//37412 ()
#define HDRX_PATGEN_CONTROL                             0x0228	//37416 ()
#define HDRX_DE_WIDTH_CLAMP                             0x022C	//37420 ()
#define HDRX_VIDEO_MUTE_RGB                             0x0230	//37424 ()
#define HDRX_VIDEO_MUTE_YC                              0x0234	//37428 ()
#define HDRX_DITHER_CTRL                                0x0238	//37432 ()
#define HDRX_VIDEO_CRC_TEST_VALUE                       0x0250	//37456 ()
#define HDRX_VIDEO_CRC_TEST_STS                         0x0254	//37460 ()
#define HDRX_VIDEO_RSRV                                 0x0258	//37464 ()
#define HDRX_VIDEO_TST_CTRL                             0x025C	//37468 ()
#define HDRX_SDP_CTRL                                   0x0260	//37472 (C9HDRX_HDMI_RX_CORE_SDP)
#define HDRX_SDP_AV_MUTE_CTRL                           0x0264	//37476 ()
#define HDRX_SDP_STATUS                                 0x0268	//37480 ()
#define HDRX_SDP_IRQ_CTRL                               0x026C	//37484 ()
#define HDRX_SDP_IRQ_STATUS                             0x0270	//37488 ()
#define HDRX_AU_CONV_CTRL                               0x0274	//37492 ()
#define HDRX_PACK_CTRL                                  0x0278	//37496 (C9HDRX_HDMI_RX_CORE_SDP)
#define HDRX_PACKET_SEL                                 0x027C	//37500 (C9HDRX_HDMI_RX_CORE_SDP)
#define HDRX_PKT_STATUS                                 0x0280	//37504 (C9HDRX_HDMI_RX_CORE_SDP)
#define HDRX_UNDEF_PKT_CODE                             0x0284	//37508 ()
#define HDRX_PACK_GUARD_CTRL                            0x0288	//37512 ()
#define HDRX_NOISE_DET_CTRL                             0x028C	//37516 ()
#define HDRX_PACK_CT                                    0x0290	//37520 ()
#define HDRX_ERROR_CT                                   0x0294	//37524 ()
#define HDRX_AUTO_PACK_PROCESS                          0x0298	//37528 ()
#define HDRX_AVI_INFOR_TIMEOUT_THRESH                   0x029C	//37532 ()
#define HDRX_AU_INFO_TIMEOUT_THRESH                     0x02A0	//37536 ()
#define HDRX_AVI_AUTO_FIELDS                            0x02A4	//37540 ()
#define HDRX_SDP_PRSNT_STS                              0x02A8	//37544 ()
#define HDRX_SDP_MUTE_SRC_STATUS                        0x02AC	//37548 ()

#define HDRX_SDP_ACR_N_VALUE                            0x02B0	/*for Cannes*/
#define HDRX_SDP_ACR_CTS_VALUE                          0x02B4	/*for Cannes*/
#define HDRX_SDP_ACR_NCTS_STB                           0x02B8	/*for Cannes*/
#define HDRX_AUBUFF_ERR_STB_DE_COUNT                    0x02BC	/*for Cannes*/

#define HDRX_SDP_RSRV                                   0x02C8	//37576 (C9HDRX_HDMI_RX_CORE_SDP)
#define HDRX_SDP_TST_CTRL                               0x02CC	//37580 (C9HDRX_HDMI_RX_CORE_SDP)
#define HDRX_SDP_AUD_CTRL                               0x02D0	//37584 ()
#define HDRX_SDP_AUD_MUTE_CTRL                          0x02D4	//37588 ()
#define HDRX_SDP_AUD_IRQ_CTRL                           0x02D8	//37592 ()
#define HDRX_SDP_AUDIO_IRQ_STATUS                       0x02DC	//37596 ()
#define HDRX_SDP_AUD_STS                                0x02E0	//37600 ()
#define HDRX_AUD_MUTE_SRC_STS                           0x02E4	//37604 ()
#define HDRX_SDP_AUD_ERROR_CT                           0x02E8	//37608 ()
#define HDRX_SDP_AUD_BUF_CNTRL                          0x02EC	//37612 ()
#define HDRX_SDP_AUD_BUF_DELAY                          0x02F0	//37616 ()
#define HDRX_SDP_AUD_BUFPTR_DIF                         0x02F4	//37620 ()
#define HDRX_SDP_AUD_BUFPTR_DIF_THRESH_A                0x02F8	//37624 ()
#define HDRX_SDP_AUD_BUFPTR_DIF_THRESH_B                0x02FC	//37628 ()
#define HDRX_SDP_AUD_LINE_THRESH                        0x0300	//37632 ()
#define HDRX_SDP_AUD_OUT_CNTRL                          0x0304	//37636 ()
#define HDRX_SDP_AUD_CHAN_RELOC                         0x0308	//37640 ()
#define HDRX_SDP_AUD_CH_STS_0                           0x030C	//37644 ()
#define HDRX_SDP_AUD_CH_STS_1                           0x0310	//37648 ()
#define HDRX_SDP_AUD_CH_STS_2                           0x0314	//37652 ()
#define HDRX_SDP_AUD_RSRV                               0x0330	//37680 (C9HDRX_HDMI_RX_CORE_AUD)
#define HDRX_SDP_AUD_TST_CTRL                           0x0334	//37684 ()
#define HDRX_HDCP_CONTROL                               0x0440	//37952 (C9HDCP_HDRX)
#define HDRX_HDCP_ADDR                                  0x0444	//37956 (C9HDCP_HDRX)
#define HDRX_HDCP_WIN_MOD                               0x0448	//37960 (C9HDCP_HDRX)
#define HDRX_HDCP_BKSV_0                                0x044C	//37964 (C9HDCP_HDRX)
#define HDRX_HDCP_BKSV_1                                0x0450	//37968 (C9HDCP_HDRX)
#define HDRX_HDCP_STATUS                                0x0454	//37972 (C9HDCP_HDRX)
#define HDRX_HDCP_STATUS_EXT                            0x0458	//37976 (C9HDCP_HDRX)
#define HDRX_HDCP_TBL_IRQ_INDEX                         0x045C	//37980 (C9HDCP_HDRX)
#define HDRX_HDCP_TBL_RD_INDEX                          0x0460	//37984 (C9HDCP_HDRX)
#define HDRX_HDCP_TBL_RD_DATA                           0x0464	//37988 (C9HDCP_HDRX)
#define HDRX_HDCP_UPSTREAM_CTRL                         0x0480	//38016 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_UPSTREAM_STATUS                       0x0484	//38020 ()
#define HDRX_HDCP_BSTATUS                               0x0488	//38024 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_TIMEOUT                               0x048C	//38028 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_TIMEBASE                              0x0490	//38032 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_V0_0                                  0x0494	//38036 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_V0_1                                  0x0498	//38040 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_V0_2                                  0x049C	//38044 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_V0_3                                  0x04A0	//38048 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_V0_4                                  0x04A4	//38052 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_M0_0                                  0x04A8	//38056 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_M0_1                                  0x04AC	//38060 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_KSV_FIFO                              0x04B0	//38064 (C9HDCP_HDRX_RPTR)
#define HDRX_HDCP_KSV_FIFO_CTRL                         0x04B4	//38068 ()
#define HDRX_HDCP_KSV_FIFO_STS                          0x04B8	//38072 (C9HDCP_HDRX_RPTR)
#define HDMI_INST_SOFT_RESETS                           0x0500	//38144 (C9HDMI_INSTRUMENT )
#define HDMI_INST_HARD_RESETS                           0x0504	//38148 (C9HDMI_INSTRUMENT)
#define HDMI_HOST_CONTROL                               0x0508	//38152 (C9HDMI_INSTRUMENT)
#define HDMI_AFR_CONTROL                                0x050C	//38156 (C9HDMI_INSTRUMENT)
#define HDMI_AFB_CONTROL                                0x0510	//38160 (C9HDMI_INSTRUMENT)
#define HDMI_INPUT_IRQ_MASK                             0x0514	//38164 (C9HDMI_INSTRUMENT)
#define HDMI_INPUT_IRQ_STATUS                           0x0518	//38168 (C9HDMI_INSTRUMENT)
#define HDMI_TEST_IRQ                                   0x051C	//38172 (C9HDMI_INSTRUMENT)
#define INSTRU_CLOCK_CONFIG                             0x0520	//38176 (C9HDMI_INSTRUMENT)
#define HDMI_IFM_CLK_CONTROL                            0x0524	//38180 (C9HDMI_INSTRUMENT)
#define HDMI_INSTRUMENT_POLARITY_CONTROL                0x0528	//38184 ()
#define HDMI_IFM_CTRL                                   0x0560	//38240 (C9HDMI_IFM)
#define HDMI_IFM_WATCHDOG                               0x0564	//38244 (C9HDMI_IFM)
#define HDMI_IFM_HLINE                                  0x0568	//38248 (C9HDMI_IFM)
#define HDMI_IFM_CHANGE                                 0x056C	//38252 (C9HDMI_IFM)
#define HDMI_IFM_HS_PERIOD                              0x0570	//38256 (C9HDMI_IFM)
#define HDMI_IFM_HS_PULSE                               0x0574	//38260 (C9HDMI_IFM)
#define HDMI_IFM_VS_PERIOD                              0x0578	//38264 (C9HDMI_IFM)
#define HDMI_IFM_VS_PULSE                               0x057C	//38268 (C9HDMI_IFM)
#define HDMI_IFM_SPACE                                  0x0580	//38272 (C9HDMI_IFM)
#define HDMI_INPUT_CONTROL                              0x05B0	//38320 (C9HDMI_IMD)
#define HDMI_INPUT_HS_DELAY                             0x05B4	//38324 (C9HDMI_IMD)
#define HDMI_INPUT_VS_DELAY                             0x05B8	//38328 (C9HDMI_IMD)
#define HDMI_INPUT_H_ACT_START                          0x05BC	//38332 (C9HDMI_IMD)
#define HDMI_INPUT_H_ACT_WIDTH                          0x05C0	//38336 (C9HDMI_IMD)
#define HDMI_INPUT_V_ACT_START_ODD                      0x05C4	//38340 (C9HDMI_IMD)
#define HDMI_INPUT_V_ACT_START_EVEN                     0x05C8	//38344 (C9HDMI_IMD)
#define HDMI_INPUT_V_ACT_LENGTH                         0x05CC	//38348 (C9HDMI_IMD)
#define HDMI_INPUT_FREEZE_VIDEO                         0x05D0	//38352 (C9HDMI_IMD)
#define HDMI_INPUT_FLAGLINE                             0x05D4	//38356 (C9HDMI_IMD)
#define HDMI_INPUT_VLOCK_EVENT                          0x05D8	//38360 (C9HDMI_IMD)
#define HDMI_INPUT_FRAME_RESET_LINE                     0x05DC	//38364 (C9HDMI_IMD)
#define HDMI_INPUT_RFF_READY_LINE                       0x05E0	//38368 (C9HDMI_IMD)
#define HDMI_INPUT_HMEASURE_START                       0x05E4	//38372 (C9HDMI_IMD)
#define HDMI_INPUT_HMEASURE_WIDTH                       0x05E8	//38376 (C9HDMI_IMD)
#define HDMI_INPUT_VMEASURE_START                       0x05EC	//38380 (C9HDMI_IMD)
#define HDMI_INPUT_VMEASURE_LENGTH                      0x05F0	//38384 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_CONTROL                          0x05F4	//38388 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_HTOTAL                           0x05F8	//38392 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_VTOTAL                           0x05FC	//38396 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_HSTART                           0x0600	//38400 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_HWIDTH                           0x0604	//38404 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_VSTART                           0x0608	//38408 (C9HDMI_IMD)
#define HDMI_INPUT_IBD_VLENGTH                          0x060C	//38412 (C9HDMI_IMD)
#define HDMI_INPUT_PIXGRAB_H                            0x0610	//38416 (C9HDMI_IMD)
#define HDMI_INPUT_PIXGRAB_V                            0x0614	//38420 (C9HDMI_IMD)
#define HDMI_INPUT_PIXGRAB_RED                          0x0618	//38424 (C9HDMI_IMD)
#define HDMI_INPUT_PIXGRAB_GRN                          0x061C	//38428 (C9HDMI_IMD)
#define HDMI_INPUT_PIXGRAB_BLU                          0x0620	//38432 (C9HDMI_IMD)
#define PU_CONFIG                                       0x0640	//38464 (C9HDMI_PU)
#define HDRX_CRC_CTRL                                   0x0644	//38468 (C9HDMI_PU)
#define HDRX_CRC_ERR_STATUS                             0x0648	//38472 (C9HDMI_PU)
#define HDRX_CRC_RED                                    0x064C	//38476 (C9HDMI_PU)
#define HDRX_CRC_GREEN                                  0x0650	//38480 (C9HDMI_PU)
#define HDRX_CRC_BLUE                                   0x0654	//38484 (C9HDMI_PU)
#define PU_DEBUG_TEST_CTRL                              0x0658	//38488 (C9HDMI_PU)
#define PU_SPARE_REG                                    0x065C	//38492 (C9HDMI_PU)
#define DVI_HDCP_CTRL                                   0x0660	//38496 (C9HDMI_PU)
#define HDRX_TOP_IRQ_STATUS                             0x0664

#define DUAL_DVI0_CTRL                                  0x0668	//46692 (C9DUAL_DVI)
#define DUAL_DVI0_IRQ_AFR_AFB_STATUS                    0x066C	//46696 (C9DUAL_DVI)
#define DUAL_DVI0_AUTO_DELAY_STATUS                     0x0670	//46700 (C9DUAL_DVI)
#define DUAL_DVI0_DBG_CTRL                              0x0674	//46704 (C9DUAL_DVI)

#define HDRX_I2S_TX_CONTROL                             0x0700
#define HDRX_I2S_TX_CLK_CNTRL                           0x0704
#define HDRX_I2S_TX_BIT_WIDTH                           0x0708
#define HDRX_I2S_TX_DATA_LSB_POS                        0x070C
#define HDRX_I2S_TX_SYNC_SYS                            0x0710
#define HDRX_I2S_TX_COMMON                              0x0714
#define HDRX_I2S_TX_IRQ_EN                              0x0718
#define HDRX_I2S_TX_IRQ_STS                             0x071C

#define HDCP_T1_INIT_CTRL                               0x0740
#define HDCP_KEY_RD_CTRL                                0x0744
#define HDCP_KEY_RD_STATUS                              0x0748
#define HDRX_KEYHOST_CONTROL                            0x0760	//37936 (C9HDCP_KEYHOST)
#define HDRX_KEYHOST_STATUS                             0x0764	//37940 (C9HDCP_KEYHOST)

#define SOFT_RESETS_2                                   0xc808

// CSM base address 0x000
#define     HDRX_CSM_CTRL                               0x0000
#define     HDRX_CSM_TEST_BUS_SEL                       0x0004
#define     HDRX_CSM_IRQ_STATUS                         0x0008
#define     HPD_DIRECT_CTRL                             0x000C
#define     HPD_DIRECT_STATUS                           0x0010
#define     HPD_DIRECT_IRQ_CTRL                         0x0014
#define     HPD_DIRECT_IRQ_STATUS                       0x0018
#define     EDPD_DIRECT_CTRL                            0x001C
#define     EDPD_DIRECT_STATUS                          0x0020

#if 0
#define DVI0_RX_PHY_RES_CTRL                            0xB000	//45056 (C9DVI0_DVI0_RX_PHY_TOP)
#define DVI0_RX_PHY_CLK_SEL                             0xB004	//45060 (C9DVI0_DVI0_RX_PHY_TOP)
#define DVI0_RX_PHY_CLK_MS_CTRL                         0xB008	//45064 (C9DVI0_DVI0_RX_PHY_TOP)
#define DVI0_RX_PHY_IP_VERSION                          0xB00C	//45068 (C9DVI0_DVI0_RX_PHY_TOP)
#define DVI0_RX_PHY_CTRL                                0xB048	//45128 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_STATUS                              0xB04C	//45132 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_PHS_PICK                            0xB050	//45136 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_SIGQUAL                             0xB054	//45140 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_PHS_A_SCR                           0xB058	//45144 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_PHS_B_SCR                           0xB05C	//45148 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_PHS_C_SCR                           0xB060	//45152 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_IRQ_EN                              0xB064	//45156 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_IRQ_STS                             0xB068	//45160 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_RX_PHY_TST_CTRL                            0xB098	//45208 (C9DVI0_DVI0_RX_PHY_DG)
#define DVI0_TCLK_SEL                                   0xB100	//45312 ()
#define DVI0_LNK_CLK_SEL                                0xB104	//45316 ()
#define DVI0_VCLK_SEL                                   0xB108	//45320 ()
#define DVI0_OUT_PIX_CLK_SEL                            0xB10C	//45324 ()
#define DVI0_AUCLK_SEL                                  0xB110	//45328 ()
#define DVI0_RESET_CTRL                                 0xB114	//45332 ()
#define DVI0_CLK_FST_MS_CTRL                            0xB118	//45336 ()
#define DVI0_CLK_MS_FST_STATUS                          0xB11C	//45340 ()
#define DVI0_CLK_RSRV                                   0xB12C	//45356 ()
#define DVI0_IRQ_STATUS                                 0xB130	//45360 ()
#define DVI0_HPD_CTRL                                   0xB134	//45364 (C9DVI0_DVI0_RX_CORE_DVI0_TOP)
#define DVI0_HPD_STATUS                                 0xB138	//45368 ()
#define DVI0_HPD_IRQ_EN                                 0xB13C	//45372 ()
#define DVI0_HPD_IRQ_STS                                0xB140	//45376 ()
#define DVI0_MUTE_TOP_STATUS                            0xB144	//45380 ()
#define DVI0_MUTE_TOP_IRQ_EN                            0xB148	//45384 ()
#define DVI0_MUTE_TOP_IRQ_STS                           0xB14C	//45388 ()
#define DVI0_RX_DIG_IP_VERSION                          0xB150	//45392 ()
#define DVI0_TOP_RSRV                                   0xB168	//45416 (C9DVI0_DVI0_RX_CORE_DVI0_TOP)
#define DVI0_TOP_TST_CTRL                               0xB16C	//45420 ()
#define DVI0_MAIN_LINK_STATUS                           0xB170	//45424 ()
#define DVI0_INPUT_SEL                                  0xB174	//45428 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_LINK_CONFIG                                0xB178	//45432 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_LINK_ERR_CTRL                              0xB17C	//45436 ()
#define DVI0_PHY_DEC_ERR_STATUS                         0xB180	//45440 ()
#define DVI0_SIGNL_DET_CTRL                             0xB184	//45444 ()
#define DVI0_FREQ_MEAS_PERIOD                           0xB188	//45448 ()
#define DVI0_FREQ_TH                                    0xB18C	//45452 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_FREQ_MEAS_CT                               0xB190	//45456 ()
#define DVI0_CLK_STBL_ZN                                0xB194	//45460 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_MEAS_RESULT                                0xB198	//45464 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_MEAS_IRQ_EN                                0xB19C	//45468 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_MEAS_IRQ_STS                               0xB1A0	//45472 ()
#define DVI0_TMDS_RX_CTL                                0xB1A4	//45476 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_TMDS_RX_BIST_STB_CTRL                      0xB1A8	//45480 ()
#define DVI0_TMDS_RX_BIST_CTRL                          0xB1AC	//45484 ()
#define DVI0_TMDS_RX_BIST_STATUS                        0xB1B0	//45488 ()
#define DVI0_PHY_LNK_CRC_TEST_STS                       0xB1D4	//45524 ()
#define DVI0_LINK_RSRV                                  0xB1D8	//45528 (C9DVI0_DVI0_RX_CORE_LINK)
#define DVI0_LINK_TST_CTRL                              0xB1DC	//45532 ()
#define DVI0_SUB_SAMPLER                                0xB1E0	//45536 ()
#define DVI0_VID_CONV_CTRL                              0xB1E4	//45540 ()
#define DVI0_VID_BUF_OFFSET                             0xB1E8	//45544 ()
#define DVI0_MD_CHNG_MT_DEL                             0xB1EC	//45548 ()
#define DVI0_VID_BUF_DELAY                              0xB1F0	//45552 ()
#define DVI0_VID_BUF_FILT_ZONE                          0xB1F4	//45556 ()
#define DVI0_COLOR_DEPH_STATUS                          0xB1F8	//45560 ()
#define DVI0_VIDEO_BUF_IRQ_EN                           0xB1FC	//45564 ()
#define DVI0_VIDEO_BUF_IRQ_STS                          0xB200	//45568 ()
#define DVI0_VID_LNK_RSRV                               0xB218	//45592 ()
#define DVI0_VID_LNK_TST_CTRL                           0xB21C	//45596 ()
#define DVI0_VIDEO_CTRL                                 0xB220	//45600 (C9DVI0_DVI0_RX_CORE_VIDEO)
#define DVI0_VID_REGEN_CTRL                             0xB224	//45604 ()
#define DVI0_PATGEN_CONTROL                             0xB228	//45608 ()
#define DVI0_DE_WIDTH_CLAMP                             0xB22C	//45612 ()
#define DVI0_VIDEO_MUTE_RGB                             0xB230	//45616 ()
#define DVI0_VIDEO_MUTE_YC                              0xB234	//45620 ()
#define DVI0_DITHER_CTRL                                0xB238	//45624 ()
#define DVI0_VIDEO_CRC_TEST_VALUE                       0xB250	//45648 ()
#define DVI0_VIDEO_CRC_TEST_STS                         0xB254	//45652 ()
#define DVI0_VIDEO_RSRV                                 0xB258	//45656 (C9DVI0_DVI0_RX_CORE_VIDEO)
#define DVI0_VIDEO_TST_CTRL                             0xB25C	//45660 ()
#define DVI0_SDP_CTRL                                   0xB260	//45664 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_SDP_AV_MUTE_CTRL                           0xB264	//45668 ()
#define DVI0_SDP_STATUS                                 0xB268	//45672 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_SDP_IRQ_CTRL                               0xB26C	//45676 ()
#define DVI0_SDP_IRQ_STATUS                             0xB270	//45680 ()
#define DVI0_AU_CONV_CTRL                               0xB274	//45684 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_PACK_CTRL                                  0xB278	//45688 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_PACKET_SEL                                 0xB27C	//45692 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_PKT_STATUS                                 0xB280	//45696 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_UNDEF_PKT_CODE                             0xB284	//45700 ()
#define DVI0_PACK_GUARD_CTRL                            0xB288	//45704 ()
#define DVI0_NOISE_DET_CTRL                             0xB28C	//45708 ()
#define DVI0_PACK_CT                                    0xB290	//45712 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_ERROR_CT                                   0xB294	//45716 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_AUTO_PACK_PROCESS                          0xB298	//45720 ()
#define DVI0_AVI_INFOR_TIMEOUT_THRESH                   0xB29C	//45724 ()
#define DVI0_AU_INFO_TIMEOUT_THRESH                     0xB2A0	//45728 ()
#define DVI0_AVI_AUTO_FIELDS                            0xB2A4	//45732 ()
#define DVI0_SDP_PRSNT_STS                              0xB2A8	//45736 ()
#define DVI0_SDP_MUTE_SRC_STATUS                        0xB2AC	//45740 ()
#define DVI0_SDP_RSRV                                   0xB2C8	//45768 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_SDP_TST_CTRL                               0xB2CC	//45772 (C9DVI0_DVI0_RX_CORE_SDP)
#define DVI0_SDP_AUD_CTRL                               0xB2D0	//45776 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_SDP_AUD_MUTE_CTRL                          0xB2D4	//45780 ()
#define DVI0_SDP_AUD_IRQ_CTRL                           0xB2D8	//45784 ()
#define DVI0_SDP_AUDIO_IRQ_STATUS                       0xB2DC	//45788 ()
#define DVI0_SDP_AUD_STS                                0xB2E0	//45792 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_AUD_MUTE_SRC_STS                           0xB2E4	//45796 ()
#define DVI0_SDP_AUD_ERROR_CT                           0xB2E8	//45800 ()
#define DVI0_SDP_AUD_BUF_CNTRL                          0xB2EC	//45804 ()
#define DVI0_SDP_AUD_BUF_DELAY                          0xB2F0	//45808 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_SDP_AUD_BUFPTR_DIF                         0xB2F4	//45812 ()
#define DVI0_SDP_AUD_BUFPTR_DIF_THRESH_A                0xB2F8	//45816 ()
#define DVI0_SDP_AUD_BUFPTR_DIF_THRESH_B                0xB2FC	//45820 ()
#define DVI0_SDP_AUD_LINE_THRESH                        0xB300	//45824 ()
#define DVI0_SDP_AUD_OUT_CNTRL                          0xB304	//45828 ()
#define DVI0_SDP_AUD_CHAN_RELOC                         0xB308	//45832 ()
#define DVI0_SDP_AUD_CH_STS_0                           0xB30C	//45836 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_SDP_AUD_CH_STS_1                           0xB310	//45840 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_SDP_AUD_CH_STS_2                           0xB314	//45844 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_SDP_AUD_RSRV                               0xB330	//45872 (C9DVI0_DVI0_RX_CORE_AUD)
#define DVI0_SDP_AUD_TST_CTRL                           0xB334	//45876 ()
#define DVI0_KEYHOST_CONTROL                            0xB430	//46128 (C9HDCP_KEYHOST)
#define DVI0_KEYHOST_STATUS                             0xB434	//46132 (C9HDCP_KEYHOST)
#define DVI0_HDCP_CONTROL                               0xB440	//46144 (C9HDCP_HDRX)
#define DVI0_HDCP_ADDR                                  0xB444	//46148 (C9HDCP_HDRX)
#define DVI0_HDCP_WIN_MOD                               0xB448	//46152 (C9HDCP_HDRX)
#define DVI0_HDCP_BKSV_0                                0xB44C	//46156 (C9HDCP_HDRX)
#define DVI0_HDCP_BKSV_1                                0xB450	//46160 (C9HDCP_HDRX)
#define DVI0_HDCP_STATUS                                0xB454	//46164 (C9HDCP_HDRX)
#define DVI0_HDCP_STATUS_EXT                            0xB458	//46168 (C9HDCP_HDRX)
#define DVI0_HDCP_TBL_IRQ_INDEX                         0xB45C	//46172 (C9HDCP_HDRX)
#define DVI0_HDCP_TBL_RD_INDEX                          0xB460	//46176 (C9HDCP_HDRX)
#define DVI0_HDCP_TBL_RD_DATA                           0xB464	//46180 (C9HDCP_HDRX)
#define DVI0_HDCP_UPSTREAM_CTRL                         0xB480	//46208 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_UPSTREAM_STATUS                       0xB484	//46212 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_BSTATUS                               0xB488	//46216 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_TIMEOUT                               0xB48C	//46220 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_TIMEBASE                              0xB490	//46224 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_V0_0                                  0xB494	//46228 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_V0_1                                  0xB498	//46232 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_V0_2                                  0xB49C	//46236 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_V0_3                                  0xB4A0	//46240 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_V0_4                                  0xB4A4	//46244 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_M0_0                                  0xB4A8	//46248 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_M0_1                                  0xB4AC	//46252 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_KSV_FIFO                              0xB4B0	//46256 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_KSV_FIFO_CTRL                         0xB4B4	//46260 (C9HDCP_DVI0_RPTR)
#define DVI0_HDCP_KSV_FIFO_STS                          0xB4B8	//46264 (C9HDCP_DVI0_RPTR)
#define DVI0_INST_SOFT_RESETS                           0xB500	//46336 (C9DVI0_INSTRUMENT )
#define DVI0_INST_HARD_RESETS                           0xB504	//46340 (C9DVI0_INSTRUMENT)
#define DVI0_HOST_CONTROL                               0xB508	//46344 (C9DVI0_INSTRUMENT)
#define DVI0_AFR_CONTROL                                0xB50C	//46348 (C9DVI0_INSTRUMENT)
#define DVI0_AFB_CONTROL                                0xB510	//46352 (C9DVI0_INSTRUMENT)
#define DVI0_INPUT_IRQ_MASK                             0xB514	//46356 (C9DVI0_INSTRUMENT)
#define DVI0_INPUT_IRQ_STATUS                           0xB518	//46360 (C9DVI0_INSTRUMENT)
#define DVI0_TEST_IRQ                                   0xB51C	//46364 (C9DVI0_INSTRUMENT)
#define DVI0_INSTRU_CLOCK_CONFIG                        0xB520	//46368 (C9DVI0_INSTRUMENT)
#define DVI0_IFM_CLK_CONTROL                            0xB524	//46372 (C9DVI0_INSTRUMENT)
#define DVI0_INSTRUMENT_POLARITY_CONTROL                0xB528	//46376 ()
#define DVI0_IFM_CTRL                                   0xB560	//46432 (C9DVI0_IFM)
#define DVI0_IFM_WATCHDOG                               0xB564	//46436 (C9DVI0_IFM)
#define DVI0_IFM_HLINE                                  0xB568	//46440 (C9DVI0_IFM)
#define DVI0_IFM_CHANGE                                 0xB56C	//46444 (C9DVI0_IFM)
#define DVI0_IFM_HS_PERIOD                              0xB570	//46448 (C9DVI0_IFM)
#define DVI0_IFM_HS_PULSE                               0xB574	//46452 (C9DVI0_IFM)
#define DVI0_IFM_VS_PERIOD                              0xB578	//46456 (C9DVI0_IFM)
#define DVI0_IFM_VS_PULSE                               0xB57C	//46460 (C9DVI0_IFM)
#define DVI0_IFM_SPACE                                  0xB580	//46464 (C9DVI0_IFM)
#define DVI0_INPUT_CONTROL                              0xB5B0	//46512 (C9DVI0_IMD)
#define DVI0_INPUT_HS_DELAY                             0xB5B4	//46516 (C9DVI0_IMD)
#define DVI0_INPUT_VS_DELAY                             0xB5B8	//46520 (C9DVI0_IMD)
#define DVI0_INPUT_H_ACT_START                          0xB5BC	//46524 (C9DVI0_IMD)
#define DVI0_INPUT_H_ACT_WIDTH                          0xB5C0	//46528 (C9DVI0_IMD)
#define DVI0_INPUT_V_ACT_START_ODD                      0xB5C4	//46532 (C9DVI0_IMD)
#define DVI0_INPUT_V_ACT_START_EVEN                     0xB5C8	//46536 (C9DVI0_IMD)
#define DVI0_INPUT_V_ACT_LENGTH                         0xB5CC	//46540 (C9DVI0_IMD)
#define DVI0_INPUT_FREEZE_VIDEO                         0xB5D0	//46544 (C9DVI0_IMD)
#define DVI0_INPUT_FLAGLINE                             0xB5D4	//46548 (C9DVI0_IMD)
#define DVI0_INPUT_VLOCK_EVENT                          0xB5D8	//46552 (C9DVI0_IMD)
#define DVI0_INPUT_FRAME_RESET_LINE                     0xB5DC	//46556 (C9DVI0_IMD)
#define DVI0_INPUT_RFF_READY_LINE                       0xB5E0	//46560 (C9DVI0_IMD)
#define DVI0_INPUT_HMEASURE_START                       0xB5E4	//46564 (C9DVI0_IMD)
#define DVI0_INPUT_HMEASURE_WIDTH                       0xB5E8	//46568 (C9DVI0_IMD)
#define DVI0_INPUT_VMEASURE_START                       0xB5EC	//46572 (C9DVI0_IMD)
#define DVI0_INPUT_VMEASURE_LENGTH                      0xB5F0	//46576 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_CONTROL                          0xB5F4	//46580 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_HTOTAL                           0xB5F8	//46584 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_VTOTAL                           0xB5FC	//46588 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_HSTART                           0xB600	//46592 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_HWIDTH                           0xB604	//46596 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_VSTART                           0xB608	//46600 (C9DVI0_IMD)
#define DVI0_INPUT_IBD_VLENGTH                          0xB60C	//46604 (C9DVI0_IMD)
#define DVI0_INPUT_PIXGRAB_H                            0xB610	//46608 (C9DVI0_IMD)
#define DVI0_INPUT_PIXGRAB_V                            0xB614	//46612 (C9DVI0_IMD)
#define DVI0_INPUT_PIXGRAB_RED                          0xB618	//46616 (C9DVI0_IMD)
#define DVI0_INPUT_PIXGRAB_GRN                          0xB61C	//46620 (C9DVI0_IMD)
#define DVI0_INPUT_PIXGRAB_BLU                          0xB620	//46624 (C9DVI0_IMD)
#define DVI0_CONFIG                                     0xB640	//46656 (C9DVI0_PU)
#define DVI0_CRC_CTRL                                   0xB644	//46660 (C9DVI0_PU)
#define DVI0_CRC_ERR_STATUS                             0xB648	//46664 (C9DVI0_PU)
#define DVI0_CRC_RED                                    0xB64C	//46668 (C9DVI0_PU)
#define DVI0_CRC_GREEN                                  0xB650	//46672 (C9DVI0_PU)
#define DVI0_CRC_BLUE                                   0xB654	//46676 (C9DVI0_PU)
#define DVI0_DEBUG_TEST_CTRL                            0xB658	//46680 (C9DVI0_PU)
#define DVI0_SPARE_REG                                  0xB65C	//46684 (C9DVI0_PU)
#define DVI0_HDCP_CTRL                                  0xB660	//46688 (C9DVI0_PU)
#define DUAL_DVI0_CTRL                                  0xB664	//46692 (C9DUAL_DVI)
#define DUAL_DVI0_IRQ_AFR_AFB_STATUS                    0xB668	//46696 (C9DUAL_DVI)
#define DUAL_DVI0_AUTO_DELAY_STATUS                     0xB66C	//46700 (C9DUAL_DVI)
#define DUAL_DVI0_DBG_CTRL                              0xB670	//46704 (C9DUAL_DVI)
#define DVI1_RX_PHY_RES_CTRL                            0xB700	//46848 (C9DVI1_DVI1_RX_PHY_TOP)
#define DVI1_RX_PHY_CLK_SEL                             0xB704	//46852 (C9DVI1_DVI1_RX_PHY_TOP)
#define DVI1_RX_PHY_CLK_MS_CTRL                         0xB708	//46856 (C9DVI1_DVI1_RX_PHY_TOP)
#define DVI1_RX_PHY_IP_VERSION                          0xB70C	//46860 (C9DVI1_DVI1_RX_PHY_TOP)
#define DVI1_RX_PHY_CTRL                                0xB748	//46920 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_STATUS                              0xB74C	//46924 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_PHS_PICK                            0xB750	//46928 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_SIGQUAL                             0xB754	//46932 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_PHS_A_SCR                           0xB758	//46936 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_PHS_B_SCR                           0xB75C	//46940 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_PHS_C_SCR                           0xB760	//46944 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_IRQ_EN                              0xB764	//46948 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_IRQ_STS                             0xB768	//46952 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_RX_PHY_TST_CTRL                            0xB798	//47000 (C9DVI1_DVI1_RX_PHY_DG)
#define DVI1_TCLK_SEL                                   0xB800	//47104 ()
#define DVI1_LNK_CLK_SEL                                0xB804	//47108 ()
#define DVI1_VCLK_SEL                                   0xB808	//47112 ()
#define DVI1_OUT_PIX_CLK_SEL                            0xB80C	//47116 ()
#define DVI1_AUCLK_SEL                                  0xB810	//47120 ()
#define DVI1_RESET_CTRL                                 0xB814	//47124 ()
#define DVI1_CLK_FST_MS_CTRL                            0xB818	//47128 ()
#define DVI1_CLK_MS_FST_STATUS                          0xB81C	//47132 ()
#define DVI1_CLK_RSRV                                   0xB82C	//47148 ()
#define DVI1_IRQ_STATUS                                 0xB830	//47152 ()
#define DVI1_HPD_CTRL                                   0xB834	//47156 (C9DVI1_DVI1_RX_CORE_DVI1_TOP)
#define DVI1_HPD_STATUS                                 0xB838	//47160 ()
#define DVI1_HPD_IRQ_EN                                 0xB83C	//47164 ()
#define DVI1_HPD_IRQ_STS                                0xB840	//47168 ()
#define DVI1_MUTE_TOP_STATUS                            0xB844	//47172 ()
#define DVI1_MUTE_TOP_IRQ_EN                            0xB848	//47176 ()
#define DVI1_MUTE_TOP_IRQ_STS                           0xB84C	//47180 ()
#define DVI1_RX_DIG_IP_VERSION                          0xB850	//47184 ()
#define DVI1_TOP_RSRV                                   0xB868	//47208 (C9DVI1_DVI1_RX_CORE_DVI1_TOP)
#define DVI1_TOP_TST_CTRL                               0xB86C	//47212 ()
#define DVI1_MAIN_LINK_STATUS                           0xB870	//47216 ()
#define DVI1_INPUT_SEL                                  0xB874	//47220 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_LINK_CONFIG                                0xB878	//47224 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_LINK_ERR_CTRL                              0xB87C	//47228 ()
#define DVI1_PHY_DEC_ERR_STATUS                         0xB880	//47232 ()
#define DVI1_SIGNL_DET_CTRL                             0xB884	//47236 ()
#define DVI1_FREQ_MEAS_PERIOD                           0xB888	//47240 ()
#define DVI1_FREQ_TH                                    0xB88C	//47244 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_FREQ_MEAS_CT                               0xB890	//47248 ()
#define DVI1_CLK_STBL_ZN                                0xB894	//47252 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_MEAS_RESULT                                0xB898	//47256 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_MEAS_IRQ_EN                                0xB89C	//47260 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_MEAS_IRQ_STS                               0xB8A0	//47264 ()
#define DVI1_TMDS_RX_CTL                                0xB8A4	//47268 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_TMDS_RX_BIST_STB_CTRL                      0xB8A8	//47272 ()
#define DVI1_TMDS_RX_BIST_CTRL                          0xB8AC	//47276 ()
#define DVI1_TMDS_RX_BIST_STATUS                        0xB8B0	//47280 ()
#define DVI1_PHY_LNK_CRC_TEST_STS                       0xB8D4	//47316 ()
#define DVI1_LINK_RSRV                                  0xB8D8	//47320 (C9DVI1_DVI1_RX_CORE_LINK)
#define DVI1_LINK_TST_CTRL                              0xB8DC	//47324 ()
#define DVI1_SUB_SAMPLER                                0xB8E0	//47328 ()
#define DVI1_VID_CONV_CTRL                              0xB8E4	//47332 ()
#define DVI1_VID_BUF_OFFSET                             0xB8E8	//47336 ()
#define DVI1_MD_CHNG_MT_DEL                             0xB8EC	//47340 ()
#define DVI1_VID_BUF_DELAY                              0xB8F0	//47344 ()
#define DVI1_VID_BUF_FILT_ZONE                          0xB8F4	//47348 ()
#define DVI1_COLOR_DEPH_STATUS                          0xB8F8	//47352 ()
#define DVI1_VIDEO_BUF_IRQ_EN                           0xB8FC	//47356 ()
#define DVI1_VIDEO_BUF_IRQ_STS                          0xB900	//47360 ()
#define DVI1_VID_LNK_RSRV                               0xB918	//47384 ()
#define DVI1_VID_LNK_TST_CTRL                           0xB91C	//47388 ()
#define DVI1_VIDEO_CTRL                                 0xB920	//47392 (C9DVI1_DVI1_RX_CORE_VIDEO)
#define DVI1_VID_REGEN_CTRL                             0xB924	//47396 ()
#define DVI1_PATGEN_CONTROL                             0xB928	//47400 ()
#define DVI1_DE_WIDTH_CLAMP                             0xB92C	//47404 ()
#define DVI1_VIDEO_MUTE_RGB                             0xB930	//47408 ()
#define DVI1_VIDEO_MUTE_YC                              0xB934	//47412 ()
#define DVI1_DITHER_CTRL                                0xB938	//47416 ()
#define DVI1_VIDEO_CRC_TEST_VALUE                       0xB950	//47440 ()
#define DVI1_VIDEO_CRC_TEST_STS                         0xB954	//47444 ()
#define DVI1_VIDEO_RSRV                                 0xB958	//47448 (C9DVI1_DVI1_RX_CORE_VIDEO)
#define DVI1_VIDEO_TST_CTRL                             0xB95C	//47452 ()
#define DVI1_SDP_CTRL                                   0xB960	//47456 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_SDP_AV_MUTE_CTRL                           0xB964	//47460 ()
#define DVI1_SDP_STATUS                                 0xB968	//47464 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_SDP_IRQ_CTRL                               0xB96C	//47468 ()
#define DVI1_SDP_IRQ_STATUS                             0xB970	//47472 ()
#define DVI1_AU_CONV_CTRL                               0xB974	//47476 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_PACK_CTRL                                  0xB978	//47480 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_PACKET_SEL                                 0xB97C	//47484 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_PKT_STATUS                                 0xB980	//47488 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_UNDEF_PKT_CODE                             0xB984	//47492 ()
#define DVI1_PACK_GUARD_CTRL                            0xB988	//47496 ()
#define DVI1_NOISE_DET_CTRL                             0xB98C	//47500 ()
#define DVI1_PACK_CT                                    0xB990	//47504 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_ERROR_CT                                   0xB994	//47508 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_AUTO_PACK_PROCESS                          0xB998	//47512 ()
#define DVI1_AVI_INFOR_TIMEOUT_THRESH                   0xB99C	//47516 ()
#define DVI1_AU_INFO_TIMEOUT_THRESH                     0xB9A0	//47520 ()
#define DVI1_AVI_AUTO_FIELDS                            0xB9A4	//47524 ()
#define DVI1_SDP_PRSNT_STS                              0xB9A8	//47528 ()
#define DVI1_SDP_MUTE_SRC_STATUS                        0xB9AC	//47532 ()
#define DVI1_SDP_RSRV                                   0xB9C8	//47560 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_SDP_TST_CTRL                               0xB9CC	//47564 (C9DVI1_DVI1_RX_CORE_SDP)
#define DVI1_SDP_AUD_CTRL                               0xB9D0	//47568 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_SDP_AUD_MUTE_CTRL                          0xB9D4	//47572 ()
#define DVI1_SDP_AUD_IRQ_CTRL                           0xB9D8	//47576 ()
#define DVI1_SDP_AUDIO_IRQ_STATUS                       0xB9DC	//47580 ()
#define DVI1_SDP_AUD_STS                                0xB9E0	//47584 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_AUD_MUTE_SRC_STS                           0xB9E4	//47588 ()
#define DVI1_SDP_AUD_ERROR_CT                           0xB9E8	//47592 ()
#define DVI1_SDP_AUD_BUF_CNTRL                          0xB9EC	//47596 ()
#define DVI1_SDP_AUD_BUF_DELAY                          0xB9F0	//47600 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_SDP_AUD_BUFPTR_DIF                         0xB9F4	//47604 ()
#define DVI1_SDP_AUD_BUFPTR_DIF_THRESH_A                0xB9F8	//47608 ()
#define DVI1_SDP_AUD_BUFPTR_DIF_THRESH_B                0xB9FC	//47612 ()
#define DVI1_SDP_AUD_LINE_THRESH                        0xBA00	//47616 ()
#define DVI1_SDP_AUD_OUT_CNTRL                          0xBA04	//47620 ()
#define DVI1_SDP_AUD_CHAN_RELOC                         0xBA08	//47624 ()
#define DVI1_SDP_AUD_CH_STS_0                           0xBA0C	//47628 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_SDP_AUD_CH_STS_1                           0xBA10	//47632 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_SDP_AUD_CH_STS_2                           0xBA14	//47636 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_SDP_AUD_RSRV                               0xBA30	//47664 (C9DVI1_DVI1_RX_CORE_AUD)
#define DVI1_SDP_AUD_TST_CTRL                           0xBA34	//47668 ()
#define DVI1_KEYHOST_CONTROL                            0xBB30	//47920 (C9HDCP_KEYHOST)
#define DVI1_KEYHOST_STATUS                             0xBB34	//47924 (C9HDCP_KEYHOST)
#define DVI1_HDCP_CONTROL                               0xBB40	//47936 (C9HDCP_HDRX)
#define DVI1_HDCP_ADDR                                  0xBB44	//47940 (C9HDCP_HDRX)
#define DVI1_HDCP_WIN_MOD                               0xBB48	//47944 (C9HDCP_HDRX)
#define DVI1_HDCP_BKSV_0                                0xBB4C	//47948 (C9HDCP_HDRX)
#define DVI1_HDCP_BKSV_1                                0xBB50	//47952 (C9HDCP_HDRX)
#define DVI1_HDCP_STATUS                                0xBB54	//47956 (C9HDCP_HDRX)
#define DVI1_HDCP_STATUS_EXT                            0xBB58	//47960 (C9HDCP_HDRX)
#define DVI1_HDCP_TBL_IRQ_INDEX                         0xBB5C	//47964 (C9HDCP_HDRX)
#define DVI1_HDCP_TBL_RD_INDEX                          0xBB60	//47968 (C9HDCP_HDRX)
#define DVI1_HDCP_TBL_RD_DATA                           0xBB64	//47972 (C9HDCP_HDRX)
#define DVI1_HDCP_UPSTREAM_CTRL                         0xBB80	//48000 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_UPSTREAM_STATUS                       0xBB84	//48004 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_BSTATUS                               0xBB88	//48008 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_TIMEOUT                               0xBB8C	//48012 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_TIMEBASE                              0xBB90	//48016 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_V0_0                                  0xBB94	//48020 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_V0_1                                  0xBB98	//48024 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_V0_2                                  0xBB9C	//48028 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_V0_3                                  0xBBA0	//48032 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_V0_4                                  0xBBA4	//48036 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_M0_0                                  0xBBA8	//48040 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_M0_1                                  0xBBAC	//48044 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_KSV_FIFO                              0xBBB0	//48048 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_KSV_FIFO_CTRL                         0xBBB4	//48052 (C9HDCP_DVI1_RPTR)
#define DVI1_HDCP_KSV_FIFO_STS                          0xBBB8	//48056 (C9HDCP_DVI1_RPTR)
#define DVI1_INST_SOFT_RESETS                           0xBC00	//48128 (C9DVI1_INSTRUMENT )
#define DVI1_INST_HARD_RESETS                           0xBC04	//48132 (C9DVI1_INSTRUMENT)
#define DVI1_HOST_CONTROL                               0xBC08	//48136 (C9DVI1_INSTRUMENT)
#define DVI1_AFR_CONTROL                                0xBC0C	//48140 (C9DVI1_INSTRUMENT)
#define DVI1_AFB_CONTROL                                0xBC10	//48144 (C9DVI1_INSTRUMENT)
#define DVI1_INPUT_IRQ_MASK                             0xBC14	//48148 (C9DVI1_INSTRUMENT)
#define DVI1_INPUT_IRQ_STATUS                           0xBC18	//48152 (C9DVI1_INSTRUMENT)
#define DVI1_TEST_IRQ                                   0xBC1C	//48156 (C9DVI1_INSTRUMENT)
#define DVI1_INSTRU_CLOCK_CONFIG                        0xBC20	//48160 (C9DVI1_INSTRUMENT)
#define DVI1_IFM_CLK_CONTROL                            0xBC24	//48164 (C9DVI1_INSTRUMENT)
#define DVI1_INSTRUMENT_POLARITY_CONTROL                0xBC28	//48168 ()
#define DVI1_IFM_CTRL                                   0xBC60	//48224 (C9DVI1_IFM)
#define DVI1_IFM_WATCHDOG                               0xBC64	//48228 (C9DVI1_IFM)
#define DVI1_IFM_HLINE                                  0xBC68	//48232 (C9DVI1_IFM)
#define DVI1_IFM_CHANGE                                 0xBC6C	//48236 (C9DVI1_IFM)
#define DVI1_IFM_HS_PERIOD                              0xBC70	//48240 (C9DVI1_IFM)
#define DVI1_IFM_HS_PULSE                               0xBC74	//48244 (C9DVI1_IFM)
#define DVI1_IFM_VS_PERIOD                              0xBC78	//48248 (C9DVI1_IFM)
#define DVI1_IFM_VS_PULSE                               0xBC7C	//48252 (C9DVI1_IFM)
#define DVI1_IFM_SPACE                                  0xBC80	//48256 (C9DVI1_IFM)
#define DVI1_INPUT_CONTROL                              0xBCB0	//48304 (C9DVI1_IMD)
#define DVI1_INPUT_HS_DELAY                             0xBCB4	//48308 (C9DVI1_IMD)
#define DVI1_INPUT_VS_DELAY                             0xBCB8	//48312 (C9DVI1_IMD)
#define DVI1_INPUT_H_ACT_START                          0xBCBC	//48316 (C9DVI1_IMD)
#define DVI1_INPUT_H_ACT_WIDTH                          0xBCC0	//48320 (C9DVI1_IMD)
#define DVI1_INPUT_V_ACT_START_ODD                      0xBCC4	//48324 (C9DVI1_IMD)
#define DVI1_INPUT_V_ACT_START_EVEN                     0xBCC8	//48328 (C9DVI1_IMD)
#define DVI1_INPUT_V_ACT_LENGTH                         0xBCCC	//48332 (C9DVI1_IMD)
#define DVI1_INPUT_FREEZE_VIDEO                         0xBCD0	//48336 (C9DVI1_IMD)
#define DVI1_INPUT_FLAGLINE                             0xBCD4	//48340 (C9DVI1_IMD)
#define DVI1_INPUT_VLOCK_EVENT                          0xBCD8	//48344 (C9DVI1_IMD)
#define DVI1_INPUT_FRAME_RESET_LINE                     0xBCDC	//48348 (C9DVI1_IMD)
#define DVI1_INPUT_RFF_READY_LINE                       0xBCE0	//48352 (C9DVI1_IMD)
#define DVI1_INPUT_HMEASURE_START                       0xBCE4	//48356 (C9DVI1_IMD)
#define DVI1_INPUT_HMEASURE_WIDTH                       0xBCE8	//48360 (C9DVI1_IMD)
#define DVI1_INPUT_VMEASURE_START                       0xBCEC	//48364 (C9DVI1_IMD)
#define DVI1_INPUT_VMEASURE_LENGTH                      0xBCF0	//48368 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_CONTROL                          0xBCF4	//48372 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_HTOTAL                           0xBCF8	//48376 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_VTOTAL                           0xBCFC	//48380 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_HSTART                           0xBD00	//48384 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_HWIDTH                           0xBD04	//48388 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_VSTART                           0xBD08	//48392 (C9DVI1_IMD)
#define DVI1_INPUT_IBD_VLENGTH                          0xBD0C	//48396 (C9DVI1_IMD)
#define DVI1_INPUT_PIXGRAB_H                            0xBD10	//48400 (C9DVI1_IMD)
#define DVI1_INPUT_PIXGRAB_V                            0xBD14	//48404 (C9DVI1_IMD)
#define DVI1_INPUT_PIXGRAB_RED                          0xBD18	//48408 (C9DVI1_IMD)
#define DVI1_INPUT_PIXGRAB_GRN                          0xBD1C	//48412 (C9DVI1_IMD)
#define DVI1_INPUT_PIXGRAB_BLU                          0xBD20	//48416 (C9DVI1_IMD)
#define DVI1_CONFIG                                     0xBD40	//48448 (C9DVI1_PU)
#define DVI1_CRC_CTRL                                   0xBD44	//48452 (C9DVI1_PU)
#define DVI1_CRC_ERR_STATUS                             0xBD48	//48456 (C9DVI1_PU)
#define DVI1_CRC_RED                                    0xBD4C	//48460 (C9DVI1_PU)
#define DVI1_CRC_GREEN                                  0xBD50	//48464 (C9DVI1_PU)
#define DVI1_CRC_BLUE                                   0xBD54	//48468 (C9DVI1_PU)
#define DVI1_DEBUG_TEST_CTRL                            0xBD58	//48472 (C9DVI1_PU)
#define DVI1_SPARE_REG                                  0xBD5C	//48476 (C9DVI1_PU)
#define DVI1_HDCP_CTRL                                  0xBD60	//48480 (C9DVI1_PU)
#endif
/****************************************************************************/
/*  B I T S  D E F I N I T I O N S                                          */
/****************************************************************************/

#define DVI_MUX_FIFO_RESET                              BIT13

// CPHY_PHY_CTRL                                       (0x830C)
#define CPHY_L0_RESET                                  BIT0
#define CPHY_L1_RESET                                  BIT1
#define CPHY_L2_RESET                                  BIT2
#define CPHY_L3_RESET                                  BIT3
#define CPHY_DP_MODE                                   BIT4
#define CPHY_DP_BUS_WIDTH                              BIT5
#define CPHY_TMDS_MODE                                 BIT6
#define CPHY_SAMPLE_SWAP                               BIT7
#define CPHY_RTERM                                     0x0F00
#define CPHY_LOOPBACK_LN                               0xF000
#define CPHY_ENVBUFC_LN                                0x1F0000
#ifdef STHDMIRX_ATHENA_CUT1
#define CPHY2_DUAL_DVI_MODE                            BIT21
#else
#define CPHY_RCVRDATA_BITS_SWAP                        BIT21
#define CPHY_RCVRDATA_POLARITY                         BIT22
#define CPHY_RCVRDATA_ODD                              BIT23
#define CPHY_SPD_SEL_OW                                BIT24
#define CPHY_REF_CLK_1X_EN                             BIT25
#define CPHY_DVI_CLK_OUT_2X_EN                         BIT26
#define CPHY2_DUAL_DVI_MODE                     BIT31
#endif
#define CPHY_RTERM_SHIFT								8

// CPHY_L0_CTRL_0                                      (0x8310)
#define CPHY_L0_ACRCY_SEL_0                            0x07
#define CPHY_L0_ACRCY_SEL_0_SHIFT                      0
#define CPHY_L0_ACRCY_SEL_1                            0x38
#define CPHY_L0_ACRCY_SEL_1_SHIFT                      3
#define CPHY_L0_IB_CNTL                                0xC0
#define CPHY_L0_IB_CNTL_SHIFT                          6
#define CPHY_L0_CPTIN                                  0x3F00
#define CPHY_L0_CPTIN_SHIFT							8
#define CPHY_L0_SPD_SEL                                0xC000
#define CPHY_L0_SPD_SEL_SHIFT							14
#define CPHY_L0_GAIN                                   0x70000
#define CPHY_TMDS_MODE									BIT6
#define CPHY_L0_IDDQEN                                 BIT19
#define CPHY_L0_OOR_LOSC                               BIT20
#define CPHY_L0_DECR_BW                                BIT21|BIT22
#define CPHY_L0_DECR_BW_SHIFT                          21
#define CPHY_L0_SWAP_CAL_DIR                           BIT23
#define CPHY_L0_DIS_TRIM_CAL                           BIT24
#define CPHY_L0_EQTEST                                 BIT25
#define CPHY_L0_TMDS_LOCK2REF                          BIT31
#define CPHY_L0_CTRL_2 CPHY_L0_CTRL_0 + 2

// CPHY_L0_CTRL_4                                      (0x8314)
#define CPHY_L0_CAL_CNTL                               0x0F
#define CPHY_L0_CAL_CNTL_ZERO                               0x00
#define CPHY_L0_DIS_OSCC                               BIT4
#define CPHY_L0_ENLP                                   BIT5
#define CPHY_L0_ENRCK                                  BIT6
#define CPHY_L0_BYPASS_TRAIN                           BIT7
#define CPHY_L0_MN_DISABLE_OW                          BIT8
#define CPHY_L0_MN_DISABLE_TEST                        BIT9
#define CPHY_L0_SEL_DATA_OW                            BIT10
#define CPHY_L0_SEL_DATA_TEST                          BIT11
#define CPHY_L0_PDEN_OW                                BIT12
#define CPHY_L0_PDEN_TEST                              BIT13
#define CPHY_L0_LUDO_OW                                BIT14
#define CPHY_L0_LUDO_TEST                              BIT15
#define CPHY_L0_RSVR                                   0xFF0000
#define CPHY_L0_EQ_FPEAK                               0x7000000

// CPHY_L0_CPTRIM                                      (0x8318)
#define CPHY_L0_CPTOUT                                 0x3F
#define CPHY_L0_OUT_OF_RANGE                           BIT6

// CPHY_L1_CTRL_0                                      (0x831C)
#define CPHY_L1_ACRCY_SEL_0                            0x07
#define CPHY_L1_ACRCY_SEL_1                            0x38
#define CPHY_L1_ACRCY_SEL_1_SHIFT                      3
#define CPHY_L1_IB_CNTL                                0xC0
#define CPHY_L1_IB_CNTL_SHIFT                          6
#define CPHY_L1_CPTIN                                  0x3F00
#define CPHY_L1_SPD_SEL                                0xC000
#define CPHY_L1_GAIN                                   0x70000
#define CPHY_L1_IDDQEN                                 BIT19
#define CPHY_L1_OOR_LOSC                               BIT20
#define CPHY_L1_CTRL2_RSRV                             BIT21
#define CPHY_L1_SWAP_CAL_DIR                           BIT22
#define CPHY_L1_DIS_TRIM_CAL                           BIT23
#define CPHY_L1_EQTEST                                 BIT24
#define CPHY_L1_ZERO                                   0x3E000000
#define CPHY_L1_HDMI_REF_SEL                           BIT30
#define CPHY_L1_TMDS_LOCK2REF                          BIT31
#define CPHY_L1_CTRL_2 CPHY_L1_CTRL_0 + 2

// CPHY_L1_CTRL_4                                      (0x8320)
#define CPHY_L1_CAL_CNTL                               0x0F
#define CPHY_L1_DIS_OSCC                               BIT4
#define CPHY_L1_ENLP                                   BIT5
#define CPHY_L1_ENRCK                                  BIT6
#define CPHY_L1_BYPASS_TRAIN                           BIT7
#define CPHY_L1_MN_DISABLE_OW                          BIT8
#define CPHY_L1_MN_DISABLE_TEST                        BIT9
#define CPHY_L1_SEL_DATA_OW                            BIT10
#define CPHY_L1_SEL_DATA_TEST                          BIT11
#define CPHY_L1_PDEN_OW                                BIT12
#define CPHY_L1_PDEN_TEST                              BIT13
#define CPHY_L1_LUDO_OW                                BIT14
#define CPHY_L1_LUDO_TEST                              BIT15
#define CPHY_L1_RSVR                                   0xFF0000
#define CPHY_L1_EQ_FPEAK                               0x7000000

// CPHY_L1_CPTRIM                                      (0x8324)
#define CPHY_L1_CPTOUT                                 0x3F
#define CPHY_L1_OUT_OF_RANGE                           BIT6

// CPHY_L2_CTRL_0                                      (0x8328)
#define CPHY_L2_ACRCY_SEL_0                            0x07
#define CPHY_L2_ACRCY_SEL_1                            0x38
#define CPHY_L2_ACRCY_SEL_1_SHIFT                      3
#define CPHY_L2_IB_CNTL                                0xC0
#define CPHY_L2_IB_CNTL_SHIFT                          6
#define CPHY_L2_CPTIN                                  0x3F00
#define CPHY_L2_SPD_SEL                                0xC000
#define CPHY_L2_GAIN                                   0x70000
#define CPHY_L2_IDDQEN                                 BIT19
#define CPHY_L2_OOR_LOSC                               BIT20
#define CPHY_L2_CTRL2_RSRV                             BIT21
#define CPHY_L2_SWAP_CAL_DIR                           BIT22
#define CPHY_L2_DIS_TRIM_CAL                           BIT23
#define CPHY_L2_EQTEST                                 BIT24
#define CPHY_L2_ZERO                                   0x3E000000
#define CPHY_L2_HDMI_REF_SEL                           BIT30
#define CPHY_L2_TMDS_LOCK2REF                          BIT31
#define CPHY_L2_CTRL_2 CPHY_L2_CTRL_0 + 2

// CPHY_L2_CTRL_4                                      (0x832C)
#define CPHY_L2_CAL_CNTL                               0x0F
#define CPHY_L2_DIS_OSCC                               BIT4
#define CPHY_L2_ENLP                                   BIT5
#define CPHY_L2_ENRCK                                  BIT6
#define CPHY_L2_BYPASS_TRAIN                           BIT7
#define CPHY_L2_MN_DISABLE_OW                          BIT8
#define CPHY_L2_MN_DISABLE_TEST                        BIT9
#define CPHY_L2_SEL_DATA_OW                            BIT10
#define CPHY_L2_SEL_DATA_TEST                          BIT11
#define CPHY_L2_PDEN_OW                                BIT12
#define CPHY_L2_PDEN_TEST                              BIT13
#define CPHY_L2_LUDO_OW                                BIT14
#define CPHY_L2_LUDO_TEST                              BIT15
#define CPHY_L2_RSVR                                   0xFF0000
#define CPHY_L2_EQ_FPEAK                               0x7000000

// CPHY_L2_CPTRIM                                      (0x8330)
#define CPHY_L2_CPTOUT                                 0x3F
#define CPHY_L2_OUT_OF_RANGE                           BIT6

// CPHY_L3_CTRL_0                                      (0x8334)
#define CPHY_L3_ACRCY_SEL_0                            0x07
#define CPHY_L3_ACRCY_SEL_1                            0x38
#define CPHY_L3_ACRCY_SEL_1_SHIFT                      3
#define CPHY_L3_IB_CNTL                                0xC0
#define CPHY_L3_IB_CNTL_SHIFT                          6
#define CPHY_L3_CPTIN                                  0x3F00
#define CPHY_L3_SPD_SEL                                0xC000
#define CPHY_L3_GAIN                                   0x70000
#define CPHY_L3_IDDQEN                                 BIT19
#define CPHY_L3_OOR_LOSC                               BIT20
#define CPHY_L3_CTRL2_RSRV                             BIT21
#define CPHY_L3_SWAP_CAL_DIR                           BIT22
#define CPHY_L3_DIS_TRIM_CAL                           BIT23
#define CPHY_L3_EQTEST                                 BIT24
#define CPHY_L3_ZERO                                   0x3E000000
#define CPHY_L3_HDMI_REF_SEL                           BIT30
#define CPHY_L3_TMDS_LOCK2REF                          BIT31
#define CPHY_L3_CTRL_2 CPHY_L3_CTRL_0 + 2

// CPHY_L3_CTRL_4                                      (0x8338)
#define CPHY_L3_CAL_CNTL                               0x0F
#define CPHY_L3_DIS_OSCC                               BIT4
#define CPHY_L3_ENLP                                   BIT5
#define CPHY_L3_ENRCK                                  BIT6
#define CPHY_L3_BYPASS_TRAIN                           BIT7
#define CPHY_L3_MN_DISABLE_OW                          BIT8
#define CPHY_L3_MN_DISABLE_TEST                        BIT9
#define CPHY_L3_SEL_DATA_OW                            BIT10
#define CPHY_L3_SEL_DATA_TEST                          BIT11
#define CPHY_L3_PDEN_OW                                BIT12
#define CPHY_L3_PDEN_TEST                              BIT13
#define CPHY_L3_LUDO_OW                                BIT14
#define CPHY_L3_LUDO_TEST                              BIT15
#define CPHY_L3_RSVR                                   0xFF0000
#define CPHY_L3_EQ_FPEAK                               0x7000000

// CPHY_L3_CPTRIM                                      (0x833C)
#define CPHY_L3_CPTOUT                                 0x3F
#define CPHY_L3_OUT_OF_RANGE                           BIT6

// CPHY_RCAL_RESULT                                    (0x8340)
#define CPHY_RSRV_STATUS                               0xFF00

// CPHY_ALT_FREQ_MEAS_CTRL                             (0x8344)
#define CPHY_ALT_FREQ_DETECT_EN                        BIT0
#define CPHY_ALT_DETECT_REF_SEL                        BIT1
#define CPHY_ALT_MEAS_RESULT_SEL                       0x0C
#define CPHY_ALT_MEAS_RESULT_SEL_SHIFT                 2
#define CPHY_ALT_FREQ_MEAS_PERIOD                      0xFF00

// CPHY_ALT_FREQ_MEAS_RESULT                           (0x8348)
#define CPHY_ALT_FREQ_MEAS_RES                         0x03FF


// HDMI_RX_PHY_RES_CTRL                                 (0x0000)
#define HDMI_RX_PHY_RES_TCLK                            BIT0
#define HDMI_RX_PHY_RES_LCLK_OVR                        BIT1
#define HDMI_RX_PHY_RES_LCLK                            BIT2
#define HDMI_RX_CPHY_OVS_FIFO_RES                       BIT3
#define HDMI_RX_CPHY_TRK_FIFO_RES                       BIT4
#define HDMI_RX_PHY_RES_RSRV                            0x60
#define HDMI_RX_PHY_RES_RSRV_SHIFT                      5

// HDMI_RX_PHY_CLK_SEL                                  (0x9004)
#define HDMI_RX_PHY_LCLK_SEL                            0x03
#define HDMI_RX_PHY_LCLK_INV                            BIT3
#define HDMI_RX_PHY_LCLK_OVR_INV                        BIT7
#define HDMI_RX_PHY_LCLK_TRACK_INV                      BIT8

// HDMI_RX_PHY_CLK_MS_CTRL                              (0x9008)
#define HDMI_RX_PHY_CLK_MS_MSK                          0x0F
#define HDMI_RX_PHY_CLK_MS_EN                           BIT4
#define HDMI_RX_PHY_ENDIAN_SWAP_EN                      BIT5
#define HDMI_RX_PHY_LANE_CLK_DET_SEL                    0xC0
#define HDMI_RX_PHY_LANE_CLK_DET_SEL_SHIFT              6

// HDMI_RX_PHY_IP_VERSION                               (0x900C)
#define HDMI_RX_PHY_IP_BUGFIX_VERSI                     0x0F
#define HDMI_RX_PHY_IP_MINOR_VERSI                      0xF0
#define HDMI_RX_PHY_IP_MINOR_VERSI_SHIFT                4
#define HDMI_RX_PHY_IP_MAJOR_VERSI                      0x0F00

// HDMI_RX_PHY_CTRL                                     (0x9048)
#define HDMI_RX_PHY_PWR_CTRL                            0x03
#define HDMI_RX_TMDS_SPEED_GRADE                        0x0C
#define HDMI_RX_TMDS_SPEED_GRADE_SHIFT                  2
#define HDMI_RX_PHY_SWP_CH0_CH2                         BIT4
#define HDMI_RX_PHY_CH_INV                              BIT5
#define HDMI_RX_PHY_CTRL_CLR_SCR                        BIT6
#define HDMI_RX_PHY_CTRL_RSRV                           0xFF80

// HDMI_RX_PHY_STATUS                                   (0x904C)
#define HDMI_RX_PHY_ANA_LCLK_PRSNT                      BIT0
#define HDMI_RX_PHY_OVR_LCLK_PRSNT                       BIT1
#define HDMI_RX_PHY_LCLK_PRSNT                          BIT2
#define HDMI_RX_PHY_LANE_CLK_PRSNT                      BIT3
#define HDMI_RX_PHY_STATUS_RSRV                         0xFFF0

// HDMI_RX_PHY_PHS_PICK                                 (0x9050)
#define HDMI_RX_PHY_WINDOW                              0x07
#define HDMI_RX_PHY_NAR_PK_EXP_EN                       0x38
#define HDMI_RX_PHY_NAR_PK_EXP_EN_SHIFT                 3
#define HDMI_RX_PHY_TRN_CH_SEL                          0xC0
#define HDMI_RX_PHY_TRN_CH_SEL_SHIFT                    6
#define HDMI_RX_PHY_SET_A                               0x0700
#define HDMI_RX_PHY_SET_B                               0x7000
#define HDMI_RX_PHY_SET_C                               0x70000
#define HDMI_RX_PHY_BW_TRACKING                         0x300000
#define HDMI_RX_PHY_DIS_EBUF                            0x1C00000
#define HDMI_RX_PHY_TRC_FILTER_EN                       BIT25

// HDMI_RX_PHY_IRQ_EN                                   (0x9064)
#define HDMI_RX_PHY_MS_ANA_LCLK_IRQ_EN                  BIT0
#define HDMI_RX_PHY_MS_OVR_LCLK_IRQ_EN                  BIT1
#define HDMI_RX_PHY_MS_LCLK_IRQ_EN                      BIT2
#define HDMI_RX_PHY_MS_LANE_CLK_IRQ_EN                  BIT3
#define HDMI_RX_PHY_IRQ_EN_RSRV                         0x30
#define HDMI_RX_PHY_IRQ_EN_RSRV_SHIFT                   4

// HDMI_RX_PHY_IRQ_STS                                  (0x9068)
#define HDMI_RX_PHY_MS_ANA_LCLK_IRQ_STS                 BIT0
#define HDMI_RX_PHY_MS_OVR_LCLK_IRQ_STS                 BIT1
#define HDMI_RX_PHY_MS_LCLK_IRQ_STS                     BIT2
#define HDMI_RX_PHY_MS_LANE_CLK_IRQ_STS                 BIT3
#define HDMI_RX_PHY_IRQ_STS_RSRV                        0x30
#define HDMI_RX_PHY_IRQ_STS_RSRV_SHIFT                  4

// HDMI_RX_PHY_TST_CTRL                                 (0x9098)
#define HDMI_RX_PHY_TST_BUS_SEL                         0x0F
#define HDMI_RX_PHY_TRC_FILTER_SEL                      BIT4
#define HDMI_CORE_PHY_TB_SEL                            BIT5

// HDRX_TCLK_SEL                                        (0x9100)
#define HDRX_TCLK_SELECT                                BIT0
#define HDRX_TCLK_INV                                   BIT3
#define HDRX_TCLK_SEL_MASK                              0x01

// HDRX_LNK_CLK_SEL                                     (0x9104)
#define HDRX_LCLK_SEL                                   0x03
#define HDRX_LCLK_SLW_NO_CLK_EN                          BIT2
#define HDRX_LCLK_INV                                   BIT3

// HDRX_VCLK_SEL                                        (0x9108)
#define HDRX_VCLK_SELECT                                0x03
#define HDRX_VCLK_INV                                   BIT3
#define HDRX_VCLK_SEL_MASK                              0x03

// HDRX_OUT_PIX_CLK_SEL                                 (0x910C)
#define HDRX_PIX_CLK_OUT_SEL                            0x03
#define HDRX_PIX_CLK_OUT_INV                            BIT3

// HDRX_AUCLK_SEL                                       (0x9110)
#define HDRX_AUCLK_SELECT                               0x03
#define HDRX_AUCLK_INV                                  BIT3
#define HDRX_AUCLK_SEL_MASK                             0x03
// HDRX_RESET_CTRL                                      (0x9114)
#define HDRX_RESET_LINK                                 BIT0
#define HDRX_RESET_HDMI_SDP                             BIT1
#define HDRX_RESET_VIDEO                                BIT2
#define HDRX_RESET_TCLK                                 BIT3
#define HDRX_RESET_AUDIO                                BIT4
#define HDRX_RESET_ALL                                  BIT5
#define HDRX_NO_SIGNL_RESET_EN                          BIT6
#define HDRX_RESET_ON_PORT_CHNG_EN                      BIT7

// HDRX_CLK_FST_MS_CTRL                                 (0x9118)
#define HDRX_CLK_FST_MS_MSK                             0x0F
#define HDRX_CLK_MS_EN                                  BIT4

// HDRX_CLK_MS_FST_STATUS                               (0x911C)
#define HDRX_LCLK_PRSNT                                 BIT0
#define HDRX_VCLK_PRSNT                                 BIT1
#define HDRX_AUCLK_PRSNT                                BIT2
#define HDRX_LCLK_MS_ST_CHNG                            BIT8
#define HDRX_VCLK_MS_ST_CHNG                            BIT9
#define HDRX_AUCLK_MS_ST_CHNG                           BIT10

// HDRX_CLK_RSRV                                        (0x912C)
#define HDRX_CLK_RSVD                                   0xFF

// HDRX_IRQ_STATUS                                      (0x9130)
#define HDRX_LINK_IRQ_STS                               BIT0
#define HDRX_SDP_IRQ_STS                                BIT1
#define HDRX_AUDIO_IRQ_STS                              BIT2
#define HDRX_HDCP_IRQ_STS                               BIT3
#define HDRX_TOP_IRQ_STS                                BIT4
#define HDRX_VID_IRQ_STS                                BIT5
#define HDRX_IRQ_STS_RSRV                               0xC0
#define HDRX_IRQ_STS_RSRV_SHIFT                         6

// HDRX_HPD_CTRL                                        (0x9134)
#define HDRX_HPD_CTL                                    BIT0
#define HDRX_HPD_CTRL_RSRV                              0xFE
#define HDRX_HPD_CTRL_RSRV_SHIFT                        1

// HDRX_HPD_IRQ_EN                                      (0x913C)
#define HDRX_HPD_RIS_IRQ_EN                             BIT0
#define HDRX_HPD_FALL_IRQ_EN                            BIT1

// HDRX_HPD_IRQ_STS                                     (0x9140)
#define HDRX_HPD_RIS_IRQ_STS                            BIT0
#define HDRX_HPD_FALL_IRQ_STS                           BIT1

// HDRX_MUTE_TOP_STATUS                                 (0x9144)
#define HDRX_TOP_VID_MUTE_STATUS                        BIT0
#define HDRX_TOP_AUD_MUTE_STATUS                        BIT1

// HDRX_MUTE_TOP_IRQ_EN                                 (0x9148)
#define HDRX_TOP_VID_MUTE_IRQ_EN                        BIT0
#define HDRX_TOP_AUD_MUTE_IRQ_EN                        BIT1

// HDRX_MUTE_TOP_IRQ_STS                                (0x914C)
#define HDRX_TOP_VID_MUTE_IRQ_STS                       BIT0
#define HDRX_TOP_AUD_MUTE_IRQ_ST                        BIT1

// HDMI_RX_DIG_IP_VERSION                               (0x9150)
#define HDMI_RX_DIG_IP_BUGFIX_VERSION                   0x0F
#define HDMI_RX_DIG_IP_MINOR_VERSION                    0xF0
#define HDMI_RX_DIG_IP_MINOR_VERSION_SHIFT              4
#define HDMI_RX_DIG_IP_MAJOR_VERSION                    0x0F00

// HDRX_TOP_TST_CTRL                                    (0x916C)
#define HDRX_TOP_TST_BUS_SE                             0x1F
#define HDRX_TOP_TST_CTL                                0xE0
#define HDRX_TOP_TST_CTL_SHIFT                          5

// HDRX_MAIN_LINK_STATUS                                (0x9170)
#define HDRX_LINK_CLK_DETECTED                          BIT0
#define HDRX_HDMI_SIGN_DETECTED                         BIT1
#define HDRX_HDMI_1_3_DETECTED                          BIT2
#define HDRX_LINK_RESET_STATUS                          BIT3
#define HDRX_STATUS_RSRV                                0xF0
#define HDRX_STATUS_RSRV_SHIFT                          4

// HDRX_INPUT_SEL                                       (0x9174)
#define HDRX_INPUT_RSRVD                                0x0F
#define HDRX_INP_PHY_SEL                                0x30
#define HDRX_INP_PHY_SEL_SHIFT                          4

// HDRX_LINK_CONFIG                                     (0x9178)
#define HDRX_MAN_HDMI_EN                                BIT0
#define HDRX_AUTO_MODE_EN                               BIT1
#define HDRX_DE_SELECT                                  0x0C
#define HDRX_DE_SELECT_SHIFT                            2
#define HDRX_SYNC_SEL                                   0x30
#define HDRX_SYNC_SEL_SHIFT                             4
#define HDRX_NEW_OVR_ALGN_EN                            BIT6
#define HDRX_CH0_CH2_SWP_EN                             BIT7
#define HDRX_CH_POLARITY_INV                            0x0700
#define HDRX_FREQ_SEL_AUTO_DI                           BIT11
#define HDRX_PHY_DEC_ERR_SEL                            BIT12
#define HDRX_LINK_CFG_RSRV                              0xE000

// HDRX_LINK_ERR_CTRL                                   (0x917C)
#define HDRX_DECODER_ERR_SEL                            0x03
#define HDRX_PHY_DEC_ERR_CLR                            BIT2

// HDRX_PHY_DEC_ERR_STATUS                              (0x9180)
#define HDRX_PHY_DEC_ERR_CT                             0x0FFF
#define HDRX_PHY_DEC_ERR_STAT                           0x7000
#define HDRX_PHY_DEC_ERR_RSRV                           BIT15

// HDRX_SIGNL_DET_CTRL                                  (0x9184)
#define HDRX_SGNL_MEAS_EN                               BIT0
#define HDRX_LNK_BY_CLK_DET_EN                          BIT1
#define HDRX_MEAS_CIRC_SEL                              BIT2
#define HDRX_CLK_CHNG_FILT_EN                           BIT3
#define HDRX_MEAS_RD_MD                                 0x70
#define HDRX_MEAS_RD_MD_SHIFT                           4
#define HDRX_MEAS_SYNC_INV                              BIT7
#define HDRX_AV_MUTE_ON_CLK_CHNG_EN                     BIT8

// HDRX_MEAS_IRQ_EN                                     (0x919C)
#define HDRX_CLK_LOST_IRQ_EN                            BIT0
#define HDRX_VSYNC_EDGE_IRQ_EN                          BIT1
#define HDRX_HSYNC_EDGE_IRQ_EN                          BIT2
#define HDRX_DE_EDGE_IRQ_EN                             BIT3
#define HDRX_MEAS_COUNT_IRQ_EN                          BIT4
#define HDRX_MEAS_DUR_IRQ_EN                            BIT5
#define HDRX_MEAS_CLK_IRQ_EN                            BIT6
#define HDRX_CLK_CHNG_IRQ_EN                            BIT7
#define HDRX_MEAS_IRQ_EN_RSRV                           BIT10

// HDRX_MEAS_IRQ_STS                                    (0x91A0)
#define HDRX_CLK_LOST_IRQ_STS                           BIT0
#define HDRX_VSYNC_EDGE_IRQ_STS                         BIT1
#define HDRX_HSYNC_EDGE_IRQ_STS                         BIT2
#define HDRX_DE_EDGE_IRQ_STS                            BIT3
#define HDRX_MEAS_COUNT_IRQ_STS                         BIT4
#define HDRX_MEAS_DUR_IRQ_STS                           BIT5
#define HDRX_MEAS_CLK_IRQ_STS                           BIT6
#define HDRX_CLK_CHNG_IRQ_STS                           BIT7
#define HDRX_VSYNC_LEVEL                                BIT8
#define HDRX_HSYNC_LEVEL                                BIT9
#define HDRX_MEAS_IRQ_STS_RSRV                          BIT10

// HDRX_TMDS_RX_CTL                                     (0x91A4)
#define HDRX_TMDS_CTLV                                  BIT0
#define HDRX_TMDS_CTLH                                  BIT1
#define HDRX_TMDS_CTL0                                  BIT2
#define HDRX_TMDS_CTL1                                  BIT3
#define HDRX_TMDS_CTL2                                  BIT4
#define HDRX_TMDS_CTL3                                  BIT5

// HDRX_TMDS_RX_BIST_STB_CTRL                           (0x91A8)
#define TMDS_RX_BIST_COMPUTE_NEW_SI                     BIT0
#define TMDS_RX_BIST_PRBS_CMP_EN                        BIT1

// HDRX_TMDS_RX_BIST_CTRL                               (0x91AC)
#define TMDS_RX_BIST_TEST_CH_SEL                        0x03
#define TMDS_RX_BIST_CMP_MODE                           BIT2
#define TMDS_RX_BIST_DATA_SWAP_E                        BIT3
#define TMDS_RX_BIST_SEQ_LENGTH                         0x70
#define TMDS_RX_BIST_SEQ_LENGTH_SHIFT                   4
#define TMDS_RX_BIST_MAX_TIMER_C                        0x0380
#define TMDS_RX_BIST_EXPECTED_SIG                       0x3FFFC00
#define TMDS_RX_BIST_TEST_RSRV                          0xFC000000

// HDRX_TMDS_RX_BIST_STATUS                             (0x91B0)
#define TMDS_RX_BIST_PRBS_CMP_                          BIT0
#define TMDS_RX_BIST_PRBS_ERRO                          BIT1
#define TMDS_RX_BIST_SIGNATURE_                         BIT6
#define TMDS_RX_BIST_ERROR                              BIT7
#define TMDS_RX_BIST_ERROR_CNT                          0x0700
#define TMDS_RX_BIST_SIGNATURE                          0x7FFF800
#define TMDS_RX_BIST_TEST_STATU                         0xF8000000

// HDRX_PHY_LNK_CRC_TEST_STS                            (0x91D4)
#define HDRX_PHY_LNK_CRC_VAL                            0xFFFF
#define HDRX_PHY_LNK_CRC_VAL_UPD_STS                    BIT16

// HDRX_LINK_RSRV                                       (0x91D8)
#define HDRX_LNK_RSRV                                   0xFFFF

// HDRX_LINK_TST_CTRL                                   (0x91DC)
#define HDRX_PHY_LNK_CRC_CALC_EN                        BIT0
#define HDRX_LINK_TST_CTL                               0xFE
#define HDRX_LINK_TST_CTL_SHIFT                         1

// HDRX_SUB_SAMPLER                                     (0x91E0)
#define HDRX_VID_CLK_DIVIDE                             0x0F
#define HDRX_SUBSAMPLER_AUTO                            BIT4
#define HDRX_SUBSAMPLER_SEL                             BIT5
#define HDRX_SOL_ADJUST_EN                              BIT6
#define HDRX_DATA_SBS_SYNC_EN                           BIT7

// HDRX_VID_CONV_CTRL                                   (0x91E4)
#define HDRX_VID_BUF_CORRECT_MD                         BIT0
#define HDRX_MD_CHNG_MT_TYPE                            BIT1
#define HDRX_MUTE_EDGE_EN                               BIT2
#define HDRX_VIDEO_BUFF_BYPASS                          BIT3
#define HDRX_VID_ERR_DIS                                BIT4
#define HDRX_MUTE_PIX_DEPTH_CHN                         BIT5
#define HDRX_BUF_VID_ERR_FILT_EN                        BIT6
#define HDRX_VID_COLOR_EXT_EN                           BIT7

// HDRX_VID_BUF_DELAY                                   (0x91F0)
#define HDMI_VID_BUF_DELAY                              0xFF

// HDRX_COLOR_DEPH_STATUS                               (0x91F8)
#define HDRX_VID_COLOR_DEPH                             0x0F
#define HDRX_VID_PHASE                                  0xF0
#define HDRX_VID_PHASE_SHIFT                            4

// HDRX_VIDEO_BUF_IRQ_EN                                (0x91FC)
#define HDRX_VID_BUF_ERROR_IRQ_E                        BIT0
#define HDRX_VID_IRQ_EN_RSRV                            0xFE
#define HDRX_VID_IRQ_EN_RSRV_SHIFT                      1

// HDRX_VIDEO_BUF_IRQ_STS                               (0x9200)
#define HDRX_VID_BUF_ERROR_IRQ_ST                       BIT0
#define HDRX_VID_IRQ_STS_RSRV                           0xFE
#define HDRX_VID_IRQ_STS_RSRV_SHIFT                     1

// HDRX_VIDEO_CTRL                                      (0x9220)
#define HDRX_VID_SWAP_R_B                               BIT1
#define HDRX_VID_MUTE_YUV444_YUV42                      BIT2
#define HDRX_VID_VMUTE_AUTO                             BIT3
#define HDRX_VID_YUV422_MD_EN                           BIT4
#define HDRX_VID_COLOR_AUTO                             BIT5
#define HDRX_VID_SWAP_R_B_AUTO                          BIT6
#define HDRX_VID_OUT_MODE                               BIT7
#define HDRX_VID_OUT_MODE_OLD_OP                        BIT8

// HDRX_VID_REGEN_CTRL                                  (0x9224)
#define HDRX_DE_REGEN_DIS                               BIT0
#define HDRX_DE_THRESHOLD                               0x06
#define HDRX_DE_THRESHOLD_SHIFT                         1
#define HDRX_DE_WIDTH_CLAMP_EN                          BIT3
#define HDRX_DVI_HS_REGEN                               BIT4
#define HDRX_DVI_VS_REGEN                               BIT5
#define HDRX_DE_RSRV                                    0xC0
#define HDRX_DE_RSRV_SHIFT                              6

// HDRX_PATGEN_CONTROL                                  (0x9228)
#define HDRX_VIDEO_PAT_SEL                              0x0F
#define HDRX_VIDEO_TPG_EN                               BIT4
#define HDRX_VIDEO_PAT_COLOR_CT                         0x60
#define HDRX_VIDEO_PAT_COLOR_CT_SHIFT                   5
#define HDRX_VIDEO_PAT_DATA_INV                         BIT7
#define HDRX_VIDEO_PAT_COLOR                            0xFF00

// HDRX_VIDEO_MUTE_RGB                                  (0x9230)
#define HDRX_VIDEO_MUTE_BLU                             0x1F
#define HDRX_VIDEO_MUTE_GRN                             0x07E0
#define HDRX_VIDEO_MUTE_RED                             0xF800

// HDRX_VIDEO_MUTE_YC                                   (0x9234)
#define HDRX_VIDEO_MUTE_CB                              0x1F
#define HDRX_VIDEO_MUTE_Y                               0x07E0
#define HDRX_VIDEO_MUTE_CR                              0xF800

// HDRX_DITHER_CTRL                                     (0x9238)
#define HDRX_DITHER_EN                                  BIT0
#define HDRX_DITH_OUT_SEL                               BIT1
#define HDRX_DITH_RSRV                                  BIT2
#define HDRX_DITH_TYPE                                  BIT3
#define HDRX_DITH_SEL                                   0x30
#define HDRX_DITH_SEL_SHIFT                             4
#define HDRX_DITH_MOD                                   0xC0
#define HDRX_DITH_MOD_SHIFT                             6

// HDRX_VIDEO_CRC_TEST_VALUE                            (0x9250)
#define HDRX_VIDEO_CRC_VAL_A                            0xFFFF
#define HDRX_VIDEO_CRC_VAL_B                            0xFFFF0000

// HDRX_VIDEO_CRC_TEST_STS                              (0x9254)
#define HDRX_VIDEO_CRC_VAL_A_UPD_STS                    BIT0
#define HDRX_VIDEO_CRC_VAL_B_UPD_STS                    BIT1

// HDRX_VIDEO_TST_CTRL                                  (0x925C)
#define HDRX_VIDEO_TST_CRC_CALC_EN                      BIT0
#define HDRX_VIDEO_TST_CTL                              0xFE
#define HDRX_VIDEO_TST_CTL_SHIFT                        1

// HDRX_SDP_CTRL                                        (0x9260)
#define HDRX_GCP_FRM_TRESH                              0x0F
#define HDRX_GCP_DEEP_DIS                               BIT4
#define HDRX_SDP_CTRL_RSRV                              0x60
#define HDRX_SDP_CTRL_RSRV_SHIFT                        5
#define HDRX_SDP_TST_IRQ_EN                             BIT7
#define HDRX_ACR_NCTS_PROG_EN                           BIT8  /*for Cannes*/
#define HDRX_ACR_DISABLE_AUTO_NCTS_STROBE               BIT9  /*for Cannes*/
#define HDRX_AUBUF_ERR_STB_OLD_LOGIC_EN                 BIT10 /*for Cannes*/

// HDRX_SDP_AV_MUTE_CTRL                                (0x9264)
#define HDRX_AV_MUTE_WINDOW_EN                          BIT0
#define HDRX_AV_MUTE_AUTO_EN                            BIT1
#define HDRX_AV_MUTE_SOFT                               BIT2
#define HDRX_A_MUTE_SOFT                                BIT3
#define HDRX_CLEAR_AV_MUTE_FLAG                         BIT4
#define HDRX_CLR_AVMUTE_EXT_EN                          BIT5
#define HDRX_CAPTURED_A_MUTE_EN                         BIT6
#define HDRX_CLR_CAPTURED_A_MUTE                        BIT7
#define HDRX_AV_MUTE_ON_NO_HDCP_AUTH_EN                  BIT8
#define HDRX_A_MUTE_ON_DVI_EN                           BIT9
#define HDRX_CLR_AVMUTE_ON_NOISE_EN                     BIT10
#define HDRX_CLR_AVMUTE_ON_TMT_DIS                      BIT11
#define HDRX_IGNR_AVMUTE_ON_CLR_SET_DIS                 BIT12
#define HDRX_MUTE_CTRL_RSRV                             0xE000
#define HDRX_CLR_AVMUTE_TMT_VAL                         0xFF0000

// HDRX_SDP_STATUS                                      (0x9268)
#define HDRX_PACK_MEM_BUSY                              BIT0
#define HDRX_AV_MUTE_STATUS                             BIT1
#define HDRX_AV_MUTE_GCP_STATUS                         BIT2
#define HDRX_AV_MUTE_EXT_STATUS                         BIT3
#define HDRX_NOISE_DETECTED                             BIT4
#define HDRX_GCP_OUT_OF_WINDOW                          BIT5

// HDRX_SDP_IRQ_CTRL                                    (0x926C)
#define HDRX_IF_VS_IRQ_EN                               BIT0
#define HDRX_IF_AVI_IRQ_EN                              BIT1
#define HDRX_IF_SPD_IRQ_EN                              BIT2
#define HDRX_IF_AUDIO_IRQ_EN                            BIT3
#define HDRX_IF_MPEG_IRQ_EN                             BIT4
#define HDRX_ACP_IRQ_EN                                 BIT5
#define HDRX_ISRC1_IRQ_EN                               BIT6
#define HDRX_ISRC2_IRQ_EN                               BIT7
#define HDRX_ACR_IRQ_EN                                 BIT8
#define HDRX_IF_UNDEF_IRQ_EN                            BIT9
#define HDRX_GCP_IRQ_EN                                 BIT10
#define HDRX_GMETA_IRQ_EN                               BIT11
#define HDRX_MUTE_EVENT_IRQ_E                           BIT12
#define HDRX_NOISE_DET_IRQ_EN                           BIT13

// HDRX_SDP_IRQ_STATUS                                  (0x9270)
#define HDRX_IF_VS_IRQ_STS                              BIT0
#define HDRX_IF_AVI_IRQ_STS                             BIT1
#define HDRX_IF_SPD_IRQ_STS                             BIT2
#define HDRX_IF_AUDIO_IRQ_STS                           BIT3
#define HDRX_IF_MPEG_IRQ_STS                            BIT4
#define HDRX_ACP_IRQ_STS                                BIT5
#define HDRX_ISRC1_IRQ_STS                              BIT6
#define HDRX_ISRC2_IRQ_STS                              BIT7
#define HDRX_ACR_IRQ_STS                                BIT8
#define HDRX_IF_UNDEF_IRQ_STS                           BIT9
#define HDRX_GCP_IRQ_STS                                BIT10
#define HDRX_GMETA_IRQ_STS                              BIT11
#define HDRX_MUTE_EVENT_IRQ_STS                         BIT12
#define HDRX_NOISE_DET_IRQ_STS                          BIT13

// HDRX_AU_CONV_CTRL                                    (0x9274)
#define HDRX_AUDIO_CONV_EN                              BIT0
#define HDRX_AMUTE_NVALID_E                             BIT1

// HDRX_PACK_CTRL                                       (0x9278)
#define HDRX_IF_UNDEF_ADDR                              0x0F
#define HDRX_IF_SAVE_UNDEF                              BIT4
#define HDRX_CLEAR_ALL_PACKS                            BIT5
#define HDRX_PACKET_RD_REQ                              BIT6
#define HDRX_PACK_RSRV                                  BIT7

// HDRX_PKT_STATUS                                      (0x9280)
#define HDRX_PACKET_AVLBL                               BIT0
#define HDRX_VENDOR_AVLBL                               BIT1
#define HDRX_AVI_INFO_AVLBL                             BIT2
#define HDRX_SPD_INFO_AVLBL                             BIT3
#define HDRX_AUDIO_INFO_AVLBL                           BIT4
#define HDRX_MPEG_INFO_AVLBL                            BIT5
#define HDRX_ACP_AVLBL                                  BIT6
#define HDRX_ISRC1_AVLBL                                BIT7
#define HDRX_ISRC2_AVLBL                                BIT8
#define HDRX_ACR_AVLBL                                  BIT9
#define HDRX_GCP_AVLBL                                  BIT10
#define HDRX_GMETA_AVLBL                                BIT11
#define HDMI_RSRV_AVLBL                                 0xF000

// HDRX_UNDEF_PKT_CODE                                  (0x9284)
#define HDRX_UNDEF_PKT_COD                              0xFF

// HDRX_PACK_GUARD_CTRL                                 (0x9288)
#define HDRX_GCP_SAVE_OPTION                            BIT0
#define HDRX_ACR_SAVE_OPTION                            BIT1
#define HDRX_PACKET_SAVE_OPTION                         BIT2
#define HDRX_GCP_FOUR_BT_CTRL                           BIT3
#define HDRX_ACR_FOUR_BT_CTRL                           BIT4
#define HDRX_BLOCK_HDMI_PACKS                           BIT5
#define HDRX_PACK_GUARD_RSRV                            0xC0
#define HDRX_PACK_GUARD_RSRV_SHIFT                      6

// HDRX_NOISE_DET_CTRL                                  (0x928C)
#define HDRX_NOISE_DETECTOR_EN                          BIT0
#define HDRX_BLOCK_HDMI_PACKS_AUT                       BIT1
#define HDRX_AVMUTE_CLEAR_EN                            BIT2
#define HDRX_NOISE_AUTO_AV_MUTE_EN                       BIT3
#define HDRX_ERROR_CT_CLEAR                             BIT4
#define HDRX_NOISE_DET_RSRV_A                           0xE0
#define HDRX_NOISE_DET_RSRV_A_SHIFT                     5
#define HDRX_MEAS_PERIOD                                0x0F00
#define HDRX_NOISE_THRESH                               0x7000
#define HDRX_NOISE_DET_RSRV_B                           BIT15

// HDRX_AUTO_PACK_PROCESS                               (0x9298)
#define HDRX_AVI_PROCESS_MODE                           0x03
#define HDRX_AUTO_PACK_RSRV                             BIT2
#define HDRX_CLR_AVI_INFO_TMOUT_EN                      BIT3
#define HDRX_CLR_AU_INFO_TMOUT_EN                       BIT4
#define HDRX_AVI_PACK_STABLE_THRESH                     0xE0
#define HDRX_AVI_PACK_STABLE_THRESH_SHIFT               5
#define HDRX_AVI_PACK_STABLE_DIS_GRD                    BIT8

// HDRX_AVI_INFOR_TIMEOUT_THRESH                        (0x929C)
#define HDRX_AVI_INFO_TIMEOUT_THRESH                    0xFF

// HDRX_AVI_AUTO_FIELDS                                 (0x92A4)
#define HDRX_AVI_PR_CTRL                                0x0F
#define HDRX_AVI_YUV_CTRL                               0x30
#define HDRX_AVI_YUV_CTRL_SHIFT                         4
#define HDRX_AUTO_FIELDS_RSRV                           0xC0
#define HDRX_AUTO_FIELDS_RSRV_SHIFT                     6

// HDRX_SDP_PRSNT_STS                                   (0x92A8)
#define HDRX_IF_VS_PRSNT_STS                            BIT0
#define HDRX_IF_AVI_PRSNT_STS                           BIT1
#define HDRX_IF_SPD_PRSNT_STS                           BIT2
#define HDRX_IF_AUDIO_PRSNT_STS                         BIT3
#define HDRX_IF_MPEG_PRSNT_STS                          BIT4
#define HDRX_ACP_PRSNT_STS                              BIT5
#define HDRX_ISRC1_PRSNT_STS                            BIT6
#define HDRX_ISRC2_PRSNT_STS                            BIT7
#define HDRX_ACR_PRSNT_STS                              BIT8
#define HDRX_IF_UNDEF_PRSNT_STS                         BIT9
#define HDRX_GCP_PRSNT_STS                              BIT10
#define HDRX_GMETA_PRSNT_STS                            BIT11
#define HDRX_GCP_AVMUTE_SET_PRSNT_ST                    BIT14
#define HDRX_GCP_AVMUTE_CLR_PRSNT_ST                    BIT15

// HDRX_SDP_MUTE_SRC_STATUS                             (0x92AC)
#define HDRX_AV_MUTE_SRC_GCP                            BIT0
#define HDRX_AV_MUTE_SRC_PACK_NOISE                     BIT1
#define HDRX_AV_MUTE_SRC_HDCP_NO_AUT                    BIT2
#define HDRX_AV_MUTE_SRC_SOFT                           BIT3
#define HDRX_AV_MUTE_SRC_RSRV                           0xF0
#define HDRX_AV_MUTE_SRC_RSRV_SHIFT                     4
#define HDRX_A_MUTE_SRC_AV_MUTE                         BIT8
#define HDRX_A_MUTE_SRC_DVI                             BIT9
#define HDRX_A_MUTE_SRC_STRM                            BIT10
#define HDRX_A_MUTE_SRC_SOFT                            BIT11
#define HDRX_A_MUTE_SRC_RSRV                            0xF000

// HDRX_SDP_ACR_NCTS_STB
#define BIT_HDRX_SDP_ACR_NCTS_STB                       BIT0

// HDRX_SDP_RSRV                                        (0x92C8)

// HDRX_SDP_AUD_CTRL                                    (0x92D0)
#define HDRX_CLR_AU_BLOCK                               BIT0
#define HDRX_CLR_AU_CH_STS                              BIT1
#define HDRX_SDP_AUD_CTRL_RSRV                          0x0C
#define HDRX_SDP_AUD_CTRL_RSRV_SHIFT                    2
#define HDRX_AU_TP_SEL                                  0xF0
#define HDRX_AU_TP_SEL_SHIFT                            4

// HDRX_SDP_AUD_MUTE_CTRL                               (0x92D4)
#define HDRX_SDP_AUD_MUTE_AUTO_EN                       BIT0
#define HDRX_SDP_AUD_MUTE_SOFT                          BIT1
#define HDRX_MUTE_ON_NVALID_EN                          BIT2
#define HDRX_AU_MUTE_MD                                 BIT3
#define HDRX_AU_MUTE_ON_COMPR                           BIT4
#define HDRX_AU_MUTE_ON_SGNL_NOISE                      BIT5
#define HDRX_AU_MUTE_ON_NO_SGNL                         BIT6

// HDRX_SDP_AUD_IRQ_CTRL                                (0x92D8)
#define HDRX_SDP_AUD_LAYOUT_IRQ_EN                      BIT0
#define HDRX_SDP_AUD_STATUS_IRQ_EN                      BIT1
#define HDRX_SDP_AUD_PKT_FLAG_IRQ_EN                    BIT2
#define HDRX_SDP_AUD_MUTE_FLAG_IRQ_EN                   BIT3
#define HDRX_SDP_AUD_BUF_OVR_IRQ_EN                     BIT4
#define HDRX_SDP_AUD_BUFP_DIFA_IRQ_EN                   BIT5
#define HDRX_SDP_AUD_BUFP_DIFB_IRQ_EN                   BIT6
#define HDRX_SDP_AUD_MUTE_IRQ_EN                        BIT7

// HDRX_SDP_AUDIO_IRQ_STATUS                            (0x92DC)
#define HDRX_SDP_AUD_LAYOUT_IRQ_STS                     BIT0
#define HDRX_SDP_AUD_STATUS_IRQ_STS                     BIT1
#define HDRX_SDP_AUD_PKT_PRSNT_IRQ_STS                  BIT2
#define HDRX_SDP_AUD_MUTE_FLAG_IRQ_STS                  BIT3
#define HDRX_SDP_AUD_BUF_OVR_IRQ_STS                    BIT4
#define HDRX_SDP_AUD_BUFP_DIFA_IRQ_STS                  BIT5
#define HDRX_SDP_AUD_BUFP_DIFB_IRQ_STS                  BIT6
#define HDRX_SDP_AUD_MUTE_IRQ_STS                       BIT7

// HDRX_SDP_AUD_STS                                     (0x92E0)
#define HDRX_SDP_AUD_MUTE_STS                           BIT0
#define HDRX_SDP_AUD_LAYOUT                             BIT1
#define HDRX_SDP_AUD_PKT_PRSNT                          BIT2
#define HDRX_DST_NORMAL_DOUBLE                          BIT3
#define HDRX_SDP_AUD_MUTE_GLBL_STS                      BIT4
#define HDRX_SDP_AUD_CHNL_COUNT                         0x0F00
#define HDRX_SDP_AUD_CODING_TYPE                        0xF000
#define HDRX_AU_SAMPLE_PRSNT                            BIT16
#define HDRX_DSD_PRESENT                                BIT17
#define HDRX_DST_PRESENT                                BIT18
#define HDRX_HBR_PRESENT                                BIT19
#define HDRX_AU_SAMPLE_PRSNT_SHIFT                      16
#define HDRX_SDP_AUD_CHNL_COUNT_SHIFT                   8
#define HDRX_SDP_AUD_CODING_TYPE_SHIFT                  12


// HDRX_AUD_MUTE_SRC_STS                                (0x92E4)
#define HDRX_SDP_AUD_MUTE_EXT_SRC                       BIT0
#define HDRX_SDP_AUD_MUTE_SOFT_SRC                      BIT1
#define HDRX_MUTE_ON_NVALID_SRC                         BIT2
#define HDRX_AU_MUTE_ON_COMPR_SRC                       BIT4
#define HDRX_AU_MUTE_ON_SGNL_NS_SRC                     BIT5
#define HDRX_AU_MUTE_ON_NO_SGNL_SRC                     BIT6

// HDRX_SDP_AUD_BUF_CNTRL                               (0x92EC)
#define HDRX_SDP_AUD_CLK_COR_EN                         BIT0
#define HDRX_SDP_AUD_STOP_CLK_COR_THRESH_B              BIT1
#define HDRX_BUF_EXT_EN                                 BIT2
#define HDRX_AU_WIDTH20_EN                              BIT3
#define HDRX_SDP_AUD_REP_SAMPLE_ON_ERR                  BIT4
#define HDRX_SDP_AUD_SKIP_SAMPLE_ERR                    BIT5
#define HDRX_REPLACE_NVALID                             BIT6
#define HDRX_DST_WIDE_EN                                BIT7

// HDRX_SDP_AUD_BUFPTR_DIF_THRESH_A                     (0x92F8)
#define HDRX_SDP_AUD_BUFPTR_DIF_THRESH_                 0xFF

// HDRX_SDP_AUD_BUFPTR_DIF_THRESH_B                     (0x92FC)

// HDRX_SDP_AUD_OUT_CNTRL                               (0x9304)
#define HDRX_SDP_AUD_OUT_EN                             BIT0
#define HDRX_SDP_AUD_OUT_DEN                            BIT1
#define HDRX_DSD_MODE_EN                                BIT2
#define HDRX_SDP_AUD_CLK_MODE                           0xF0
#define HDRX_SDP_AUD_CLK_MODE_SHIFT                     4

// HDRX_SDP_AUD_CHAN_RELOC                              (0x9308)
#define HDRX_SDP_AUD_CH0_REL                            0x03
#define HDRX_SDP_AUD_CH1_REL                            0x0C
#define HDRX_SDP_AUD_CH1_REL_SHIFT                      2
#define HDRX_SDP_AUD_CH2_REL                            0x30
#define HDRX_SDP_AUD_CH2_REL_SHIFT                      4
#define HDRX_SDP_AUD_CH3_REL                            0xC0
#define HDRX_SDP_AUD_CH3_REL_SHIFT                      6
#define HDRX_SDP_AUD_CH01_SWAP                          BIT8
#define HDRX_SDP_AUD_CH23_SWAP                          BIT9
#define HDRX_SDP_AUD_CH45_SWAP                          BIT10
#define HDRX_SDP_AUD_CH67_SWAP                          BIT11

// HDRX_KEYHOST_CONTROL                                 (0x9430)
#define KEYHOST_POWER_ON                                BIT0
#define KEYHOST_KEYS_LOADED                             BIT1
#define KEYHOST_CONTROL_RSRVA                           0x0C
#define KEYHOST_CONTROL_RSRVA_SHIFT                     2
#define KEYHOST_RESET                                   BIT4
#define KEYHOST_CONTROL_RSRVB                           0xE0
#define KEYHOST_CONTROL_RSRVB_SHIFT                     5

// HDRX_KEYHOST_STATUS                                  (0x9434)
#define KEYHOST_ACTIVE                                  BIT0
#define KEYHOST_STATUS_RESRVA                           0x1E
#define KEYHOST_STATUS_RESRVA_SHIFT                     1
#define KEYHOST_STATUS_RESRVB                           0xE0
#define KEYHOST_STATUS_RESRVB_SHIFT                     5

// HDRX_HDCP_CONTROL                                    (0x9440)
#define HDRX_HDCP_EN                                    BIT0
#define HDRX_HDCP_DECRYPT_EN                            BIT1
#define HDRX_HDCP_BKSV_LOAD_EN                          BIT2
#define HDRX_HDCP_LINKCHK_FNUM                          0x38
#define HDRX_HDCP_LINKCHK_FNUM_SHIFT                    3
#define HDRX_HDCP_TWS_IRQ_EN                            BIT6
#define HDRX_HDCP_TEST_TWS_IRQ                          BIT7
#define HDRX_HDCP_FORCE_ENC_DIS                         BIT8
#define HDRX_HDCP_1_1_CAPABLE                           BIT9
#define HDRX_HDCP_HDMI_CAPABLE                          BIT10
#define HDRX_HDCP_FAST_REAUTHENT_EN                      BIT11
#define HDRX_HDCP_ENC_SIGNALING                         0x3000
#define HDRX_HDCP_AVMUTE_AUTO_EN                        BIT14
#define HDRX_HDCP_AVMUTE_IGNORE                         BIT15
#define HDRX_HDCP_FORCE_HDMI_MODE_                      BIT16

// HDRX_HDCP_ADDR                                       (0x9444)
#define HDRX_HDCP_ADDR_VAL                              0x7F
#define HDRX_HDCP_AKSVREAD_EN                           BIT7

// HDRX_HDCP_WIN_MOD                                    (0x9448)
#define HDRX_HDCP_WIN_ADJ                               0x0F
#define HDRX_HDCP_WIN_ALG_SEL                           BIT4

// HDRX_HDCP_STATUS                                     (0x9454)
#define HDRX_HDCP_DEC_STATUS                            BIT0
#define HDRX_HDCP_AUTHENTICATION                        BIT1
#define HDRX_HDCP_DVI_CTRL_STATU                        0x3C
#define HDRX_HDCP_DVI_CTRL_STATU_SHIFT                  2
#define HDRX_HDCP_EESS_STATUS                           BIT6
#define HDRX_HDCP_1_1_STATUS                            BIT7
#define HDRX_HDCP_KEYBUS_ACTIVE                         BIT8

// HDRX_HDCP_STATUS_EXT                                 (0x9458)
#define HDRX_HDCP_RE_AUTHENTICATION                     BIT0
#define HDRX_HDCP_I2C_IRQ_STATUS                        BIT1
#define HDRX_HDCP_AVMUTE_IGNORED                        BIT2
#define HDRX_HDCP_STATUS_EXT_RSRV                       BIT3

// HDRX_HDCP_TBL_IRQ_INDEX                              (0x945C)
#define HDRX_HDCP_TWS_IRQ_INDEX                         0x7F

// HDRX_HDCP_UPSTREAM_CTRL                              (0x9480)
#define HDRX_HDCP_REPEATER                              BIT0
#define HDRX_HDCP_DOWNSTREAM_FAIL                       BIT1
#define HDRX_HDCP_DOWNSTREAM_READY                      BIT2
#define HDRX_HDCP_KSV_LIST_READY                        BIT3
#define HDRX_HDCP_KSV_FIFO_RESET                        BIT4
#define HDRX_HDCP_UPSTREAM_CTRL_RSRV1                   0xE0
#define HDRX_HDCP_UPSTREAM_CTRL_RSRV1_SHIFT             5
#define HDRX_HDCP_STATE_UNAUTHENTICATED_IRQ_EN          BIT8
#define HDRX_HDCP_STATE_WAIT_DWNSTRM_IRQ_EN             BIT9
#define HDRX_HDCP_STATE_ASSMBL_KSV_LIST_IRQ_EN          BIT10
#define HDRX_HDCP_TIMER_EXPIRED_IRQ_EN                  BIT11
#define HDRX_HDCP_UPSTREAM_CTRL_RSRV2                   0xFFFFF000

// HDRX_HDCP_UPSTREAM_STATUS                            (0x9484)
#define HDRX_HDCP_STATE_UNAUTHENTICATED                 BIT0
#define HDRX_HDCP_STATE_WAIT_DWNSTRM                    BIT1
#define HDRX_HDCP_STATE_ASSMBL_KSV_LIST                 BIT2
#define HDRX_HDCP_STATE_AUTHENTICATED                   BIT3
#define HDRX_HDCP_STATE_RSRV1                           0xF0
#define HDRX_HDCP_STATE_RSRV1_SHIFT                     4
#define HDRX_HDCP_STATE_UNAUTHENTICATED_ST              BIT8
#define HDRX_HDCP_STATE_WAIT_DWNSTRM_STS                BIT9
#define HDRX_HDCP_STATE_ASSMBL_KSV_LIST_STS             BIT10
#define HDRX_HDCP_TIMER_EXPIRED_STS                     BIT11
#define HDRX_HDCP_STATE_RSRV2                           0xFFFFF000

// HDRX_HDCP_BSTATUS                                    (0x9488)
#define HDRX_HDCP_DEVICE_COUNT                          0x7F
#define HDRX_HDCP_MAX_DEV_EXCEEDED                      BIT7
#define HDRX_HDCP_DEPTH                                 0x0700
#define HDRX_HDCP_MAX_CASCADE_EXCEEDED                  BIT11
#define HDRX_HDCP_BSTATUS_RSRV                          0xFFFFF000

// HDRX_HDCP_TIMEOUT                                    (0x948C)
#define HDRX_HDCP_TIMEOUT_PER                           0x3F
#define HDRX_HDCP_TIMEOUT_RSRV                          0xFFFFFFC0

// HDRX_HDCP_KSV_FIFO_CTRL                              (0x94B4)
#define HDRX_HDCP_KSV_FIFO_SCL_STRCH_EN                 BIT0
#define HDRX_HDCP_KSV_FIFO_CTRL_RSRV1                   BIT1
#define HDRX_HDCP_KSV_FIFO_NE_IRQ_EN                    BIT2
#define HDRX_HDCP_KSV_FIFO_NF_IRQ_EN                    BIT3
#define HDRX_HDCP_KSV_FIFO_AF_IRQ_EN                    BIT4
#define HDRX_HDCP_KSV_FIFO_F_IRQ_EN                     BIT5
#define HDRX_HDCP_KSV_FIFO_AE_IRQ_EN                    BIT6
#define HDRX_HDCP_KSV_FIFO_E_IRQ_EN                     BIT7
#define HDRX_HDCP_KSV_FIFO_CTRL_RSRV                    0xFFFFFF00

// HDRX_HDCP_KSV_FIFO_STS                               (0x94B8)
#define HDRX_HDCP_KSV_FIFO_BUSY                         BIT0
#define HDRX_HDCP_KSV_FIFO_ERROR                        BIT1
#define HDRX_HDCP_KSV_FIFO_NE                           BIT2
#define HDRX_HDCP_KSV_FIFO_NF                           BIT3
#define HDRX_HDCP_KSV_FIFO_AF                           BIT4
#define HDRX_HDCP_KSV_FIFO_F                            BIT5
#define HDRX_HDCP_KSV_FIFO_AE                           BIT6
#define HDRX_HDCP_KSV_FIFO_E                            BIT7
#define HDRX_HDCP_KSV_FIFO_STS_RSRV                     0xFFFFFF00

/* HDRX_I2S_TX_CONTROL                                  (0x8540) */
#define HDRX_I2S_TX_EN                                  BIT0
#define HDRX_I2S_TX_ORDER_SEL                           BIT1
#define HDRX_I2S_TX_ALIGN_SEL                           BIT2
#define HDRX_I2S_TX_OUT_EN                              BIT3
#define HDRX_I2S_TX_OUT_CH0_EN                          BIT4
#define HDRX_I2S_TX_OUT_CH1_EN                          BIT5
#define HDRX_I2S_TX_OUT_CH2_EN                          BIT6
#define HDRX_I2S_TX_OUT_CH3_EN                          BIT7
#define HDRX_I2S_TX_LPCM_MD_DIS                         BIT8
#define HDRX_I2S_TX_HEADER_POS                          BIT9

/* HDRX_I2S_TX_CLK_CNTRL                                (0x8544) */
#define HDRX_I2S_TX_CLK_DIV                             0x03
#define HDRX_I2S_TX_WRD_CLK_POL                         BIT2
#define HDRX_I2S_TX_BIT_CLK_POL                         BIT3
#define HDRX_I2S_TX_WRD_CLK_MODE                        BIT4

// HDMI_INST_SOFT_RESETS                                (0x9500)
#define HDMI_IFM_RESET                                  BIT0
#define HDMI_IMD_RESET                                  BIT1

// HDMI_INST_HARD_RESETS                                (0x9504)
#define HDMI_IFM_HARD_RESET                             BIT0
#define HDMI_IMD_HARD_RESET                             BIT1

// HDMI_HOST_CONTROL                                    (0x9508)
#define LOCK_RD_REG                                     BIT0
#define LOCK_VALID_IMD                                  BIT1
#define LOCK_VALID_IFM                                  BIT2

// HDMI_AFR_CONTROL                                     (0x950C)
#define HDMI_AFR_DISP_IFM_ERR_EN                        BIT0
#define HDMI_AFR_DISP_IBD_ERR_EN                        BIT1
#define HDMI_AFR_DISP_CLK_ERR_EN                        BIT2
#define HDMI_AFR_DISP_VID_MUTE_EN                       BIT3
#define HDMI_AFR_DDDS_IFM_ERR_EN                        BIT4
#define HDMI_AFR_DDDS_IBD_ERR_EN                        BIT5
#define HDMI_AFR_DDDS_CLK_ERR_EN                        BIT6
#define HDMI_AFR_DDDS_VID_MUTE_EN                       BIT7

// HDMI_AFB_CONTROL                                     (0x9510)
#define HDMI_AFB_IFM_ERR_EN                             BIT0
#define HDMI_AFB_IBD_ERR_EN                             BIT1
#define HDMI_AFB_CLK_ERR_EN                             BIT2
#define HDMI_AFB_VID_MUTE_EN                            BIT3

// HDMI_INPUT_IRQ_MASK                                  (0x9514)
#define HDMI_INPUT_NO_HS_MASK                           BIT0
#define HDMI_INPUT_NO_VS_MASK                           BIT1
#define HDMI_INPUT_HS_PERIOD_ERR_MASK                   BIT2
#define HDMI_INPUT_VS_PERIOD_ERR_MASK                   BIT3
#define HDMI_INPUT_DE_CHANGE_MASK                       BIT4
#define HDMI_INPUT_INTLC_ERR_MASK                       BIT5
#define HDMI_INPUT_ACTIVE_MASK                          BIT6
#define HDMI_INPUT_BLANK_MASK                           BIT7
#define HDMI_INPUT_LINEFLAG_MASK                        BIT8
#define HDMI_INPUT_VS_MASK                              BIT9
#define HDMI_INPUT_AFR_DETECT_MASK                      BIT10
#define HDMI_ANA_LCLK_LOST_MASK                         BIT11
#define HDMI_TEST_IRQ_MASK                              BIT12
#define HDMI_OVR_LCLK_LOST_MASK                         BIT13
#define HDMI_VDDS_OPEN_LOOP_MASK                        BIT14

// HDMI_INPUT_IRQ_STATUS                                (0x9518)
#define HDMI_INPUT_NO_HS                                BIT0
#define HDMI_INPUT_NO_VS                                BIT1
#define HDMI_INPUT_HS_PERIOD_ERR                        BIT2
#define HDMI_INPUT_VS_PERIOD_ERR                        BIT3
#define HDMI_INPUT_DE_CHANGE                            BIT4
#define HDMI_INPUT_INTLC_ERR                            BIT5
#define HDMI_INPUT_VACTIVE                              BIT6
#define HDMI_INPUT_VBLANK                               BIT7
#define HDMI_INPUT_LINEFLAG                             BIT8
#define HDMI_INPUT_VS                                   BIT9
#define HDMI_INPUT_AFR_DETECT                           BIT10
#define HDMI_ANA_LCLK_LOST                              BIT11
#define HDMI_OVR_LCLK_LOST                              BIT13
#define HDMI_VDDS_OPEN_LOOP                             BIT14

// HDMI_TEST_IRQ                                        (0x951C)
#define HDMI_GENERATE_TEST_IRQ                          BIT0

// INSTRU_CLOCK_CONFIG                                  (0x9520)
#define TCLK_DIV                                        0x07FC

// HDMI_IFM_CLK_CONTROL                                 (0x9524)
#define HDMI_IFM_CLK_DISABLE                            BIT0

// HDMI_INSTRUMENT_POLARITY_CONTROL                     (0x9528)
#define HDMI_VS_POL                                     BIT0
#define HDMI_HS_POL                                     BIT1
#define HDMI_DV_POL                                     BIT2

// HDMI_IFM_CTRL                                        (0x9560)
#define HDMI_IFM_EN                                     BIT0
#define HDMI_IFM_MEASEN                                 BIT1
#define HDMI_IFM_HOFFSETEN                              BIT2
#define HDMI_IFM_FIELD_SEL                              BIT3
#define HDMI_IFM_FIELD_EN                               BIT4
#define HDMI_IFM_INT_ODD_EN                             BIT5
#define HDMI_IFM_SPACE_SEL                              BIT6
#define HDMI_IFM_ODD_INV                                BIT7
#define HDMI_IFM_FIELD_DETECT_MO                        BIT8
#define HDMI_IFM_FREERUN_ODD                            BIT11
#define HDMI_IFM_SOURCE_SEL                             0x3000
#define HDMI_WD_H_POLARITY                              BIT14
#define HDMI_WD_V_POLARITY                              BIT15

// HDMI_IFM_WATCHDOG                                    (0x9564)
#define HDMI_IFM_H_WATCHDOG                             0xFF
#define HDMI_IFM_V_WATCHDOG                             0x3F00

// HDMI_IFM_CHANGE                                      (0x956C)
#define HDMI_IFM_H_CHG_THRESH                           0x07
#define HDMI_IFM_V_CHG_THRESH                           0x38
#define HDMI_IFM_V_CHG_THRESH_SHIFT                     3
#define SPARE_HDMI_IFM_CHANGE                           0xC0
#define SPARE_HDMI_IFM_CHANGE_SHIFT                     6
#define HDMI_IFM_LOWER_ODD                              0x0F00
#define HDMI_IFM_UPPER_ODD                              0xF000

// HDMI_IFM_SPACE                                       (0x9580)
#define HDMI_IFM_CSYNC_TO_SAMPLE_SPACE                  0x0F
#define HDMI_IFM_SAMPLE_TO_CSYNC_SPACE                  0xF0
#define HDMI_IFM_SAMPLE_TO_CSYNC_SPACE_SHIFT            4

// HDMI_INPUT_CONTROL                                   (0x95B0)
#define IP_RUN_EN                                       BIT0
#define IP_EXT_DV_EN                                    BIT1
#define IP_INTLC_EN                                     BIT2
#define IP_HS_DELAY_EN                                  BIT3
#define IP_ADD_OFFSET                                   BIT4
#define IP_PIXGRAB_EN                                   BIT5
#define IP_ODD_FIELD_ONLY                               BIT7
#define IP_444_EN                                       BIT8
#define IP_BLACK_OFFSET                                 BIT11
#define FEATURE_DET_EN                                  BIT12
#define FEATURE_DET_RGB_SEL                             0x6000

// HDMI_INPUT_HS_DELAY                                  (0x95B4)
#define HDMI_INPUT_HDELAY                               0xFFFF

// HDMI_INPUT_VS_DELAY                                  (0x95B8)
#define HDMI_INPUT_VDELAY                               0xFFFF

// HDMI_INPUT_H_ACT_START                               (0x95BC)
#define HDMI_INPUT_H_ACTIV_START                        0xFFFF

// HDMI_INPUT_H_ACT_WIDTH                               (0x95C0)
#define HDMI_INPUT_H_ACTIV_WIDTH                        0xFFFF

// HDMI_INPUT_V_ACT_START_ODD                           (0x95C4)
#define HDMI_INPUT_V_ACTIV_START_ODD                    0xFFFF

// HDMI_INPUT_V_ACT_START_EVEN                          (0x95C8)
#define HDMI_INPUT_V_ACTIV_START_EVE                    0xFFFF

// HDMI_INPUT_V_ACT_LENGTH                              (0x95CC)
#define HDMI_INPUT_V_ACTIV_LENGTH                       0xFFFF

// HDMI_INPUT_FREEZE_VIDEO                              (0x95D0)
#define HDMI_INPUT_FREEZE                               BIT0

// HDMI_INPUT_FRAME_RESET_LINE                          (0x95DC)
#define HDMI_INPUT_FRAME_RESET_LIN                      0xFFFF

// HDMI_INPUT_IBD_CONTROL                               (0x95F4)
#define HDMI_INPUT_MEASURE_DE_nDATA                     BIT0
#define HDMI_INPUT_IBD_MEASWINDOW_EN                    BIT1
#define HDMI_INPUT_IBD_RGB_SEL                          0x0C
#define HDMI_INPUT_IBD_RGB_SEL_SHIFT                    2
#define HDMI_INPUT_DET_THOLD_SEL                        0xF0
#define HDMI_INPUT_DET_THOLD_SEL_SHIFT                  4
#define HDMI_INPUT_IBD_EN                               BIT8
#define HDMI_INPUT_IBD_CHG_THRESH                       0x0600

// HDMI_INPUT_PIXGRAB_H                                 (0x9610)
#define HDMI_INPUT_H_PGRAB                              0xFFFF

// HDMI_INPUT_PIXGRAB_V                                 (0x9614)
#define HDMI_INPUT_V_PGRAB                              0xFFFF

// PU_CONFIG                                            (0x9640)
#define MASK_VIDEO_MUTE                                 BIT0
#define ENDIAN_SWAP_EN_SECURE                           BIT1
#define ENDIAN_SWAP_EN_NON_SECURE                       BIT2
#define MASK_AFR_DTG                                    BIT3
#define MASK_AFR_DDDS                                   BIT4
#define CLK_HDMI_PIX_POL                                BIT5

// HDRX_CRC_CTRL                                        (0x9644)
#define CRC_EN                                          BIT0
#define INTLC_EN                                        BIT1
#define CRC_SEL                                         BIT2
#define CRC_SOFT_RESET                                  BIT3

// HDRX_CRC_ERR_STATUS                                  (0x9648)
#define CRC_ERR_F0_RED                                  BIT0
#define CRC_ERR_F0_GREEN                                BIT1
#define CRC_ERR_F0_BLUE                                 BIT2
#define CRC_ERR_F1_RED                                  BIT3
#define CRC_ERR_F1_GREEN                                BIT4
#define CRC_ERR_F1_BLUE                                 BIT5
#define HDMI_MEM_BBAD                                   0x03C0
#define HDMI_MEM_BEND                                   0x3C00

// HDRX_CRC_RED                                         (0x964C)
#define CRC_RED                                         0xFFFF

// HDRX_CRC_GREEN                                       (0x9650)
#define CRC_GREEN                                       0xFFFF

// HDRX_CRC_BLUE                                        (0x9654)
#define CRC_BLUE                                        0xFFFF

// PU_DEBUG_TEST_CTRL                                   (0x9658)
#define HDMI_TEST_BUS_SEL                               0x07
#define HDMI_TEST_BUS_ENABLE                            BIT3

// PU_SPARE_REG                                         (0x965C)
#define HDMI_PU_SPARE                                   0x07

// DVI_HDCP_CTRL                                        (0x9660)
#define DVI_RX_HDCP_EXT_SYNC_S                          BIT0
#define DVI_RX_HDCP_EXT_CTRL_S                          BIT1

// DUAL_DVI0_CTRL                                       (0xB664)
#define DUAL_DVI_OUTPUT_EN                              BIT0
#define DUAL_DVI_PIX_CLK_SEL                            BIT1
#define DUAL_DVI_CTRL_SEL                               BIT2
#define DUAL_DVI_DUAL_TO_SINGLE                         BIT3
#define DUAL_DVI_DATA_PRI_SEC_S                         BIT4
#define DUAL_DVI_FIFO_SOFT_RESE                         BIT5
#define DUAL_DVI_MAN_PRI_SEC_DE                         BIT6
#define DUAL_DVI_MAN_SEC_PRI_DE                         0x0C00
#define DUAL_DVI_RSRV                                   0xF000

// DUAL_DVI0_IRQ_AFR_AFB_STATUS                         (0xB668)
#define DUAL_DVI_PRI_RX_CORE_IR                         BIT0
#define DUAL_DVI_PRI_RX_PHY_IRQ                         BIT1
#define DUAL_DVI_PRI_RX_IFM_IRQ                         BIT2
#define DUAL_DVI_PRI_RX_ AFB                            BIT3
#define DUAL_DVI_PRI_RX_DDDS_AF                         BIT4
#define DUAL_DVI_PRI_RX_DTG_AFR                         BIT5
#define DUAL_DVI_SEC_RX_CORE_IR                         BIT6
#define DUAL_DVI_SEC_RX_PHY_IRQ                         BIT7
#define DUAL_DVI_SEC_RX_IFM_IRQ                         BIT8
#define DUAL_DVI_SEC_RX_ AFB                            BIT9
#define DUAL_DVI_SEC_RX_DDDS_A                          BIT10
#define DUAL_DVI_SEC_RX_DTG_AF                          BIT11

// DUAL_DVI0_AUTO_DELAY_STATUS                          (0xB66C)
#define DUAL_DVI_AUTO_PRI_DE_PO                         0x03
#define DUAL_DVI_AUTO_SEC_DE_P                          0x0C
#define DUAL_DVI_AUTO_SEC_DE_P_SHIFT                    2

// DUAL_DVI0_DBG_CTRL                                   (0xB670)
#define DUAL_DVI_TST_BUS_SEL                            0x07

#define HDMI_RX_PHY_EQ_GAIN                             0x70000UL
#define HDMI_RX_PHY_EQ_GAIN_SHIFT                       16

#define HDMI_RX_PHY_EQ_PEAK                             0x700UL
#define HDMI_RX_PHY_EQ_PEAK_SHIFT                       8

/* HDRX_HPD_CTRL                                        (0x8034) */
#define HDRX_HPD_CTRL_MASK                              BIT0
#define HDRX_HPD_CTRL_RSRV                              0xFE
#define HDRX_HPD_CTRL_RSRV_SHIFT                        1

/* HDRX_CSM_CTRL                                        (0x83CC) */
#define HDRX_CSM_HPD_SEL_MASK                           0x03
#define HDRX_CSM_HPD_SEL_SHIFT                          0
#define HDRX_CSM_HPD_REG_SEL                            BIT2
#define HDRX_CSM_HPD_AU_DIS                             BIT3
#define HDRX_CSM_I2C_MCCS_SEL_MASK                      0x30
#define HDRX_CSM_I2C_MCCS_SEL_SHIFT                     4
#define HDRX_CSM_I2C_MCCS_REG_SEL                       BIT6
#define HDRX_CSM_I2C_MASTER_SEL_MASK                    0x180
#define HDRX_CSM_I2C_MASTER_SEL_SHIFT                   7
#define HDRX_CSM_I2C_MASTER_REG_SEL                     BIT9

/* HDRX_CSM_IRQ_STATUS                                  (0x83D4) */
#define HDRX_CSM_HPD_IRQ_STS                            BIT0
#define HDRX_CSM_I2C_SLV1_IRQ_STS                       BIT1
#define HDRX_CSM_I2C_SLV2_IRQ_STS                       BIT2
#define HDRX_CSM_I2C_SLV3_IRQ_STS                       BIT3
#define HDRX_CSM_I2C_SLV4_IRQ_STS                       BIT4
#define HDRX_CSM_I2C_MASTER_IRQ_STS                     BIT5
#define HDRX_CSM_CEC_IRQ_STS                            BIT6
#define HDRX_CSM_I2C_SLV5_IRQ_STS                       BIT7
#define HDRX_CSM_I2C_SLV6_IRQ_STS                       BIT8
#define HDRX_CSM_I2C_SLV7_IRQ_STS                       BIT9
#define HDRX_CSM_I2C_SLV8_IRQ_STS                       BIT10

/* HPD_DIRECT_CTRL                                      (0x83D8) */
#define HPD_DIRECT_INP_A                                BIT0
#define HPD_DIRECT_INP_B                                BIT1
#define HPD_DIRECT_INP_C                                BIT2
#define HPD_DIRECT_INP_D                                BIT3
#define HPD_DIRECT_INP_E                                BIT4

/* HPD_DIRECT_STATUS                                    (0x83DC) */
#define HPD_STATUS_INP_A                                BIT0
#define HPD_STATUS_INP_B                                BIT1
#define HPD_STATUS_INP_C                                BIT2
#define HPD_STATUS_INP_D                                BIT3
#define HPD_STATUS_INP_E                                BIT4

/* HPD_DIRECT_IRQ_CTRL                                  (0x83E0) */
#define HPD_RISING_INP_A_IRQ_EN                         BIT0
#define HPD_FALL_INP_A_IRQ_EN                           BIT1
#define HPD_RISING_INP_B_IRQ_EN                         BIT2
#define HPD_FALL_INP_B_IRQ_EN                           BIT3
#define HPD_RISING_INP_C_IRQ_EN                         BIT4
#define HPD_FALL_INP_C_IRQ_EN                           BIT5
#define HPD_RISING_INP_D_IRQ_EN                         BIT6
#define HPD_FALL_INP_D_IRQ_EN                           BIT7
#define HPD_RISING_INP_E_IRQ_EN                         BIT8
#define HPD_FALL_INP_E_IRQ_EN                           BIT9

/* HPD_DIRECT_IRQ_STATUS                                (0x83E4) */
#define HPD_STATUS_RISING_INP_A                         BIT0
#define HPD_STATUS_FALL_INP_A                           BIT1
#define HPD_STATUS_RISING_INP_B                         BIT2
#define HPD_STATUS_FALL_INP_B                           BIT3
#define HPD_STATUS_RISING_INP_C                         BIT4
#define HPD_STATUS_FALL_INP_C                           BIT5
#define HPD_STATUS_RISING_INP_D                         BIT6
#define HPD_STATUS_FALL_INP_D                           BIT7
#define HPD_STATUS_RISING_INP_E                         BIT8
#define HPD_STATUS_FALL_INP_E                           BIT9

/* EDPD_DIRECT_CTRL                                     (0x83E8) */
#define EDPD_DIRECT_INP_A                               BIT0
#define EDPD_DIRECT_INP_B                               BIT1
#define EDPD_DIRECT_INP_C                               BIT2
#define EDPD_DIRECT_INP_D                               BIT3
#define EDPD_DIRECT_INP_E                               BIT4

/* EDPD_DIRECT_STATUS                                   (0x83EC) */
#define EDPD_STATUS_INP_A                               BIT0
#define EDPD_STATUS_INP_B                               BIT1
#define EDPD_STATUS_INP_C                               BIT2
#define EDPD_STATUS_INP_D                               BIT3
#define EDPD_STATUS_INP_E                               BIT4

#endif
