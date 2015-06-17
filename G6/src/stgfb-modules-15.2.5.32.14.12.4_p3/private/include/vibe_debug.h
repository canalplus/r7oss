/***********************************************************************
 *
 * File: private/include/vibe_debug.h
 * Copyright (c) 2007-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Operating System Debug Services Abstract Interface
 *
\***********************************************************************/

#ifndef VIBE_DEBUG_H
#define VIBE_DEBUG_H

#include <vibe_os.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* print in color for level 0,1 */
#define SET_COLOR       27
/* Text attributes */
#define ATTR_OFF        0    /* All attributes off */
#define BOLD            1    /* Bold on */
#define UNDERSCORE      4    /* Underscore (on monochrome display adapter only) */
#define BLINK           5    /* Blink on */
#define REVERSE         7    /* Reverse video on */
#define CONCEALED       8    /* Concealed on */
/* Foreground colors */
#define BLACK           30
#define RED             31
#define GREEN           32
#define YELLOW          33
#define BLUE            34
#define MAGENTA         35
#define CYAN            36
#define WHITE           37
/* Background colors */
#define BK_BLACK        40
#define BK_RED          41
#define BK_GREEN        42
#define BK_YELLOW       43
#define BK_BLUE         44
#define BK_MAGENTA      45
#define BK_CYAN         46
#define BK_WHITE        47

enum trc_id {                     // [family -] trace description
  TRC_ID_BY_CONSOLE,              // TRACE OUTPUT - console
  TRC_ID_BY_KPTRACE,              //                kptrace
  TRC_ID_BY_STM,                  //                stm probe

  TRC_ID_ERROR,                   // GENERAL - error
  TRC_ID_MAIN_INFO,               //           main informations
  TRC_ID_UNCLASSIFIED,            //           unclassified
  TRC_ID_QUEUE_BUFFER,            //           Displays a short description (size, charac...) of the buffers queued
  TRC_ID_PICT_QUEUE_RELEASE,      //           Used to check if all the buffer queued are properly released
  TRC_ID_PICTURE_SCHEDULING,      //           Shows info about the picture selected by the planes for next VSync (useful to understand a freeze)
  TRC_ID_VSYNC,                   //           Display timing info about hard VSync IRQ and Threaded IRQ
  TRC_ID_DISPLAY_CB_LATENCY,      //           Display callback latency info
  TRC_ID_CONTEXT_CHANGE,          //           Indicate when the context has changed

  TRC_ID_STKPI_LOCK_DURATION,     // STKPI - Lock duration
  TRC_ID_API_DEVICE,              //         device APIs
  TRC_ID_API_OUTPUT,              //         output APIs
  TRC_ID_API_PLANE,               //         plane APIs
  TRC_ID_API_SOURCE,              //         source APIs
  TRC_ID_API_SOURCE_PIXEL_STREAM, //         source pixel stream APIs
  TRC_ID_API_SOURCE_QUEUE,        //         source queue APIs
  TRC_ID_API_PIXEL_CAPTURE,       //         pixel capture APIs
  TRC_ID_API_SCALER,              //         scaler APIs
  TRC_ID_API_VXI,                 //         vxi APIs

  TRC_ID_BUFFER_DESCRIPTOR,       // DISPLAY_SOURCE - Print the BufferDescriptor of pictures queued on a Source
  TRC_ID_PICT_REFUSED,            //                  Refused pictures (happens during Swap of video planes)

  TRC_ID_VIDEO_PLANE,             // VIDEO_PLANE - General info
  TRC_ID_FIELD_INVERSION,         //               Highlight field inversions
  TRC_ID_VDP_REG,                 //               VDP registers
  TRC_ID_VDP,                     //               VDP driver
  TRC_ID_HQVDPLITE_REG,           //               HQVDPLITE registers
  TRC_ID_HQVDPLITE_DEI,           //               HQVDPLITE DEI and Motion Buffers
  TRC_ID_HQVDPLITE,               //               HQVDPLITE driver

  TRC_ID_MISR_REG,                // MISR - MISR registers
  TRC_ID_MISR,                    //        MISR driver

  TRC_ID_GDP_PLANE,               // GRAPHIC_PLANE - GDP
  TRC_ID_VBI_PLANE,               //                 VBI

  TRC_ID_DENC,                    // ANALOG_OUTPUT - DENC
  TRC_ID_VTG,                     //                 VTG
  TRC_ID_FSYNTH,                  //                 Frequency synthetizer
  TRC_ID_HDFORMATTER,             //                 HD formatter

  TRC_ID_SCALER,                  // SCALER - scaler

  TRC_ID_HDMI,                    // HDMI - tbd

  TRC_ID_DISPLAY_LINK,            // DISPLAY_LINK - tbd

  TRC_ID_CAPTURE,                 // CAPTURE - tbd

  TRC_ID_HDMI_RX,                 // HDMI_RX - tbd

  TRC_ID_VXI,                     // VXI - tbd

  TRC_ID_VIBE_OS,                 // VIBE_OS - tbd

