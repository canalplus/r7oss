#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "linux/stm/mss.h"

typedef unsigned int uint32_t;

#define PAGE_SIZE (4*1024)
#define Printf printf

#define NAME "mss_test_user"


#define BUF_OFFSET	7
#define BUF_SIZE	(3*4*1024)


const char fill_byte1 = 0x62;
const char fill_byte2 = 0x99;

static int fd_src;
static int fd_sink;

static void fill_buffer(char *buffer, int size, char fill)
{
	memset(buffer, fill, size);
	return;
}

static int validate_buffer(char *buffer, int size, char fill)
{
	int errorcode = 0;
	int i;
	for (i = 0; i < size; i++) {
		if (buffer[i] != fill) {
			printf("Error check :@0x%08x =0x%02hhx (expected 0x%02hhx)\n",
				      (int) &(buffer[i]), buffer[i], fill);
			errorcode = -1;
		}
	}
	return errorcode;
}

void mss_test_connect()
{
	int command;
	if (ioctl(fd_sink, MSS_SINK_CONNECT, &command) < 0) {
		printf("<%s> ioctl failed\n", __FUNCTION__);
		return;
	}
}

void mss_test_disconnect()
{
	int command;
	if (ioctl(fd_sink, MSS_SINK_DISCONNECT, &command) < 0) {
		printf("<%s> ioctl failed\n", __FUNCTION__);
		return;
	}
}

void mss_read(int bytes)
{
	mss_read_test_scatterlist();
}

int mss_read_test_scatterlist()
{
	char		*readbuf;
	char		*read_buf_start; /*full buffer start */
	uint32_t	read_buf_size;
	int num, i;
	char		*start;	/*full buffer start */
	uint32_t	size;
	char		fill;
	int		error;
	int		errorcode = 0;

	readbuf = (char *) malloc(BUF_SIZE + BUF_OFFSET);
	if (readbuf == NULL) {
		printf("Unable to allocate readbuffer of size = %d\n");
		return -1;
	}

	read_buf_start = readbuf;
	read_buf_size = BUF_SIZE + BUF_OFFSET;

	/*	Printf("full buffer @0x%08x-0x%08x\n" ,
	*		(int)read_buf_start,(int)read_buf_start+read_buf_size);
	*/

	for (size = 1; size <= read_buf_size; size = size * 2) {
		Printf("size = %d\n", size);
		error = 0;
		for (start = read_buf_start;
			((uint32_t) (start + size) < (uint32_t) (read_buf_start
			+ read_buf_size)
			&& !error); start++) {

#if 0
			Printf("full buffer @0x%08x-0x%08x\n",
				      (uint32_t) read_buf_start,
				      (uint32_t) read_buf_start + read_buf_size);
			Printf("work buffer @0x%08x-0x%08x size=%d\n",
				      (uint32_t) start, (uint32_t) start + size, size);
#endif

			fill_buffer(read_buf_start, read_buf_size, fill_byte1);

			num = read(fd_sink, start, size);

			if (num < 0) {
				printf("read failed\n", errno);
				error = 1;
				break;
			}

			errorcode = validate_buffer(
				      read_buf_start,
				      (uint32_t) start - (uint32_t) read_buf_start,
				      fill_byte1);
			if (errorcode != 0) {
				Printf("full buffer @0x%08x-0x%08x\n",
					      (uint32_t) read_buf_start,
					      (uint32_t) read_buf_start + read_buf_size
					      );
				Printf("work buffer @0x%08x-0x%08x size=%d\n",
					      (uint32_t) start,
					      (uint32_t) start + size,
					      size);
				printf("Validate buffer Error :	before buffer\n");
				error = 1;
			}


			errorcode = validate_buffer(start, size, fill_byte2);
			if (errorcode != 0) {
				Printf("full buffer @0x%08x-0x%08x\n",
					      (uint32_t) read_buf_start,
					      (uint32_t) read_buf_start + read_buf_size);

				Printf("work buffer @0x%08x-0x%08x size=%d\n",
					      (uint32_t) start,
					      (uint32_t) start + size, size);
				printf("Validatebuffer Error:inside buffer\n");
				error = 1;
			} else if (error == 1) {
				Printf("Inside buffer : validate buffer ok\n");
			}


			errorcode = validate_buffer(
				      ((char *) (uint32_t) start + size),
				      read_buf_size - ((uint32_t) start + size -
				      (uint32_t) read_buf_start),
				      fill_byte1);
			if (errorcode != 0) {
				Printf("full buffer @0x%08x-0x%08x\n",
					      (uint32_t) read_buf_start,
					      (uint32_t) read_buf_start + read_buf_size);

				Printf("work buffer @0x%08x-0x%08x size=%d\n",
					      (uint32_t) start,
					      (uint32_t) start + size, size);
				printf("Validate bufferError:After buffer\n");
				error = 1;
			} else if (error == 1) {
				Printf("After buffer : validate buffer ok\n");
			}

		}
		if (error) {
			printf("error : break loop\n");
			break; /*  for loop for all sizes */
		}
	}

	free(readbuf);

#define BIG_BUFFER_SIZE  (512*1024+7)

	/* #define BIG_BUFFER_SIZE  188*192*2 */		/* same as gst-apps buffer size */
	readbuf = (char *) malloc(BIG_BUFFER_SIZE);
	if (readbuf == NULL) {
		printf("unable to allocate readbuf of size = %d\n",
			      BIG_BUFFER_SIZE);
		return -1;
	}

	fill_buffer(readbuf,
		      BIG_BUFFER_SIZE,
		      fill_byte1);
	num = read(fd_sink, readbuf, BIG_BUFFER_SIZE);

	if (num < 0)
		printf("read failed\n", errno);


	errorcode = validate_buffer(readbuf,
		      BIG_BUFFER_SIZE, fill_byte2);
	if (errorcode != 0) {
		printf("error in big buffer\n");
		return -1;
	}
	free(readbuf);
}



int main(int argc, char *argv[])
{
	char option;
	int bytes = 0;
	printf("MSS test application started\n");
	while (1) {
		printf("\nenter option(o/r/w/c/d/e):");
		scanf("%c", &option);
		switch (option) {
		case 'o':
			fd_sink = open("/dev/mss_sink0", O_RDWR);
			if (fd_sink < 0)
				printf(" open failed\n");
			fd_src = open("/dev/mss_src0", O_RDWR);
			if (fd_src < 0)
				printf(" open failed\n");
			break;
		case 'r':
			printf("Enter the number of bytes to read");
			scanf("%d", &bytes);
			mss_read(bytes);
			break;
		case 'w':
			printf("Enter the number of bytes to write");
			scanf("%d", &bytes);
			/* mss_write(bytes); */
			break;
		case 'c':
			printf("connect memsink to memsrc\n");
			mss_test_connect();
			break;
		case 'd':
			printf("disconnect memsink to memsrc\n");
			mss_test_disconnect();
			break;
		case 'e':
			close(fd_sink);
			close(fd_src);
			goto end;
			break;
		default:
			break;
		}
	}
end:
	return 0;
}
