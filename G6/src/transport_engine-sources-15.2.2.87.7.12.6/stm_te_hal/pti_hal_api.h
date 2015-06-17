/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_hal_api.h
   @brief  The PTI HAL API

   This defines the API for the PTI HAL.  This is the equivalent of stpti.h for HAL.

   The idea is to create a structure of function pointers for every function in the HAL.  These are
   populated by constant structures contained in each object in the HAL, just before the object
   is registered (in pti_driver.c).

   When calling the HAL you must used the MACRO defined here called... stptiHAL_call()
   This will lookup the function required (a function pointer) and once dereferenced it can be
   called.

   The reason we have this complication is to allow multiple HALs to coexist in the driver.  It
   should be possible to have a mixed architecture (e.g. TANGO with a PTI4, or TANGO with Software
   DEMUX).

   All PTI HALs must provide functions for the API defined below, even if they just return
   ST_ERROR_FEATURE_NOT_SUPPORTED.

 */

#ifndef _PTI_HAL_API_H_
#define _PTI_HAL_API_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */

/* DO NOT INCLUDE stpti.h !!!!
   We must have no dependence on the STAPI API, to remain API neutral, and avoid getting locked into
   backwards compatibility issues.  The STAPI PTI API will need to convert STAPI PTI API types to
   the HAL API types.  pti_stpti.h imports specifically allowed defines from stpti.h (such as
   STPTI_ErrorType_t) */
#include "pti_stpti.h"		/* include sprecifically allowed defines from stpti.h */

/* Includes from the HAL / ObjMan level */
#include "objman/pti_object.h"

/* DO NOT INCLUDE object headers !!!!
 * We must aim to be hardware neutral here.  Object headers can include this file, but not vice
 * versa. */

/* Exported Constants ------------------------------------------------------ */

#define stptiHAL_SYNC_BYTE                (0x47)
#define stptiHAL_DVB_PID_BIT_SIZE         (13)

/* Exported Types ---------------------------------------------------------- */

/* Put stuff here for things that might be global to the HAL, i.e. shared between objects, or to
 * be exposed as part of the the API */
typedef enum {
	stptiHAL_DVB_TS_PROTOCOL,
	stptiHAL_A3_TS_PROTOCOL,
	stptiHAL_DSS_TS_PROTOCOL,
} stptiHAL_TransportProtocol_t;

typedef enum {
	stptiHAL_BUFFER_OVERFLOW_EVENT,
	stptiHAL_CC_ERROR_EVENT,
	stptiHAL_SCRAMBLE_TOCLEAR_EVENT,
	stptiHAL_CLEAR_TOSCRAM_EVENT,
	stptiHAL_INTERRUPT_FAIL_EVENT,
	stptiHAL_INVALID_DESCRAMBLE_KEY_EVENT,
	stptiHAL_INVALID_PARAMETER_EVENT,
	stptiHAL_TRANSPORT_ERROR_EVENT,
	stptiHAL_PCR_RECEIVED_EVENT,
	stptiHAL_PES_ERROR_EVENT,
	stptiHAL_SECTIONS_DISCARDED_ON_CRC_EVENT,
	stptiHAL_DATA_ENTRY_COMPLETE_EVENT,
	stptiHAL_MARKER_ERROR_EVENT,
	stptiHAL_INVALID_SECONDARY_PID_PACKET_EVENT,
	stptiHAL_END_EVENT,
} stptiHAL_EventType_t;

/* The API for the pDevice Object ------------------------------------------ */
/* This must NOT be used by the API layer. It should be only used internally (HAL - HAL calls),
   API layer calls should be done via vDevices.  The pDevice object's existance is static, which
   helps for debugging (stptiHAL_pDevice[0]). */
typedef struct {
	U32 Base;
	U32 SizeInPkts;
	U32 PktLength;
	U32 Channel;
} stptiHAL_pDeviceConfigLiveParams_t;

typedef struct {
	unsigned int NumbervDevice;                  /* Max number of vDevices on this pDevice */
	unsigned int NumberSlots;                    /* Number of slots */
	unsigned int NumberDMAs;                     /* Number of dma structures */
	unsigned int NumberHWSectionFilters;         /* Max number of section filters */
	unsigned int NumberIndexer;                  /* Number of indexers */
	unsigned int NumberStatusBlk;                /* Number of status blocks */
} stptiHAL_pDeviceConfigStatus_t;

