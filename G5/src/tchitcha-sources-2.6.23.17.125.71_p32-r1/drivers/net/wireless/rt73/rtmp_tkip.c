/***************************************************************************
 * RT2x00 SourceForge Project - http://rt2x00.serialmonkey.com             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   Licensed under the GNU GPL                                            *
 *   Original code supplied under license from RaLink Inc, 2004.           *
 ***************************************************************************/

/***************************************************************************
 *	Module Name:	rtmp_tkip.c
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Paul Wu		02-25-02	Initial
 *
 ***************************************************************************/

#include	"rt_config.h"

// Rotation functions on 32 bit values
#define ROL32( A, n ) \
	( ((A) << (n)) | ( ((A)>>(32-(n))) & ( (1UL << (n)) - 1 ) ) )
#define ROR32( A, n ) ROL32( (A), 32-(n) )

/*
	========================================================================

	Routine	Description:
		Convert from UCHAR[] to ULONG in a portable way

	Arguments:
      pMICKey		pointer to MIC Key

	Return Value:
		None

	Note:

	========================================================================
*/
ULONG	RTMPTkipGetUInt32(
	IN	PUCHAR	pMICKey)
{
	ULONG	res = 0;
	INT		i;

	for (i = 0; i < 4; i++)
	{
		res |= (*pMICKey++) << (8 * i);
	}

	return res;
}

/*
	========================================================================

	Routine	Description:
		Convert from ULONG to UCHAR[] in a portable way

	Arguments:
      pDst			pointer to destination for convert ULONG to UCHAR[]
      val			the value for convert

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPTkipPutUInt32(
	IN OUT	PUCHAR		pDst,
	IN		ULONG		val)
{
	INT i;

	for(i = 0; i < 4; i++)
	{
		*pDst++ = (UCHAR) (val & 0xff);
		val >>= 8;
	}
}

/*
	========================================================================

	Routine	Description:
		Set the MIC Key.

	Arguments:
      pAdapter		Pointer to our adapter
      pMICKey		pointer to MIC Key

	Return Value:
		None

	Note:

	========================================================================
*/
VOID    RTMPTkipSetMICKey(
	IN	PTKIP_KEY_INFO	pTkip,
	IN	PUCHAR			pMICKey)
{
	// Set the key
	pTkip->K0 = RTMPTkipGetUInt32(pMICKey);
	pTkip->K1 = RTMPTkipGetUInt32(pMICKey + 4);
	// and reset the message
	pTkip->L = pTkip->K0;
	pTkip->R = pTkip->K1;
	pTkip->nBytesInM = 0;
	pTkip->M = 0;
}

