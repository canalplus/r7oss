#include <sys/types.h>
#include <sys/ioctl.h>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include <linux/fb.h>

#include <include/stmdisplaytypes.h> /* ULONG */

/*
 * This test builds against the version of stmfb.h in this source tree, rather
 * than the one that is shipped as part of the kernel headers package for
 * consistency. Normal user applications should use <linux/stmfb.h>
 */
#include <linux/drivers/video/stmfb.h>


#if 0
  include/stmdisplayplane.h

  OUTPUT_NONE   = 0,
  OUTPUT_BKG    = (1L<<0),
  OUTPUT_VID1   = (1L<<1),
  OUTPUT_VID2   = (1L<<2),
  OUTPUT_GDP1   = (1L<<3),
  OUTPUT_GDP2   = (1L<<4),
  OUTPUT_GDP3   = (1L<<5),
  OUTPUT_GDP4   = (1L<<6),
  OUTPUT_CUR    = (1L<<9),
  OUTPUT_VBI    = (1L<<13),  // A plane linked to a GDP for VBI waveforms
  OUTPUT_NULL   = (1L<<14),  // A Null plane for virtual framebuffer support
  OUTPUT_ALP    = (1L<<15),
#endif

int main(int argc, char **argv)
{
  if(argc < 3) {
    fprintf(stderr, "%s (0|1) planeid [planeid planeid ...]\n", argv[0]);
    return 1;
  }

  int fbfd;
  char *devname       = "/dev/fb0";

  if((fbfd = open(devname, O_RDWR)) < 0)
  {
    fprintf(stderr, "Unable to open %s: %d (%m)\n", devname, errno);
    return 2;
  }

  int en = (argv[1][0] == '1');
  ULONG planes = 0;
  int i;
  for(i = 2; i < argc; i++) {
    errno = 0;
    unsigned long id = strtoul(argv[i], NULL, 0);
    if(errno != 0) {
      fprintf(stderr, "invalid plane id '%s'\n", argv[i]);
      return 3;
    }

    planes |= 1 << id;
    printf("planes[%ld]:%d\n", id, 1);
    printf("planes = 0x%08lX\n", planes);
  }

  if(en) {
    planes |= (1 << (sizeof(planes) * 8 - 1));
  }
  printf("planes = 0x%08lX (en = %d)\n", planes, en);

  int ret = ioctl(fbfd, STMFBIO_SET_PLANES, planes);
  if(ret) {
    printf("ret = %d\n", ret);
  }


  return 0;
}