typedef enum {
	stptiHAL_PDEVICE_STARTED,
	stptiHAL_PDEVICE_SLEEPING,
	stptiHAL_PDEVICE_STOPPED,
	stptiHAL_PDEVICE_POWERDOWN,
} stptiHAL_pDevicePowerState_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_pDeviceEventTask) (FullHandle_t pDeviceHandle);								/* stptiHAL_pDeviceEventTask */
	 ST_ErrorCode_t(*HAL_pDeviceEventTaskAbort) (FullHandle_t pDeviceHandle);							/* stptiHAL_pDeviceEventTaskAbort */
	 ST_ErrorCode_t(*HAL_pDeviceLiveTask) (FullHandle_t pDeviceHandle);								/* stptiHAL_pDeviceLiveTask */
	 ST_ErrorCode_t(*HAL_pDeviceConfigureLive) (FullHandle_t pDeviceHandle, stptiHAL_pDeviceConfigLiveParams_t * LiveParams_p);	/* stptiHAL_pDeviceConfigureLive */
	 ST_ErrorCode_t(*HAL_pDeviceEnableSWLeakyPID) (FullHandle_t pDeviceHandle, unsigned int LiveChannel, BOOL StreamIsActive);	/* stptiHAL_pDeviceEnableSWLeakyPID */
	 ST_ErrorCode_t(*HAL_pDeviceSWLeakyPIDAvailable) (FullHandle_t pDeviceHandle, BOOL * IsAvailable);				/* stptiHAL_pDeviceSWLeakyPIDAvailable */
	 ST_ErrorCode_t(*HAL_pDeviceGetCapability) (FullHandle_t pDeviceHandle, stptiHAL_pDeviceConfigStatus_t *ConfigStatus_p);		/* stptiHAL_pDeviceGetCapability */
	 ST_ErrorCode_t(*HAL_pDeviceGetCycleCount) (FullHandle_t pDeviceHandle, unsigned int *count);					/* stptiHAL_pDeviceGetCycleCount */
	 ST_ErrorCode_t(*HAL_pDeviceSetPowerState) (FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState);			/* stptiHAL_pDeviceSetPowerState */
 	 ST_ErrorCode_t(*HAL_pDeviceGetPlatformDevice) (FullHandle_t pDeviceHandle, struct platform_device **pdev);			/* stptiHAL_pDeviceGetPlatformDevice */
} stptiHAL_pDeviceAPI_t;

/* The API for the vDevice Object ------------------------------------------ */
typedef struct {
	void (*EventBufferOverflowNotify_p) (FullHandle_t SlotHandle, FullHandle_t BufferHandle);
	void (*EventCCErrorNotify_p) (FullHandle_t SlotHandle, U16 ExpectedCC, U16 ReceivedCC);
	void (*EventScramToClearNotify_p) (FullHandle_t SlotHandle);
	void (*EventClearToScramNotify_p) (FullHandle_t SlotHandle);
	void (*EventInvalidDescramblerKeyNotify_p) (FullHandle_t SlotHandle);
	void (*EventInvalidParameterNotify_p) (FullHandle_t SlotHandle);
	void (*EventPacketErrorNotify_p) (FullHandle_t SlotHandle);
	void (*EventPCRNotify_p) (FullHandle_t SlotHandle, U32 PCRLsw, U32 PCRBit32, U16 PCRExt, U32 ArrivalLsw,
				  U32 ArrivalBit32, U16 ArrivalExt, BOOL Discont);
	void (*EventPESErrorNotify_p) (FullHandle_t BufferHandle);
	void (*EventSectionCRCDiscardNotify_p) (FullHandle_t SlotHandle);
	void (*EventDataEntryNotify_p) (FullHandle_t SlotHandle, FullHandle_t DataEntryHandle);
	void (*EventMarkerErrorNotify_p) (FullHandle_t SlotHandle, U8 Marker, U32 MarkerID0, U32 MarkerID1);
	void (*EventSecondaryPidPktNotify_p) (FullHandle_t SlotHandle);
} stptiHAL_EventFuncPtrs_t;

typedef struct {
	int NumberOfSlots;
	int NumberOfSectionFilters;
	stptiHAL_TransportProtocol_t TSProtocol;
	int PacketSize;
	unsigned int StreamID;
	BOOL ForceDiscardSectionOnCRCError;
	stptiHAL_EventFuncPtrs_t EventFuncPtrs;
} stptiHAL_vDeviceAllocationParams_t;

typedef struct {
	U32 PacketCount;
	U32 TransportErrorCount;
	U32 CCErrorCount;
	U32 Utilization;
	U32 BufferOverflowCount;
} stptiHAL_vDeviceStreamStatistics_t;

typedef struct {
	int NumberSlots;
	int NumberDMAs;
	int NumberHWSectionFilters;
	BOOL AlternateOutputSupport;
	BOOL PidWildcardingSupport;
	BOOL RawStreamDescrambling;
	stptiHAL_TransportProtocol_t Protocol;
} stptiHAL_vDeviceConfigStatus_t;

