/***********************************************************************
 *
 * File: STMCommon/fmdsw.h
 * Copyright (c) 2004-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* WARNING: This file was documented using Doxygen 1.3.9.1 commands */

/*!@defgroup FMDSWAPI FMDSW API
 * @{
 */
#ifndef __FMDSW_H
#define __FMDSW_H

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* Exported Constants -------------------------------------------------------*/
/*===========================================================================*/

/*===========================================================================*/
/* Exported Types -----------------------------------------------------------*/
/*===========================================================================*/

/* STAPI compatibility types */
typedef unsigned int U32;
typedef int ST_BOOL;
typedef unsigned char U8;
#define FALSE (1==2)
#define TRUE  (!FALSE)
#define NULL  ((void *) 0)
/* Common driver error constants */
#define ST_DRIVER_ID   0
#define ST_DRIVER_BASE (ST_DRIVER_ID << 16)
enum
{
  ST_NO_ERROR = ST_DRIVER_BASE,       /* 0 */
  ST_ERROR_BAD_PARAMETER,             /* 1 Bad parameter passed       */
  ST_ERROR_NO_MEMORY,                 /* 2 Memory allocation failed   */
  ST_ERROR_UNKNOWN_DEVICE,            /* 3 Unknown device name        */
  ST_ERROR_ALREADY_INITIALIZED,       /* 4 Device already initialized */
  ST_ERROR_NO_FREE_HANDLES,           /* 5 Cannot open device again   */
  ST_ERROR_OPEN_HANDLE,               /* 6 At least one open handle   */
  ST_ERROR_INVALID_HANDLE,            /* 7 Handle is not valid        */
  ST_ERROR_FEATURE_NOT_SUPPORTED,     /* 8 Feature unavailable        */
  ST_ERROR_INTERRUPT_INSTALL,         /* 9 Interrupt install failed   */
  ST_ERROR_INTERRUPT_UNINSTALL,       /* 10 Interrupt uninstall failed */
  ST_ERROR_TIMEOUT,                   /* 11 Timeout occured            */
  ST_ERROR_DEVICE_BUSY                /* 12 Device is currently busy   */
};

/*!\brief API returns only common ST error codes */
#define FMDSW_Error_t  U32

/*!\brief Identify the FMD HW device for an instance of the FMD algorithm.

The FMDSW API supports mutliple instances with a device context per instance.\n
The context of the device is initialized in FMDSW_Init() and
a device index is passed as a parameter for other API functions.
The device index is used to retrieved the internal device context of the
algorithm.

\see FMDSW_Init()
*/
typedef enum
{
  kFmdDeviceMain,        /*!< Main FMD device */
  kFmdDeviceSecondary,   /*!< Secondary FMD device */
  kMaxNbOfFmdDevices     /*!< Maximum number of instances */
} FMDSW_FmdDevice_t;


/*!\brief FMD HW analysis status

\see \n \ref REF2
\see FMDSW_FmdHwThreshold_t
\see FMDSW_Process()
*/
typedef struct
{
  U32 ulCfd_sum;
  U32 ulField_sum;
  U32 ulPrev_bd_count;
  U32 ulScene_count;
  ST_BOOL bMove_status;
  ST_BOOL bRepeat_status;
  U32 ulH_nb;
  U32 ulV_nb;
} FMDSW_FmdHwStatus_t;

/*!\brief FMD HW analysis thresholds

The meaning of the thresholds is detailed in FMDSW_FmdHwStatus_t.
\see \n \ref REF2
\see FMDSW_FmdHwStatus_t
\see FMDSW_Process()
*/
typedef struct
{
  U32 ulT_scene;
  U32 ulT_repeat;
  U32 ulT_mov;
  U32 ulT_num_mov_pix;
  U32 ulT_noise;
} FMDSW_FmdHwThreshold_t;

/*!\brief FMD HW analysis window

This window defines the area in the \b active video for FMD analysis.
It is computed by FMDSW_SetSource() based on the source active window and
shall be used in order to program the FMD HW cell.

\warning
The windows are defined in a field not in a frame.\n
The upper left corner in a field is (0,0).

\see FMDSW_SetSource()
*/
typedef struct
{
  U32 ulFmdvl_start;
  U32 ulFmdvp_start;
  U32 ulH_nb;
  U32 ulV_nb;
  U32 ulH_block_sz;
  U32 ulV_block_sz;
  U32 ulOffsetX;
  U32 ulOffsetY;
  U32 ulWidth;
  U32 ulHeight;
} FMDSW_FmdHwWindow_t;


