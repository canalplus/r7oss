/*
 *  Multi-Target Trace solution
 *
 *  MTT - sample software.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <sys/poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <sched.h>
#include <signal.h>

#include "mttsample.h"
#include "mtt.h"

#define GNU_SOURCE

/* The descriptor associated to my driver */
static int dev_fd = -2;

struct buffer_t {
	int valid;
	unsigned int data[BUFF_SIZE];
};

/* Trace component associated to my process */
static mtt_comp_handle_t main_trc_handle;

static struct buffer_t buffers[DMA_BUFFERS];	/* dma buffers */
static struct buffer_t *cur_buf;	/* current dma buffer */
static int last_valid_index;	/* index of next buffer */
static int running;		/* processing status */

static __thread unsigned my_cpu;
static __thread unsigned tick;
static int cpu_number;

static pthread_t capture_thread;
static pthread_t processing_thread;
static sem_t proc1_sem;
static pthread_mutex_t mutex;

/*
 * Assuming root is running this code.
 */
static void create_devfile(void)
{
	FILE *proc_devices = NULL;
	char dev_line[256];
	int mknod = 0;
	char *argv[6] = { "mknod", CHARDEV_PATH, "c", NULL, "0", NULL };

	/*check if dev file is created. */
	if (access(CHARDEV_PATH, R_OK) != 0) {
		mtt_print(main_trc_handle, MTT_LEVEL_INFO,
			  "Opening /proc/devices\n");
		proc_devices = fopen("/proc/devices", "r");
		if (!proc_devices) {
			mtt_print(main_trc_handle, MTT_LEVEL_ERROR,
				  "Could not open /proc/devices\n");
			printf("could not open /proc/devices\n");
			exit(-2);
		}

		mtt_print(main_trc_handle, MTT_LEVEL_INFO,
			  "Parsing /proc/devices\n");
		while (fgets(dev_line, 256, proc_devices) != NULL) {
			char *mtt_offset;
			pid_t pid;
			mtt_offset = strstr(dev_line, DEVICE_NAME);

			if (mtt_offset != NULL) {
				mknod = 1;

				/*strip dev_line to the major value. */
				*(mtt_offset - 1) = '\0';

				argv[3] = dev_line;

				pid = fork();
				if (pid == 0) {
					execv("/bin/mknod", argv);
					exit(0);
				} else {
					/*shell invocation is so slow... */
					sleep(1);
				}
			}
		}

		if (!mknod) {
			mtt_print(main_trc_handle, MTT_LEVEL_ERROR,
				  "Could not find my device\n");
			printf("could not find chardev\n.");
			exit(-2);
		}
	}

	mtt_print(main_trc_handle, MTT_LEVEL_INFO, "Opening " CHARDEV_PATH);
	printf("Opening " CHARDEV_PATH "\n.");

	dev_fd = open(CHARDEV_PATH, O_RDWR);
	if (dev_fd <= 0) {
		mtt_print(main_trc_handle, MTT_LEVEL_ERROR,
			  "Could not open my device\n");
		printf("could not open chardev\n.");
		exit(-2);
	}

	mtt_print(main_trc_handle, MTT_LEVEL_INFO, "dev_fd = %d\n", dev_fd);
	printf("dev_fd = %d\n", dev_fd);
}

static int count_cpus(void)
{
	DIR *cpus = opendir("/sys/devices/system/cpu");
	struct dirent *cpu;
	int num;

	if (cpus == NULL)
		return 1;

	while (1) {
		cpu = readdir(cpus);
		if (cpu == NULL)
			break;
		if ((sscanf(cpu->d_name, "cpu%i", &num) == 1)
		    && (num >= cpu_number))
			cpu_number = num + 1;
	}
	return cpu_number;
}

/* Do some fake pre-processing on input data
 * before sending to processing task */
static int inputdata_formating()
{
	int ix;
	int jx;

	for (ix = 0, jx = 0; ix < 2000; ix++)
		jx += (ix * 3);

	return jx;
}

/**
 * simulates a capture thread reading from a device
 */