typedef enum {
	stptiHAL_POWER_STATE_NORMAL = 0,
	stptiHAL_POWER_STATE_ACTIVE_STANDBY = 1,
	stptiHAL_POWER_STATE_PASSIVE_STANDBY = 2,
} stptiHAL_vDevicePowerState_t;

typedef enum {
	stptiHAL_VDEVICE_DISCARD_DUPLICATE_PKTS = 1,
} stptiHAL_vDeviceFeature_t;

#define stptiHAL_StreamIDNone (0x0000FFFF)

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_vDeviceDebug) (FullHandle_t vDeviceHandle, char *DebugClass, char *String, int *StringSize_p, int MaxStringSize, int Offset, int *EOF_p);		/* stptiHAL_vDeviceDebug */
	 ST_ErrorCode_t(*HAL_vDeviceLookupSlotForPID) (FullHandle_t vDeviceHandle, U16 PIDtoFind, FullHandle_t * SlotHandleArray_p, int SlotArraySize, int *SlotsMatching);	/* stptiHAL_vDeviceLookupSlotForPID */
	 ST_ErrorCode_t(*HAL_vDeviceLookupPIDs) (FullHandle_t vDeviceHandle, U16 * PIDArray_p, U16 PIDArraySize, U16 * PIDsFound);						/* stptiHAL_vDeviceLookupPIDs */
	 ST_ErrorCode_t(*HAL_vDeviceGetStreamID) (FullHandle_t vDeviceHandle, U32 * StreamID_p, BOOL * UseTimerTag_p);								/* stptiHAL_vDeviceGetStreamID */
	 ST_ErrorCode_t(*HAL_vDeviceSetStreamID) (FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag);									/* stptiHAL_vDeviceSetStreamID */
	 ST_ErrorCode_t(*HAL_vDeviceSetEvent) (FullHandle_t vDeviceHandle, stptiHAL_EventType_t Event, BOOL Enable);								/* stptiHAL_vDeviceSetEvent */
	 ST_ErrorCode_t(*HAL_vDeviceGetTSProtocol) (FullHandle_t vDeviceHandle, stptiHAL_TransportProtocol_t * Protocol_p, U8 * PacketSize_p);					/* stptiHAL_vDeviceGetTSProtocol */
	 ST_ErrorCode_t(*HAL_vDeviceGetStreamStatistics) (FullHandle_t vDeviceHandle, stptiHAL_vDeviceStreamStatistics_t * Statistics_p);					/* stptiHAL_vDeviceGetStreamStatistics */
	 ST_ErrorCode_t(*HAL_vDeviceResetStreamStatistics) (FullHandle_t vDeviceHandle);											/* stptiHAL_vDeviceResetStreamStatistics */
	 ST_ErrorCode_t(*HAL_vDeviceGetCapability) (FullHandle_t vDeviceHandle, stptiHAL_vDeviceConfigStatus_t * ConfigStatus_p);						/* stptiHAL_vDeviceGetCapability */
	 ST_ErrorCode_t(*HAL_vDeviceIndexesEnable) (FullHandle_t vDeviceHandle, BOOL Enable);											/* stptiHAL_vDeviceIndexesEnable */
	 ST_ErrorCode_t(*HAL_vDeviceFirmwareReset) (FullHandle_t vDeviceHandle);												/* stptiHAL_vDeviceFirmwareReset */
	 ST_ErrorCode_t(*HAL_vDevicePowerDown) (FullHandle_t vDeviceHandle);													/* stptiHal_vDevicePowerDown */
	 ST_ErrorCode_t(*HAL_vDevicePowerUp) (FullHandle_t vDeviceHandle);													/* stptiHal_vDevicePowerUp */
	 ST_ErrorCode_t(*HAL_vDeviceFeatureEnable) (FullHandle_t vDeviceHandle, stptiHAL_vDeviceFeature_t Feature, BOOL Enable);						/* stptiHAL_vDeviceFeatureEnable */
	 ST_ErrorCode_t(*HAL_vDeviceGetTimer) (FullHandle_t vDeviceHandle, U32 *TimerValue_p, U64 *SystemTime_p);								/* stptiHAL_vDeviceGetTimer */
} stptiHAL_vDeviceAPI_t;

/* The API for the Session Object ------------------------------------------ */
typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
} stptiHAL_SessionAPI_t;

/* The API for the Software Injector Object -------------------------------- */
typedef enum {
	stptiHAL_PKT_INCLUDES_STFE_TAGS = 0x00000001,
	stptiHAL_PKT_INCLUDES_DNLA_TTS_TAGS = 0x00000002,
	stptiHAL_INJECTING_FROM_VIRTUAL_ADDRESS = 0x00000100,
} stptiHAL_InjectionFlags_t;

