/******************************************************************************
COPYRIGHT (C) STMicroelectronics 2008.  All rights reserved.
******************************************************************************/

/**
   @file   pti_tp_api.h
   @brief  (a.k.a. exported_api.h) This file defines the interface between TP and the Host.

   This file implements the typedefs and structures used for initialisation and status when the
   transport processor is running.

   @warning THIS FILE IS COPIED FROM THE TANGO_TP BUILD AREA WHEN THE TP CODE IS BUILT.  YOU SHOULD
   NOT CHANGE THE COPY OF THE FILE FOUND IN THE HOST DRIVER.

 */

#ifndef _PTI_TP_API_H_
#define _PTI_TP_API_H_

/* RULES FOR VERSION NUMBERS...
 *                                              AABBCCDD                       A B  C D
 * 1. The version number is in BCD pairs:  e.g. 01011500 represents TP version 1.1.15.0

 * 2. The first 3 pairs (A,B,C) are the main release version id, the last pair (D) is just used
 *    for patches done AFTER a release is made.
 *
 * 3. The last pair (D) the Patch field, should be reset to zero for any offical full release.
 *
 * Only the first 3 char pairs are checked in the driver for TP / HOST driver compatibility.
 * Only the first 8 chars are stored within the interface structure ".Version"
 *
 */

#define STPTI_TP_VERSION_ID                                          "01022501_TP_VERSION"
/*                                                                     ^ ^ ^ ^
    MAJOR           (BCD value) ---------------------------------------' | | |
    Minor           (BCD value) -----------------------------------------' | |
    SubMinor        (BCD value) -------------------------------------------' |
                                                                             |
    Bug fix/ patch  (only increase if patching an existing release) ---------'

 **** THIS VERSION NUMBER IS INDEPENDENT OF THE STPTI5 RELEASE NUMBER ***

*/


/*
 * The following string is printed to the transport engine debug buffer
 * when the TP starts
 */

/*
 * DON'T MODIFY the below macro.
 * It is automatically updated by the Makefile, when the f/w is copied copied
 * into the driver
 */
#define STPTI_FIRMWARE_SCM_REVISION "d216f079f9b733fb534ab538ef170d6b043cf636"


/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* SDK2 includes */
#ifdef __STXP70CC__
/*
 * SDK2 stddefs.h not available for STxP70,
 * so we define them to STxP70 native defines
 */
#include <inttypes.h>
typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
#else
#include "stddefs.h"
#endif

/* Includes from API level */

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

/* wrap up gcc attributes into something a little more explanatory */
#define SIZE_AS_U8      mode(QI)
#define SIZE_AS_U16     mode(HI)
#define SIZE_AS_U32     mode(SI)

/* Maximum number of slots which can be chained together */
#define MAX_CHAINED_SLOTS 8
#define MAX_NUM_VDEVICES 16
#define MAX_NUM_STARTCODE_PER_PKT 16
#define MAX_STARTCODE_CONTEXT_BYTES 4


/* Hardware Limitation Workaround List */
#define GNBvd50868_SLOW_RATE_STREAM_WA             /* Software DDTS is GNBvd81614 - improve channel change time by timing out on slow rate pid streams */



/* should be sync'd with the host CPU */
typedef struct
{
    volatile U32 channel;
    volatile U32 buffer_base;
    volatile U32 buffer_size_in_pkts;
    volatile U32 packet_len;
} stptiTP_Live_t;


typedef enum {
    INJECTION_EXCLUDES_TAGS      = 0x00000001,
    INJECTION_INCLUDES_DLNA_TAGS = 0x00000002,
    DISCARD_PREVIOUS_PARTIAL_PKT = 0x80000000,
} stptiTP_InjectionFlags_t;


#define PLAYBACK_TAG_IS_VDEVICE_INDEX 0x8000
typedef struct
{
    volatile U32 channel;                   /* Keep first for checksum */
    volatile U32 buffer_base_pb;
    volatile U32 num_of_bytes_pb;
    volatile U32 next_node;
    volatile U32 packet_len_pb;
    volatile stptiTP_InjectionFlags_t injection_flags;
    volatile U32 tag_header0;
    volatile U32 tag_header1;
    volatile U16 ctrl;                      /* blu-ray only */
    volatile U16 key_index;                 /* blu-ray only */
    volatile U32 checksum;                  /* Keep Last */
} stptiTP_Playback_t;

