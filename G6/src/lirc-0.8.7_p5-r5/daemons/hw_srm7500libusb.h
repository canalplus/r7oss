#ifndef __HW_SRM7500LIBUSB_H__
#define __HW_SRM7500LIBUSB_H__

//USB stuff
#define PACKET_SIZE_MAX 64

//packet types
#define MCPS_DATA_indication      0x42
#define MLME_ASSOCIATE_indication 0x47
#define MLME_ASSOCIATE_response   0x48
#define MLME_ORPHAN_indication    0x52
#define MLME_ORPHAN_response      0x53
#define MLME_RESET_request        0x54
#define MLME_RESET_confirm        0x55
#define MLME_COMM_STATUS_indication  0x5a
#define MLME_SET_request          0x5b
#define MLME_SET_confirm          0x5c
#define MLME_START_request        0x5d
#define MLME_START_confirm        0x5e

#define PIB_ATTR_macCoordShort_Address   0x4b
#define PIB_ATTR_macExtendedAddress      0x6f
#define PIB_ATTR_macPANId                0x50
#define PIB_ATTR_macShortAddress         0x53
#define PIB_ATTR_macAssociation_Permit   0x41
#define PIB_ATTR_macRxOnWhenIdle         0x52
#define PIB_ATTR_macBeaconPayload_Length 0x46
#define PIB_ATTR_macBeaconPayload        0x45

#define MLME_TRUE  1
#define MLME_FALSE 0

#define SRM7500_KEY_DOWN 1
#define SRM7500_KEY_REPEAT 2
#define SRM7500_KEY_UP 3

#pragma pack(1)
typedef struct {
	u_int8_t   time[4];
	u_int8_t   length;
	u_int8_t   type;
	union {
		struct {
			u_int8_t  SrcAddrMode;
			u_int8_t  SrcPANId[2];
			u_int8_t  SrcAddr[2];   //Note: only valid for SrcAddrMode==0x02
			u_int8_t  DstAddrMode;
			u_int8_t  DstPANId[2];
			u_int8_t  DstAddr[2];   //Note: only valid for DstAddrMode==0x02
			u_int8_t  msduLength;   // <= aMaxMACFrameSize, so it is most probably one byte
			u_int8_t  data[PACKET_SIZE_MAX-6-10];
		} zig;
		u_int8_t data[PACKET_SIZE_MAX-6];
	};
} philipsrf_incoming_t;

typedef struct {
	u_int8_t   length;
	u_int8_t   type;
	union {
		u_int8_t data[PACKET_SIZE_MAX-2];
	};
} philipsrf_outgoing_t;
#pragma pack()
#endif
