#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>

int main (void)
{
  short buf[256*2*100];
  int hifi, precision=8, stereo=1, rate=8000, odelay, old = 0x7ffffff, x;
  
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
    if (ioctl (hifi, SNDCTL_DSP_GETODELAY, &odelay) < 0) {
    	perror("ioctl");
    	return 4;
    }
    if (old != odelay) {
	    printf ("%c %10d %10d\n", old < odelay ? '*' : ' ', odelay, odelay % 256);
	    old = odelay;
    }
    if (odelay == 0)
      break;
  }
  return 0;
}
