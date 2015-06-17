#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>

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

#define PACKET_SIZE 188
#define MAX_PIDS 8192

int adapter = 2;
int demux = 0;
int numpids = 8192;
int numpackets = 8192;
int numfilters = 64;
int verbose = 0;

struct filter
{
	int enabled;
	int fd;
	int expected;
	int actual;
};

static inline unsigned short ts_pid(const unsigned char *buf)
{
	return ((buf[1] & 0x1f) << 8) + buf[2];
}

static unsigned char* genTS(const unsigned int numPackets, const unsigned int numPids)
{
	unsigned int count = 0;
	unsigned short pid = 0;
	unsigned char* packets = NULL;
	int m = 0;

	if(numPackets == 0)
		return NULL;

	packets = (unsigned char*)malloc(sizeof(unsigned char)*numPackets*PACKET_SIZE);

	while(count < numPackets) {
		int index = count*PACKET_SIZE;

		packets[index] = 0x47;
		packets[index+1] = ((pid >> 8) & 0x1f) | PUSI;
		packets[index+2] = (pid & 0xff);
		packets[index+3] = PAYLOAD_EXISTS | CC(index/PACKET_SIZE);

		for(m=4;m<PACKET_SIZE;m++)
			packets[index+m] = index & 0xff;

		count++;
		pid++;

		if(pid >= numPids)
			pid = 0;
	}

	return packets;
}

static int filter_pes(const int fd, const short pid)
{
	int res = -1;
	struct dmx_pes_filter_params param;
	param.pid = pid;
	param.input = DMX_IN_DVR;
	param.output = DMX_OUT_TS_TAP;
	param.pes_type = DMX_PES_OTHER;
	param.flags = DMX_IMMEDIATE_START;

	res = ioctl(fd, DMX_SET_PES_FILTER, &param);
	if (res)
	{
		printf("error setting pes filter for PID 0x%03x - %s\n",pid,strerror(errno));
		return 0;
	}

	return 1;
}

static int open_device(const char* filename, int flags)
{
	int fd = -1;

	if ((fd = open(filename,flags)) < 0)
	{
	    printf("Couldn't open '%s'\n",filename);
	    exit(1);
	}

	return fd;
}

static void show_help()
{
	printf("Usage:\n");
	printf("\t-a number : use given adapter (default %d)\n",adapter);
	printf("\t-d number : use given dvr/demux (default %d)\n",demux);
	printf("\t-n number : how many TS packets to generate\n");
	printf("\t-p number : how many PIDs to generate in a sample TS (1-8192)\n");
	printf("\t-f number : how many PIDs to filter on (1-128 or 8192)\n");
	printf("\t-v        : verbose mode\n");
	printf("\t-h        : this help text\n");
}

