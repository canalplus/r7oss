/*
 * posix_openpt.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

int posix_openpt(int oflag)
{
	return open("/dev/ptmx", oflag);
}
