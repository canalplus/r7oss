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
 *	Module Name:	mlme.h
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	idamlaj		05-10-2006	Import rfmon implementation
 *	idamlaj		14-10-2006	Mac Address Changing
 *	idamlaj		14-10-2006	RFMONTx (based on MarkW's code)
 *	RomainB     31-12-2006  RFMONTx getter, update of some ioctl values
 *
 ***************************************************************************/

#ifndef _OID_H_
#define _OID_H_

#include <linux/wireless.h>

// Ralink defined OIDs
#if WIRELESS_EXT <= 11
#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE                              0x8BE0
#endif
#define SIOCIWFIRSTPRIV								SIOCDEVPRIVATE
#endif

#define RT_PRIV_IOCTL								(SIOCIWFIRSTPRIV + 0x01)
#define RT_PRIV_IOCTL_WPA_SUPPLICANT				(SIOCIWFIRSTPRIV + 0x0E)
#define RTPRIV_IOCTL_SET							(SIOCIWFIRSTPRIV + 0x02)

#ifdef DBG
#define RTPRIV_IOCTL_BBP				(SIOCIWFIRSTPRIV + 0x03)
#define RTPRIV_IOCTL_MAC				(SIOCIWFIRSTPRIV + 0x05)
#endif

#define RTPRIV_IOCTL_ADHOCOFDM				(SIOCIWFIRSTPRIV + 0x06)
#define RTPRIV_IOCTL_AUTH				(SIOCIWFIRSTPRIV + 0x07)
#define RTPRIV_IOCTL_WEPSTATUS				(SIOCIWFIRSTPRIV + 0x08)
#define RTPRIV_IOCTL_STATISTICS				(SIOCIWFIRSTPRIV + 0x09)
#define RTPRIV_IOCTL_WPAPSK				(SIOCIWFIRSTPRIV + 0x0A)
#define RTPRIV_IOCTL_PSM				(SIOCIWFIRSTPRIV + 0x0B)
#define RTPRIV_IOCTL_SETRFMONTX				(SIOCIWFIRSTPRIV + 0x0C)
#define RTPRIV_IOCTL_GETRFMONTX				(SIOCIWFIRSTPRIV + 0x0D)
#define RTPRIV_IOCTL_GSITESURVEY			(SIOCIWFIRSTPRIV + 0x0F)
#define RTPRIV_IOCTL_GETRAAPCFG				(SIOCIWFIRSTPRIV + 0x11)
#define RTPRIV_IOCTL_FORCEPRISMHEADER			(SIOCIWFIRSTPRIV + 0x12)

#define OID_GET_SET_TOGGLE							0x8000

#define OID_GEN_MACHINE_NAME                        0x0001021A

//RaConfig (Query/SetInformation)-->
#define OID_802_11_NETWORK_TYPES_SUPPORTED          0x0103
#define OID_802_11_NETWORK_TYPE_IN_USE              0x0104
#define OID_802_11_RSSI_TRIGGER                     0x0107
#define OID_802_11_NUMBER_OF_ANTENNAS               0x010B
#define OID_802_11_RX_ANTENNA_SELECTED              0x010C
#define OID_802_11_TX_ANTENNA_SELECTED              0x010D
#define OID_802_11_SUPPORTED_RATES                  0x010E
#define OID_802_11_ADD_WEP                          0x0112
#define OID_802_11_REMOVE_WEP                       0x0113
#define OID_802_11_DISASSOCIATE                     0x0114
#define OID_802_11_PRIVACY_FILTER                   0x0118
#define OID_802_11_ASSOCIATION_INFORMATION          0x011E
#define OID_802_11_TEST                             0x011F

#define RT_OID_802_11_COUNTRY_REGION                0x0507
#define OID_802_11_BSSID_LIST_SCAN                  0x0508
#define OID_802_11_SSID                    			0x0509  //also in get
#define OID_802_11_BSSID                   			0x050A  //also in get
#define RT_OID_802_11_RADIO                         0x050B  //also in get
#define RT_OID_802_11_PHY_MODE                      0x050C  //also in get
#define RT_OID_802_11_STA_CONFIG                    0x050D  //also in get
#define OID_802_11_DESIRED_RATES           		    0x050E
#define RT_OID_802_11_PREAMBLE                      0x050F  //also in get
#define OID_802_11_WEP_STATUS                       0x0510  //also in get
#define OID_802_11_AUTHENTICATION_MODE              0x0511  //also in get
#define OID_802_11_INFRASTRUCTURE_MODE              0x0512  //also in get
#define RT_OID_802_11_RESET_COUNTERS                0x0513
#define OID_802_11_RTS_THRESHOLD           		    0x0514  //also in get
#define OID_802_11_FRAGMENTATION_THRESHOLD          0x0515  //also in get
#define OID_802_11_POWER_MODE              		    0x0516  //also in get
#define OID_802_11_TX_POWER_LEVEL                   0x0517
#define RT_OID_802_11_ADD_WPA                       0x0518
#define OID_802_11_REMOVE_KEY                       0x0519
#define OID_802_11_ADD_KEY                          0x0520
#define OID_802_11_CONFIGURATION           		    0x0521  //also in get
#define OID_802_11_TX_PACKET_BURST			        0x0522
#define RT_OID_802_11_QUERY_NOISE_LEVEL             0x0523
#define RT_OID_802_11_EXTRA_INFO	                0x0524

#define RT_OID_DEVICE_NAME                          0x0607
#define RT_OID_VERSION_INFO                         0x0608
#define OID_802_11_BSSID_LIST              		    0x0609
#define OID_802_3_CURRENT_ADDRESS                   0x060A
#define OID_GEN_MEDIA_CONNECT_STATUS                0x060B
#define RT_OID_802_11_QUERY_LINK_STATUS             0x060C
#define OID_802_11_RSSI                    		    0x060D
#define OID_802_11_STATISTICS                       0x060E
#define OID_GEN_RCV_OK                              0x060F
#define OID_GEN_RCV_NO_BUFFER                       0x0610
#define RT_OID_802_11_QUERY_EEPROM_VERSION          0x0611
#define RT_OID_802_11_QUERY_FIRMWARE_VERSION        0x0612
#define RT_OID_802_11_QUERY_LAST_RX_RATE            0x0613
#define RT_OID_802_11_TX_POWER_LEVEL_1              0x0614
#define RT_OID_802_11_QUERY_PIDVID                  0x0615

//#if WPA_SUPPLICANT_SUPPORT
#define OID_SET_COUNTERMEASURES                     0x0616
#define OID_802_11_SET_IEEE8021X                    0x0617
#define OID_802_11_SET_IEEE8021X_REQUIRE_KEY        0x0618
#define OID_802_11_PMKID                            0x0620
#define RT_OID_WPA_SUPPLICANT_SUPPORT               0x0621
#define RT_OID_WE_VERSION_COMPILED                  0x0622
#define OID_SET_WSC_IE_PROBE_REQ                    0x0624
#define OID_802_11_RCV_BEACON                       0x0625
//#endif

#define OID_802_11_ENCRYPTION_STATUS                OID_802_11_WEP_STATUS
#define OID_802_11_RELOAD_DEFAULTS                  0x011B
//<-- RaConfig (Query/SetInformation)


//
// Ralink defined OIDs ******************
//
#ifdef TEST_MODE_SUPPORT
#define RT_OID_ENTER_TEST_MODE						0x0D720101
#define RT_OID_EXIT_TEST_MODE						0x0D720102

#define RT_OID_START_BULK_OUT						0x0D73010E
#define RT_OID_STOP_BULK_OUT						0x0D730115

#define RT_OID_VENDOR_SELECT_CHANNEL				0x0D730112
#define RT_OID_VENDOR_SET_TX_POWER					0x0D730113

#if 0
#define RT_OID_START_BULK_IN						0x0D720108
#define RT_OID_STOP_BULK_IN							0x0D730116
#else
#define RT_OID_START_RX								0x0D720108
#define RT_OID_STOP_RX								0x0D730116
#endif

#define RT_OID_START_CONT_TX						0x0D730120
#define RT_OID_START_CARRIER_TX						0x0D730122
#define RT_OID_STOP_TX								0x0D730121//stop BBP test mode continuous/carrier Tx.
#define RT_OID_VENDOR_GET_COUNTERS					0x0D73011B
#define RT_OID_VENDOR_GET_TX_FRAGMENTS_COUNT		0x0D73011C
#endif  /* TEST_MODE_SUPPORT */

#define RT_OID_VENDOR_GET_COUNTERS					0x0D73011B

#define RT_OID_USB_VENDOR_RESET						0x0D730101
#define RT_OID_USB_VENDOR_UNPLUG					0x0D730102
#define RT_OID_USB_VENDOR_SWITCH_FUNCTION			0x0D730103

#define RT_OID_MULTI_WRITE_MAC						0x0D730107
#define RT_OID_MULTI_READ_MAC						0x0D730108
#define RT_OID_USB_VENDOR_EEPROM_WRITE				0x0D73010A
#define RT_OID_USB_VENDOR_EEPROM_READ				0x0D73010B

#define RT_OID_USB_VENDOR_ENTER_TESTMODE			0x0D73010C
#define RT_OID_USB_VENDOR_EXIT_TESTMODE				0x0D73010D
#define RT_OID_USB_GET_DEVICE_DESC					0x0D730110
#define RT_OID_VENDOR_WRITE_BBP						0x0D730119
#define RT_OID_VENDOR_READ_BBP						0x0D730118
#define RT_OID_VENDOR_WRITE_RF						0x0D73011A

#define RT_OID_VENDOR_FLIP_IQ						0x0D73011D


#define RT_OID_SET_PER_RATE_TX_RATE_SWITCHING_STRUC	0x0D730123
#define RT_OID_GET_BBP_R17_TUNING_MODE				0x0D730124
#define RT_OID_SET_BBP_R17_TUNING_MODE				(OID_GET_SET_TOGGLE | RT_OID_GET_BBP_R17_TUNING_MODE)
#define RT_OID_GET_TEST_MODE_BBP_TUNING_MODE		0x0D730125
#define RT_OID_SET_TEST_MODE_BBP_TUNING_MODE		(OID_GET_SET_TOGGLE | RT_OID_GET_TEST_MODE_BBP_TUNING_MODE)
#define RT_OID_NOR_FLASH_ERASE_BLOCK				0x0D730126
#define RT_OID_NOR_FLASH_WRITE						0x0D730127
#define RT_OID_NOR_FLASH_READ						0x0D730128
#define RT_OID_NOR_FLASH_GET						0x0D730129
//#define RT_OID_SET_GENERAL_TX_RATE_SWITCHING_STRUC	0x0D730124
//#define RT_OID_GET_TX_RATE_SWITCHING_COUNTERS		0x0D730125
//used by driver internally
#define RT_OID_USB_RESET_BULK_OUT					0x0D730210
#define RT_OID_USB_RESET_BULK_IN					0x0D730211
#define RT_OID_SET_PSM_BIT_SAVE						0x0D730212
#define RT_OID_SET_RADIO							0x0D730214
#define RT_OID_UPDATE_TX_RATE						0x0D730216
#define OID_802_11_ADD_KEY_WEP						0x0D730218
#define RT_OID_RESET_FROM_ERROR						0x0D73021A
#define RT_OID_LINK_DOWN							0x0D73021B
#define RT_OID_RESET_FROM_NDIS						0x0D73021C
#define RT_OID_PERIODIC_EXECUT						0x0D73021D
#define RT_OID_TEST_PERIODIC_EXECUT					0x0D73021E
#define RT_OID_ASICLED_EXECUT						0x0D73021F
#define RT_OID_CHECK_GPIO							0x0D730215
#define RT_OID_REMOVE_ALLKEYS						0x0D730220
#define RT_PERFORM_SOFT_DIVERSITY					0x0D730221
#define RT_OID_FORCE_WAKE_UP						0x0D730222
#define RT_OID_SET_PSM_BIT_ACTIVE					0x0D730223
#define RT_CMD_RESET_MLME							0x0D730224
#ifdef NETOPIA
#define RT_OID_UPDATE_R17							0x0D730213
#endif


#define RT_OID_802_11_BSSID                   (OID_GET_SET_TOGGLE | OID_802_11_BSSID)
#define RT_OID_802_11_SSID                    (OID_GET_SET_TOGGLE | OID_802_11_SSID)
#define RT_OID_802_11_INFRASTRUCTURE_MODE     (OID_GET_SET_TOGGLE | OID_802_11_INFRASTRUCTURE_MODE)
#define RT_OID_802_11_ADD_WEP                 (OID_GET_SET_TOGGLE | OID_802_11_ADD_WEP)
#define RT_OID_802_11_ADD_KEY                 (OID_GET_SET_TOGGLE | OID_802_11_ADD_KEY)
#define RT_OID_802_11_REMOVE_WEP              (OID_GET_SET_TOGGLE | OID_802_11_REMOVE_WEP)
#define RT_OID_802_11_REMOVE_KEY              (OID_GET_SET_TOGGLE | OID_802_11_REMOVE_KEY)
#define RT_OID_802_11_DISASSOCIATE            (OID_GET_SET_TOGGLE | OID_802_11_DISASSOCIATE)
#define RT_OID_802_11_AUTHENTICATION_MODE     (OID_GET_SET_TOGGLE | OID_802_11_AUTHENTICATION_MODE)
#define RT_OID_802_11_PRIVACY_FILTER          (OID_GET_SET_TOGGLE | OID_802_11_PRIVACY_FILTER)
#define RT_OID_802_11_BSSID_LIST_SCAN         (OID_GET_SET_TOGGLE | OID_802_11_BSSID_LIST_SCAN)
#define RT_OID_802_11_WEP_STATUS              (OID_GET_SET_TOGGLE | OID_802_11_WEP_STATUS)
#define RT_OID_802_11_RELOAD_DEFAULTS         (OID_GET_SET_TOGGLE | OID_802_11_RELOAD_DEFAULTS)
#define RT_OID_802_11_NETWORK_TYPE_IN_USE     (OID_GET_SET_TOGGLE | OID_802_11_NETWORK_TYPE_IN_USE)
#define RT_OID_802_11_TX_POWER_LEVEL          (OID_GET_SET_TOGGLE | OID_802_11_TX_POWER_LEVEL)
#define RT_OID_802_11_RSSI_TRIGGER            (OID_GET_SET_TOGGLE | OID_802_11_RSSI_TRIGGER)
#define RT_OID_802_11_FRAGMENTATION_THRESHOLD (OID_GET_SET_TOGGLE | OID_802_11_FRAGMENTATION_THRESHOLD)
#define RT_OID_802_11_RTS_THRESHOLD           (OID_GET_SET_TOGGLE | OID_802_11_RTS_THRESHOLD)
#define RT_OID_802_11_RX_ANTENNA_SELECTED     (OID_GET_SET_TOGGLE | OID_802_11_RX_ANTENNA_SELECTED)
#define RT_OID_802_11_TX_ANTENNA_SELECTED     (OID_GET_SET_TOGGLE | OID_802_11_TX_ANTENNA_SELECTED)
#define RT_OID_802_11_SUPPORTED_RATES         (OID_GET_SET_TOGGLE | OID_802_11_SUPPORTED_RATES)
#define RT_OID_802_11_DESIRED_RATES           (OID_GET_SET_TOGGLE | OID_802_11_DESIRED_RATES)
#define RT_OID_802_11_CONFIGURATION           (OID_GET_SET_TOGGLE | OID_802_11_CONFIGURATION)
#define RT_OID_802_11_POWER_MODE              (OID_GET_SET_TOGGLE | OID_802_11_POWER_MODE)

#define RT_OID_802_11_QUERY_PREAMBLE          0x0D710101
#define RT_OID_802_11_SET_PREAMBLE            (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_PREAMBLE)

#define RT_OID_802_11_QUERY_AC_CAM            0x0D710104
#define RT_OID_802_11_SET_AC_CAM              (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_AC_CAM)
#ifdef DBG
#define RT_OID_802_11_QUERY_HARDWARE_REGISTER 0x0D710105
#define RT_OID_802_11_SET_HARDWARE_REGISTER   (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_HARDWARE_REGISTER)
#endif
#define RT_OID_802_11_QUERY_RACONFIG          0x0D710106
#define RT_OID_802_11_SET_RACONFIG            (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_RACONFIG)

#define RT_OID_802_11_QUERY_COUNTRY_REGION     0x0D710107
#define RT_OID_802_11_SET_COUNTRY_REGION       (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_COUNTRY_REGION)

#define	RT_OID_802_11_QUERY_RADIO			  0x0D710108
#define RT_OID_802_11_SET_RADIO      		  (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_RADIO)

#define RT_OID_802_11_QUERY_PHY_MODE          0x0D71010C
#define RT_OID_802_11_SET_PHY_MODE            (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_PHY_MODE)

#define RT_OID_802_11_QUERY_STA_CONFIG        0x0D710111
#define RT_OID_802_11_SET_STA_CONFIG          (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_STA_CONFIG)

//wpa counter measure test
#define RT_OID_802_11_QUERY_GEN_MIC_ERROR     0x0D710115
#define RT_OID_802_11_SET_GEN_MIC_ERROR       (OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_GEN_MIC_ERROR)

#define RT_OID_802_11_QUERY_CURRENT_CHANNEL_ID 0x0D710117

#define	RT_OID_802_11_RSSI_2				  0x0D710125
#define RT_OID_802_11_SET_TX_RATES            (OID_GET_SET_TOGGLE | 0x0D710127)
#define RT_OID_802_11_QUERY_IEEE80211H			0x0D710128
#define RT_OID_802_11_SET_IEEE80211H			(OID_GET_SET_TOGGLE | RT_OID_802_11_QUERY_IEEE80211H)

//#if WPA_SUPPLICANT_SUPPORT
#define	RT_ASSOC_EVENT_FLAG                         0x0101
#define	RT_DISASSOC_EVENT_FLAG                      0x0102
#define	RT_REQIE_EVENT_FLAG                         0x0103
#define	RT_RESPIE_EVENT_FLAG                        0x0104
#define	RT_ASSOCINFO_EVENT_FLAG                     0x0105
#define RT_PMKIDCAND_FLAG                           0x0106
//#endif

//
// IEEE 802.11 Structures and definitions
//
// new types for Media Specific Indications
#define NDIS_802_11_LENGTH_SSID         32
#define NDIS_802_11_LENGTH_RATES        8
#define NDIS_802_11_LENGTH_RATES_EX     16

typedef enum _NDIS_802_11_STATUS_TYPE
{
	Ndis802_11StatusType_Authentication,
	Ndis802_11StatusType_MediaStreamMode,
	Ndis802_11StatusType_PMKID_CandidateList,
	Ndis802_11StatusTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_STATUS_TYPE, *PNDIS_802_11_STATUS_TYPE;


typedef UCHAR   NDIS_802_11_MAC_ADDRESS[6];

typedef struct _NDIS_802_11_STATUS_INDICATION
{
    NDIS_802_11_STATUS_TYPE StatusType;
} NDIS_802_11_STATUS_INDICATION, *PNDIS_802_11_STATUS_INDICATION;

// mask for authentication/integrity fields
#define NDIS_802_11_AUTH_REQUEST_AUTH_FIELDS        0x0f

#define NDIS_802_11_AUTH_REQUEST_REAUTH             0x01
#define NDIS_802_11_AUTH_REQUEST_KEYUPDATE          0x02
#define NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR     0x06
#define NDIS_802_11_AUTH_REQUEST_GROUP_ERROR        0x0E

typedef struct _NDIS_802_11_AUTHENTICATION_REQUEST
{
    ULONG Length;            // Length of structure
    NDIS_802_11_MAC_ADDRESS Bssid;
    ULONG Flags;
} NDIS_802_11_AUTHENTICATION_REQUEST, *PNDIS_802_11_AUTHENTICATION_REQUEST;

// Added new types for OFDM 5G and 2.4G
typedef enum _NDIS_802_11_NETWORK_TYPE
{
    Ndis802_11FH,
    Ndis802_11DS,
    Ndis802_11OFDM5,
    Ndis802_11OFDM24,
    Ndis802_11Automode,
    Ndis802_11NetworkTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;

typedef struct PACKED _NDIS_802_11_NETWORK_TYPE_LIST
{
    ULONG                       NumberOfItems;  // in list below, at least 1
    NDIS_802_11_NETWORK_TYPE    NetworkType [1];
} NDIS_802_11_NETWORK_TYPE_LIST, *PNDIS_802_11_NETWORK_TYPE_LIST;

typedef enum _NDIS_802_11_POWER_MODE
{
    Ndis802_11PowerModeCAM,
    Ndis802_11PowerModeMAX_PSP,
    Ndis802_11PowerModeFast_PSP,
    Ndis802_11PowerModeMax      // not a real mode, defined as an upper bound
} NDIS_802_11_POWER_MODE, *PNDIS_802_11_POWER_MODE;

typedef ULONG   NDIS_802_11_TX_POWER_LEVEL; // in milliwatts

//
// Received Signal Strength Indication
//
typedef LONG    NDIS_802_11_RSSI;           // in dBm

typedef struct _NDIS_802_11_CONFIGURATION_FH
{
    ULONG           Length;            // Length of structure
    ULONG           HopPattern;        // As defined by 802.11, MSB set
    ULONG           HopSet;            // to one if non-802.11
    ULONG           DwellTime;         // units are Kusec
} NDIS_802_11_CONFIGURATION_FH, *PNDIS_802_11_CONFIGURATION_FH;

typedef struct _NDIS_802_11_CONFIGURATION
{
   ULONG                           Length;             // Length of structure
   ULONG                           BeaconPeriod;       // units are Kusec
   ULONG                           ATIMWindow;         // units are Kusec
   ULONG                           DSConfig;           // Frequency, units are kHz
   NDIS_802_11_CONFIGURATION_FH    FHConfig;
} NDIS_802_11_CONFIGURATION, *PNDIS_802_11_CONFIGURATION;

typedef struct _NDIS_802_11_STATISTICS
{
    ULONG           Length;             // Length of structure
    LARGE_INTEGER   TransmittedFragmentCount;
    LARGE_INTEGER   MulticastTransmittedFrameCount;
    LARGE_INTEGER   FailedCount;
    LARGE_INTEGER   RetryCount;
    LARGE_INTEGER   MultipleRetryCount;
    LARGE_INTEGER   RTSSuccessCount;
    LARGE_INTEGER   RTSFailureCount;
    LARGE_INTEGER   ACKFailureCount;
    LARGE_INTEGER   FrameDuplicateCount;
    LARGE_INTEGER   ReceivedFragmentCount;
    LARGE_INTEGER   MulticastReceivedFrameCount;
    LARGE_INTEGER   FCSErrorCount;
} NDIS_802_11_STATISTICS, *PNDIS_802_11_STATISTICS;

typedef  ULONG  NDIS_802_11_KEY_INDEX;
typedef ULONGLONG   NDIS_802_11_KEY_RSC;

// Key mapping keys require a BSSID
typedef struct _NDIS_802_11_KEY
{
    ULONG           Length;             // Length of this structure
    ULONG           KeyIndex;
    ULONG           KeyLength;          // length of key in bytes
    NDIS_802_11_MAC_ADDRESS BSSID;
    NDIS_802_11_KEY_RSC KeyRSC;
    UCHAR           KeyMaterial[1];     // variable length depending on above field
} NDIS_802_11_KEY, *PNDIS_802_11_KEY;

typedef struct _NDIS_802_11_REMOVE_KEY
{
    ULONG                   Length;        // Length of this structure
    ULONG                   KeyIndex;
    NDIS_802_11_MAC_ADDRESS BSSID;
} NDIS_802_11_REMOVE_KEY, *PNDIS_802_11_REMOVE_KEY;

typedef struct PACKED _NDIS_802_11_WEP
{
    ULONG     Length;        // Length of this structure
    ULONG     KeyIndex;      // 0 is the per-client key, 1-N are the global keys
    ULONG     KeyLength;     // length of key in bytes
    UCHAR     KeyMaterial[1];// variable length depending on above field
} NDIS_802_11_WEP, *PNDIS_802_11_WEP;


typedef enum _NDIS_802_11_NETWORK_INFRASTRUCTURE
{
    Ndis802_11IBSS,
    Ndis802_11Infrastructure,
    Ndis802_11AutoUnknown,
    Ndis802_11Monitor,
    Ndis802_11InfrastructureMax     // Not a real value, defined as upper bound
} NDIS_802_11_NETWORK_INFRASTRUCTURE, *PNDIS_802_11_NETWORK_INFRASTRUCTURE;

// PMKID Structures
typedef UCHAR   NDIS_802_11_PMKID_VALUE[16];

typedef struct _BSSID_INFO
{
	NDIS_802_11_MAC_ADDRESS BSSID;
	NDIS_802_11_PMKID_VALUE PMKID;
} BSSID_INFO, *PBSSID_INFO;

typedef struct _NDIS_802_11_PMKID
{
	ULONG Length;
	ULONG BSSIDInfoCount;
	BSSID_INFO BSSIDInfo[1];
} NDIS_802_11_PMKID, *PNDIS_802_11_PMKID;

//Added new types for PMKID Candidate lists.
typedef struct _PMKID_CANDIDATE {
	NDIS_802_11_MAC_ADDRESS BSSID;
	ULONG Flags;
} PMKID_CANDIDATE, *PPMKID_CANDIDATE;

typedef struct _NDIS_802_11_PMKID_CANDIDATE_LIST
{
	ULONG Version;       // Version of the structure
	ULONG NumCandidates; // No. of pmkid candidates
	PMKID_CANDIDATE CandidateList[1];
} NDIS_802_11_PMKID_CANDIDATE_LIST, *PNDIS_802_11_PMKID_CANDIDATE_LIST;

//Flags for PMKID Candidate list structure
#define NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED	0x01


// Add new authentication modes
typedef enum _NDIS_802_11_AUTHENTICATION_MODE
{
    Ndis802_11AuthModeOpen,
    Ndis802_11AuthModeShared,
    Ndis802_11AuthModeAutoSwitch,
    Ndis802_11AuthModeWPA,
    Ndis802_11AuthModeWPAPSK,
    Ndis802_11AuthModeWPANone,
    Ndis802_11AuthModeWPA2,
    Ndis802_11AuthModeWPA2PSK,
    Ndis802_11AuthModeMax           // Not a real mode, defined as upper bound
} NDIS_802_11_AUTHENTICATION_MODE, *PNDIS_802_11_AUTHENTICATION_MODE;

#define WPA1AKMBIT	    1
#define WPA2AKMBIT	    2
#define WPA1PSKAKMBIT   4
#define WPA2PSKAKMBIT   8

typedef UCHAR NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];        // Set of 8 data rates
typedef UCHAR NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];  // Set of 16 data rates

typedef struct PACKED _NDIS_802_11_SSID
{
    ULONG   SsidLength;         // length of SSID field below, in bytes; this can be zero.
    UCHAR   Ssid[NDIS_802_11_LENGTH_SSID];           // SSID information field
} NDIS_802_11_SSID, *PNDIS_802_11_SSID;


typedef struct PACKED _NDIS_WLAN_BSSID
{
    ULONG                               Length;     // Length of this structure
    NDIS_802_11_MAC_ADDRESS             MacAddress; // BSSID
    UCHAR                               Reserved[2];
    NDIS_802_11_SSID                    Ssid;       // SSID
    ULONG                               Privacy;    // WEP encryption requirement
    NDIS_802_11_RSSI                    Rssi;       // receive signal, strength in dBm
    NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
    NDIS_802_11_CONFIGURATION           Configuration;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
    NDIS_802_11_RATES                   SupportedRates;
} NDIS_WLAN_BSSID, *PNDIS_WLAN_BSSID;

typedef struct PACKED _NDIS_802_11_BSSID_LIST
{
    ULONG           NumberOfItems;      // in list below, at least 1
    NDIS_WLAN_BSSID Bssid[1];
} NDIS_802_11_BSSID_LIST, *PNDIS_802_11_BSSID_LIST;

// Added Capabilities, IELength and IEs for each BSSID
typedef struct PACKED _NDIS_WLAN_BSSID_EX
{
    ULONG                               Length;             // Length of this structure
    NDIS_802_11_MAC_ADDRESS             MacAddress;         // BSSID
    UCHAR                               Reserved[2];
    NDIS_802_11_SSID                    Ssid;               // SSID
    ULONG                               Privacy;            // WEP encryption requirement
    NDIS_802_11_RSSI                    Rssi;               // receive signal
                                                            // strength in dBm
    NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
    NDIS_802_11_CONFIGURATION           Configuration;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
    NDIS_802_11_RATES_EX                SupportedRates;
    ULONG                               IELength;
    UCHAR                               IEs[1];
} NDIS_WLAN_BSSID_EX, *PNDIS_WLAN_BSSID_EX;

typedef struct _NDIS_802_11_BSSID_LIST_EX
{
    ULONG                   NumberOfItems;      // in list below, at least 1
    NDIS_WLAN_BSSID_EX      Bssid[1];
} NDIS_802_11_BSSID_LIST_EX, *PNDIS_802_11_BSSID_LIST_EX;

typedef struct _NDIS_802_11_FIXED_IEs
{
    UCHAR Timestamp[8];
    USHORT BeaconInterval;
    USHORT Capabilities;
} NDIS_802_11_FIXED_IEs, *PNDIS_802_11_FIXED_IEs;

typedef struct _NDIS_802_11_VARIABLE_IEs
{
    UCHAR ElementID;
    UCHAR Length;    // Number of bytes in data field
    UCHAR data[1];
} NDIS_802_11_VARIABLE_IEs, *PNDIS_802_11_VARIABLE_IEs;

typedef  ULONG   NDIS_802_11_FRAGMENTATION_THRESHOLD;

typedef  ULONG   NDIS_802_11_RTS_THRESHOLD;

typedef  ULONG   NDIS_802_11_ANTENNA;

typedef enum _NDIS_802_11_PRIVACY_FILTER
{
    Ndis802_11PrivFilterAcceptAll,
    Ndis802_11PrivFilter8021xWEP
} NDIS_802_11_PRIVACY_FILTER, *PNDIS_802_11_PRIVACY_FILTER;

// Added new encryption types
// Also aliased typedef to new name
typedef enum _NDIS_802_11_WEP_STATUS
{
    Ndis802_11WEPEnabled,
    Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
    Ndis802_11WEPDisabled,
    Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
    Ndis802_11WEPKeyAbsent,
    Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
    Ndis802_11WEPNotSupported,
    Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
    Ndis802_11Encryption2Enabled,
    Ndis802_11Encryption2KeyAbsent,
    Ndis802_11Encryption3Enabled,
    Ndis802_11Encryption3KeyAbsent
} NDIS_802_11_WEP_STATUS, *PNDIS_802_11_WEP_STATUS,
  NDIS_802_11_ENCRYPTION_STATUS, *PNDIS_802_11_ENCRYPTION_STATUS;

typedef enum _NDIS_802_11_RELOAD_DEFAULTS
{
   Ndis802_11ReloadWEPKeys
} NDIS_802_11_RELOAD_DEFAULTS, *PNDIS_802_11_RELOAD_DEFAULTS;

#define NDIS_802_11_AI_REQFI_CAPABILITIES      1
#define NDIS_802_11_AI_REQFI_LISTENINTERVAL    2
#define NDIS_802_11_AI_REQFI_CURRENTAPADDRESS  4

#define NDIS_802_11_AI_RESFI_CAPABILITIES      1
#define NDIS_802_11_AI_RESFI_STATUSCODE        2
#define NDIS_802_11_AI_RESFI_ASSOCIATIONID     4

typedef struct _NDIS_802_11_AI_REQFI
{
    USHORT Capabilities;
    USHORT ListenInterval;
    NDIS_802_11_MAC_ADDRESS  CurrentAPAddress;
} NDIS_802_11_AI_REQFI, *PNDIS_802_11_AI_REQFI;

typedef struct _NDIS_802_11_AI_RESFI
{
    USHORT Capabilities;
    USHORT StatusCode;
    USHORT AssociationId;
} NDIS_802_11_AI_RESFI, *PNDIS_802_11_AI_RESFI;

typedef struct _NDIS_802_11_ASSOCIATION_INFORMATION
{
    ULONG                   Length;
    USHORT                  AvailableRequestFixedIEs;
    NDIS_802_11_AI_REQFI    RequestFixedIEs;
    ULONG                   RequestIELength;
    ULONG                   OffsetRequestIEs;
    USHORT                  AvailableResponseFixedIEs;
    NDIS_802_11_AI_RESFI    ResponseFixedIEs;
    ULONG                   ResponseIELength;
    ULONG                   OffsetResponseIEs;
} NDIS_802_11_ASSOCIATION_INFORMATION, *PNDIS_802_11_ASSOCIATION_INFORMATION;

typedef struct _NDIS_802_11_AUTHENTICATION_EVENT
{
    NDIS_802_11_STATUS_INDICATION       Status;
    NDIS_802_11_AUTHENTICATION_REQUEST  Request[1];
} NDIS_802_11_AUTHENTICATION_EVENT, *PNDIS_802_11_AUTHENTICATION_EVENT;

typedef struct _NDIS_802_11_TEST
{
    ULONG Length;
    ULONG Type;
    union
    {
        NDIS_802_11_AUTHENTICATION_EVENT AuthenticationEvent;
        NDIS_802_11_RSSI RssiTrigger;
    }tt;
} NDIS_802_11_TEST, *PNDIS_802_11_TEST;

typedef enum _RT_802_11_PREAMBLE {
    Rt802_11PreambleLong,
    Rt802_11PreambleShort,
    Rt802_11PreambleAuto
} RT_802_11_PREAMBLE, *PRT_802_11_PREAMBLE;

// 2005-03-08 match current RaConfig.
typedef enum _RT_802_11_PHY_MODE {
    PHY_11BG_MIXED,
    PHY_11B,
    PHY_11A,
    PHY_11ABG_MIXED,
    PHY_11G
} RT_802_11_PHY_MODE;

typedef enum _RT_802_11_ADHOC_MODE {
	ADHOC_11B,
	ADHOC_11BG_MIXED,
	ADHOC_11G,
	ADHOC_11A,
	ADHOC_11ABG_MIXED
} RT_802_11_ADHOC_MODE;

// put all proprietery for-query objects here to reduce # of Query_OID
typedef struct _RT_802_11_LINK_STATUS {
    ULONG   CurrTxRate;         // in units of 0.5Mbps
    ULONG   ChannelQuality;     // 0..100 %
    ULONG   TxByteCount;        // both ok and fail
    ULONG   RxByteCount;        // both ok and fail
} RT_802_11_LINK_STATUS, *PRT_802_11_LINK_STATUS;

// structure for query/set hardware register - MAC, BBP, RF register
typedef struct _RT_802_11_HARDWARE_REGISTER {
    ULONG   HardwareType;       // 0:MAC, 1:BBP, 2:RF register
    ULONG   Offset;             // Q/S register offset addr
    ULONG   Data;               // R/W data buffer
} RT_802_11_HARDWARE_REGISTER, *PRT_802_11_HARDWARE_REGISTER;

// structure to tune BBP R17 "RX AGC VGC init"
//typedef struct _RT_802_11_RX_AGC_VGC_TUNING {
//    UCHAR   FalseCcaLowerThreshold;  // 0-255, def 10
//    UCHAR   FalseCcaUpperThreshold;  // 0-255, def 100
//    UCHAR   VgcDelta;                // R17 +-= VgcDelta whenever flase CCA over UpprThreshold
//                                     // or lower than LowerThresholdupper threshold
//    UCHAR   VgcUpperBound;           // max value of R17
//} RT_802_11_RX_AGC_VGC_TUNING, *PRT_802_11_RX_AGC_VGC_TUNING;

// structure to query/set STA_CONFIG
typedef struct _RT_802_11_STA_CONFIG {
    ULONG   EnableTxBurst;      // 0-disable, 1-enable
    ULONG   EnableTurboRate;    // 0-disable, 1-enable 72/100mbps turbo rate
    ULONG   UseBGProtection;    // 0-AUTO, 1-always ON, 2-always OFF
    ULONG   UseShortSlotTime;   // 0-no use, 1-use 9-us short slot time when applicable
    ULONG   AdhocMode; 			// 0-11b rates only (WIFI spec), 1 - b/g mixed, 2 - g only
    ULONG   HwRadioStatus;      // 0-OFF, 1-ON, default is 1, Read-Only
    ULONG   Rsv1;               // must be 0
    ULONG   SystemErrorBitmap;  // ignore upon SET, return system error upon QUERY
} RT_802_11_STA_CONFIG, *PRT_802_11_STA_CONFIG;

typedef struct _RT_VERSION_INFO{
    UCHAR       DriverVersionW;
    UCHAR       DriverVersionX;
    UCHAR       DriverVersionY;
    UCHAR       DriverVersionZ;
    UINT        DriverBuildYear;
    UINT        DriverBuildMonth;
    UINT        DriverBuildDay;
} RT_VERSION_INFO, *PRT_VERSION_INFO;

//
// Defines the state of the LAN media
//
typedef enum _NDIS_MEDIA_STATE
{
    NdisMediaStateConnected,
    NdisMediaStateDisconnected
} NDIS_MEDIA_STATE, *PNDIS_MEDIA_STATE;



// Definition of extra information code
#define	GENERAL_LINK_UP			0x0			// Link is Up
#define	GENERAL_LINK_DOWN		0x1			// Link is Down
#define	HW_RADIO_OFF			0x2			// Hardware radio off
#define	SW_RADIO_OFF			0x3			// Software radio off
#define	AUTH_FAIL				0x4			// Open authentication fail
#define	AUTH_FAIL_KEYS			0x5			// Shared authentication fail
#define	ASSOC_FAIL				0x6			// Association failed
#define	EAP_MIC_FAILURE			0x7			// Deauthencation because MIC failure
#define	EAP_4WAY_TIMEOUT		0x8			// Deauthencation on 4-way handshake timeout
#define	EAP_GROUP_KEY_TIMEOUT	0x9			// Deauthencation on group key handshake timeout
#define	EAP_SUCCESS				0xa			// EAP succeed
#define	DETECT_RADAR_SIGNAL		0xb         // Radar signal occur in current channel

#define EXTRA_INFO_CLEAR		0xffffffff

#endif // _OID_H_
