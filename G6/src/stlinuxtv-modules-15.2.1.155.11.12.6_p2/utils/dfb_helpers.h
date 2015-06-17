#ifndef __DFB_HELPERS_H__
#define __DFB_HELPERS_H__

#include <string.h>
#include <direct/debug.h>
#include <direct/clock.h>


typedef struct {
     int       magic;

     int       frames;
     float     fps;
     long long fps_time;
     char      fps_string[20];
} FPSData;


static void
__attribute__((unused))
fps_init (FPSData *data)
{
  D_ASSERT (data != NULL);

  memset (data, 0, sizeof (FPSData));
  data->fps_time = direct_clock_get_millis ();

  D_MAGIC_SET (data, FPSData);
}

static inline void
__attribute__((unused))
fps_count (FPSData *data,
           int      interval)
{
  long long diff;
  long long now = direct_clock_get_millis ();

  D_MAGIC_ASSERT (data, FPSData);

  ++data->frames;

  diff = now - data->fps_time;
  if (diff >= interval)
    {
      data->fps = data->frames * 1000 / (float) diff;

      snprintf (data->fps_string, sizeof (data->fps_string),
                "%.1f", data->fps);

      data->fps_time = now;
      data->frames   = 0;
    }
}


#ifndef NDEBUG

#include <errno.h>
#include <direct/system.h>

#define NOCOL      "\033[0m"

#define BLACKCOL   "\033[0;30m"
#define LGRAYCOL   "\033[0;37m"

#define REDCOL     "\033[0;31m"          // dvb warning
#define GREENCOL   "\033[0;32m"          // ui message
#define BROWNCOL   "\033[0;33m"
#define BLUECOL    "\033[0;34m"          // dvb message
#define PURPLECOL  "\033[0;35m"          // ui warning
#define CYANCOL    "\033[0;36m"

#define DGRAYCOL   "\033[1;30m"          // ca message
#define WHITECOL   "\033[1;37m"          // ca warning

#define LREDCOL     "\033[1;31m"
#define LGREENCOL   "\033[1;32m"         // dfb message
#define YELLOWCOL   "\033[1;33m"         // gfx print
#define LBLUECOL    "\033[1;34m"         // osd layer print
#define LPURPLECOL  "\033[1;35m"         // dfb warning
#define LCYANCOL    "\033[1;36m"         // scl layer print


#ifndef ltv_print
#define ltv_print(format, args...) \
  ( { printf ("(%.5d) " __FILE__ ": " format, direct_gettid (), ## args); } )
#endif
#ifndef ltv_message
#define ltv_message(format, args...) \
  ( { printf (BLUECOL "(%.5d) message: " __FILE__ "(%d): " format NOCOL "\n", direct_gettid (), __LINE__ , ## args); } )
#endif
#ifndef ltv_warning
#define ltv_warning(format, args...) \
  ( { printf (REDCOL "(%.5d) warning: " __FILE__ "(%d): " format NOCOL "\n", direct_gettid (), __LINE__, ## args); } )
#endif

#define DFBCHECK(x...)                                         \
  ( {                                                          \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        typeof(errno) errno_backup = errno;                    \
        ltv_warning ("%s <%d>", __FILE__, __LINE__ );     \
        DirectFBError (#x, err);                               \
        errno = errno_backup;                                  \
      }                                                        \
    err;                                                       \
  } )

#define LTV_CHECK(x...)                                               \
  ( {                                                                 \
    int ret = x;                                                      \
    if (ret < 0)                                                      \
      {                                                               \
        typeof(errno) errno_backup = errno;                           \
        ltv_warning ("%s: %s: %d (%m)", __FUNCTION__, #x, errno);     \
        errno = errno_backup;                                         \
      }                                                               \
    ret;                                                              \
  } )

#else /* NDEBUG */

#ifndef ltv_print
#define ltv_print(format, args...)    ( { } )
#endif
#ifndef ltv_message
#define ltv_message(format, args...)  ( { } )
#endif
#ifndef ltv_warning
#define ltv_warning(format, args...)  ( { } )
#endif

#define DFBCHECK(x...)      ( { x; } )
#define LTV_CHECK(x...) ( { x; } )

#endif /* NDEBUG */


#endif /* __DFB_HELPERS_H__ */