static void *capture_thread_func(void *data)
{
	struct timeval a;
	struct timeval b;
	long elapsed;
	long elapsed_prev;
	mtt_comp_handle_t comp_handle;
	uint32_t type_info = MTT_TRACEVECTOR_UINT32(5);
	uint32_t level = MTT_LEVEL_INFO;
	uint32_t rdsiz = 0;
	uint32_t logbuf[5];

	my_cpu = (unsigned long)data;
	elapsed_prev = 10 * SECOND;
	last_valid_index = 0;
	tick = 0;

	/* Open a trace component to gather all trace events
	 * related to this task */
	mtt_open(MTT_COMP_ID_ANY, "Player:GetData", &comp_handle);
	logbuf[0] = (uint32_t) comp_handle;

	mtt_print(comp_handle, level, "Capture_thread start on cpu%d.\n",
		  my_cpu);
	printf("Capture_thread start on cpu%d.\n", my_cpu);

	/* fist read to synchonize with kernel timer */
	gettimeofday(&a, NULL);
	read(dev_fd, buffers[0].data, BUFF_SIZE * sizeof(int));

	/* do a blocking read on the "audio" device */
	do {
		pthread_mutex_lock(&mutex);

		cur_buf = &buffers[last_valid_index];
		if (!cur_buf->valid) {
			pthread_mutex_unlock(&mutex);

			mtt_print(comp_handle, level,
				  "Wait for data from driver...");

			/* This buffer is free and can be read */
			rdsiz =
			    read(dev_fd, cur_buf->data,
				 BUFF_SIZE * sizeof(int));
			mtt_trace(comp_handle, MTT_LEVEL_USER0,
				  MTT_TRACEITEM_UINT32, &rdsiz, "read_size");

			/* Compute elapsed time since last irq */
			gettimeofday(&b, NULL);
			elapsed =
			    b.tv_usec - a.tv_usec + SECOND * (b.tv_sec -
							      a.tv_sec);
			a.tv_usec = b.tv_usec;
			a.tv_sec = b.tv_sec;

			/* Trace data vector */
			logbuf[1] = (uint32_t) rdsiz;
			logbuf[2] = (uint32_t) last_valid_index;
			logbuf[3] = (uint32_t) tick;
			logbuf[4] = (uint32_t) elapsed;
			mtt_trace(comp_handle, level, type_info,
				  (const void *)(logbuf), "data-read");

			/* Check if i missed a frame */
			if (elapsed > 2 * elapsed_prev) {
				mtt_print(comp_handle, MTT_LEVEL_WARNING,
					  "Pipe underrun, data dropout\n");
				printf("pipe underrun, data dropout !\n");
			} else {
				elapsed_prev = elapsed;
			}

			/* feed the buffer to processing thread */
			cur_buf->valid = 1;

			printf("%d: read dma[%d], elapsed %ld ms\n",
			       tick, last_valid_index, elapsed / 1000);

			last_valid_index++;
			last_valid_index &= DMA_BUFFERS_MASK;

			/* Do some data conditionning before sending
			 * to processing task...*/
			inputdata_formating();

			/* Send signal to processing task... */
			mtt_print(comp_handle, MTT_LEVEL_INFO,
				  "Send signal to processing task....");

			sem_post(&proc1_sem);

			tick++;
		} else {
			/* Cannot get any buffer... */
			mtt_print(comp_handle, MTT_LEVEL_ERROR,
				  "Overrun on buffer[%d]\n", last_valid_index);
			printf("overrun on buffer[%d]\n", last_valid_index);
			pthread_mutex_unlock(&mutex);
			exit(0);
		}
	} while (tick < MAX_BUFFS);

	/* Done all what i have to do, terminate the demo */
	mtt_print(comp_handle, MTT_LEVEL_WARNING, "Capture_thread stop\n");

	ioctl(dev_fd, DEMOAPP_IOCSTOP, NULL);
	running = 0;

	printf("Capture_thread stop\n");
	mtt_close(comp_handle);

	/*post again to help stop the proc thread */
	sem_post(&proc1_sem);

	return NULL;
}

