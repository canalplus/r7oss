/*
 * acp-transmission.c
 *
 * Copyright (C) STMicroelectronics Limited 2008. All rights reserved.
 *
 * Test to exercise the sending of ACP info frames
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
        struct stmhdmiio_data_packet acp = {};
        int fd;
        int result;

        fd = open(HDMIDEV, O_RDWR);
        assert(fd);

        acp.type = 4;
        if (argc >= 2)
                acp.version = strtol(argv[1], NULL, 0);
        else
                acp.version = 1;

        result = ioctl(fd, STMHDMIIO_SEND_DATA_PACKET, &acp);
        if(result<0)
          perror("failed");

        close(fd);

        return 0;
}