typedef struct {
	int MaxNumberOfNodes;
} stptiHAL_SoftwareInjectorConfigParams_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_SoftwareInjectorAddInjectNode) (FullHandle_t SoftwareInjectorHandle, U8 * Data, U32 Size, stptiHAL_InjectionFlags_t InjectionFlags);	/* stptiHAL_SoftwareInjectorAddInjectNode */
	 ST_ErrorCode_t(*HAL_SoftwareInjectorStart) (FullHandle_t SoftwareInjectorHandle);										/* stptiHAL_SoftwareInjectorStart */
	 ST_ErrorCode_t(*HAL_SoftwareInjectorWaitForCompletion) (FullHandle_t SoftwareInjectorHandle, int TimeoutInMS);							/* stptiHAL_SoftwareInjectorWaitForCompletion */
	 ST_ErrorCode_t(*HAL_SoftwareInjectorAbort) (FullHandle_t SoftwareInjectorHandle);										/* stptiHAL_SoftwareInjectorAbort */
	 ST_ErrorCode_t(*HAL_SoftwareInjectorFlush) (FullHandle_t SoftwareInjectorHandle);										/* stptiHAL_SoftwareInjectorFlush */
} stptiHAL_SoftwareInjectorAPI_t;

/* The API for the Slot Object --------------------------------------------- */
typedef enum {
	stptiHAL_SLOT_TYPE_NULL = 0x00,		/* This enum MUST be kept in sync with STPTI_SlotType_t in stpti.h */
	stptiHAL_SLOT_TYPE_SECTION = 0x01,	/* and stptiTP_SlotMode_t in pti_hal_api.h/exported_api.h          */
	stptiHAL_SLOT_TYPE_PES = 0x02,
	stptiHAL_SLOT_TYPE_RAW = 0x03,
	stptiHAL_SLOT_TYPE_PCR = 0x04,
	stptiHAL_SLOT_TYPE_EMM = 0x05,		/* EMM Slot handling is dependent on CA vendor, but in most cases is reassigned as a SECTION slot */
	stptiHAL_SLOT_TYPE_ECM = 0x06,		/* ECM Slot handling is dependent on CA vendor, but in most cases is reassigned as a SECTION slot */
	stptiHAL_SLOT_TYPE_VIDEO_ES = 0x07,
	stptiHAL_SLOT_TYPE_AUDIO_ES = 0x08,
	stptiHAL_SLOT_TYPE_PARTIAL_PES = 0x80,	/**< This is virtual slot type, this maps to a RAW slot in the TP */
} stptiHAL_SlotMode_t;

typedef enum {
	stptiHAL_SECONDARY_TYPE_NONE,
	stptiHAL_SECONDARY_TYPE_PRIM,
	stptiHAL_SECONDARY_TYPE_SEC
} stptiHAL_SecondaryType_t;

typedef enum {
	stptiHAL_UNSCRAMBLED = 0,
	stptiHAL_SCRAMBLED_ODD = 1,
	stptiHAL_SCRAMBLED_EVEN = 2,
	stptiHAL_PES_SCRAMBLED = 4
} stptiHAL_ScrambledState_t;

typedef enum {
	stptiHAL_SLOT_SUPPRESS_CC,
	stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG,
	stptiHAL_SLOT_CC_FIXUP,
	stptiHAL_SLOT_OUTPUT_BUFFER_COUNT
} stptiHAL_SlotFeature_t;

#define stptiHAL_InvalidPID (0xE000)
#define stptiHAL_WildCardPID (0x2000)

typedef struct {
	stptiHAL_SlotMode_t SlotMode;
	BOOL SuppressMetaData;
	BOOL SoftwareCDFifo;
	BOOL DataEntryReplacement;
	BOOL DataEntryInsertion;
	BOOL SuppressCCCheck;
	BOOL PrefixTSTag;
	BOOL PrefixDNLATSTag;
} stptiHAL_SlotConfigParams_t;

typedef enum {
	stptiHAL_SECUREPATH_OUTPUT_NODE_CLEAR,
	stptiHAL_SECUREPATH_OUTPUT_NODE_SCRAMBLED
} stptiHAL_SecureOutputNode_t;

