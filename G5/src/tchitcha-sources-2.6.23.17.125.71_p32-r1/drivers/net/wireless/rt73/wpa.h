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
 *	Module Name:	wpa.h
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *
 ***************************************************************************/

#ifndef __WPA_H__
#define __WPA_H__

// WpaPsk EAPOL Key descripter frame format related length
#define LEN_KEY_DESC_NONCE          32
#define LEN_KEY_DESC_IV             16
#define LEN_KEY_DESC_RSC            8
#define LEN_KEY_DESC_ID             8
#define LEN_KEY_DESC_REPLAY         8
#define LEN_KEY_DESC_MIC            16

//EPA VERSION
#define EAPOL_VER                   1
#define DESC_TYPE_TKIP              1
#define DESC_TYPE_AES               2
#define RSN_KEY_DESC                0xfe
#define WPA1_KEY_DESC		        0xFE
#define WPA2_KEY_DESC		        0x2

#define LEN_MASTER_KEY              32

// EAPOL EK, MK
#define LEN_EAP_EK                  16
#define LEN_EAP_MICK                16
#define LEN_EAP_KEY                 ((LEN_EAP_EK)+(LEN_EAP_MICK))
// TKIP key related
#define LEN_TKIP_EK                 16
#define LEN_TKIP_RXMICK             8
#define LEN_TKIP_TXMICK             8
#define LEN_AES_EK                  16
#define LEN_AES_KEY                 LEN_AES_EK
#define LEN_TKIP_KEY                ((LEN_TKIP_EK)+(LEN_TKIP_RXMICK)+(LEN_TKIP_TXMICK))
#define TKIP_AP_TXMICK_OFFSET       ((LEN_EAP_KEY)+(LEN_TKIP_EK))
#define TKIP_AP_RXMICK_OFFSET       (TKIP_AP_TXMICK_OFFSET+LEN_TKIP_TXMICK)
#define TKIP_GTK_LENGTH             ((LEN_TKIP_EK)+(LEN_TKIP_RXMICK)+(LEN_TKIP_TXMICK))
#define LEN_PTK                     ((LEN_EAP_KEY)+(LEN_TKIP_KEY))
//#define MAX_LEN_OF_RSNIE            48

//EAP Packet Type
#define EAPPacket       0
#define EAPOLStart      1
#define EAPOLLogoff     2
#define EAPOLKey        3
#define EAPOLASFAlert   4
#define EAPTtypeMax     5

#define EAPOL_MSG_INVALID   0
#define EAPOL_PAIR_MSG_1    1
#define EAPOL_PAIR_MSG_3    2
#define EAPOL_GROUP_MSG_1   3

//#if WPA_SUPPLICANT_SUPPORT

/* RFC 3748 - Extensible Authentication Protocol (EAP) */

struct eap_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including code and identifier; network byte order */
	/* followed by length-4 octets of data */
} __attribute__ ((packed));

enum { EAP_CODE_REQUEST = 1, EAP_CODE_RESPONSE = 2, EAP_CODE_SUCCESS = 3,
       EAP_CODE_FAILURE = 4 };

#define	LENGTH_EAP_H				4

/* EAP Request and Response data begins with one octet Type. Success and
 * Failure do not have additional data. */

typedef enum {
	EAP_TYPE_NONE = 0,
	EAP_TYPE_IDENTITY = 1 /* RFC 3748 */,
	EAP_TYPE_NOTIFICATION = 2 /* RFC 3748 */,
	EAP_TYPE_NAK = 3 /* Response only, RFC 3748 */,
	EAP_TYPE_MD5 = 4, /* RFC 3748 */
	EAP_TYPE_OTP = 5 /* RFC 3748 */,
	EAP_TYPE_GTC = 6, /* RFC 3748 */
	EAP_TYPE_TLS = 13 /* RFC 2716 */,
	EAP_TYPE_LEAP = 17 /* Cisco proprietary */,
	EAP_TYPE_SIM = 18 /* draft-haverinen-pppext-eap-sim-12.txt */,
	EAP_TYPE_TTLS = 21 /* draft-ietf-pppext-eap-ttls-02.txt */,
	EAP_TYPE_AKA = 23 /* draft-arkko-pppext-eap-aka-12.txt */,
	EAP_TYPE_PEAP = 25 /* draft-josefsson-pppext-eap-tls-eap-06.txt */,
	EAP_TYPE_MSCHAPV2 = 26 /* draft-kamath-pppext-eap-mschapv2-00.txt */,
	EAP_TYPE_TLV = 33 /* draft-josefsson-pppext-eap-tls-eap-07.txt */,
	EAP_TYPE_FAST = 43 /* draft-cam-winget-eap-fast-00.txt */,
	EAP_TYPE_PAX = 46, /* draft-clancy-eap-pax-04.txt */
	EAP_TYPE_EXPANDED_NAK = 254 /* RFC 3748 */,
	EAP_TYPE_PSK = 255 /* EXPERIMENTAL - type not yet allocated
			    * draft-bersani-eap-psk-09 */
} EapType;
//#endif

