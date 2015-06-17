#ifndef __DEBUG_HELPER_H__
#define __DEBUG_HELPER_H__


/* Some colour for debug */
#define BLACK   "\033[22;30m"
#define RED     "\033[22;31m"
#define GREEN   "\033[22;32m"
#define BROWN   "\033[22;33m"



/* These messages should be provided through DirectFB's error and warning */
#define DEBUG_ERROR(x)          D_ERROR x
#define DEBUG_WARNING(x)        D_WARN x


#ifdef DEBUG
/* *Very* Verbose */
#  define DEBUG_INFO(x)           D_PRINTF x
/* Outputs various Buffer and surface sizes */
#  define DEBUG_SCALE(x)          D_PRINTF x
/* Outputs Timing Information */
#  define DEBUG_PERFORMANCE(x)    D_PRINTF x

#else

#  define DEBUG_INFO(x)   __debug_msg x
#  define DEBUG_SCALE(x)  __debug_msg x
static inline int __attribute__ ((format (printf, 1, 2)))
__debug_msg (const char *format, ...)
{
  return 0;
}

#endif


#ifdef DIRECT_BUILD_DEBUG

#  define deb_gettimeofday(tv,tz)  gettimeofday(tv, tz)
#  define deb_timersub(a,b,res)    timersub(a, b, res)

#else

#  define deb_gettimeofday(tv,tz)  ( { } )
#  define deb_timersub(a,b,res)    ( { } )

#endif



#endif /* __DEBUG_HELPER_H__ */