typedef enum {
	stptiHAL_SECONDARY_PID_MODE_NONE,
	stptiHAL_SECONDARY_PID_MODE_SUBSTITUTION,
	stptiHAL_SECONDARY_PID_MODE_INSERTION,
	stptiHAL_SECONDARY_PID_MODE_INSERTDELETE
} stptiHAL_SecondaryPidMode_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_SlotSetPID) (FullHandle_t SlotHandle, U16 Pid, BOOL ResetSlot);								/* stptiHAL_SlotSetPID */
	 ST_ErrorCode_t(*HAL_SlotGetPID) (FullHandle_t SlotHandle, U16 * Pid_p);									/* stptiHAL_SlotGetPID */
	 ST_ErrorCode_t(*HAL_SlotGetState) (FullHandle_t SlotHandle, U32 * TSPacketCount_p, stptiHAL_ScrambledState_t * CurrentScramblingState_p);	/* stptiHAL_SlotGetState */
	 ST_ErrorCode_t(*HAL_SlotEnableEvent) (FullHandle_t SlotHandle, stptiHAL_EventType_t Event, BOOL Enable);					/* stptiHAL_SlotEnableEvent */
	 ST_ErrorCode_t(*HAL_SlotRemapScramblingBits) (FullHandle_t SlotHandle, U8 SCBitsToMatch, U8 ReplacementSCBits);				/* stptiHAL_SlotRemapScramblingBits */
	 ST_ErrorCode_t(*HAL_SlotSetCorruption) (FullHandle_t SlotHandle, BOOL EnableCorruption, U8 CorruptionOffset, U8 CorruptionValue);		/* stptiHAL_SlotSetCorruption */
	 ST_ErrorCode_t(*HAL_SlotFeatureEnable) (FullHandle_t SlotHandle, stptiHAL_SlotFeature_t Feature, BOOL Enable);					/* stptiHAL_SlotFeatureEnable */
	 ST_ErrorCode_t(*HAL_SlotSetSecurePathOutputNode) (FullHandle_t SlotHandle, stptiHAL_SecureOutputNode_t OutputNode);				/* stptiHAL_SlotSetSecurePathOutputNode */
	 ST_ErrorCode_t(*HAL_SlotSetSecurePathID) (FullHandle_t SlotHandle, U32 PathID);								/* stptiHAL_SlotSetSecurePathID */
	 ST_ErrorCode_t(*HAL_SlotGetMode) (FullHandle_t SlotHandle, U16 * Mode_p);									/* stptiHAL_SlotGetMode */
	 ST_ErrorCode_t(*HAL_SlotSetSecondaryPid) (FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle, stptiHAL_SecondaryPidMode_t Mode);	/* stptiHAL_SlotSetSecondaryPid */
	 ST_ErrorCode_t(*HAL_SlotGetSecondaryPid) (FullHandle_t SlotHandle, stptiHAL_SecondaryPidMode_t * Mode);					/* stptiHAL_SlotGetSecondaryPid */
	 ST_ErrorCode_t(*HAL_SlotClearSecondaryPid) (FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle);					/* stptiHAL_SlotClearSecondaryPid */
	 ST_ErrorCode_t(*HAL_SlotSetRemapPID) (FullHandle_t SlotHandle, U16 Pid);									/* stptiHAL_SlotSetRemapPID */
} stptiHAL_SlotAPI_t;

/* The API for the Index Object -------------------------------------------- */
typedef enum {
	/* MSByte matches AF flags, LSByte reserved for AF Extension */
	stptiHAL_AF_DISCONTINUITY_INDICATOR = 0x80,
	stptiHAL_AF_RANDOM_ACCESS_INDICATOR = 0x40,
	stptiHAL_AF_PRIORITY_INDICATOR = 0x20,
	stptiHAL_AF_PCR_FLAG = 0x10,
	stptiHAL_AF_OPCR_FLAG = 0x08,
	stptiHAL_AF_SPLICING_POINT_FLAG = 0x04,
	stptiHAL_AF_PRIVATE_DATA_FLAG = 0x02,
	stptiHAL_AF_ADAPTION_EXTENSION_FLAG = 0x01,
} stptiHAL_AdditionalTransportFlags_t;

typedef enum {
	stptiHAL_INDEX_PUSI = 0x00000008,
	stptiHAL_INDEX_SCRAMBLE_TO_EVEN = 0x00000010,
	stptiHAL_INDEX_SCRAMBLE_TO_ODD = 0x00000020,
	stptiHAL_INDEX_SCRAMBLE_TO_CLEAR = 0x00000040,
	stptiHAL_INDEX_CLEAR_TO_SCRAMBLE = 0x00000080,
	stptiHAL_INDEX_PES_PTS = 0x00000100,
	stptiHAL_INDEX_FIRST_RECORDED_PKT = 0x00200000,
} stptiHAL_EventFlags_t;