/* Make sure this matches the packet_flags structure on the TP */
/* This structure MUST match the packet_flags_t in TP file TP_SC_Com.h */
typedef enum
{
    /* Packet Information Bits (populated by header_processing) */
    /* will be found in ob->buffer.flags */

    /* These are flags that the pkt has a particular property/fault, they do not all result in EVENTS (see comments) */

    STATUS_BLK_CC_ERROR                     = 0x00000001,       /* (PER PKT EVENT) set if CC error */
    STATUS_BLK_INVALID_PARAMETER            = 0x00000002,       /* (PER PKT EVENT) set if badly formatted transport packet */
    STATUS_BLK_TRANSPORT_ERROR_FLAG         = 0x00000004,       /* (PER PKT EVENT) set if Transport error indicator in 4byte DVB Transport Header */
    STATUS_BLK_PUSI_FLAG                    = 0x00000008,       /* set if PUSI in 4byte DVB Transport Header */

    STATUS_BLK_SCRAMBLE_TOEVEN              = 0x00000010,       /* set if clear or odd scrambling to even */
    STATUS_BLK_SCRAMBLE_TOODD               = 0x00000020,       /* set if clear or even scrambling to even */
    STATUS_BLK_SCRAMBLE_TOCLEAR             = 0x00000040,       /* (PER SLOT EVENT) set if odd or even scrambling to clear */
    STATUS_BLK_CLEAR_TOSCRAMBLE             = 0x00000080,       /* (PER SLOT EVENT) set if clear to even or odd scrambling */

    STATUS_BLK_PES_PTS                      = 0x00000100,       /* set if PES PTS present in PES header */

    /* DVB only */
    STATUS_BLK_SECONDARY_PID_DISCARDED      = 0x00000400,       /* set if secondary pid feature discards a secondary packet */
    STATUS_BLK_SECONDARY_PID_CC_ERROR       = 0x00000800,       /* set if secondary pid feature see a CC error a secondary packet */
    STATUS_BLK_PCR_RECEIVED                 = 0x00001000,       /* (PER SLOT EVENT) set if PCR in AF */
    STATUS_BLK_DISCONTINUITY_FLAG           = 0x00002000,       /* set if discontinuity flag set in AF */

    STATUS_BLK_MARKER_ERROR                 = 0x00004000,       /* effectively a (PER_SLOT_EVENT) */

    /* Slot Processing Events (populated by packet_processing*.c) */
    /* these will NOT be found in ob->buffer.flags, only in StatusBlk_WR_p->Flags */

    /* These are used in status blocks along with the above packet information bits. */

    STATUS_BLK_PES_ERROR                    = 0x00010000,       /* (PER SLOT EVENT) error when parsing PES */
    STATUS_BLK_SECTION_CRC_DISCARD          = 0x00020000,       /* (PER SLOT EVENT) CRC error when parsing section */
    STATUS_BLK_STATUS_BLK_OVERFLOW          = 0x00040000,       /* (PER SLOT EVENT) status block overflow (not servicing interrupts quickly enough) */
    STATUS_BLK_BUFFER_OVERFLOW              = 0x00080000,       /* (PER SLOT EVENT) a buffer overflow */
    STATUS_BLK_DATA_ENTRY_COMPLETE          = 0x00100000,       /* data entry packet output */
    STATUS_BLK_FIRST_RECORD_PKT             = 0x00200000,       /* First Recorded Pkt */

} stptiTP_StatusBlk_Flags_t;



/* This is an important section...  It sets the properties for each of the above events.  It is used
   by the driver for setting up the default masks for the events.

   IF YOUR EVENT IS NOT IN THE DEFINES BELOW THERE IS A HIGH CHANCE IT WILL GET MASKED!!!!
   (the defines below control the default event masking setup by the host driver). */

/* Indicates which events must be slot enabled to be active */
#define EVENT_MASK_SLOT_ENABLED_EVENTS      ( STATUS_BLK_SCRAMBLE_TOCLEAR | STATUS_BLK_CLEAR_TOSCRAMBLE | STATUS_BLK_PCR_RECEIVED )

/* Indicates which events must be vdevice enabled to be active */
#define EVENT_MASK_VDEVICE_ENABLED_EVENTS   ( STATUS_BLK_CC_ERROR | STATUS_BLK_INVALID_PARAMETER | STATUS_BLK_TRANSPORT_ERROR_FLAG | \
                                              STATUS_BLK_SCRAMBLE_TOCLEAR | STATUS_BLK_CLEAR_TOSCRAMBLE | \
                                              STATUS_BLK_PES_ERROR | STATUS_BLK_SECTION_CRC_DISCARD | \
                                              STATUS_BLK_STATUS_BLK_OVERFLOW | STATUS_BLK_BUFFER_OVERFLOW | \
                                              STATUS_BLK_DATA_ENTRY_COMPLETE | STATUS_BLK_MARKER_ERROR | STATUS_BLK_SECONDARY_PID_DISCARDED | STATUS_BLK_SECONDARY_PID_CC_ERROR )

/* Events that should be raised only once per packet (i.e. only the first slot and not for every chained slot) */
#define EVENT_MASK_ONCE_PER_PKT             ( STATUS_BLK_CC_ERROR | STATUS_BLK_INVALID_PARAMETER | STATUS_BLK_TRANSPORT_ERROR_FLAG | STATUS_BLK_SECONDARY_PID_DISCARDED | STATUS_BLK_SECONDARY_PID_CC_ERROR )

/* Indicates which events should NOT be disabled when raised */
#define EVENT_MASK_PERSISTENT               ( STATUS_BLK_SCRAMBLE_TOCLEAR | STATUS_BLK_CLEAR_TOSCRAMBLE | \
                                              STATUS_BLK_PCR_RECEIVED | STATUS_BLK_DATA_ENTRY_COMPLETE | STATUS_BLK_MARKER_ERROR )

/* A convenient define to list all the events */
#define EVENT_MASK_ALL_EVENTS               ( EVENT_MASK_SLOT_ENABLED_EVENTS | EVENT_MASK_VDEVICE_ENABLED_EVENTS )



typedef enum
{
    /* Found in DVB AF flags */
    TP_INDEX_DISCONTINUITY_INDICATOR    = 0x80,       /* MSByte matches AF flags */
    TP_INDEX_RANDOM_ACCESS_INDICATOR    = 0x40,       /* MSByte matches AF flags */
    TP_INDEX_PRIORITY_INDICATOR         = 0x20,       /* MSByte matches AF flags */
    TP_INDEX_PCR_FLAG                   = 0x10,       /* MSByte matches AF flags */
    TP_INDEX_OPCR_FLAG                  = 0x08,       /* MSByte matches AF flags */
    TP_INDEX_SPLICING_POINT_FLAG        = 0x04,       /* MSByte matches AF flags */
    TP_INDEX_PRIVATE_DATA_FLAG          = 0x02,       /* MSByte matches AF flags */
    TP_INDEX_ADAPTION_EXTENSION_FLAG    = 0x01,       /* MSByte matches AF flags */
    /* The LSByte is reserved, in case there is a future requirement to index on AF extension flags */

} stptiTP_Indexer_AdditionalTransportIndexMask_t __attribute__ ((SIZE_AS_U8));


