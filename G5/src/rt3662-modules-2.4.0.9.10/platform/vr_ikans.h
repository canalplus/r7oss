/****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************
 
    Module Name:
    vr_ikans.h
 
    Abstract:
    Only for IKANOS Vx160 or Vx180 platform.
 
    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Sample Lin	01-28-2008    Created

 */

#ifndef MODULE_IKANOS
#define IKANOS_EXTERN	extern
#else
#define IKANOS_EXTERN
#endif // MODULE_IKANOS //


typedef int (*PF_HARD_START_XMIT)(struct sk_buff *, struct net_device *);


IKANOS_EXTERN void VR_IKANOS_FP_Init(iNIC_PRIVATE *pAd,
									 PF_HARD_START_XMIT pfun);


IKANOS_EXTERN int IKANOS_DataFramesTx(struct sk_buff *pSkb,
										struct net_device *pNetDev);

IKANOS_EXTERN void IKANOS_DataFrameRx(iNIC_PRIVATE *pAd,
									  unsigned char dev_id, 
										void *pRxParam,
										struct sk_buff *pSkb,
										UINT32 Length);

IKANOS_EXTERN INT32 IKANOS_WlanDataFramesTx(
	IN void					*_pAdBuf,
	IN struct net_device	*pNetDev);
										

/* End of vr_ikans.h */
