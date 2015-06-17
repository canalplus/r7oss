#include <stdio.h>
#include <stdlib.h>
#include <asm/ioctls.h>

#define CONSOLE		10		/* Alt + Fx */

int main(void)
{
	char console[2] = {11, CONSOLE};	/* cmd, console */

	if (ioctl(0, TIOCLINUX, &console) < 0)
		perror("TIOCLINUX");
}