#define MAX_FILTERS_PER_VDEVICE            64
typedef struct
{
    /* Need to force alignment to 8 byte boundary because SH4 compiler doesn't automatically do this */
    volatile U32   FilterCAMOffset;
    volatile U32   Spare;               /* present for alignment reasons */
    volatile U64   __attribute__ ((aligned (8))) OneShotFilterMask;
    volatile U64   __attribute__ ((aligned (8))) ForceCRCFilterMask;
    volatile U64   __attribute__ ((aligned (8))) PnmmNotLmmMask;     /* bit mask to indicate if a filter requires long or PNMM CAM useage */
    volatile U8    __attribute__ ((aligned (8))) NotMatchValueA[MAX_FILTERS_PER_VDEVICE];                               /* h/w requires it to be U64 aligned (1byte per filter) */
    volatile U64   __attribute__ ((aligned (8))) NotMatchControlA[(MAX_FILTERS_PER_VDEVICE*2)/(8*sizeof(U64))];         /* h/w requires it to be U64 aligned (2bits per filter) */
} stptiTP_CamTable_t;

typedef struct
{
    volatile stptiTP_StatusBlk_Flags_t Flags;

    volatile U16 SlotIndex;
    volatile U16 DMAIndex;

    volatile U32 ArrivalTime0;
    volatile U32 ArrivalTime1;

    volatile U32 Pcr0;
    volatile U32 Pcr1;

    volatile U32 BufferPacketNumber;

    volatile U8  ExpectedCC:4;
    volatile U8  ReceivedCC:4;
    volatile U8  vDevice;
    volatile U16 DataEntrySlotIndex;

    volatile U8  MarkerType;
    volatile U32 MarkerID0;
    volatile U32 MarkerID1;
}stptiTP_StatusBlk_t;

#define TP_SIGNALLING_QUEUE_LENGTH 256              /* (optimisation) must be a power of 2, see src/callback.c::CheckForSignal */

/* SYNC_TP_*                       0x0000-0xFFFD are for DMA_WAIT on a specific buffer see main.c */
#define SYNC_TP_SLOT_WAIT          0xFFFE
#define SYNC_TP_SYNCHRONISED       0xFFFF

#define SYNC_TP_ENTRY_DONE          0x00000000
#define SYNC_TP_ENTRY_WAITING       0xC0000000
#define SYNC_TP_ENTRY_CLEAR_SLOT    0x80000000
#define SYNC_TP_ENTRY_CLEAR_VDEVICE 0x40000000
#define SYNC_TP_ENTRY_DATA_MASK     0x0000FFFF

typedef struct
{
    volatile U32                    StatusBlk_WR_index;     /* Next Block to write to (Index into the StatusBlk buffer is used to avoid pointer conversion) */
    volatile U32                    StatusBlk_RD_index;     /* Last Block Read (Index into the StatusBlk buffer is used to avoid pointer conversion) */
    volatile U8                     SignallingQ_RD_index;
    volatile U8                     SignallingQ_WR_index;
    volatile U16                    SyncTP;                 /* Used for DMA_WAIT (a dma being IDLE) and SLOT_WAIT */
    volatile U32                    Spare;
    volatile U32                    SyncTPClearEntry;
    volatile U8                     ResetIdleCounters;      /* if not 0, IdleCount, NotIdleCount are cleared */
    volatile U8                     Spare2[3];
    volatile U32                    DebugCLP;               /* Consecutive Live Packets - a bandwidth indicator */
    volatile U32                    IdleCount;              /* cycles in idle */
    volatile U32                    NotIdleCount;           /* cycles not idle */
    volatile U32                    AvgIdleCount;       /* Average cycles in idle per 10k loops */
    volatile U32                    AvgNotIdleCount;    /* Average cycles not idle per 10k loops */
    volatile U32                    StartFlushSTFE;
    volatile U32                    StopFlushSTFE;
    volatile U32                    SWLeakyPIDChannels;     /* Set by host to indicate if pids are being collected on an input channel */
    volatile U32                    PacketsPreHeaderProcessing;
    volatile U32                    PacketsWithSyncError;
    volatile U32                    PacketsPushedToSP;
    volatile U32                    PushFailuresToSP;
    volatile U32                    PacketsPulledFromSP;
    volatile U32                    pid_table_base;

    volatile U8                     SignallingQ[TP_SIGNALLING_QUEUE_LENGTH];

} stptiTP_pDeviceInfo_t;


typedef enum
{
    PACKET_FORMAT_DVB               = 0x0001,
    PACKET_FORMAT_BD                = 0x0004,
} stptiTP_vDeviceMode_t __attribute__ ((SIZE_AS_U16));              /* stptiTP_vDeviceMode_t */


typedef enum
{
    VDEVICE_ENABLE_RAW_SLOT_INDEXES = 0x00000001,
    VDEVICE_DISABLE_RAW_SLOT_DMA    = 0x00000002,
    VDEVICE_DISCARD_DUPLICATE_PKTS  = 0x00000004
} stptiTP_vDeviceFlags_t;


/* bottom 4 bits are the CC */
/* bits 7&8 are the scrambling control bits for TS */
#define STATUS_TS_LEVEL_SC_OFFSET 6
/* bits 9&10 are the scrambling control bits for PES */
#define STATUS_PES_LEVEL_SC_OFFSET 8
typedef enum
{
    STATUS_SLOT_STATE_CLEAR         = 0x0000,
    STATUS_CC_MASK                  = 0x000F,
    STATUS_CC_VALID                 = 0x0010,
    STATUS_CC_DUPLICATE_REJECTED    = 0x0020,
    STATUS_TS_LEVEL_SC_MASK         = 0x00C0,           /* note STATUS_TS_LEVEL_SC_OFFSET */
    STATUS_PES_LEVEL_SC_MASK        = 0x0300,           /* note STATUS_PES_LEVEL_SC_OFFSET */
    STATUS_TS_LEVEL_SC_NOT_SYNCED   = 0x0400,
    STATUS_SECONDARY_PID_SEC_RXED   = 0x1000,
    STATUS_SECONDARY_PID_DISCARD_P  = 0x2000,
    STATUS_SECONDARY_PID_MASK       = 0x3000,
    STATUS_CC_RESET_MASK            = 0xFFC0            /* used to clear only CC state in the TP */
} stptiTP_SlotState_t __attribute__ ((SIZE_AS_U16));                  /* stptiTP_SlotState_t */