/****************************************************************************/
/*     Structures & Functions API                                           */
/****************************************************************************/

/*! \brief FMD, RFD and SCD SW algorithms parameters.

Thresholds are set-up at device initialization and can not be changed during
run time, see FMDSW_Init().\n
The size of a block for BBD is also a parameter of the SW algorithm because
different FMD HW cells have different fixed block sizes:

\see FMDSW_Init()
*/
typedef struct
{
  U32 ulH_block_sz;  /*!< Horizontal block size for BBD */
  U32 ulV_block_sz;  /*!< Vertical block size for BBD */
  U32 count_miss;    /*!< Delay for a film mode detection */
  U32 count_still;   /*!< Delay for a still mode detection */
  U32 t_noise;       /*!< Noise threshold */
  U32 k_cfd1;        /*!< Consecutive field difference factor 1 */
  U32 k_cfd2;        /*!< Consecutive field difference factor 2 */
  U32 k_cfd3;        /*!< Consecutive field difference factor 3 */
  U32 t_mov;         /*!< Moving pixel threshold */
  U32 t_num_mov_pix; /*!< Moving block threshold */
  U32 t_repeat;      /*!< Threshold on BBD for a field repetition */
  U32 t_scene;       /*!< Threshold on BBD for a scene change  */
  U32 k_scene;       /*!< Percentage of blocks with BBD > t_scene */
  U8  d_scene;       /*!< Scene change detection delay (1,2,3 or 4) */
} FMDSW_FmdInitParams_t;

/*! \brief FMDSW parameters for standalone debugging.

\see \n FMDSW_Init()
*/
typedef struct {
  U32   ulFieldStart;
  char *pcLogFile;
} FMDSW_FmdDebugParams_t;

/*! \brief Results of FMD, RFD and SCD SW analysis.

Field Motion, Repeat Field and Scene Change information for the current field
is returned by FMDSW_Process() in a FMDSW_FmdFieldInfo_t structure.

\see FMDSW_Process()
*/
typedef struct
{
  U8      ucFieldIndex;
  U8      ucPattern;
  ST_BOOL bPhase;
  ST_BOOL bFilmMode;
  ST_BOOL bStillMode;
  ST_BOOL bSceneChange;
} FMDSW_FmdFieldInfo_t;

/*!\brief Field parity */
typedef enum
{
  FMDSW_kBottom,
  FMDSW_kTop
} FMDSW_FieldParity_t;

/*!\brief Vertical frequency of the source */
typedef enum
{
  k50Hz,
  k60Hz
} FMDSW_SourceFrequency_t;

/*!\brief Source characteristics used by FMDSW module

The source characteristics are inputs of FMDSW_SetSource(). The API uses the
source crop window to compute and return the FMD analysis window.
The FMD analysis window shall be programmed in the FMD HW cell.

\see \n FMDSW_SetSource()
*/
typedef struct
{
  U32 ulOffsetX;
  U32 ulOffsetY;
  U32 ulWidth;
  U32 ulHeight;
  FMDSW_SourceFrequency_t eFmdSourceFreq;
} FMDSW_SourceParams_t;


#ifdef USE_FMD

#ifdef BUILTIN_FMD

  /* call FMD library directly */
  #define BIOS_FUNC1(call, name, type1, arg1) \
    FMDSW_Error_t \
    name (type1 arg1);

  #define BIOS_FUNC2(call, name, type1, arg1, type2, arg2) \
    FMDSW_Error_t \
    name (type1 arg1, type2 arg2);

  #define BIOS_FUNC3(call, name, type1, arg1, type2, arg2, type3, arg3) \
    FMDSW_Error_t \
    name (type1 arg1, type2 arg2, type3 arg3);

  #define BIOS_FUNC4(call, name, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
    FMDSW_Error_t \
    name (type1 arg1, type2 arg2, type3 arg3, type4 arg4);

