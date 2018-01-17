#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

#include <dsmcc/dsmcc.h>
#include <dsmcc/dsmcc-tsparser.h>

static int g_running = 1;
static int g_complete = 0;
static pthread_mutex_t g_mutex;
static pthread_cond_t g_cond;

static void sigint_handler(int signal)
{
	(void) signal;
	fprintf(stderr, "sigint\n");

	pthread_mutex_lock(&g_mutex);
	g_running = 0;
	pthread_cond_signal(&g_cond);
	pthread_mutex_unlock(&g_mutex);
}

static void logger(int severity, const char *message)
{
	char *sev;

	switch (severity)
	{
		case DSMCC_LOG_DEBUG:
			sev = "[debug]";
			break;
		case DSMCC_LOG_WARN:
			sev = "[warn]";
			break;
		case DSMCC_LOG_ERROR:
			sev = "[error]";
			break;
		default:
			sev = "";
			break;
	}
	fprintf(stderr, "[dsmcc]%s %s\n", sev, message);
}

static int get_pid_for_assoc_tag(void *arg, uint16_t assoc_tag, uint16_t *pid)
{
	/* fake, should find PID from assoc_tag using PMT */
	fprintf(stderr, "[main] Callback: Getting PID for association tag 0x%04x\n", assoc_tag);
	(void) arg;
	(void) pid;
	return -1;
}

static int add_section_filter(void *arg, uint16_t pid, uint8_t *pattern, uint8_t *equalmask, uint8_t *notequalmask, uint16_t depth)
{
	char *p, *em, *nem;
	int i;

	(void) arg;
	p = malloc(depth * 2 + 1);
	em = malloc(depth * 2 + 1);
	nem = malloc(depth * 2 + 1);
	for (i = 0;i < depth; i++)
	{
		sprintf(p + i * 2, "%02hhx", pattern[i]);
		sprintf(em + i * 2, "%02hhx", equalmask[i]);
		sprintf(nem + i * 2, "%02hhx", notequalmask[i]);
	}
	fprintf(stderr, "[main] Callback: Add section filter on PID 0x%04x: %s|%s|%s\n", pid, p, em, nem);
	free(nem);
	free(em);
	free(p);

	return 0;
}

static bool dentry_check(void *arg, uint32_t queue_id, uint32_t cid, bool dir, const char *path, const char *fullpath)
{
	(void) arg;

	fprintf(stderr, "[main] Callback(%u): Dentry check 0x%08x:%s:%s -> %s\n",
			queue_id, cid, dir ? "directory" : "file", path, fullpath);

	return 1;
}

static void dentry_saved(void *arg, uint32_t queue_id, uint32_t cid, bool dir, const char *path, const char *fullpath)
{
	(void) arg;

	fprintf(stderr, "[main] Callback(%u): Dentry saved 0x%08x:%s:%s -> %s\n",
			queue_id, cid, dir ? "directory" : "file", path, fullpath);
};

static void download_progression(void *arg, uint32_t queue_id, uint32_t cid, uint32_t downloaded, uint32_t total)
{
	(void) arg;

	fprintf(stderr, "[main] Callback(%u): Carousel 0x%08x: %u/%u\n",
			queue_id, cid, downloaded, total);
}

static void carousel_status_changed(void *arg, uint32_t queue_id, uint32_t cid, int newstatus)
{
	const char *status;

	(void) arg;

	switch (newstatus)
	{
		case DSMCC_STATUS_PARTIAL:
			status = "PARTIAL";
			break;
		case DSMCC_STATUS_DOWNLOADING:
			status = "DOWNLOADING";
			break;
		case DSMCC_STATUS_TIMEDOUT:
			status = "TIMED-OUT";
			break;
		case DSMCC_STATUS_DONE:
			status = "DONE";
			break;
		default:
			status = "Unknown!";
			break;
	}
	fprintf(stderr, "[main] Callback(%u): Carousel 0x%08x status changed to %s\n",
			queue_id, cid, status);

	if (newstatus == DSMCC_STATUS_TIMEDOUT || newstatus == DSMCC_STATUS_DONE)
	{
		pthread_mutex_lock(&g_mutex);
		g_complete = 1;
		pthread_cond_signal(&g_cond);
		pthread_mutex_unlock(&g_mutex);
	}
};