typedef enum
{
    SLOTTYPE_NULL                   = 0x00,
    SLOTTYPE_SECTION                = 0x01,
    SLOTTYPE_PES                    = 0x02,
    SLOTTYPE_RAW                    = 0x03,
    SLOTTYPE_PCR                    = 0x04,
    SLOTTYPE_EMM                    = 0x05,
    SLOTTYPE_ECM                    = 0x06,
    SLOTTYPE_VIDEO_ES               = 0x07,
    SLOTTYPE_AUDIO_ES               = 0x08,
} stptiTP_SlotMode_t  __attribute__ ((SIZE_AS_U8));                   /* stptiTP_SlotMode_t */

#define DNLA_TS_TAG_SIZE (4)

typedef enum
{
    SLOT_FLAGS_SUPPRESS_CC_CHECK      = 0x0001,
    SLOT_FLAGS_SUPPRESS_METADATA      = 0x0002,
    SLOT_FLAGS_NO_WINDBACK_ON_ERROR   = 0x0004,                       /* Only affects PES slots, ES doesn't wind back, SECTIONs winds back depending on filter setting */
                                     /* 0x0008 - is available */
    SLOT_FLAGS_OUTPUT_SCR_NODE        = 0x0010,                       /* Only affects RAW */
                                     /* 0x0020 - is available */
    SLOT_FLAGS_PREFIX_DNLA_TS_TAG     = 0x0040,                       /* Only affects RAW */
    SLOT_FLAGS_ENTRY_REPLACEMENT      = 0x0080,                       /* Only affects RAW */
    SLOT_FLAGS_ENTRY_INSERTION        = 0x0100,                       /* Only affects RAW */
    SLOT_FLAGS_SW_CD_FIFO             = 0x0200,                       /* Only affects PES */
    SLOT_FLAGS_CC_FIXUP               = 0x0400,                       /* Only affects RAW */
    SLOT_FLAGS_COUNT_METADATA         = 0x0800                        /* Only affects ECM/SECTION slots with a RAW slot in the slot chain */
} stptiTP_SlotFlags_t  __attribute__ ((SIZE_AS_U16));

typedef enum
{
    DATA_ENTRY_STATE_EMPTY      = 0x0,
    DATA_ENTRY_STATE_LOADED     = 0x1,
    DATA_ENTRY_STATE_PROCESSING = 0x2,
    DATA_ENTRY_STATE_COMPLETE   = 0x4,
} stptiTP_DataEntryState_t __attribute__ ((SIZE_AS_U8));

#define TP_DATA_ENTRY_REPEAT_FOREVER (0x8000)

typedef struct
{
    volatile U32                       EntryAddress;                 /* External address to load replacement/insertion data from */
    volatile stptiTP_DataEntryState_t  EntryState;
    volatile U8                        EntryFromByte;
    volatile U16                       EntrySize;
    volatile U16                       EntrySlotIndex;
    volatile U16                       EntryCount;
} stptiTP_DataEntryInfo_t;

typedef enum
{
    INDEXER_STATE_SPLIT_START_CODE_CONTEXT = 0x00000001,
} stptiTP_IndexerState_t;

typedef struct
{
    volatile stptiTP_IndexerState_t indexer_state;
    volatile U8 index_scratch_buffer_plus1;
    volatile U8 scd_state;
    U16  ArrivalTime1;                           /* Upper part of the arrival time */
    U32  ArrivalTime0;                           /* Bottom half of the arrival time */
    U32  write_offset;                           /* write offset store between packets */
    U32  buffer_unit_count;                      /* Buffer Packet Count */
} stptiTP_IndexerContext_t;

typedef enum
{
    RAW_SLOT_INFO_CC_MASK           = 0x0F,
    RAW_SLOT_INFO_CC_FIXUP          = 0x80
} stptiTP_RawSlotCC_t __attribute__ ((SIZE_AS_U8));                  /* stptiTP_RawSlotCC_t */

typedef struct
{
    volatile U8                      CorruptionOffset;               /* =0 means no corruption */
    volatile U8                      CorruptionValue;
    volatile U8                      SCRemappingBits;                /* 4 pairs of bits for each TP SC bit value. B1:0 SC replacement when SC=0, B3:2 SC replacement when SC=1 ... */
    volatile U8                      CCFixup;                        /* Holds last output CC when Fixup used */
    volatile U16                     RemapPid;
    volatile stptiTP_DataEntryInfo_t DataEntry;
    stptiTP_IndexerContext_t         IndexerContext;
} stptiTP_RawSlotInfo_t;


/* align to save a multiply */
typedef struct
{
    volatile U32  __attribute__ ((aligned (16))) input_packet_count;
    volatile U32  transport_sync_error_count;           /* number of packets discard due to bad sync byte */
    volatile U32  transport_error_count;                /* number of packets with transport error indicator set */
    volatile U32  pid_mismatch_count;                   /* total pid mismatch count */
    volatile U32  cc_error_count;                       /* total countinuity count errors */
    volatile U32  buffer_overflow_count;                /* count number of buffer overflows discard data case (overwrite not counted) */
    volatile U32  pes_header_error;                      /* pes header error due to wrong descambling by SC */
    volatile U16  stream_tag;
    volatile U16  pid_filter_base;                      /* Offset into pidLookupTable at which to start search */
    volatile U16  pid_filter_size;                      /* Number of pids in table to be searched */
    volatile stptiTP_vDeviceMode_t mode;                /* Actually more general than just section parameters */
    volatile U32  STCWord0;                             /* Arrival time of the latest packet */
    volatile U32  STCWord1;                             /* Arrival time of the latest packet */
    volatile U32  event_mask;                           /* Event interrupt enables */
    volatile stptiTP_vDeviceFlags_t flags;

    volatile U16  wildcard_slot_index;
    volatile U16  use_tcu_timestamp;                    /* If this is positive then each packet is timestamped with the TCU value */

    volatile stptiTP_DataEntryInfo_t DataEntry;
} stptiTP_vDeviceInfo_t;