/*
	========================================================================

	Routine	Description:
		Calculate the MIC Value.

	Arguments:
      pAdapter		Pointer to our adapter
      uChar			Append this uChar

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPTkipAppendByte(
	IN	PTKIP_KEY_INFO	pTkip,
	IN	UCHAR 			uChar)
{
	// Append the byte to our word-sized buffer
	pTkip->M |= (uChar << (8* pTkip->nBytesInM));
	pTkip->nBytesInM++;
	// Process the word if it is full.
	if( pTkip->nBytesInM >= 4 )
	{
		pTkip->L ^= pTkip->M;
		pTkip->R ^= ROL32( pTkip->L, 17 );
		pTkip->L += pTkip->R;
		pTkip->R ^= ((pTkip->L & 0xff00ff00) >> 8) | ((pTkip->L & 0x00ff00ff) << 8);
		pTkip->L += pTkip->R;
		pTkip->R ^= ROL32( pTkip->L, 3 );
		pTkip->L += pTkip->R;
		pTkip->R ^= ROR32( pTkip->L, 2 );
		pTkip->L += pTkip->R;
		// Clear the buffer
		pTkip->M = 0;
		pTkip->nBytesInM = 0;
	}
}

/*
	========================================================================

	Routine	Description:
		Calculate the MIC Value.

	Arguments:
      pAdapter		Pointer to our adapter
      pSrc			Pointer to source data for Calculate MIC Value
      Len			Indicate the length of the source data

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPTkipAppend(
	IN	PTKIP_KEY_INFO	pTkip,
	IN	PUCHAR			pSrc,
	IN	UINT			nBytes)
{
	// This is simple
	while(nBytes > 0)
	{
		RTMPTkipAppendByte(pTkip, *pSrc++);
		nBytes--;
	}
}

/*
	========================================================================

	Routine	Description:
		Get the MIC Value.

	Arguments:
      pAdapter		Pointer to our adapter

	Return Value:
		None

	Note:
		the MIC Value is store in pAdapter->PrivateInfo.MIC
	========================================================================
*/
VOID	RTMPTkipGetMIC(
	IN	PTKIP_KEY_INFO	pTkip)
{
	// Append the minimum padding
	RTMPTkipAppendByte(pTkip, 0x5a );
	RTMPTkipAppendByte(pTkip, 0 );
	RTMPTkipAppendByte(pTkip, 0 );
	RTMPTkipAppendByte(pTkip, 0 );
	RTMPTkipAppendByte(pTkip, 0 );
	// and then zeroes until the length is a multiple of 4
	while( pTkip->nBytesInM != 0 )
	{
		RTMPTkipAppendByte(pTkip, 0 );
	}
	// The appendByte function has already computed the result.
	RTMPTkipPutUInt32(pTkip->MIC, pTkip->L);
	RTMPTkipPutUInt32(pTkip->MIC + 4, pTkip->R);
}

