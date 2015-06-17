#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/soundcard.h>

#define DEVICE "/dev/dsp"

void main( void )
{
  int fd, i, r;
  unsigned char buffer[256*1024];

  memset( buffer, 0x80, sizeof( buffer ) );	/* fill with silence */
  
  fd = open( DEVICE, O_WRONLY );
  if ( fd < 0 ) { perror( "open (playback)" ); return; }
  i = AFMT_S16_LE;
  if ( ( r = ioctl( fd, SNDCTL_DSP_SETFMT, &i ) ) < 0 ) { perror( "setfmt" ); return; }
  printf( "afmt = %i, result = %i (%i)\n", AFMT_S16_LE, i, r );
  i = 1;
  if ( ( r = ioctl( fd, SNDCTL_DSP_CHANNELS, &i ) ) < 0 ) { perror( "channels" ); return; }
  printf( "channels = 1, result = %i (%i)\n", i, r );
  i = 22050;
  if ( ( r = ioctl( fd, SNDCTL_DSP_SPEED, &i ) ) < 0 ) { perror( "rate" ); return; }
  printf( "rate = 22050, result = %i (%i)\n", i, r );
  i = 0x0008;
  if ( ( r = ioctl( fd, SNDCTL_DSP_SETFRAGMENT, &i ) ) < 0 ) { perror( "setfragment" ); return; }
  printf( "fragment = 0x20008, result = 0x%x (%i)\n", i, r );
  printf( "write - begin\n" );
  while ( 1 ) {
    write( fd, buffer, 1024 );
    usleep( 10000 );
  }
  printf( "write - end\n" );
  close( fd );
}