/* STARTCODE DETECTION AND ES EXTRACTION DEFINITIONS */

typedef enum
{
    HEADER_DAMAGED   = 0x0,
    HEADER_DONE      = 0x1,
    HEADER_FILLING   = 0x2,             /* used when we still haven't got 9 bytes in the buffer */
    HEADER_SKIPPING  = 0x4              /* used when we are skipping over bytes to get to the ES data */
} stptiTP_PES_ES_Stage_t;

typedef struct
{
    volatile stptiTP_PES_ES_Stage_t stage;
    volatile U32 valid;                 /* number of bytes valid in the array below, or the number to skip */
    volatile U8 state;                  /* state stored from the SCD hardware */
} stptiTP_SCDHeader_t;

/* SECTION FILTER STATE EXTRACTION DEFINITIONS */

typedef enum
{
    SECTION_UNLOCKED    = 0x00,
    SECTION_LOCKED      = 0x01,
    SECTION_HEADERACC   = 0x02,
    SECTION_SAVING      = 0x03,
    SECTION_DUMPING     = 0x04,
    SECTION_CLOSING     = 0x05,
    SECTION_STATE_MASK  = 0x0F,

    SECTION_ECM_IGNORE_ODD  = 0x10,
    SECTION_ECM_IGNORE_EVEN = 0x20,
    SECTION_ECM_LOCKS_MASK  = 0x30

} stptiTP_SectionStage_t __attribute__ ((SIZE_AS_U8));

/* Keep as bitwise values as LONG and PNMM modes can be mixed now */
typedef enum
{
    SF_MODE_SHORT                   = 0x00,
    SF_MODE_LONG                    = 0x01,
    SF_MODE_PNMM                    = 0x02,
    SF_MODE_INVALID                 = 0x03,
    SF_MODE_TINY                    = 0x04,    /* a virtual filtering mode */
    SF_MODE_PROPRIETARY             = 0x08,    /* a virtual filtering mode */
    SF_MODE_NONE                    = 0xFF
} stptiTP_SectionParams_t __attribute__ ((SIZE_AS_U8));

typedef enum
{
    CRC_PLUS_MPT        = 0x0,
    NO_CRC              = 0x1,
    CRC                 = 0x2,
    CHECKSUM            = 0x4
} stptiTP_CRC_FrameMode_t __attribute__ ((SIZE_AS_U8));

typedef enum
{
    DISCARD_ON_CRC         = 0x1,
    DISCARD_DUPLICATE_TIDS = 0x2
} stptiTP_FilterFlags_t __attribute__ ((SIZE_AS_U8));

typedef struct
{
    union {
        volatile U64 __attribute__ ((aligned (8))) filters_associated;
        struct {
            volatile U16 FilterData;
            volatile U16 FilterMask;
            volatile U16 FilterAnyMatchMask;
            U16 spare;
        } tiny_filter;                                              /* (Header XOR FilterData) AND FilterMask == 0  ...AND...  ( FilterBitMapMask==0 OR Header AND FilterAnyMatchMask != 0 ) */
        volatile U32 proprietary_filter_index;
    } u;

    volatile stptiTP_SectionParams_t  section_params;               /* host written */
    volatile stptiTP_FilterFlags_t filter_flags;                    /* host written */
    volatile stptiTP_SectionStage_t stage;                          /* tp written (maybe reset by host) */
    stptiTP_CRC_FrameMode_t crc_mode;                               /* tp written */

    unsigned char header[18];                                       /* (when parsing a split section header) headers are 8 or 16 byte + 2 byte which is max of 18bytes (TP only, no need to make volatile) */
    U16 valid;                                                      /* number of bytes left in the section */

    union {
        U32 crc32;
        U32 checksum;
    } edc;

    U32 spare;
} stptiTP_SectionInfo_t;

typedef struct
{
    volatile U32 filter_type;
    volatile U32 filter_definition_bytes[19];
} stptiTP_ProprietaryFilter_t;

typedef enum
{
    PES_STATE_IDLE            = 0x0,
    PES_STATE_BUILDING_HEADER = 0x1,
    PES_STATE_BUILDING_NONDET = 0x2,
    PES_STATE_BUILDING_DET    = 0x3
} stptiTP_PesState_t __attribute__ ((SIZE_AS_U8));

typedef struct
{
    volatile stptiTP_PesState_t pes_state;
    volatile U16                pes_remain_len;
    volatile U8                 pes_marker_index;                   /* Used to store the index into a buffer array for any saved markers - 0xFF == none */
    volatile U8                 pes_header[6];                      /* Used to store the start of a split PES header */
    volatile U8                 pes_header_size;                    /* Used to store the number of split header bytes storesd */
    volatile U8                 pes_streamid_filterdata;
}stptiTP_PesFsm_t;

/* For Primary PID Slot this holds...    B15 set(1),   B14 clear(0), B12:10 Secondary PID Mode
   For Secondary PID Slot this holds...  B15 clear(0), B14 set(1),   B12:0 PID of Primary */
typedef enum
{
    SECONDARY_PID_SUBSTITUTION_MODE  = 0x0400,
    SECONDARY_PID_INSERTION_MODE     = 0x0800,
    SECONDARY_PID_INSERT_DELETE_MODE = 0x1000,
    SECONDARY_PID_MODE_MASK          = 0x1C00,
    SECONDARY_PID_SECONDARY_SLOT     = 0x4000,
    SECONDARY_PID_PRIMARY_SLOT       = 0x8000,
    SECONDARY_PID_ENABLED_MASK       = 0xC000
} stptiTP_SecondaryPidMasks_t __attribute__ ((SIZE_AS_U16));