static void parse_args(int argc, char *argv[])
{
	int c;

	while((c = getopt(argc,argv,"a:d:p:n:f:vh")) != -1)
	{
		switch(c)
		{
		/*Adapter number*/
		case 'a':
			if(sscanf(optarg,"%d",&adapter) != 1)
			{
				printf("Failed to parse adapter number %s\n",optarg);
				exit(1);
			}
			break;
		/*Demux number*/
		case 'd':
			if(sscanf(optarg,"%d",&demux) != 1)
			{
				printf("Failed to parse adapter number %s\n",optarg);
				exit(1);
			}
			break;
		/*Number of PIDS to generate*/
		case 'p':
			if(!((sscanf(optarg,"%d",&numpids) == 1) && (numpids > 0) && (numpids <= 8192)))
			{
				printf("Number of PIDs to generate must be between 1 and 8192\n");
				exit(1);
			}
			break;
		/*Number of packets to generate*/
		case 'n':
			if(!((sscanf(optarg,"%d",&numpackets) == 1) && (numpackets > 0)))
			{
				printf("Number of packets to generate must be greater than 0\n");
				exit(1);
			}
			break;
		/*Number of pids to filter*/
		case 'f':
			if(!((sscanf(optarg,"%d",&numfilters) == 1) &&
			    (((numfilters > 0) && (numfilters <= 128)) || (numfilters == 8192))))
			{
				printf("Number of PIDs to filter must be between 1 and 128, or set to 8192 (pass through all PIDs)\n");
				exit(1);
			}
			break;
		/*verbose*/
		case 'v':
			verbose = 1;
			break;
		/*help*/
		case 'h':
			show_help();
			exit(1);
			break;
		default:
			printf("Unrecognised argument\n\n");
			exit(1);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int dvr,dvr2;
	int res = 0;
	int i = 0;
	int errors = 0;
	char temp[128];
	struct filter filters[MAX_PIDS+1];
	unsigned char packet[PACKET_SIZE];
	unsigned char* stream = NULL;
	fd_set set;
	struct timeval timeout;

	parse_args(argc,argv);

	sprintf(temp,"/dev/dvb/adapter%d/dvr%d",adapter,demux);
	dvr = open_device(temp,O_WRONLY);
	dvr2 = open_device(temp,O_RDONLY | O_NONBLOCK);

	stream = genTS(numpackets,numpids);

	if(stream == NULL)
	{
		printf("TS generation failed\n");
		exit(1);
	}

	if(filters == NULL)
	{
		printf("Failed to allocate memory for filters\n");
		exit(1);
	}

	sprintf(temp,"/dev/dvb/adapter%d/demux%d",adapter,demux);

	memset(filters,0,MAX_PIDS*sizeof(struct filter));

	if(numfilters == 8192)
	{
		for(i=0;i<MAX_PIDS;i++)
		{
			filters[i].enabled = 1;
		}

		filters[8192].fd = open(temp,O_RDWR);
		filter_pes(filters[8192].fd,8192);
	}
	else
	{
		for(i=0;i<numfilters;i++)
		{
			filters[i].enabled = 1;
			filters[i].fd = open(temp,O_RDWR);

			filter_pes(filters[i].fd,i);
		}
	}

	for(i=0;i<(numpackets*PACKET_SIZE);i+=PACKET_SIZE)
	{
		int pid = ts_pid(stream+i);

		if((pid >= 0) && (pid <8192))
		{
			if(filters[pid].enabled)
				filters[pid].expected++;
		}
		else
		{
			printf("Somehow generated a packet with an invalid PID...");
			exit(1);
		}
	}

	/*Write packets to the DVR device*/
        res = write(dvr,stream,PACKET_SIZE*numpackets);

        if(res != PACKET_SIZE*numpackets)
            printf("Failed to write %d bytes to dvr - %d\n",PACKET_SIZE*numpackets,res);
	else
	    printf("Wrote %d bytes to dvr\n",res);

	FD_ZERO(&set);
	FD_SET(dvr2,&set);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	while(select(dvr2+1,&set,NULL,NULL,&timeout))
	{
		if(FD_ISSET(dvr2,&set))
		{
			res = read(dvr2,packet,PACKET_SIZE);

			if(res == PACKET_SIZE)
			{
				int pid = ts_pid(packet);
				filters[pid].actual++;

				if(verbose)
				{
					printf("Read packet with PID 0x%03x\n",ts_pid(packet));
				}
			}
			else
			{
				printf("Read unexpected length packet, %d instead of %d\n",res,PACKET_SIZE);
			}
		}
		else
		{
			printf("Read %d bytes from dvr - stream finished.\n",res);
			break;
		}
        }

	for(i=0;i<MAX_PIDS;i++)
	{
		if(filters[i].enabled)
		{
			if(filters[i].expected != filters[i].actual)
			{
				errors++;
				printf("Recieved %d packets with PID 0x%03x, expected %d.\n",filters[i].actual,i,filters[i].expected);
			}
		}
		else
		{
			if(filters[i].actual != 0)
			{
				errors++;
				printf("Recieved %d packets with PID 0x%03x when filter was not set.\n",filters[i].actual,i);
			}
		}
	}

	if(numfilters == 8192)
	{
		close(filters[8192].fd);
	}
	else
	{
		for(i=0;i<numfilters;i++)
		{
			close(filters[i].fd);
		}
	}

	free(stream);

	close(dvr2);
	close(dvr);

	if(errors > 0)
	{
		printf("Test failed.\n");
		exit(1);
	}
	else
	{
		printf("Test passed.\n");
		exit(0);
	}

	return 0;
}
