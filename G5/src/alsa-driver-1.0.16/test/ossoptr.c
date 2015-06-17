#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>

int main (void)
{
  //short buf[256*2*100];
  short buf[6*256];
  int hifi, precision=8, stereo=1, rate=8000, old = 0x7ffffff, x;
  struct count_info info;
  
  hifi = open ("/dev/dsp", O_WRONLY);
  if (hifi < 0 ||
  ioctl(hifi, SNDCTL_DSP_SAMPLESIZE, &precision) == -1 ||
  ioctl(hifi, SNDCTL_DSP_STEREO, &stereo) == -1 ||
  ioctl(hifi, SNDCTL_DSP_SPEED, &rate) == -1) {
    fprintf (stderr, "Unable to open sound device\n");
    return 3;
  }
  memset(buf, 0, sizeof(buf));
  write(hifi, buf, sizeof(buf));
  for (x=0;;x++) {
    //usleep (50000); // Take this out, to experiment
    if (ioctl (hifi, SNDCTL_DSP_GETOPTR, &info) < 0) {
    	perror("ioctl");
    	return 4;
    }
    if (old != info.bytes) {
	    printf ("%c %10d %10d\n", old < info.bytes ? '*' : ' ', info.bytes, info.bytes % 256);
	    old = info.bytes;
    }
  }
  return 0;
}