/*
	========================================================================

	Routine	Description:
		Init Tkip function.

	Arguments:
      pAdapter		Pointer to our adapter
		pTKey       Pointer to the Temporal Key (TK), TK shall be 128bits.
		KeyId		TK Key ID
		pTA			Pointer to transmitter address
		pMICKey		pointer to MIC Key

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPInitTkipEngine(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PUCHAR			pKey,
	IN	UCHAR			KeyId,
	IN	PUCHAR			pTA,
	IN	PUCHAR			pMICKey,
	IN	PUCHAR			pTSC,
	OUT	PULONG			pIV16,
	OUT	PULONG			pIV32)
{
	TKIP_IV	tkipIv;

	// Prepare 8 bytes TKIP encapsulation for MPDU
	memset(&tkipIv, 0, sizeof(TKIP_IV));
	tkipIv.IV16.field.rc0 = *(pTSC + 1);
	tkipIv.IV16.field.rc1 = (tkipIv.IV16.field.rc0 | 0x20) & 0x7f;
	tkipIv.IV16.field.rc2 = *pTSC;
	tkipIv.IV16.field.CONTROL.field.ExtIV = 1;  // 0: non-extended IV, 1: an extended IV
	tkipIv.IV16.field.CONTROL.field.KeyID = KeyId;
	memcpy(&tkipIv.IV32, (pTSC + 2), 4);   // Copy IV

	*pIV16 = tkipIv.IV16.word;
	*pIV32 = tkipIv.IV32;
}

/*
	========================================================================

	Routine	Description:
		Init MIC Value calculation function which include set MIC key &
		calculate first 16 bytes (DA + SA + priority +  0)

	Arguments:
      pAdapter		Pointer to our adapter
		pTKey       Pointer to the Temporal Key (TK), TK shall be 128bits.
		pDA			Pointer to DA address
		pSA			Pointer to SA address
		pMICKey		pointer to MIC Key

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPInitMICEngine(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PUCHAR			pKey,
	IN	PUCHAR			pDA,
	IN	PUCHAR			pSA,
	IN  UCHAR           UserPriority,
	IN	PUCHAR			pMICKey)
{
	ULONG Priority = UserPriority;

	// Init MIC value calculation
	RTMPTkipSetMICKey(&pAdapter->PrivateInfo.Tx, pMICKey);
	// DA
	RTMPTkipAppend(&pAdapter->PrivateInfo.Tx, pDA, MAC_ADDR_LEN);
	// SA
	RTMPTkipAppend(&pAdapter->PrivateInfo.Tx, pSA, MAC_ADDR_LEN);
	// Priority + 3 bytes of 0
	RTMPTkipAppend(&pAdapter->PrivateInfo.Tx, (PUCHAR)&Priority, 4);
}

/*
	========================================================================

	Routine	Description:
		Compare MIC value of received MSDU

	Arguments:
		pAdapter	Pointer to our adapter
		pSrc        Pointer to the received Plain text data
		pDA			Pointer to DA address
		pSA			Pointer to SA address
		pMICKey		pointer to MIC Key
		Len         the length of the received plain text data exclude MIC value

	Return Value:
		TRUE        MIC value matched
		FALSE       MIC value mismatched

	Note:

	========================================================================
*/
BOOLEAN	    RTMPTkipCompareMICValue(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PUCHAR			pSrc,
	IN	PUCHAR			pDA,
	IN	PUCHAR			pSA,
	IN	PUCHAR			pMICKey,
	IN	UINT			Len)
{
	UCHAR	OldMic[8];
	ULONG	Priority = 0;
	INT		i;

	// Init MIC value calculation
	RTMPTkipSetMICKey(&pAdapter->PrivateInfo.Rx, pMICKey);
	// DA
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pDA, MAC_ADDR_LEN);
	// SA
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pSA, MAC_ADDR_LEN);
	// Priority + 3 bytes of 0
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, (PUCHAR)&Priority, 4);

	// Calculate MIC value from plain text data
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pSrc, Len);

	// Get MIC valude from received frame
	memcpy(OldMic, pSrc + Len, 8);

	// Get MIC value from decrypted plain data
	RTMPTkipGetMIC(&pAdapter->PrivateInfo.Rx);

	// Move MIC value from MSDU, this steps should move to data path.
	// Since the MIC value might cross MPDUs.
	if(!NdisEqualMemory(pAdapter->PrivateInfo.Rx.MIC, OldMic, 8))
	{
		DBGPRINT(RT_DEBUG_ERROR, "! TKIP MIC Error !\n");  //MIC error.
		DBGPRINT(RT_DEBUG_ERROR, "Orig MIC value =");  //MIC error.
		for (i = 0; i < 8; i++)
		{
			DBGPRINT_RAW(RT_DEBUG_ERROR, "%02x:", OldMic[i]);  //MIC error.
		}
		DBGPRINT(RT_DEBUG_ERROR, "\n");  //MIC error.
		DBGPRINT(RT_DEBUG_ERROR, "Calculated MIC value =");  //MIC error.
		for (i = 0; i < 8; i++)
		{
			DBGPRINT_RAW(RT_DEBUG_ERROR, "%02x:", pAdapter->PrivateInfo.Rx.MIC[i]);  //MIC error.
		}
		DBGPRINT_RAW(RT_DEBUG_ERROR, "\n");  //MIC error.
		return (FALSE);
	}
	return (TRUE);
}

