/*
 * hdmi-audio-cfg.c
 *
 * Copyright (C) STMicroelectronics Limited 2008. All rights reserved.
 *
 * Test to exercise the configuration of audio info frame data.
 */
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

#include <linux/drivers/stm/coredisplay/stmhdmi.h>

#define HDMIDEV "/dev/hdmi0.0"

int main(int argc, char *argv[])
{
        struct stmhdmiio_audio info = {
        };
        int fd;
        int result;
        int channel_count = 8;
        int speaker_mapping = 0x1f;

        if (argc >= 2)
                channel_count = strtol(argv[1], NULL, 0);
        if (argc >= 3)
                speaker_mapping = strtol(argv[2], NULL, 0);

        printf("channel_count = %d\n", channel_count);
        printf("speaker_mapping = %d (%#x)\n", speaker_mapping, speaker_mapping);

        fd = open(HDMIDEV, O_RDWR);
        assert(fd);

        info.channel_count = channel_count;
        info.speaker_mapping = speaker_mapping;
        result = ioctl(fd, STMHDMIIO_SET_AUDIO_DATA, &info);
        if(result<0)
          perror("failed");

        close(fd);

        return 0;
}
