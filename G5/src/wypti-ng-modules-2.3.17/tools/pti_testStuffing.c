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
#include <sys/poll.h>

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
    int len;
    int pid;
    int pkts = 0;
    struct pollfd pfd[4];
    int recv[4] = { 0, 0, 0, 0};
    int stuffing[4] = { 0, 0, 0, 0};

    for (n=0;n<4;n++) {
        sprintf(temp,"/dev/dvb/adapter2/dvr%d",n);
        if ((dvr_fd[n] = open(temp,O_WRONLY)) < 0) {
            printf("Couldn't open '%s'\n",temp);
            exit(-1);
        }
        
        if ((dvr_fd2[n] = open(temp,O_RDONLY | O_NONBLOCK)) < 0) {
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

    struct dmx_pes_filter_params param, param2;
    param.pid = 0x0000;
    param.input = DMX_IN_DVR;
    param.output = DMX_OUT_TS_TAP;
    param.pes_type = DMX_PES_OTHER;
    param.flags = DMX_IMMEDIATE_START;

    for (n=0;n<4;n++) {
	res = ioctl(demux_fd[n], DMX_SET_PES_FILTER, &param);
	if (res) { printf("error setting demux section on %d returned %s\n",n,strerror(errno)); }
    }

    static unsigned char packet[188];

    pfd[0].fd = dvr_fd2[0];
    pfd[0].events = POLLIN;
    pfd[1].fd = dvr_fd2[1];
    pfd[1].events = POLLIN;
    pfd[2].fd = dvr_fd2[2];
    pfd[2].events = POLLIN;
    pfd[3].fd = dvr_fd2[3];
    pfd[3].events = POLLIN;

    while(pkts < 100) {
        for(n=0; n<4; n++) {
            int m;
            res = write(dvr_fd[n],data,188*16);
            if (res != (188*16))
                printf("Got result %d from %d\n",res,n);
            
            for(m=0; m<16; m++) {
                if (poll(pfd,4,1)){
                    if (pfd[n].revents & POLLIN){
                        len = read(dvr_fd2[n],packet,188);
                        if (len < 0){
                            perror("recording");
                            return -1;
                        }
                        if (len!=188) {
                            printf("!!! Received result %d from %d\n",res,n);
                        }
                        if ((packet[3] & (ADAPTATION_FIELD | PAYLOAD_EXISTS)) == 0) {
                            stuffing[n]++;
                        }
                        if (res==188) {
                            recv[n]++;
                        }
                    }
                }
            }
        }

        if (pkts % 10 == 0) printf("Injected: %d\n", pkts);

        pkts++;
    }

    if (recv[0] > 0 || stuffing[0] > 0) printf("Channel 0: FAIL: RECV: %d / STUFFING: %d\n", recv[0], stuffing[0]); 
    if (recv[1] > 0 || stuffing[1] > 0) printf("Channel 1: FAIL: RECV: %d / STUFFING: %d\n", recv[1], stuffing[1]);
    if (recv[2] > 0 || stuffing[2] > 0) printf("Channel 2: FAIL: RECV: %d / STUFFING: %d\n", recv[2], stuffing[2]);
    if (recv[3] > 0 || stuffing[3] > 0) printf("Channel 3: FAIL: RECV: %d / STUFFING: %d\n", recv[3], stuffing[3]); 
}