static int parse_stream(FILE *ts, struct dsmcc_state *state, struct dsmcc_tsparser_buffer **buffers)
{
	char buf[188];
	int ret = 0;
	int rc;

	while (g_running && !g_complete)
	{
		rc = fread(buf, 1, 188, ts);
		if (rc < 0)
		{
			fprintf(stderr, "read error : %s\n", strerror(errno));
			ret = -1;
			break;
		}
		else if (rc == 0)
		{
			// EOF, parse remaining data
			dsmcc_tsparser_parse_buffered_sections(state, *buffers);
			break;
		}
		else
		{
			dsmcc_tsparser_parse_packet(state, buffers, (unsigned char*) buf, rc); 
		}
	}

	return ret;
}

int main(int argc, char **argv)
{
	struct dsmcc_state *state;
	int status = 0;
	int carousel_type = DSMCC_OBJECT_CAROUSEL;
	char *downloadpath;
	FILE *ts;
	uint16_t pid;
	uint32_t qid;
	int log_level = DSMCC_LOG_DEBUG;
	struct dsmcc_tsparser_buffer *buffers = NULL;
	struct dsmcc_dvb_callbacks dvb_callbacks;
	struct dsmcc_carousel_callbacks car_callbacks;

	if(argc < 4)
	{
		fprintf(stderr, "usage %s [-d] [-q] <file> <pid> <downloadpath>\n -q    almost quiet\n -d    data carousel\n", argv[0]);
		return -1;
	}

	while(argc > 4)
	{
		if(!strcmp(argv[1], "-d"))
		{
			fprintf(stderr, "data carousel mode\n");
			carousel_type = DSMCC_DATA_CAROUSEL;
			argv++;
			argc--;
		}
		else if(!strcmp(argv[1], "-q"))
		{
			fprintf(stderr, "almost quiet mode\n");
			log_level = DSMCC_LOG_ERROR;
			argv++;
			argc--;
		}
		else
			break; // assume options end
	}

	sscanf(argv[2], "%hu", &pid);
	downloadpath = argv[3];

	signal(SIGINT, sigint_handler);

	fprintf(stderr, "start\n");

	if (!strcmp(argv[1], "-"))
		ts = stdin;
	else
		ts = fopen(argv[1], "r");
	if (ts)
	{
		pthread_mutex_init(&g_mutex, NULL);
		pthread_cond_init(&g_cond, NULL);

		dsmcc_set_logger(&logger, log_level);

		dvb_callbacks.get_pid_for_assoc_tag = &get_pid_for_assoc_tag;
		dvb_callbacks.add_section_filter = &add_section_filter;
		state = dsmcc_open("/tmp/dsmcc-cache", 1, &dvb_callbacks);

		dsmcc_tsparser_add_pid(&buffers, pid);

		car_callbacks.dentry_check = &dentry_check;
		car_callbacks.dentry_saved = &dentry_saved;
		car_callbacks.download_progression = &download_progression;
		car_callbacks.carousel_status_changed = &carousel_status_changed;
		qid = dsmcc_queue_carousel2(state, carousel_type, pid, 0, downloadpath, &car_callbacks);

		status = parse_stream(ts, state, &buffers);

		pthread_mutex_lock(&g_mutex);
		if (!g_complete)
		{
			struct timeval now;
			struct timespec ts;
			fprintf(stderr, "Waiting 60s for carousel completion...\n");
			gettimeofday(&now, NULL);
			ts.tv_sec = now.tv_sec + 60;
			ts.tv_nsec = now.tv_usec * 1000;
			if (pthread_cond_timedwait(&g_cond, &g_mutex, &ts) == ETIMEDOUT)
				fprintf(stderr, "Time out!\n");
		}
		pthread_mutex_unlock(&g_mutex);

		dsmcc_dequeue_carousel(state, qid);

		dsmcc_close(state);
		dsmcc_tsparser_free_buffers(&buffers);
	}
	else
	{
		fprintf(stderr, "open '%s' error : %s\n", argv[1], strerror(errno));
		return -1;
	}

	if (ts != stdin)
		fclose(ts);

	return status;
}
