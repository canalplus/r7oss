/*
   [ -z ${STM_BUILD_ID} ] && export STM_BUILD_ID=2.4
   [ -Z ${ARCH} ] && export ARCH=sh4
   [ -z ${STM_BASE_PREFIX} ] && export STM_BASE_PREFIX=/opt/STM/STLinux-${STM_BUILD_ID}
   [ -z ${PKG_CONFIG} ] && export PKG_CONFIG=${STM_BASE_PREFIX}/host/bin/pkg-config
   [ -z ${PKG_CONFIG_SYSROOT_DIR} ] && export PKG_CONFIG_SYSROOT_DIR=${STM_BASE_PREFIX}/devkit/${ARCH}/target
   [ -z ${PKG_CONFIG_LIBDIR} ] && export PKG_CONFIG_LIBDIR=${PKG_CONFIG_SYSROOT_DIR}/usr/lib/pkgconfig

  sh4-linux-gcc \
     -Wall -Os -g3 \
     -lrt \
     bltload.c \
     -o ${PKG_CONFIG_SYSROOT_DIR}/tmp/bltload
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/stmfb.h>
#include <linux/stm/bdisp2_shared_ioctl.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define USEC_PER_SEC 1000000
# define TIMESPEC_TO_TIMEVAL(tv, ts) {                              \
    (tv)->tv_sec = (ts)->tv_sec;                                    \
    (tv)->tv_usec = (ts)->tv_nsec / 1000;                           \
}

int
main (int argc, char *argv[])
{
  struct timespec current;
  struct timeval  oldtime[2];

  int fd_k = -1, fd_u = -1;

  /* for a description of these names, have a look in the stgfx2 source code
     in stm-gfxdriver.c */
  /* device names as used by stgfx2 */
  static const char *devnames_u[] = {
    "/dev/stm-bdispII.1.1", /* STiH415 */
    "/dev/stm-bdispII.1.4", /* STi7108 */
    "/dev/stm-bdispII.1",   /* STi7105 */
    "/dev/stm-bdispII.0",   /* STi7109 */
  };
  /* devices used inside the kernel */
  static const char *devnames_k[] = {
    "/dev/stm-bdispII.0.0", /* STiH415 */
    "/dev/stm-bdispII.0.0", /* STi7108 */
    "/dev/stm-bdispII.0",   /* STi7105 */
    NULL,   /* STi7109 */
  };
  unsigned int i;

  for (i = 0; i < sizeof(devnames_u) / sizeof(devnames_u[0]); ++i)
    {
      fd_u = open (devnames_u[i], O_RDONLY);
      if (fd_u == -1)
        continue;
      fd_k = open (devnames_k[i], O_RDONLY);
      break;
    }

  if (fd_u == -1)
    fd_u = open ("/dev/fb0", O_RDONLY);

  if (fd_u == -1)
    {
      fprintf (stderr, "Could not open blitter device\n");
      exit (1);
    }

  clock_gettime (CLOCK_MONOTONIC, &current);
  TIMESPEC_TO_TIMEVAL (&oldtime[0], &current);
  oldtime[1] = oldtime[0];

  while (1)
    {
      int i;
      int fds[2] = { fd_u, fd_k };
      char buf[100];
      int pos = 0;

      for (i = 0; (i < 2) && (fds[i] != -1); ++i)
        {
          unsigned long load1;
          unsigned long long load2 = -1;
          unsigned long long ticks_past;
          struct timeval currenttime, difftime;

          if (ioctl (fds[i], STM_BLITTER_GET_BLTLOAD2, &load2) != 0
              && i == 0)
            {
              /* fallback for stmfb */
              if (ioctl (fds[i], STM_BLITTER_GET_BLTLOAD, &load1) == 0)
                load2 = load1;
            }

          if (load2 != -1)
            {
//              printf ("load %llu\n", load2);
              clock_gettime (CLOCK_MONOTONIC, &current);
              TIMESPEC_TO_TIMEVAL (&currenttime, &current);
              timersub (&currenttime, &oldtime[i], &difftime);

              ticks_past = (difftime.tv_sec * USEC_PER_SEC + difftime.tv_usec);
              oldtime[i] = currenttime;

              pos += snprintf (&buf[pos], sizeof (buf) - pos,
                               "past[%c]: %6llu -> stat: %8llu  ", (i == 0) ? 'u' : 'k', ticks_past, load2);
              pos += snprintf (&buf[pos], sizeof (buf) - pos,
                               "load %3.2f%%    ", (double) (load2 * 100) / ticks_past);
            }
          printf ("%s\r", buf);
          usleep (100 * 1000);
        }
    }

  return 0;
}