/* Be very careful when updating the overal size of stptiTP_SlotInfo_t, we need to keep the pointer
   arithmetic simple when indexing SlotInfo (i.e. ideally keep the size a 2 power) */
typedef struct
{
    volatile stptiTP_SlotState_t         slot_state;                  /* TP writeable only (header_processing ONLY - only valid for the head of a slot chain!) */
    volatile U8                          remaining_pes_header_length; /* TP writeable, HOST writeable only on slot setup                                       */
    volatile stptiTP_SlotMode_t          slot_mode;                   /* HOST writeable only (but only on slot setup)                                          */
    volatile stptiTP_SecondaryPidMasks_t secondary_pid_info;          /* HOST writeable only                                                                   */
    volatile stptiTP_SlotFlags_t         slot_flags;                  /* HOST writeable only                                                                   */
    volatile U16                         next_slot;                   /* 0xFFFF marks the end of the list                                                      */
    volatile U16                         key_index;                   /* Key to be passed to SP                                                                */
    volatile stptiTP_StatusBlk_Flags_t   event_mask;                  /* used to store the events to raise to the host                                         */
    volatile U16                         indexer;
    volatile U16                         dma_record;                  /* number of the DMA record to use for the transfer                                      */
    volatile U32                         packet_count;                /* a count of pid matched tp packets processed by this slot                              */

    /* This must start on a U64 (8byte) boundry for section_info (currently the size of the union is 40bytes,
     * starting 24bytes into the SlotInfo_t). */
    union
    {
        stptiTP_SCDHeader_t            scd_headers;                   /* Only used for SLOTTYPE_ES                                                             */
        stptiTP_SectionInfo_t          section_info;                  /* Only used for SLOTTYPE_SECTION                                                        */
        stptiTP_PesFsm_t               pes_fsm;                       /* Only used for SLOTTYPE_PES                                                            */
        stptiTP_RawSlotInfo_t          raw_slot_info;                 /* Only used for SLOTTYPE_RAW                                                            */
    } u;
} stptiTP_SlotInfo_t;


#define DMA_INFO_NO_SIGNALLING   (0x7FFFFFFF)
#define DMA_INFO_ALLOW_OVERFLOW  (0x80000000)

#define DMA_INFO_MARK_OVERFLOWED_OVERWRITE (0x01)  /* Bit set (and cleared) by the TP in dms_end to indicate the wr ptr has overtaken the rd ptr, existing data in buffer is overwritten */
#define DMA_INFO_MARK_OVERFLOWED_DISCARD   (0x02)  /* Bit set (and cleared) by the TP to indicate buffer is full and input data is being discarded */
#define DMA_INFO_MARK_RESET_OVERFLOW       (0x04)  /* Bit set by the host in dma_end to indicate the rd ptr has been reset, cleared by the TP to ack */

typedef struct
{
    volatile U32 base;                       /* lower address of the DMA buffer (physical address) */
    volatile U32 size;                       /* size of the buffer */
    volatile U32 read_offset;                /* read pointer */
    volatile U32 write_offset;               /* current write pointer, not safe for host to use */
    volatile U32 qwrite_offset_pending;      /* quantized write pointer (updated BEFORE xfr), not safe for host to use (used for windback) */
    volatile U32 qwrite_offset;              /* quantized write pointer (updated after xfr), safe for host to use */
    volatile U32 signal_threshold;           /* marker point at which a buffer signal is raised (IRQ) (in terms of "fullness"), B31 used for overflow control */
    volatile U32 buffer_unit_count;          /* a count of the number of quantisation units output */
} stptiTP_DMAInfo_t;


typedef enum
{
    /* Parcel ids */
    INDEXER_EVENT_PARCEL                        = 0x00,      /* Output if enabled AND index_on_event_mask is met */
    INDEXER_ADDITIONAL_TRANSPORT_EVENT_PARCEL   = 0x01,      /* Output if enabled AND additional_transport_index_mask is met */
    INDEXER_STARTCODE_EVENT_PARCEL              = 0x02,      /* Output if enabled AND SCs remain after filtering */

    INDEXER_INDEX_PARCEL                        = 0x20,      /* Output if any of the above are output */

    INDEXER_STARTCODE_SPLIT                     = 0xFF,      /* Privately used by TP to mark the index_scratch_buffer is being used for split Start codes */
} stptiTP_IndexerParcelID_t __attribute__ ((SIZE_AS_U8));

typedef enum
{
    INDEXER_OUTPUT_EVENT                        = 1<<INDEXER_EVENT_PARCEL,
    INDEXER_OUTPUT_ADDITIONAL_TRANSPORT_EVENT   = 1<<INDEXER_ADDITIONAL_TRANSPORT_EVENT_PARCEL,
    INDEXER_OUTPUT_START_CODE_EVENT             = 1<<INDEXER_STARTCODE_EVENT_PARCEL,
} stptiTP_IndexerParcelSelection_t;

typedef enum
{
    INDEXER_OUTPUT_REF_PUSI             = 0x00000001
} stptiTP_IndexerConfig_t;


typedef struct
{
    volatile stptiTP_IndexerParcelSelection_t parcel_selection;
    volatile stptiTP_IndexerConfig_t indexer_config;
    volatile stptiTP_StatusBlk_Flags_t index_on_event_mask;         /* for indexing on events */

    volatile stptiTP_Indexer_AdditionalTransportIndexMask_t additional_transport_index_mask;    /* adaption field indexes */
    volatile U8 spare;
    volatile U16 output_dma_record;

    volatile U32 mpeg_sc_mask[256/32];

    volatile U32 index_count;                   /* a count of the number of indexes output (not accurate if using index chaining) */

    volatile U16 next_chained_indexer;
    volatile U16 spare2;

    U32 spare3[2];                              /* padded to 64 bytes to avoid compiler using multiply when indexing this array */
} stptiTP_IndexerInfo_t;