// EAPOL Key Information definition within Key descriptor format
typedef struct PACKED _KEY_INFO
{
#ifdef BIG_ENDIAN
    UCHAR	KeyAck:1;
    UCHAR	Install:1;
    UCHAR	KeyIndex:2;
    UCHAR	KeyType:1;
    UCHAR	KeyDescVer:3;
    UCHAR	Rsvd:3;
    UCHAR	EKD_DL:1;       // EKD for AP; DL for STA
    UCHAR	Request:1;
    UCHAR	Error:1;
    UCHAR	Secure:1;
    UCHAR	KeyMic:1;
#else
    UCHAR   KeyMic:1;
    UCHAR   Secure:1;
    UCHAR   Error:1;
    UCHAR   Request:1;
    UCHAR   EKD_DL:1;       // EKD for AP; DL for STA
    UCHAR   Rsvd:3;
    UCHAR   KeyDescVer:3;
    UCHAR   KeyType:1;
    UCHAR   KeyIndex:2;
    UCHAR   Install:1;
    UCHAR   KeyAck:1;
#endif
}   KEY_INFO, *PKEY_INFO;

// EAPOL Key descriptor format
typedef struct PACKED _KEY_DESCRIPTER
{
    UCHAR       Type;
    KEY_INFO    KeyInfo;
    UCHAR       KeyLength[2];
    UCHAR       ReplayCounter[LEN_KEY_DESC_REPLAY];
    UCHAR       KeyNonce[LEN_KEY_DESC_NONCE];
    UCHAR       KeyIv[LEN_KEY_DESC_IV];
    UCHAR       KeyRsc[LEN_KEY_DESC_RSC];
    UCHAR       KeyId[LEN_KEY_DESC_ID];
    UCHAR       KeyMic[LEN_KEY_DESC_MIC];
    UCHAR       KeyDataLen[2];
    UCHAR       KeyData[MAX_LEN_OF_RSNIE];
}   KEY_DESCRIPTER, *PKEY_DESCRIPTER;

typedef struct PACKED _EAPOL_PACKET
{
    UCHAR               Version;
    UCHAR               Type;
    UCHAR               Len[2];
    KEY_DESCRIPTER      KeyDesc;
}   EAPOL_PACKET, *PEAPOL_PACKET;

//802.11i D10 page 83
typedef struct  _GTK_ENCAP
{
    UCHAR               Kid:2;
    UCHAR               tx:1;
    UCHAR               rsv:5;
    UCHAR               rsv1;
    UCHAR               GTK[32];
}   GTK_ENCAP, *PGTK_ENCAP;

typedef struct  _KDE_ENCAP
{
    UCHAR               Type;
    UCHAR               Len;
    UCHAR               OUI[3];
    UCHAR               DataType;
    GTK_ENCAP      GTKEncap;
}   KDE_ENCAP, *PKDE_ENCAP;

// For supplicant state machine states. 802.11i Draft 4.1, p. 97
// We simplified it
typedef enum    _WpaState
{
    SS_NOTUSE,              // 0
    SS_START,               // 1
    SS_WAIT_MSG_3,          // 2
    SS_WAIT_GROUP,          // 3
    SS_FINISH,              // 4
    SS_KEYUPDATE,           // 5
}   WPA_STATE;

#endif   // __WPA_H__