  TRC_ID_COREDISPLAYTEST,         // COREDISPLAYTEST - tbd
  TRC_ID_COREDISPLAYTEST_MAIN_INFO,//                  Selection of traces useful to understand the behavior of the display test module

  TRC_ID_SDP,                     // SDP - Secure Data Path

  TRC_ID_LAST
};

void vibe_break(const char *cond, const char *file, unsigned int line);
void vibe_printf(const char *fmt,...) __attribute__ ((format(printf,1,2)));
void vibe_kpprintf(const char *fmt,...);
void vibe_trace( int id, char c, const char *pf, const char *fmt, ... ) __attribute__ ((format(printf,4,5)));

/*
 * ASSERTF(Cond, PrintfArg)
 * Halts execution if the condition is false and prints the formated string
 * ASSERTF(ptr == NULL, ("A number %d", number));
 *
 * Definitions if we have access to IOS
 * Always compile asserts
 */
#define SUBTRC(...) TRC( TRC_ID_ERROR, __VA_ARGS__ )
#define ASSERTF(x, y) { if (!(x)) { SUBTRC y ; vibe_break(#x, __FILE__, __LINE__); } }

#ifdef DEBUG
#define DASSERTF(x, y, ...)     ASSERTF(x, y)
#define DASSERTF2(x, y, z, ...) ASSERTF(x, y)
#else
#define DASSERTF(x, y, ...) { if (!(x)) { SUBTRC y ; return __VA_ARGS__ ;} }
#define DASSERTF2(x, y, z, ...) { if (!(x)) { SUBTRC y ; vibe_os_unlock_resource( z ); return __VA_ARGS__ ;} }
#endif


/*
 * These macros are used in order to check an API object handle's validity
 * Note that these cannot be conditional on DEBUG because all of the
 * codebase must be compiled in a consistent fashion for this checking
 * to work correctly.
 */
#define stm_coredisplay_is_handle_valid(obj, type)            \
     ({                                                       \
       int __valid = ((obj) != 0) && ((obj)->magic == (type));\
       STM_VIBE_BUG_ON(!__valid);                      \
       __valid;                                               \
     })

#define stm_coredisplay_magic_set(obj, type) \
    ({                                       \
      (obj)->magic = (type);                 \
    })

#define stm_coredisplay_magic_clear(obj) \
     ({                                  \
       (obj)->magic = 0x0;               \
     })

/*
 * Some basic pointer checking for the API entrypoints
 */
#define ADDR_LOWER_BOUNDARY 4096
#define CHECK_ADDRESS(x) (((uint32_t)(x) >= ADDR_LOWER_BOUNDARY))


//TRC macros
#if defined(CONFIG_DISPLAY_REMOVE_TRACES)

#define IS_TRC_ID_ENABLED( id ) 0
#define TRCGEN( id, c, ... )
#define TRCIN( id, ... )
#define TRCOUT( id, ... )
#define TRC( id, ... )
#define TRCBL( id, ... )

#else // CONFIG_DISPLAY_REMOVE_TRACES

#define IS_TRC_ID_ENABLED( id )                                  \
    (                                                            \
        (id >= (sizeof(trcmask)/sizeof(unsigned int)*32)) ?      \
            trcmask[0] & (1 << TRC_ID_UNCLASSIFIED) :            \
            trcmask[id/32] & (1 << (id % 32))                    \
    )

#define TRCGEN( id, c, ... )                                     \
    if (IS_TRC_ID_ENABLED(id)) {                                 \
        vibe_trace( id, c, __PRETTY_FUNCTION__, __VA_ARGS__ );   \
    }

#define TRCIN( id, ... )                        \
  do {                                          \
    TRCGEN( id, '>', " "__VA_ARGS__ );          \
  } while ( 0 )

#define TRCOUT( id, ... )                       \
  do {                                          \
    TRCGEN( id, '<', " "__VA_ARGS__ );          \
  } while ( 0 )

#define TRC( id, ... )                          \
  do {                                          \
    TRCGEN( id, '.', " "__VA_ARGS__ );          \
  } while ( 0 )

// Print a Blank Line
#define TRCBL( id, ... )                        \
  do {                                          \
    TRCGEN( id, ' ', " " );                     \
  } while ( 0 )

#endif // CONFIG_DISPLAY_REMOVE_TRACES



#ifdef DISPLAY_DEBUG

#define DEBUG_CHECK_VSYNC_LOCK_PROTECTION(dev)            \
    ({                                                    \
      (dev)->DebugCheckVSyncLockTaken(__PRETTY_FUNCTION__);    \
    })

#define DEBUG_CHECK_THREAD_CONTEXT()                                   \
    ({                                                                 \
      if ( vibe_os_in_interrupt() != 0)                                \
        TRC(TRC_ID_ERROR, "Error: %s not called from thread context",  \
                          __PRETTY_FUNCTION__);                        \
    })

#else /* not DISPLAY_DEBUG */

    #define DEBUG_CHECK_VSYNC_LOCK_PROTECTION(dev)
    #define DEBUG_CHECK_THREAD_CONTEXT()

#endif /* DISPLAY_DEBUG */


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* VIBE_DEBUG_H */