#else /* !BUILTIN_FMD */

  #include <linux/stm/st_bios.h>

  /* FMD library is in BIOS */
  #define BIOS_FUNC1(call, name, type1, arg1) \
    static inline FMDSW_Error_t \
    name (type1 arg1) \
    { \
      return INLINE_BIOS (call, 1, arg1); \
    }

  #define BIOS_FUNC2(call, name, type1, arg1, type2, arg2) \
    static inline FMDSW_Error_t \
    name (type1 arg1, type2 arg2) \
    { \
      return INLINE_BIOS (call, 2, arg1, arg2); \
    }

  #define BIOS_FUNC3(call, name, type1, arg1, type2, arg2, type3, arg3) \
    static inline FMDSW_Error_t \
    name (type1 arg1, type2 arg2, type3 arg3) \
    { \
      return INLINE_BIOS (call, 3, arg1, arg2, arg3); \
    }

  #define BIOS_FUNC4(call, name, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
    static inline FMDSW_Error_t \
    name (type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
    { \
      return INLINE_BIOS (call, 4, arg1, arg2, arg3, arg4); \
    }

#endif /* BUILTIN_FMD */


/*!\brief Initialize a device instance of the FMDSW module */
BIOS_FUNC3 (37, FMDSW_Init,
            const FMDSW_FmdDevice_t,      eDeviceIndex,
            const FMDSW_FmdInitParams_t  * const, pstFmdInitParams,
            const FMDSW_FmdDebugParams_t * const, pstFmdDebugParams);

/*!\brief Calculate the thresholds to be used by FMD hardware.
Thresholds depend on the initialization parameters only.
*/
BIOS_FUNC2 (38, FMDSW_CalculateHwThresh,
            const FMDSW_FmdDevice_t,  eDeviceIndex,
            FMDSW_FmdHwThreshold_t   * const, hw_thresh);

/*!\brief Calculate the source characteristics used by FMD hard- and software */
BIOS_FUNC3 (39, FMDSW_CalculateHwWindow,
            const FMDSW_FmdDevice_t,    eDeviceIndex,
            const FMDSW_SourceParams_t * const, source_params,
            FMDSW_FmdHwWindow_t        * const, hw_window);

/*!\brief Set the source characteristics used by FMDSW module */
BIOS_FUNC2 (40, FMDSW_SetSource,
            const FMDSW_FmdDevice_t,   eDeviceIndex,
            const FMDSW_FmdHwWindow_t * const, pstFmdHwWindow);

/*!\brief Reset FMD algorithm */
BIOS_FUNC1 (41, FMDSW_Reset,
            const FMDSW_FmdDevice_t, eDeviceIndex);

/*!\brief Process FMD, RFD and SCD SW analysis */
BIOS_FUNC4 (42, FMDSW_Process,
            const FMDSW_FmdDevice_t,   eDeviceIndex,
            const FMDSW_FmdHwStatus_t * const, pstFmdHwStatus,
            FMDSW_FmdFieldInfo_t      * const, pstFmdFieldInfo,
            const ST_BOOL,             bSkip);

/*!\brief Terminate an instance of FMDSW module */
BIOS_FUNC1 (43, FMDSW_Terminate,
            const FMDSW_FmdDevice_t, eDeviceIndex);

#else /* !USE_FMD */

  /* so the compiler will not warn about uninitialized/unused variables etc.
     because the code in question will not even be compiled without USE_FMD */
#  define FMDSW_Init(x,y,z)                ST_ERROR_FEATURE_NOT_SUPPORTED
#  define FMDSW_Terminate(x)               ST_ERROR_FEATURE_NOT_SUPPORTED
#  define FMDSW_CalculateHwThresh(x,y)     ST_ERROR_FEATURE_NOT_SUPPORTED
#  define FMDSW_Reset(x)                   ST_ERROR_FEATURE_NOT_SUPPORTED
#  define FMDSW_CalculateHwWindow(x,y,z)   ( { g_pIOS->ZeroMemory (z, sizeof (*(z))); } )
#  define FMDSW_SetSource(x,y)             ST_ERROR_FEATURE_NOT_SUPPORTED
#  define FMDSW_Process(x,y,z,a)           ( { } )

#endif /* USE_FMD */

#ifdef __cplusplus
}
#endif

#endif /** @} __FMDSW_H */