typedef enum {
	stptiHAL_NO_STARTCODE_INDEXING = 0x00000000,
	stptiHAL_INDEX_STARTCODES_WITH_CONTEXT = 0x00000001,
} stptiHAL_IndexerStartCodeMode_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_IndexTransportEvents) (FullHandle_t IndexHandle, stptiHAL_EventFlags_t EventFlags, stptiHAL_AdditionalTransportFlags_t AdditionalFlags, BOOL Enable);	/* stptiHAL_IndexTransportEvents */
	 ST_ErrorCode_t(*HAL_IndexOutputStartCodes) (FullHandle_t IndexHandle, U32 * StartCodeMask, stptiHAL_IndexerStartCodeMode_t Mode);						/* stptiHAL_IndexOutputStartCodes */
	 ST_ErrorCode_t(*HAL_IndexReset) (FullHandle_t IndexHandle);															/* stptiHAL_IndexReset */
	 ST_ErrorCode_t(*HAL_IndexChain) (FullHandle_t IndexHandle, FullHandle_t * Indexes2Chain, int NumberOfIndexes);									/* stptiHAL_IndexChain */
} stptiHAL_IndexAPI_t;

/* The API for the Buffer Object ------------------------------------------- */
#define stptiHAL_CURRENT_READ_OFFSET 0xFFFFFFFF

typedef enum {
	stptiHAL_READ_IGNORE_QUANTISATION = 0,
	stptiHAL_READ_AS_UNITS_NO_TRUNCATION,
	stptiHAL_READ_AS_UNITS_ALLOW_TRUNCATION,
} stptiHAL_BufferReadQuantisationRule_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_BufferSetThreshold) (FullHandle_t BufferHandle, U32 UpperThreshold);																																	/* stptiHAL_BufferSetThreshold */
	 ST_ErrorCode_t(*HAL_BufferFlush) (FullHandle_t BufferHandle);																																					/* stptiHAL_BufferFlush */
	 ST_ErrorCode_t(*HAL_BufferFiltersFlush) (FullHandle_t BufferHandle, FullHandle_t * FilterHandles, int NumberOfHandles);																													/* stptiHAL_BufferFiltersFlush */
	 ST_ErrorCode_t(*HAL_BufferGetWriteOffset) (FullHandle_t BufferHandle, U32 * WriteOffset_p);																																	/* stptiHAL_BufferGetWriteOffset */
	 ST_ErrorCode_t(*HAL_BufferRead) (FullHandle_t BufferHandle, stptiHAL_BufferReadQuantisationRule_t ReadAsUnits, U32 * ReadOffset_p, U32 LeadingBytesToDiscard, void *DestBuffer1_p, U32 DestSize1, void *DestBuffer2_p, U32 DestSize2, void *MetaData_p, U32 MetaDataSize, ST_ErrorCode_t(*CopyFunction) (void **, const void *, size_t), U32 * BytesRead);	/* stptiHAL_BufferRead */
	 ST_ErrorCode_t(*HAL_BufferSetReadOffset) (FullHandle_t BufferHandle, U32 ReadOffset);																																		/* stptiHAL_BufferSetReadOffset */
	 ST_ErrorCode_t(*HAL_BufferStatus) (FullHandle_t BufferHandle, U32 * BufferSize_p, U32 * BytesInBuffer_p, U32 * BufferUnitCount_p, U32 * FreeSpace_p, U32 * UnitsInBuffer_p, U32 * NonUnitBytesInBuffer_p, BOOL * OverflowedFlag_p);																/* stptiHAL_BufferStatus */
	 ST_ErrorCode_t(*HAL_BufferType) (FullHandle_t BufferHandle, stptiHAL_SlotMode_t * InheritedSlotType);																																/* stptiHAL_BufferType */
	 ST_ErrorCode_t(*HAL_BufferSetOverflowControl) (FullHandle_t BufferHandle, BOOL DiscardInputOnOverflow);																															/* stptiHAL_BufferSetOverflowControl */
} stptiHAL_BufferAPI_t;

typedef struct {
	BOOL ManuallyAllocatedBuffer;	/**< TRUE if buffer has been allocated by the user */
	BOOL PhysicalAddressSupplied;	/**< TRUE if BufferStart is a physical address rather than a (kernel) virtual one */
	void *BufferStart_p;		/**< BufferStart only relevent for ManuallyAllocatedBuffer (must be cache aligned) */
	U32 BufferSize;			/**< BufferSize (must be cache aligned for ManuallyAllocatedBuffer) */
} stptiHAL_BufferConfigParams_t;

typedef struct {
	BOOL CRC_OK;
	unsigned int FiltersMatched;
	FullHandle_t FilterHandles[64];
} stptiHAL_SectionFilterMetaData_t;