/* Stream Availability definitions */

#define STREAM_AVAILIABLITY_LIVE_CHANNELS       32
#define STREAM_AVAILIABLITY_PLAYBACK_CHANNELS   32


/* SECTION FILTER DEFINITIONS */
#define MAX_SECT_PAYLOAD_LEN          183 // 184 byte DVB payload minus 1 byte pointer field
#define SECTION_CRC_METADATA_LENGTH     1 // 1 byte for CRC status (Was 4 ??)
#define MINIMUM_SECTION_LENGTH          3 // 1 byte Table ID + 2 for SSI,PI,length = 3 (Was 8 ??)
#define SECTION_SLOT_INDEX_LENGTH       1 // Upto 256 slots so 1 byte is enough
#define SECTION_MATCHBYTES_LENGTH       8 // One bit for every filter (64 filters)
#define SECTION_BYTES_MAX (MAX_SECT_PAYLOAD_LEN/MINIMUM_SECTION_LENGTH * (SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH + MINIMUM_SECTION_LENGTH + SECTION_CRC_METADATA_LENGTH))

#define PES_METADATA_LENGTH 4
#define PES_HEADER_LENGTH   6

/* This is a checksum to guard against discrepencies between this file used when compiling the TP
 * and this file used for compiling the HOST driver.  Not the most robust checksum, but done this
 * way with the intention that the preprocessor will evaluate this to a value at compile time
 * (saving us instructions). */
#define SHARED_MEMORY_INTERFACE_CHECKSUM  (   sizeof(stptiTP_Interface_t)                       \
                                              ^ (sizeof(stptiTP_Live_t)<<1)                     \
                                              ^ (sizeof(stptiTP_Playback_t)<<2)                 \
                                              ^ (sizeof(stptiTP_pDeviceInfo_t)<<3)              \
                                              ^ (sizeof(stptiTP_vDeviceInfo_t)<<4)              \
                                              ^ (sizeof(stptiTP_SlotInfo_t)<<5)                 \
                                              ^ (sizeof(stptiTP_CamTable_t)<<6)                 \
                                              ^ (sizeof(stptiTP_IndexerInfo_t)<<7)              \
                                              ^ (sizeof(stptiTP_ProprietaryFilter_t)<<8)        \
                                              ^ (sizeof(stptiTP_SectionInfo_t)<<9)              \
                                              ^ (sizeof(stptiTP_DMAInfo_t)<<10)                 \
                                              ^ (sizeof(marker_data_t)<<11)                     \
                                              ^ (sizeof(stptiTP_StatusBlk_t)<<12)     )



typedef enum
{
    TP_FIRMWARE_TIMER_COUNTER_MASK  = 0x0000000F, /* Keep this value as the LSBs */
    TP_FIRMWARE_RESET_SHARED_MEMORY = 0x00000010,
    TP_FIRMWARE_ALLOW_POWERDOWN     = 0x00000020,
    TP_FIRMWARE_BYPASS_SECURE_COPRO = 0x00000040,
    TP_FIRMWARE_USE_TIMER_COUNTER   = 0x00000080,
} stptiTP_FirmwareConfig_t;

#define TP_SCRATCH_AREA_SIZE (2560)     /* must be large enough to hold section filter CAM settings for 64 filters */

/* Marker storage buffer structure */
typedef struct
{
    volatile U8   allocated;
    volatile U8   next_index;
    volatile U8   data[26];
} marker_data_t;



/*
 * This is the interface structure to provide the pointers to the shared memory interface.  This
 * provides the state and configuration information for the TP.
 *
 * The driver will take a copy of this from the TP's dDEM and update the pointers so that it can
 * use them directly.  All pointers here should point to structures in the shared memory
 * interface array structure, so that the shared memory interface region can be saved and restored
 * for powerdown modes.  It also helps partition information for security reasons.
 *
 * This interface relies on pointers being the same size on the TP as on the host.
 *
 * The host should only write to this structure, or the shared memory interface.  On secure devices
 * accesses outside of these regions will result in security violations.
 *
 * This is for BOOT TIME configuration, think carefully whether you want the configuration here,
 * or in stptiTP_pDeviceInfo_t (where post boot time configuration should be done).
 *
 */