/* Do some fake data-processing on data before sending to output */
static unsigned int
signal_processing(mtt_comp_handle_t comp_handle, int cur_index,
		unsigned int si)
{
	unsigned int k, s;
	s = si;

	/*pass 1 */
	mtt_trace(comp_handle, MTT_LEVEL_USER2, 0, NULL, "pass1");
	for (k = 0; k < BUFF_SIZE - 1; k++) {
		s = 8 * (buffers[cur_index].data[k] /
			 10) + 2 * (buffers[cur_index].data[(k + 1)] / 10);
		buffers[cur_index].data[k] = s;
	}

	/*pass 2 */
	mtt_trace(comp_handle, MTT_LEVEL_USER2, 0, NULL, "pass2");
	for (k = 0; k < BUFF_SIZE - 1; k++) {
		s = 7 * (buffers[cur_index].data[k] /
			 10) + 3 * (buffers[cur_index].data[(k + 1)] / 10);
		buffers[cur_index].data[k] = s;
	}

	/*pass 3 */
	mtt_trace(comp_handle, MTT_LEVEL_USER2, 0, NULL, "pass3");
	for (k = 0; k < BUFF_SIZE - 1; k++) {
		s = 6 * (buffers[cur_index].data[k] /
			 10) + 4 * (buffers[cur_index].data[(k + 1)] / 10);
		buffers[cur_index].data[k] = s;
	}
	return s;
}

/**
 * Simulates a processing thread, writes back to a device
 */
static void *processing_thread_func(void *data)
{
	int index;
	int cur_index = 0;
	int rc = 0;
	static unsigned int sk_prev;
	mtt_comp_handle_t comp_handle;
	uint32_t type_info = MTT_TRACEVECTOR_UINT32(5);
	uint32_t level = MTT_LEVEL_INFO;
	uint32_t wrsiz = 0;
	uint32_t logbuf[5];

	tick = 0;
	my_cpu = (unsigned long)data;

	/* Open a trace component to gather all trace events
	 * related to this task */
	mtt_open(MTT_COMP_ID_ANY, "Player:ProcessData", &comp_handle);
	logbuf[0] = (uint32_t) comp_handle;

	mtt_print(comp_handle, level, "Process thread starts on cpu%d.\n",
		  my_cpu);
	printf("processing thread starts on cpu%d.\n", my_cpu);

	/* do a blocking read on the "audio" device */
	while (running && (rc != ETIMEDOUT)) {
		/* wait for a buffer to be posted from the data capture task */
		mtt_print(comp_handle, level,
			  "Wait for signal from capture thread...");
		sem_wait(&proc1_sem);
		mtt_print(comp_handle, level,
			  "Received signal to process data.");

		for (index = 0; index < DMA_BUFFERS; index++) {
			unsigned int s;
			cur_index += index;
			cur_index &= DMA_BUFFERS_MASK;

			/* process any available dma. */
			if (buffers[cur_index].valid) {
				printf("%d: calc dma[%d]...", tick, cur_index);
				/* low pass filter */
				s = sk_prev;
				s = signal_processing(comp_handle, cur_index,
						      s);
				sk_prev = s;
				printf("done.\n");

				/* write back filtered data */
				wrsiz =
				    write(dev_fd, buffers[cur_index].data,
					  BUFF_SIZE * sizeof(int));

				/* Trace vector recording what's happending */
				logbuf[1] = (uint32_t) wrsiz;
				logbuf[2] = (uint32_t) cur_index;
				logbuf[3] = (uint32_t) tick;
				logbuf[4] = (uint32_t) 0;
				mtt_trace(comp_handle, level, type_info,
					  (const void *)(logbuf), "data-write");

				/* Release the processed buffer */
				pthread_mutex_lock(&mutex);
				buffers[cur_index].valid = 0;
				pthread_mutex_unlock(&mutex);
			}
		}
		tick++;
	}

	/* End of processing */
	if (rc) {
		mtt_print(comp_handle, MTT_LEVEL_ERROR,
			  "Processing thread stopped with error %d\n", rc);
		printf("Processing thread stopped with error %d\n", rc);
	} else {
		mtt_print(comp_handle, MTT_LEVEL_WARNING,
			  "Processing thread stopped.\n");
		printf("Processing thread stopped.\n");
	}
	return NULL;
}

