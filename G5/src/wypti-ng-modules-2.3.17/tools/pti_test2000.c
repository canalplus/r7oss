#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/ca.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>

#define EVEN_KEY         (0x2 << 6)
#define ODD_KEY          (0x3 << 6)
#define ADAPTATION_FIELD (1<<5)
#define PAYLOAD_EXISTS   (1<<4)
#define PUSI (1<<6)
#define CC(x) ((x) & 0xf)

int main(int argc, char *argv[])
{
    int dvr_fd[4],dvr_fd2[4], demux_fd[4];
    char temp[128];
    int n;
    static unsigned char data[188*16];
    int res;
    int pid;

    for (n=0;n<4;n++) {
	sprintf(temp,"/dev/dvb/adapter2/dvr%d",n);
	if ((dvr_fd[n] = open(temp,O_WRONLY)) < 0) {
	    printf("Couldn't open '%s'\n",temp);
	    exit(-1);
	}

	if ((dvr_fd2[n] = open(temp,O_RDONLY /*| O_NONBLOCK*/)) < 0) {
	    printf("Couldn't open '%s'\n",temp);
	    exit(-1);
	}

	sprintf(temp,"/dev/dvb/adapter2/demux%d",n);
	if ((demux_fd[n] = open(temp,O_RDWR)) < 0) {
	    printf("Couldn't open '%s'\n",temp);
	    exit(-1);
	}
    }

    for (n=0;n<188*16;n+=188) {
	int m;
	pid = 0x10 /*(0x10+(n/188))*/;
	data[n] = 0x47;
	data[n+1] = (pid >> 8) & 0x1f | PUSI;
	data[n+2] = (pid & 0xff);
	data[n+3] = PAYLOAD_EXISTS | CC(n/188);
	for(m=4;m<188;m++)
	    data[n+m] = n & 0xff;
    }
    
//    for (n=0;n<10;n++) printf("%02x ",data[n]);
//    printf("\n");

    struct dmx_pes_filter_params param;
    param.pid = 0x2000;
    param.input = DMX_IN_DVR;
    param.output = DMX_OUT_TS_TAP;
    param.pes_type = DMX_PES_OTHER;
    param.flags = DMX_IMMEDIATE_START;

    for (n=0;n<4;n++) {
	res = ioctl(demux_fd[n], DMX_SET_PES_FILTER, &param);
	if (res) { printf("error setting demux section on %d returned %s\n",n,strerror(errno)); }
    }

    static unsigned char packet[188];

    while(1) {
       for(n=0; n<4; n++) {
		int m;
		res = write(dvr_fd[n],data,188*16);
		if (res != (188*16))
			printf("Got result %d from %d\n",res,n);

		for(m=0; m<16; m++) {
			while (1) {
				res = read(dvr_fd2[n],packet,188);
				if (res!=188) {
					printf("!!! Received result %d from %d\n",res,n);
				}
				if ((packet[3] & (ADAPTATION_FIELD | PAYLOAD_EXISTS)) != 0) {
					break;
				} else {
					printf("# stuffing packet, ignoring\n");
				}
			}
			if (res==188) {
				res = memcmp(packet, &data[m*188], 188);
				if (res) { 
					int i;
					printf("! packet mismatch on %d\n",n);
					printf("   orig: ");
					for(i = 0; i < 20; i++ )
						printf("%02x ", data[i+m*188]);
					printf("\n");
					printf("   recv: ");
					for(i = 0; i < 20; i++ )
						printf("%02x ", packet[i]);
					printf("\n");
				} else {
					printf("* got a packet\n");
				}
			} else {
				printf("!!! couldn't get a packet from %d cause %d(%s)\n",n,res,strerror(errno));
			}
		}
	}
	printf("one loop\n");
   }
}