typedef struct {
	FullHandle_t IndexedSlotHandle;
	FullHandle_t IndexedBufferHandle;
	U32 Clk27MHzDiv300Bit32;	/**< PacketArrivalTime MSBit (90kHz) */
	U32 Clk27MHzDiv300Bit31to0;	/**< PacketArrivalTime LSBits (90kHz) */
	U16 Clk27MHzModulus300;		/**< PacketArrivalTime Extension (27MHz) */
	U32 PCR27MHzDiv300Bit32;	/**< (if PCR) PCR MSBit (90kHz) */
	U32 PCR27MHzDiv300Bit31to0;	/**< (if PCR) PCR LSBits (90kHz) */
	U16 PCR27MHzModulus300;		/**< (if PCR) PCR Extension (27MHz) */
	U32 BufferPacketCount;
	U32 BufferOffset;

	stptiHAL_EventFlags_t EventFlags;
	stptiHAL_AdditionalTransportFlags_t AdditionalFlags;

	U8 NumberOfMPEGStartCodes;
	U8 MPEGStartCodeValue;
	U8 MPEGStartCodeOffset;
	U8 MPEGStartIPB;
} stptiHAL_IndexEventData_t;

/* The API for the Filter Object ------------------------------------------- */
typedef enum {
	stptiHAL_NO_FILTER,
	stptiHAL_TINY_FILTER,
	stptiHAL_SHORT_FILTER,
	stptiHAL_LONG_FILTER,
	stptiHAL_PNMM_FILTER,
	stptiHAL_PROPRIETARY_FILTER,
	stptiHAL_SHORT_VNMM_FILTER,
	stptiHAL_LONG_VNMM_FILTER,
	stptiHAL_PNMM_VNMM_FILTER,
	stptiHAL_PES_STREAMID_FILTER /* Number not important -  not use in the TP */
} stptiHAL_FilterType_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_FilterSetDiscardOnCRCErr) (FullHandle_t FilterHandle, BOOL Discard);														/* stptiHAL_FilterSetDiscardOnCRCErr */
	 ST_ErrorCode_t(*HAL_FilterEnable) (FullHandle_t FilterHandle, BOOL Enable);																/* stptiHAL_FilterEnable */
	 ST_ErrorCode_t(*HAL_FilterUpdate) (FullHandle_t FilterHandle, stptiHAL_FilterType_t FilterType, BOOL OneShotMode, BOOL ForceCRCCheck, U8 * FilterData_p, U8 * FilterMask_p, U8 * FilterSense_p);	/* stptiHAL_FilterUpdate */
	 ST_ErrorCode_t(*HAL_FilterUpdateProprietaryFilter) (FullHandle_t FilterHandle, stptiHAL_FilterType_t FilterType, void *FilterData_p, size_t SizeofFilterData);						/* stptiHAL_FilterUpdateProprietaryFilter */
} stptiHAL_FilterAPI_t;

typedef struct {
	stptiHAL_FilterType_t FilterType;
} stptiHAL_FilterConfigParams_t;

/* The API for the Signal Object ------------------------------------------- */
typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_SignalAbort) (FullHandle_t SignalHandle);							/* stptiHAL_SignalAbort */
	 ST_ErrorCode_t(*HAL_SignalWait) (FullHandle_t SignalHandle, FullHandle_t * BufferHandle, U32 TimeoutMS);	/* stptiHAL_SignalWait */
} stptiHAL_SignalAPI_t;

/* The API for the DataEntry Object -------------------------------------------- */
typedef struct {
	U8 NumTSPackets;

} stptiHAL_DataEntryAllocateParams_t;

typedef struct {
	U8 *Data_p;		/**< Pointer to Entry Data allocated */
	U16 DataSize;		/**< Size of Data - multiple of packet size */
	U8 RepeatCount;		/**< Number of times to repeat this entry */
	U8 FromByte;		/**< Number of bytes to skip on packet replacement */
	BOOL NotifyEvent;	/**< Enable/Disable notify event */
} stptiHAL_DataEntryConfigParams_t;

typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	 ST_ErrorCode_t(*HAL_DataEntryConfigure) (FullHandle_t DataEntryHandle, stptiHAL_DataEntryConfigParams_t * Params_p);	/* stptiHAL_DataEntryConfigure */
} stptiHAL_DataEntryAPI_t;

/* The API for the Container Object ---------------------------------------- */
typedef struct {
	ObjectManagerFunctions_t ObjectManagementFunctions;
	/* There should be no other functions added this object - it is intended as just a container of information */
} stptiHAL_ContainerAPI_t;

/* The Complete API for the HAL -------------------------------------------- */
typedef struct {
	stptiHAL_pDeviceAPI_t pDevice;
	stptiHAL_vDeviceAPI_t vDevice;
	stptiHAL_SessionAPI_t Session;
	stptiHAL_SoftwareInjectorAPI_t SoftwareInjector;
	stptiHAL_BufferAPI_t Buffer;
	stptiHAL_FilterAPI_t Filter;
	stptiHAL_SignalAPI_t Signal;
	stptiHAL_IndexAPI_t Index;
	stptiHAL_SlotAPI_t Slot;
	stptiHAL_DataEntryAPI_t DataEntry;
	stptiHAL_ContainerAPI_t Container;
} HAL_API_t;