/*
 * Create_thread - create threads
 */
static int create_threads(void)
{
	pthread_attr_t attr;
	cpu_set_t cpus;
	unsigned long cur_cpu = 0;

	pthread_attr_init(&attr);

	count_cpus();

	if (cpu_number > 1) {
		CPU_ZERO(&cpus);
		CPU_SET(0, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
		cur_cpu++;
	}

	if (pthread_create
	    (&capture_thread, &attr, capture_thread_func, (void *)0) < 0) {
		printf("Couldn't create data capture thread\n");
		return -1;
	}

	if (cpu_number > 1) {
		CPU_ZERO(&cpus);
		CPU_SET(cur_cpu, &cpus);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
	}

	if (pthread_create
	    (&processing_thread, &attr, processing_thread_func,
	     (void *)cur_cpu) < 0) {
		printf("Couldn't create data processing thread\n");
		return -1;
	}

	return 0;
}

/* When we receive SIGTERM, close the driver file, remove the pidfile
 * and close the logfile before exiting. */
static void sigtermhandler(int sig, siginfo_t *siginfo, void *context)
{
	printf("Stopping test\n");

	pthread_mutex_destroy(&mutex);
	ioctl(dev_fd, DEMOAPP_IOCSTOP, NULL);
	close(dev_fd);
	mtt_close(main_trc_handle);
	mtt_uninitialize();

	exit(1);
}

/* Need handlers for SIGTERM (for graceful shutdown) and SIGALRM (for ending
 * trace sessions of finite duration. */
static void install_sighandlers(void)
{
	struct sigaction term_act;

	bzero((char *)&term_act, sizeof(term_act));
	term_act.sa_sigaction = &sigtermhandler;
	term_act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &term_act, NULL) < 0) {
		printf("Failed to install SIGTERM handler");
		exit(1);
	}

	bzero((char *)&term_act, sizeof(term_act));
	term_act.sa_sigaction = &sigtermhandler;
	term_act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &term_act, NULL) < 0) {
		printf("Failed to install SIGINT handler");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	mtt_init_param_t init_param;
	last_valid_index = 0;
	running = 1;

	install_sighandlers();

	/* Initialize trace library - do not specify any particular setting,
	 * rely on those fixed by configuration */
	memset((void *)(&init_param), 0, sizeof(init_param));
	mtt_initialize(&init_param);

	mtt_open(MTT_COMP_ID_ANY, "Player", &main_trc_handle);

	memset(buffers, 0, sizeof(buffers));
	pthread_mutex_init(&mutex, NULL);
	sem_init(&proc1_sem, 0, 0);

	/* Open fake "audio" device */
	create_devfile();

	/* My processing tasks */
	create_threads();

	/* Go ! */
	mtt_print(main_trc_handle, MTT_LEVEL_INFO, "Go ! ");

	/* Start the underlying driver */
	if (ioctl(dev_fd, DEMOAPP_IOCRESET, NULL)) {
		mtt_print(main_trc_handle, MTT_LEVEL_ERROR,
			  "Couldn't send reset to driver.");
		printf("Couldn't send reset to driver.\n");
	}
	if (ioctl(dev_fd, DEMOAPP_IOCSTART, NULL)) {
		mtt_print(main_trc_handle, MTT_LEVEL_ERROR,
			  "Couldn't send start to driver.");
		printf("Couldn't send start to driver.\n");
	}

	pthread_join(capture_thread, NULL);
	pthread_join(processing_thread, NULL);

	mtt_print(main_trc_handle, MTT_LEVEL_INFO, "End of test.");

	/* Test completed  */
	pthread_mutex_destroy(&mutex);
	ioctl(dev_fd, DEMOAPP_IOCSTOP, NULL);
	close(dev_fd);
	mtt_close(main_trc_handle);
	mtt_uninitialize();

	return 0;
}