/*
	========================================================================

	Routine	Description:
		Compare MIC value of received MSDU

	Arguments:
		pAdapter	Pointer to our adapter
		pLLC		LLC header
		pSrc        Pointer to the received Plain text data
		pDA			Pointer to DA address
		pSA			Pointer to SA address
		pMICKey		pointer to MIC Key
		Len         the length of the received plain text data exclude MIC value

	Return Value:
		TRUE        MIC value matched
		FALSE       MIC value mismatched

	Note:

	========================================================================
*/
BOOLEAN	    RTMPTkipCompareMICValueWithLLC(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PUCHAR			pLLC,
	IN	PUCHAR			pSrc,
	IN	PUCHAR			pDA,
	IN	PUCHAR			pSA,
	IN	PUCHAR			pMICKey,
	IN	UINT			Len)
{
	UCHAR	OldMic[8];
	ULONG	Priority = 0;
	INT		i;

	// Init MIC value calculation
	RTMPTkipSetMICKey(&pAdapter->PrivateInfo.Rx, pMICKey);
	// DA
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pDA, MAC_ADDR_LEN);
	// SA
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pSA, MAC_ADDR_LEN);
	// Priority + 3 bytes of 0
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, (PUCHAR)&Priority, 4);

	// Start with LLC header
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pLLC, 8);

	// Calculate MIC value from plain text data
	RTMPTkipAppend(&pAdapter->PrivateInfo.Rx, pSrc, Len);

	// Get MIC valude from received frame
	memcpy(OldMic, pSrc + Len, 8);

	// Get MIC value from decrypted plain data
	RTMPTkipGetMIC(&pAdapter->PrivateInfo.Rx);

	// Move MIC value from MSDU, this steps should move to data path.
	// Since the MIC value might cross MPDUs.
	if(!NdisEqualMemory(pAdapter->PrivateInfo.Rx.MIC, OldMic, 8))
	{
		DBGPRINT(RT_DEBUG_ERROR, "! TKIP MIC Error !\n");  //MIC error.
		DBGPRINT(RT_DEBUG_ERROR, "Orig MIC value =");  //MIC error.
		for (i = 0; i < 8; i++)
		{
			DBGPRINT_RAW(RT_DEBUG_ERROR, "%02x:", OldMic[i]);  //MIC error.
		}
		DBGPRINT_RAW(RT_DEBUG_ERROR, "\n");  //MIC error.
		DBGPRINT(RT_DEBUG_ERROR, "Calculated MIC value =");  //MIC error.
		for (i = 0; i < 8; i++)
		{
			DBGPRINT_RAW(RT_DEBUG_ERROR, "%02x:", pAdapter->PrivateInfo.Rx.MIC[i]);  //MIC error.
		}
		DBGPRINT_RAW(RT_DEBUG_ERROR, "\n");  //MIC error.
		return (FALSE);
	}
	return (TRUE);
}
/*
	========================================================================

	Routine	Description:
		Copy frame from waiting queue into relative ring buffer and set
	appropriate ASIC register to kick hardware transmit function

	Arguments:
		pAdapter		Pointer	to our adapter
		PNDIS_PACKET	Pointer to Ndis Packet for MIC calculation
		pEncap			Pointer to LLC encap data
		LenEncap		Total encap length, might be 0 which indicates no encap

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPCalculateMICValue(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct sk_buff  *pSkb,
	IN	PUCHAR			pEncap,
	IN	PCIPHER_KEY		pKey)
{
	PVOID			pVirtualAddress;
	UINT			Length;
	PUCHAR			pSrc;
    UCHAR           UserPriority;

	pVirtualAddress = pSkb->data;
	Length = pSkb->len;

    UserPriority = RTMP_GET_PACKET_UP(pSkb);
	pSrc = (PUCHAR) pVirtualAddress;

	// Start Calculate MIC Value
	RTMPInitMICEngine(
		pAdapter,
		pKey->Key,
		pSrc,
		pSrc + 6,
		UserPriority,
		pKey->TxMic);

	if (pEncap != NULL)
	{
		// LLC encapsulation
		RTMPTkipAppend(&pAdapter->PrivateInfo.Tx, pEncap, 6);
		// Protocol Type
		RTMPTkipAppend(&pAdapter->PrivateInfo.Tx, pSrc + 12, 2);
	}
	Length -= 14;
	pSrc += 14;
	do
	{
		if (Length > 0)
		{
			RTMPTkipAppend(&pAdapter->PrivateInfo.Tx, pSrc, Length);
		}
	}	while (FALSE);		// End of copying payload

	// Compute the final MIC Value
	RTMPTkipGetMIC(&pAdapter->PrivateInfo.Tx);
}