typedef struct {

    /* These are used to aid debug, and determine TP activity */
    volatile U32                        DebugScratch[4];    /* (reserved) four U32's available to aid f/w debug (this will be start of dDEM) */

    volatile char                       Version[8];         /* Version string */
    volatile U32                        CheckSum;           /* Used for validating the structures in this file haven't changed */
    volatile U32                        ActivityCounter;    /* Used for checking TP is alive (always increments) */
    volatile U8                         WaitingOnStreamer;  /* When TP has stalled it can indicate if this is on a DMA operation */
    volatile U8                         TimerCounter;       /* Info when the TP is configured to use the TCU */
    volatile U8                         OutputDebugLevel;   /* Written by Host - 0 means none (default), change to 1 for errors, or 2 for errors and warnings */
    volatile U8                         Trigger;            /* Written by Host to cause a debug trigger event on the TP */

    /* Host supplied Parameters (maybe altered downwards by TP if too large) */
    volatile U32 NumberOfvDevices;                          /* will be reset to 0 if there isn't enough memory in the shared memory interface */
    volatile U32 NumberOfPids;                              /* must be at least the number of slots (a few more helps pid table allocation) */
    volatile U32 NumberOfSlots;                             /* there is a hard limit of 256 */
    volatile U32 NumberOfSectionFilters;
    volatile U32 NumberOfDMAStructures;
    volatile U32 NumberOfIndexers;
    volatile U32 NumberOfStatusBlks;                        /* there is a hard limit of 256 */
    volatile U32 SlowRateStreamTimeout;                     /* GNBvd50868_SLOW_RATE_STREAM_WA - not conditionally compiled to avoid f/w compatibility issue, 0xFFFFFFFF means disabled */

    volatile stptiTP_FirmwareConfig_t   FirmwareConfig;     /* Bootup feature configuration */

    /* TP returned parameters */
    volatile U32 NumberOfLiveChannels;
    volatile U32 NumberOfPESMarkers;
    volatile U32 SizeOfCAM;
    volatile U32 SizeOfPIDTableRegion;                      /* starting at PIDTableRegion_p - covers PIDTable_p, PIDSlotMappingTable_p too */
    volatile U32 SizeOfSharedMemoryRegion;

    /* TP returned Memory Map (pointer section) */
    volatile U8                      *SharedMemory_p;       /* Must always be first in the pointer section (marks start of pointer region) */
    volatile stptiTP_Live_t          *Live_p;
    volatile stptiTP_Playback_t      *Playback_p;
    volatile U32                     *StreamAvailability_p;
    volatile stptiTP_pDeviceInfo_t   *pDeviceInfo_p;
    volatile stptiTP_vDeviceInfo_t   *vDeviceInfo_p;
    volatile U16                     *PIDTableRegion_p;
    volatile U16                     *PIDTable_p;               /* Contained within PIDTableRegion */
    volatile U16                     *PIDSlotMappingTable_p;    /* Contained within PIDTableRegion */
    volatile stptiTP_SlotInfo_t      *SlotInfo_p;
    volatile stptiTP_DMAInfo_t       *DMAInfo_p;
    volatile stptiTP_IndexerInfo_t   *IndexerInfo_p;
    volatile U64                     *FilterCAMRegion_p;
    volatile stptiTP_CamTable_t      *FilterCAMTables_p;
    volatile stptiTP_ProprietaryFilter_t *ProprietaryFilters_p;
    volatile marker_data_t           *PesMarkerBuffers_p;
    volatile U8                      *DMAOverflowFlags_p;
    volatile U32                     *DMAPointers_p;
    volatile stptiTP_StatusBlk_t     *StatusBlk_p;          /* Must always be last in the pointer section (marks end of pointer region) */

    /* Used for xp70 printf communications (ISR will read this upon an TP printf mailbox interrupt) */
    volatile char DEBUG_BUFFER[256];

} stptiTP_Interface_t;



/* HOST to TP Mailbox Interface Bits*/
#define stptiTP_H2TP_MAILBOX0_TP_PRINTF_COMPLETE_MASK               0x80000000
#define stptiTP_H2TP_MAILBOX0_TP_PRINTF_COMPLETE_OFFSET             31
#define stptiTP_H2TP_MAILBOX0_STOP_PLAYBACK_MASK                    0x01000000
#define stptiTP_H2TP_MAILBOX0_STOP_PLAYBACK_OFFSET                  24
#define stptiTP_H2TP_MAILBOX0_START_PLAYBACK_MASK                   0x00010000
#define stptiTP_H2TP_MAILBOX0_START_PLAYBACK_OFFSET                 16
#define stptiTP_H2TP_MAILBOX0_SUPPLYING_INIT_PARAMS_MASK            0x00008000
#define stptiTP_H2TP_MAILBOX0_SUPPLYING_INIT_PARAMS_OFFSET          15
#define stptiTP_H2TP_MAILBOX0_UPDATING_NEW_PID_TABLE                0x00000100
#define stptiTP_H2TP_MAILBOX0_UPDATING_NEW_PID_OFFSET               8
#define stptiTP_H2TP_MAILBOX0_STOP_LIVE_MASK                        0x00000010
#define stptiTP_H2TP_MAILBOX0_STOP_LIVE_OFFSET                      4
#define stptiTP_H2TP_MAILBOX0_START_LIVE_MASK                       0x00000001
#define stptiTP_H2TP_MAILBOX0_START_LIVE_OFFSET                     0

#define stptiTP_H2TP_MAILBOX0_INTERRUPT_MASK                        (stptiTP_H2TP_MAILBOX0_TP_PRINTF_COMPLETE_MASK)

/* TP to HOST Mailbox Interface Bits*/
#define stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_WAITING_MASK             0x80000000
#define stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_WAITING_OFFSET           31
#define stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_OFFSET_MASK              0x7ffe0000
#define stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_OFFSET_OFFSET            17
#define stptiTP_TP2H_MAILBOX0_CHECK_FOR_PLAYBACK_COMPLETION_MASK    0x0001ffc0
#define stptiTP_TP2H_MAILBOX0_CHECK_FOR_PLAYBACK_COMPLETION_OFFSET  6
#define stptiTP_TP2H_MAILBOX0_STOP_STFE_FLUSH_MASK                  0x00000020
#define stptiTP_TP2H_MAILBOX0_STOP_STFE_FLUSH_OFFSET                5
#define stptiTP_TP2H_MAILBOX0_START_STFE_FLUSH_MASK                 0x00000010
#define stptiTP_TP2H_MAILBOX0_START_STFE_FLUSH_OFFSET               4
#define stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_OVERFLOW_MASK            0x00000008
#define stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_OVERFLOW_OFFSET          3
#define stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_SIGNALLED_MASK           0x00000004
#define stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_SIGNALLED_OFFSET         2
#define stptiTP_TP2H_MAILBOX0_HAS_A_BUFFER_SIGNALLED_MASK           0x00000002
#define stptiTP_TP2H_MAILBOX0_HAS_A_BUFFER_SIGNALLED_OFFSET         1
#define stptiTP_TP2H_MAILBOX0_TP_ACKNOWLEDGE_MASK                   0x00000001      /* Shared with MAILBOX_TP_INITIALISED */
#define stptiTP_TP2H_MAILBOX0_TP_ACKNOWLEDGE_OFFSET                 0               /* Shared with MAILBOX_TP_INITIALISED */


/* Exported Function Prototypes -------------------------------------------- */

#endif /* _PTI_TP_API_H_ */
