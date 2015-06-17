/*
 * gamut-transmission.c
 *
 * Copyright (C) STMicroelectronics Limited 2012. All rights reserved.
 *
 * Test to exercise the sending of gamut metadata info frames
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

#include <linux/kernel/drivers/stm/hdmi/stmhdmi.h>

#define HDMIDEV "/dev/hdmi0.0"

int main(int argc, char *argv[])
{
        struct stmhdmiio_data_packet gbd = {};
        int fd;
        int result;

        fd = open(HDMIDEV, O_RDWR);
        assert(fd);

        gbd.type = 0xA;
        gbd.version = 0;
        gbd.length=0x30;
        if (argc >= 2)
        {
           if (argv[1][1]!='c')
           {
              gbd.version = strtol(argv[1], NULL, 0);
              if (argc > 2)
               gbd.length = strtol(argv[2], NULL, 0);
           }
           else
           {
              result = ioctl(fd, STMHDMIIO_FLUSH_DATA_PACKET_QUEUE, STMHDMIIO_FLUSH_GAMUT_QUEUE);
              if(result<0)
                perror("failed");
              close(fd);
              return 0;
           }
        }
        result = ioctl(fd, STMHDMIIO_SEND_DATA_PACKET, &gbd);
        if(result<0)
          perror("failed");
        close(fd);
        return 0;
}
