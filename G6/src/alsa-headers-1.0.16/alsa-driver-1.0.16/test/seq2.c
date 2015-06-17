#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/soundcard.h>

#define DEVICE "/dev/sequencer"

int main(int argc, char *argv[])
{
	int fd;
	ssize_t res;
	unsigned char ev[4];

	fd = open(DEVICE, O_RDWR);
	if (fd < 0) { perror( "open (" DEVICE ")" ); return EXIT_FAILURE; }
	ev[0] = SEQ_MIDIPUTC;
	ev[1] = 0xfe;		/* active sensing */
	ev[2] = argc > 1 ? atoi(argv[1]) : 0;
	res = write(fd, &ev, sizeof(ev));
	printf("end res = %li\n", (long)res);
	close(fd);
	return EXIT_SUCCESS;
}
