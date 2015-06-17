/*
 * isrc-transmission.c
 *
 * Copyright (C) STMicroelectronics Limited 2008. All rights reserved.
 *
 * Test to exercise the sending of ISRC info frames
 */
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <linux/drivers/stm/coredisplay/stmhdmi.h>

#define HDMIDEV "/dev/hdmi0.0"

int main(int argc, char *argv[])
{
        struct stmhdmiio_isrc_data isrc = {};
        struct timespec currenttime;
        int fd;
        int result;
        int i;

        fd = open(HDMIDEV, O_RDWR);
        assert(fd);

        for(i=0;i<32;i++)
        {
          isrc.upc_ean_isrc[i] = (unsigned char)i;
        }

        clock_gettime(CLOCK_MONOTONIC,&currenttime);

        isrc.timestamp.tv_sec  = currenttime.tv_sec + 1;
        isrc.timestamp.tv_usec = currenttime.tv_nsec / 1000;
        isrc.status = ISRC_STATUS_STARTING;

        result = ioctl(fd, STMHDMIIO_SET_ISRC_DATA, &isrc);
        if(result<0)
          perror("failed");

        isrc.timestamp.tv_sec  = currenttime.tv_sec + 5;
        isrc.timestamp.tv_usec = currenttime.tv_nsec / 1000;
        isrc.status = ISRC_STATUS_INTERMEDIATE;

        result = ioctl(fd, STMHDMIIO_SET_ISRC_DATA, &isrc);
        if(result<0)
          perror("failed");

        isrc.timestamp.tv_sec  = currenttime.tv_sec + 20;
        isrc.timestamp.tv_usec = currenttime.tv_nsec / 1000;
        isrc.status = ISRC_STATUS_ENDING;

        result = ioctl(fd, STMHDMIIO_SET_ISRC_DATA, &isrc);
        if(result<0)
          perror("failed");

        isrc.timestamp.tv_sec  = currenttime.tv_sec + 25;
        isrc.timestamp.tv_usec = currenttime.tv_nsec / 1000;
        isrc.status = ISRC_STATUS_DISABLE;

        result = ioctl(fd, STMHDMIIO_SET_ISRC_DATA, &isrc);
        if(result<0)
          perror("failed");

        close(fd);

        return 0;
}
