#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/soundcard.h>

#define DEVICE "/dev/sequencer"

int main(void)
{
	int fd;
	ssize_t res;
	unsigned char ev[4];

	fd = open(DEVICE, O_RDWR);
	if (fd < 0) { perror( "open (" DEVICE ")" ); return EXIT_FAILURE; }
	while ((res = read(fd, &ev, sizeof(ev))) == sizeof(ev)) {
	  	printf("read event: bytes = 0x%x,0x%x,0x%x,0x%x\n", ev[0], ev[1], ev[2], ev[3]);
	}
	printf("end res = %li\n", (long)res);
	close(fd);
	return EXIT_SUCCESS;
}