/* (effectively) Exported Function Prototypes ------------------------------ */

void STPTIHAL_entry(void);
void STPTIHAL_exit(void);
int STPTIHAL_PowerState(void);
int TSHAL_PowerState(void);
/**
   @brief  The macro for calling HAL functions.

   This macro calls the specified HAL function.  It helps to keep the code clean as it performs a
   function pointer lookup.  It is coded in this way to allow multiple HALs to coexist in the same
   driver.

   @warning Allocation, Deallocation, Association, and Disassociation must NOT be called this way.
   Doing so will result in an exception.  Use the Object Manager functions...
   stptiOBJMAN_AllocateObject(), stptiOBJMAN_AssociateObjects(), stptiOBJMAN_DisassociateObjects(),
   stptiOBJMAN_DeallocateObject() instead.  This is because the object manager needs to track
   the changes to object relationships.

   Example of usage...
     Error = stptiHAL_call_unlocked( Slot.HAL_SlotSetPID, FullSlotHandle, Pid );

   @param  Function            The HAL function to call.  This will be prefixed by the object type
                               as listed in HAL_API_t above.  For example... Slot.HAL_SlotSetPID

   @param  FullObjectHandle    The Handle of the Object you are performing the operation on.

   @param  ...                 Extra parameters (other than the FullObjectHandle) to be passed to
                               the HAL function.

   @return                     A standard st error type as given by the HAL function.

 */

#define stptiHAL_call_unlocked( Function, FullObjectHandle, ...)               \
({                                                                             \
	ST_ErrorCode_t stptiHAL_ErrorReturn = ST_ERROR_FEATURE_NOT_SUPPORTED;  \
	HAL_API_t *stptiHAL_FunctionPool_p = (HAL_API_t *)                     \
		stptiOBJMAN_ReturnHALFunctionPool(FullObjectHandle);           \
	if( stptiHAL_FunctionPool_p == NULL )                                  \
	{                                                                      \
		stptiHAL_ErrorReturn = ST_ERROR_BAD_PARAMETER;                 \
	}                                                                      \
	else if( stptiHAL_FunctionPool_p->Function != NULL )                   \
	{                                                                      \
		stptiHAL_ErrorReturn = (*(stptiHAL_FunctionPool_p->Function))  \
					( FullObjectHandle, ## __VA_ARGS__ );  \
	}                                                                      \
	(stptiHAL_ErrorReturn);                                                \
})

/**
   @brief  The macro for calling HAL functions.

   This macro calls the specified HAL function.  It helps to keep the code clean as it performs a
   function pointer lookup.  It is coded in this way to allow multiple HALs to coexist in the same
   driver.

   @warning Allocation, Deallocation, Association, and Disassociation must NOT be called this way.
   Doing so will result in an exception.  Use the Object Manager functions...
   stptiOBJMAN_AllocateObject(), stptiOBJMAN_AssociateObjects(), stptiOBJMAN_DisassociateObjects(),
   stptiOBJMAN_DeallocateObject() instead.  This is because the object manager needs to track
   the changes to object relationships.

   Example of usage...
     Error = stptiHAL_call( Slot.HAL_SlotSetPID, FullSlotHandle, Pid );

   @param  Function            The HAL function to call.  This will be prefixed by the object type
                               as listed in HAL_API_t above.  For example... Slot.HAL_SlotSetPID

   @param  FullObjectHandle    The Handle of the Object you are performing the operation on.

   @param  ...                 Extra parameters (other than the FullObjectHandle) to be passed to
                               the HAL function.

   @return                     A standard st error type as given by the HAL function.

 */

#define stptiHAL_call( Function, FullObjectHandle, ...)                                                           \
({                                                                                                                \
    ST_ErrorCode_t stptiHAL_ErrorReturn = ST_ERROR_FEATURE_NOT_SUPPORTED;                                         \
    HAL_API_t *stptiHAL_FunctionPool_p = (HAL_API_t *)stptiOBJMAN_ReturnHALFunctionPool(FullObjectHandle);        \
	if (STPTIHAL_PowerState()) {					\
		STPTIHAL_entry();					\
		if (stptiHAL_FunctionPool_p == NULL) {			\
			stptiHAL_ErrorReturn = ST_ERROR_BAD_PARAMETER;	\
		}							\
		if (stptiHAL_FunctionPool_p->Function != NULL) {	\
			stptiHAL_ErrorReturn = (*(stptiHAL_FunctionPool_p->Function))(FullObjectHandle, ## __VA_ARGS__);	\
		}							\
		STPTIHAL_exit();					\
	}								\
	(stptiHAL_ErrorReturn);						\
})
#endif /* _PTI_HAL_API_H_ */
