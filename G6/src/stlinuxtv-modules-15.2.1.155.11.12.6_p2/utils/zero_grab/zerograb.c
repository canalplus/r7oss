#include <ctype.h>
#include <directfb.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/stmfb.h>
#include <linux/videodev2.h>
#include <linux/stmvout.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dvb_v4l2_export.h>
#include <linux/dvb/stm_dmx.h>
#include <linux/dvb/stm_ioctls.h>

#include "utils/v4l2_helper.h"
/*======================================================================*/
/* was the #include "./zerograb.h"                                      */

#define STR(a) #a
#define XSTR(a) STR(a)
#define DFBCHECK(x...)                                         \
    {                                                            \
        DFBResult err = x;                                         \
                                                               \
        if (err != DFB_OK)                                         \
        {                                                        \
            fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
            DirectFBErrorFatal( #x, err );                         \
        }                                                        \
    }

#define TRACE(fmt, args...) \
        { if (arg_verbose) printf("%s: " fmt, __func__, ##args); }

#define LOG(fmt, args...) \
        do { \
            printf (fmt, ##args); \
            fflush(stdout); \
        } while (0)

#define ERROR(fmt, args...) \
	do  { \
	    printf("***** Error: %s: " fmt, __func__, ##args); \
	    fflush(stdout); \
        } while (0)

#define _max(a,b)	( (a) > (b) ? (a) : (b) )
#define _min(a,b)	( (a) < (b) ? (a) : (b) )

#define MEGA				(1024 * 1024)
#define MAX_BUFFERS			32
#define MAX_PATH_SIZE			32
#define MAX_GRAB_INPUT 8

/*----------------------------------------------------------------------*/
#define TO_DECODER	1
#define TO_DISPLAY	2
struct buf_s {
    unsigned int    index;         /* buffer index returned by dev    */
    void           *uaddr;	   /* user address as defined by mmap */
    void           *kaddr;	   /* kernel address returned by dev. */
    unsigned int    len;           /* buffer length                   */
    unsigned long   offset;	   /* from V4L2	device                */
    int             status;        /* ON_DECODER, ON_OUTPUT	      */
    struct buf_s   *next;
};

/*----------------------------------------------------------------------*/
/* Hugly macros .... */
#define inc_counter(ctx, counter) (ctx)->counter++;
#define th_name(c)	(c)->th.name

struct thread_s {
    pthread_t        id;	/* system */
    char            *name;	/* const, set by thread itself */
    pid_t            tid;       /* system: thread id */
    int              instance;	/* instance: so far always 0   */
    unsigned int     event;     /* state of thread see set_event() */
};

struct thread_inject_s
{
    struct thread_s  th;
    int              input;	        /* 0:file, 1:tuner, 2:udp, ... */
    int              vdataid;           /* input video ID (PES/ES)     */
    int              dvbvid;	        /* LinuxDVB: inject video dev  */
    unsigned int     op_count;		/* N. of injections	       */
    unsigned int     inj_size;		/* total bytes injected        */
    unsigned int     max_inj;		/* max size injected	       */
    unsigned int     min_inj;		/* min size injected	       */
    int              arg_delay;		/* enable a inject delay of ...*/

    struct timeval   start;
     /* not used yet     */
    struct timeval   stop;
    double	     elapse_time;
    int		     arg_rate;          /*  injection rate.             */
};

struct thread_capturer_s
{
    struct thread_s  th;
    int              videoN;            /* LinuxDVB video N. (decoder) */
    int              v4l2cfid;		/* V4L2 video capture file ID  */
    int              v4l2ofid;		/* V4L2 video output file ID   */
    int              buffer_count;      /* N. of buffers managed       */

    struct timeval   start;
    struct timeval   stop;
    int		     in_count;	/* count of captured frames    */
    int		     out_count;	/* count of returned or display frames    */
    double           dequeue_mintime;
    double           dequeue_maxtime;
    int              arg_disablePTS;
    int		     arg_testCase;

     /* not used yet     */
    double	     elapse_time;
    int              arg_rate;
    int		     arg_freq;          /* likley useful to control    */
					/* injection rate.             */
					/* No just best effort         */
};

struct thread_sink_s
{
    struct thread_s  th;

    int              v4l2ofid;		/* V4L2 video output file ID   */
    int              v4l2cfid;		/* V4L2 video capture file ID  */
    int              tv_pos_w;		/* output position on TV       */
    int              tv_pos_h;

    struct timeval   start;
    struct timeval   stop;
    int		     in_count;		/* count of empty frames    */
    int		     out_count;		/* count of display frames    */
    double           dequeue_mintime;
    double           dequeue_maxtime;
    int              arg_disablePTS;

     /* not used yet     */
    double	     elapse_time;
    int		     arg_rate;          /* injection rate.             */
};

/*======================================================================
 * Various helpers and macros
*/
/* Actual synchronization is quite naive:
 * each thread descriptor has a variable (event) which is supposed
 * to contain "what the thread should do". Set of possible command
 * is quite small (see below QUIT, ...)
 * Would be worth to spend some time to put in place a better
 * mechanism, may be based on pthread signals!
 */
#define BROADCAST	    NULL
#define MYSELF		    (&ctx->th)		/* horrible! */
#define get_event()         (ctx->th.event)	/* horrible! */
#define ref_event()         (&ctx->th.event)	/* horrible! */

/* Sorry! this is endianess dependent */
#define __str4(a,b,c,d)    ( ((unsigned int)(a) << 0) | \
			     ((unsigned int)(b) << 8 ) | \
			     ((unsigned int)(c) << 16) | \
			     ((unsigned int)(d) << 24) )

#define QUIT	__str4('Q','U','I','T')	 /* quit immediately (anomalie) */
#define INIT	__str4('I','N','I','T')	 /* thread init                 */
#define RUNNING	__str4('R','U','N',' ')	 /* thread normal running       */
#define END	__str4('E','N','D',' ')	 /* thread normal end           */

/*
 * Used by threads
 */

/* Values below are absolutely arbitrary */
#define MS_BEFORE_RETRY         3

#define MAX_RETRY_ON_EAGAIN	300
/*======================================================================
 * common functions
*/
unsigned long phys2user(unsigned long phys);


#define V4L2_CAPTURE_DEVICE_NAME	"AV decoder"
#define V4L2_DISPLAY_DRIVER_NAME	"Planes"
#define V4L2_STM_CARD_NAME		"STMicroelectronics"
#define LinuxDVB_INPUT_VIDEO_DEVICE_NAME "/dev/dvb/adapter0/video"

/*======================================================================*/
/* Globals and thread contexts                                          */
/*                                                                      */
struct thread_capturer_s     th_grab;
struct thread_sink_s         th_sink;
struct thread_inject_s       th_inj;

void * thread_capturer      (void *arg);
void * thread_sink          (void *arg);
/* ------ */

int arg_verbose;                        /* used by TRACE macro only */
/*======================================================================*/

/* reading PES from: .... (for time being limited to FROM_PES_FILE) */
enum {
	NONE,
	FROM_PES_FILE,
	FROM_IP
};

struct buffer_s {
    int		  rbp;		/* required by player           */
    int		  count;	/* allocated			*/
    int		  user;		/* user reserved		*/
    unsigned int  size;         /* all buffer with same size    */
    struct buf_s  list[MAX_BUFFERS];
};
static struct buffer_s  bufs;

#ifdef NOTUSED
/***************************************************************************
Direct Frame Buffer
*/

IDirectFB *dfb;
IDirectFBSurface *primary;
IDirectFBDisplayLayer *DisplayLayer;
DFBSurfaceDescription dsc;

int screen_width;
int screen_height;

int surface_width;
int surface_height;

int run_with_font = 1;

void
dfb_init (int split_w, int split_h)
{
    /* Initialize DirectFB and create the super interface */
    DFBCHECK (DirectFBInit (NULL, NULL));
    DFBCHECK (DirectFBCreate (&dfb));

    /* Get surface from layer */
    DFBCHECK (dfb->GetDisplayLayer (dfb, DLID_PRIMARY, &DisplayLayer));
    DFBCHECK (DisplayLayer->SetCooperativeLevel
        (DisplayLayer, DLSCL_ADMINISTRATIVE));
    DFBCHECK (DisplayLayer->GetSurface (DisplayLayer, &primary));

    /* Get dimension and compute split */
    DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));
    surface_width = screen_width / split_w;
    surface_height = screen_height / split_h;

    /* /\* Clear the screen and green fill*\/ */
    /* DFBCHECK (primary->SetColor (primary, 0x00, 0xFF, 0x00, 0xFF));     */
    /* DFBCHECK (primary->FillRectangle (primary, 0, 0, screen_width, */
    /*                                screen_height)); */
}

void
dfb_surface (IDirectFBSurface ** surface, int s_width, int s_height,
         IDirectFBFont ** font, int clut)
{
    DFBSurfaceDescription dsc;
    DFBFontDescription font_dsc;
    const char *fontfile = "/usr/share/directfb-examples/fonts/decker.ttf";
    struct stat sb;

    /* Create surface */
    dsc.width = s_width;
    dsc.height = s_height;
    dsc.pixelformat = clut ? DSPF_LUT8 : DSPF_ARGB;
    dsc.caps = DSCAPS_VIDEOONLY;
    dsc.flags = DSDESC_CAPS | DSDESC_PIXELFORMAT | DSDESC_WIDTH | DSDESC_HEIGHT;
    DFBCHECK (dfb->CreateSurface (dfb, &dsc, surface));

    /* Set the surface font */

    if (stat (fontfile, &sb) == -1)
    {
        fprintf (stdout, "Run without font %s\n", fontfile);
        run_with_font = 0;
        return;
    }

    font_dsc.flags = DFDESC_HEIGHT;
    font_dsc.height = 48;
    DFBCHECK (dfb->CreateFont (dfb, fontfile, &font_dsc, font));
    DFBCHECK ((*surface)->SetFont (*surface, *font));
}

void
dfb_surface_release (IDirectFBSurface * surface, IDirectFBFont * font)
{
    font->Release (font);
    surface->Release (surface);
}

void
dfb_release ()
{
    dfb->Release (dfb);
}
#endif

/***************************************************************************
 * Generic helpers
 */
/* return thread sched policy */
static int show_thread_scheduler(int pid, char *th_name)
{
    int                   res, min, max, p;
    struct sched_param    tpa;
    char                 *m = "???";

    /* get scheduler policy */
    p = sched_getscheduler(pid);
    if (p == SCHED_OTHER) m = "SCHED_OTHER";
    if (p == SCHED_FIFO)  m = "SCHED_FIFO";
    if (p == SCHED_RR)    m = "SCHED_RR";

    /* get scheduler parameters */
    res = sched_getparam(pid, &tpa);
    if (res != 0)
	return res;

    /* get min and max priority */
    min = sched_get_priority_min(p);
    max = sched_get_priority_max(p);
    LOG("thread %s (%d [0=myself]): policy %s  priority %d (m%d - M%d)\n",
         th_name, pid, m, tpa.sched_priority, min, max);
    return 0;
}

int set_thread_scheduler(int pid, int policy, int priority, char *th_name)
{
    int                   res = 0;
    int                   p;
    struct sched_param    tpa;

    /* get scheduler policy */
    p = sched_getscheduler(pid);

    /* get scheduler parameters */
    res = sched_getparam(pid, &tpa);
    if (res != 0)
	return res;

    if ((p != policy) || (priority != tpa.sched_priority))
    {
        tpa.sched_priority = priority;
        res = sched_setscheduler(pid, policy, &tpa);
    }
    return res;
}

void set_event(unsigned int event, struct thread_s  *dest)
{
    if (dest == BROADCAST)
    {
         th_grab.th.event = event;
         th_sink.th.event = event;
         th_inj.th.event  = event;
    }
    else
	dest->event = event;
}

/*
 * returns the N. of milli sec and micro between two op.
 * 0.0 if the op. can't be compute (i.e start or end = 0)
 */
void time_diff(struct timeval start, struct timeval end,
               unsigned long *milli, unsigned long *micro)
{
    unsigned long tmp = 0;;
    /* this code will be valid (on Linux, Unix) until year 2038! */
    if ((start.tv_sec == 0) || (end.tv_sec == 0))
    {
	*milli = 0;
        *micro = 0;
        return;
    }
    tmp = end.tv_sec - start.tv_sec;	/* assumes end >= start */
    *milli = tmp * 1000;

    /* micro may wrap... */
    if (end.tv_usec >= start.tv_usec)
        tmp = end.tv_usec - start.tv_usec;
    else
        tmp = (0xFFFFFFFF - start.tv_usec) + end.tv_usec;
    *micro = tmp;
    return;
}

void my_msleep(int milli)
{
    struct timespec  ts;

    if (!milli)
	return;
    ts.tv_sec = milli / 1000;
    ts.tv_nsec = (milli % 1000) * 1000000;
    if (nanosleep(&ts, NULL) != 0)
        ERROR ("nanosleep interrupted! (%s)!\n", strerror(errno));
}

/*
 * phys2user
 * For a physical buffer address (kern. offset) returns the user address.
 */
unsigned long phys2user(unsigned long phys)
{
    int i;
    struct buf_s       *b = &bufs.list[0];
    for (i=0; i < bufs.count; i++, b++)
	if (b->offset == phys)
	    return (unsigned long)b->uaddr;
    return 0;
}

/***************************************************************************
 * Injection task:
 * Injects video data, currently only video PES from regular file.
 * Input Video stream is already opened (in read only mode), the output
 * corresponds to video LinuxDVB device.
 */

#define INJECT_BUFFER_SIZE		(64 * 1024)
void *
thread_inject (void *arg)
{
    struct thread_inject_s *ctx;     /* working context */
    video_discontinuity_t       VideoEOS = VIDEO_DISCONTINUITY_EOS;
    static  char inject_buffer[INJECT_BUFFER_SIZE];

    int injlen, res;

    ctx = (struct thread_inject_s *) arg;
    th_name(ctx)  = "++ inject";
    /* ctx->th.tid   = gettid();  assume an integer, but this is not portable */
    ctx->th.instance = 0;
    LOG ("%s started ...\n", th_name(ctx));

    while (true)
    {
        /* read from input video file */
	injlen = read(ctx->vdataid, inject_buffer, sizeof(inject_buffer));
	if (injlen < 0)
	{
            ERROR ("%s - unexpected, can't read! (%s)!\n",
                    th_name(ctx), strerror(errno));
	    goto quit;
	}
	if (injlen == 0) {
            LOG ("%s: EOF!\n", th_name(ctx));
	    VideoEOS = VIDEO_DISCONTINUITY_EOS;
	    res = ioctl(ctx->dvbvid, VIDEO_DISCONTINUITY, (void*)VideoEOS);
	    if (res < 0) {
                ERROR ("%s - EOS discontinuity injection failed (%d,%s)!\n",
                       th_name(ctx), res, strerror(errno));
	        goto quit;
	    }
	    set_event(END, MYSELF);	/* correct termination */
            goto complete;
        }

        if (get_event() == QUIT)
	    goto quit;

	if (ctx->arg_delay)
            my_msleep(ctx->arg_delay);

	/* For time being just work in "best effort"
	 * Write on LinuxDVB video can be suspended when the
	 * decode buffer is full (ex. doesn't consume)
         */
        res = write(ctx->dvbvid, inject_buffer, injlen);
	if (res != injlen)
        {
            /* unless a write error or signal should never happen */
	    if (errno == EINTR)
	    {
                ERROR ("%s - injection interrupted!\n", th_name(ctx));
	    } else {
                ERROR ("%s - unexpected! (injected %d bytes on %d)!\n",
                    th_name(ctx), res, injlen);
	    }
	    goto quit;
        }

	/* to see something going on ... */
	if ((ctx->inj_size % (5 * MEGA)) == 0)
            LOG ("%s: has injected %d Mega \n",
                 th_name(ctx), (ctx->inj_size / MEGA));

        /* updates statistic counters */
        ctx->op_count++;
        ctx->inj_size += injlen;
    }

complete:
    /* dfb_surface_release (SurfaceHandle, font); */
    LOG ("%s (in %d r/w op has injected %u bytes)\n",
         th_name(ctx), ctx->op_count, ctx->inj_size);

    return (void *)&ctx->th;   /* return thread status */

quit:
    LOG ("%s terminated because of error (injected %d bytes)!\n",
         th_name(ctx), ctx->inj_size);
    set_event(QUIT, BROADCAST);
    return NULL;
}

/***************************************************************************
 *  Option mgmt helpers:
 */
char *option_list = "VbBPhHsSc:v:o:T:d:u:";
char *help =
    "[-V] [-b -B] [-P] [-T n] [-u N] [-h|H|s|S] [-c<codec>]"
    "\n           [-v<input>] [-o<output>]  <infile>"
/*    "\n[-f<pixf>] [-w<window>]  <infile>" */
    "\n"
    "\n\t-V      : enable additional TRACE (def. OFF)"
    "\n\t-b      : capture task in BLOKING mode (def. O_NONBLOCK)"
    "\n\t-B      : sink task (empty frames back) in BLOKING mode (def. O_NONBLOCK)"
    "\n\t-P      : disable use of PTS ( by default PTS are used)"
    "\n\t        : The display may not be fluid, all executes in ""best effort"
    "\n\t        : mode"", decoder Video SYNC is also disabled."
    "\n\t-u N    : allocate additional frame buffers (default N=0)"
    "\n\t-T n    : select test case (n)."
    "\n\t        :    1,2: internal only (1 exercizes streamON/OFF and attach/detach;"
    "\n\t        :         2 exercizes SE/STLinuxTV interfacee does not display)"
    "\n\t        :    3:   a thread injects PES, a second thread captures and display"
    "\n\t        :         and a third returns empty frames to capture device."
    "\n\t-hHsS   : frame resolution and standard (one of):"
    "\n\t            -H  HD 1080*         -h  HD 720"
    "\n\t            -S  SD 625           -s  SD 525"
    "\n\t<codec> : codec type [valid values are: h264*, mpeg1, mpeg2]"
    "\n\t<input> : LinuxDVB video device (decoder) number (range 0-7. Def.0)"
    "\n\t<output>: symbolic name of video output plane device (ASCII string):"
    "\n\t            Main-VID*   Aux-VID      Main-PIP"
/*
    for time being GDPs do not accept NV12 frame format yet
    "\n\t            Main-GDP1   Aux-GDP1"
    "\n\t            Main-GDP2   Aux-GDP2"
*/
    "\n\t<infile>: input video stream (PES format only)"
    "\n"
    "\n\t   Don't forget to disable MC link between decoder and Display"
    "\n";

/*
 *    "\n\t<window>: output position on TV (origin and size  y:x:h:w)"
 *    "\n";
 */

char *example =
	"\n\tmedia-ctl -v -l '""dvb0.video0"":0->""Main-VID"":0[0]'"
	"\n\tzerograb -v0  -b  -P -c h264  <PES file>"
        "\n";
void
usage (int argc, char *argv[])
{
    fprintf (stdout, "\nUsage: %s %s", argv[0], help);
    fprintf (stdout, "\nExample: %s", example);
    exit (EXIT_FAILURE);
}

struct option_lookup
{
    char *key;
    unsigned int val;
};
static struct option_lookup pix_formats[] = {
    { "nv12",           V4L2_PIX_FMT_NV12     },
    { "nv21",           V4L2_PIX_FMT_NV21     },
    { "yuv422",         V4L2_PIX_FMT_YUV422P  },
    { NULL,             -1 }
};

static struct option_lookup encoding_std[] = {
    { "auto",           VIDEO_ENCODING_AUTO  },
    { "mpeg1",          VIDEO_ENCODING_MPEG1 },
    { "mpeg2",          VIDEO_ENCODING_MPEG2 },
    { "h264",           VIDEO_ENCODING_H264  },
    { "divx4",          VIDEO_ENCODING_DIVX4 },
    { NULL,             -1 }
};

static int opt_ascii2id(char *key, struct option_lookup *table)
{
    int i;
    for (i=0; table[i].key; i++)
        if ((key[0] == table[i].key[0]) && (0 == strcmp(key, table[i].key)))
	    break;
    return (table[i].val);
}

struct ascii_option_lookup
{
    char *key;
    int val;	/* set for default! */
};
static struct ascii_option_lookup  output_devices_tab[] = {
    { "Main-VID",       1  },  /* default */
    { "Main-GDP1",      0  },
    { "Main-GDP2",      0  },
    { "Aux-VID",        0  },
    { "Aux-GDP1",       0  },
    { "Aux-GDP2",       0  },
    { "Main-PIP",       0  },
    { NULL,             -1 }
};

static char *opt_ascii_check(char *key, struct ascii_option_lookup *table)
{
    int i;
    if (key == NULL) {
	/* search the default */
        for (i=0; table[i].key; i++) {
	    if (table[i].val)
                return table[i].key;
	}
        ERROR ("Internal error: a video output default must be defined!\n");
	exit(2);
    }
    for (i=0; table[i].key; i++)
        if (strcmp(key, table[i].key) == 0)
            return (table[i].key);
    return(NULL);
}

/***************************************************************************
 *  V4L2 and LinuxDVB helpers:
 */
int
v4l2_stream_on (int fd, int buf_type)
{
    struct v4l2_buffer buf;
    int res;

    memset (&buf, 0, sizeof (buf));

    buf.type = buf_type;
    res = ioctl (fd, VIDIOC_STREAMON, &buf.type);
    return res;
}

int
v4l2_stream_off (int fd, int buf_type)
{
    struct v4l2_buffer buf;
    int res;

    memset (&buf, 0, sizeof (buf));

    buf.type = buf_type;
    res = ioctl (fd, VIDIOC_STREAMOFF, &buf.type);
    return res;
}

int
v4l2_queue_buf (int fd, struct v4l2_buffer *buf, int buf_type, int memory)
{
    int res = 0;

    /* complete with type and memory */
    buf->type      = buf_type;
    /* buf.field     = V4L2_FIELD_ANY; */
    buf->memory    = memory;
    res = ioctl (fd, VIDIOC_QBUF, buf);
    return res;
}

int
v4l2_dequeue_buf (int fd,
		  struct v4l2_buffer *buf, int buf_type, int mem_type,
		  unsigned int *event, char *th_name)
{
    int res, _errno;
    struct timeval  start;
    struct timeval  end;
    unsigned long   milli, micro;
    int max_retry = 0;

    /* on DQBUF V4L protocols imposes only these two fields */
    buf->type   = buf_type;
    buf->memory = mem_type;

    gettimeofday(&start, NULL);
retry:
    res = ioctl (fd, VIDIOC_DQBUF, buf);
    _errno = errno;
    if (res != 0)
    {
        /* not elegant, pragmatic */
	if ((*event == END) || (*event == QUIT))
	    /* interrupted: not useful to know time spent in this case */
	    return res;

	if (_errno == EAGAIN) {
            if (max_retry < MAX_RETRY_ON_EAGAIN) {
	        max_retry++;
                my_msleep(MS_BEFORE_RETRY);
	        goto retry;
            }
	}
        gettimeofday(&end, NULL);
        time_diff(start, end, &milli, &micro);
        ERROR ("%s DQBUF has failed [%d: %s] after %d of %d milli [%lu/%lu]\n",
                   th_name, _errno,  strerror (_errno),
                   max_retry, MS_BEFORE_RETRY, milli, micro);
	errno = _errno;
	return(res);
    }
    gettimeofday(&end, NULL);
    time_diff(start, end, &milli, &micro);
    TRACE("DQBUF has got a frame from %d  after %d retry (in %lu/%lu milli)\n",
	  fd, max_retry, milli, micro);
    return res;
}
/*====================================================================== */
void *
thread_capturer (void *arg)
{
    struct thread_capturer_s *ctx;             /* working context */
    struct v4l2_buffer        buf;
    int                       res;
    unsigned int              levent;

    ctx = (struct thread_capturer_s *) arg;
    th_name(ctx) = ".. th_capturer";
    gettimeofday(&ctx->start, NULL);

    do {
        levent = get_event();
        switch (levent) {
            case INIT:
                /* do STREAMON on capture .... */
                res = v4l2_stream_on(ctx->v4l2cfid,
                                     V4L2_BUF_TYPE_VIDEO_CAPTURE);
                if (res != 0)
                {
                    ERROR ("%s: V4L2 capture STREAMON failed, (%d: %s) quit!\n",
                           th_name(ctx), errno, strerror(errno));
                    set_event(QUIT, BROADCAST);
		    break;
                }
                LOG("STREAMON on capture done!\n");
		set_event(RUNNING, MYSELF);
                break;

            case QUIT:
            case END:
                gettimeofday(&ctx->stop, NULL);
                break;

            case RUNNING:
                /* try to capture a decoded frame (wait if any) */
                memset(&buf, '\0', sizeof(struct v4l2_buffer));
                res = v4l2_dequeue_buf(ctx->v4l2cfid, &buf,
                                       V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                       V4L2_MEMORY_USERPTR,
                                       ref_event(), ctx->th.name);
                if (res != 0)
                {
                    ERROR ("%s: DQBUF failed or interrupted (%d, %s) quit!\n",
                           th_name(ctx), errno, strerror(errno));
                    set_event(QUIT, BROADCAST);
                    break;
                }

                TRACE("%s: DQBUF frame [%d, 0x%x] @ 0x%x, l.0x%x sz.0x%x PTS[%lu/%lu]\n",
                     th_name(ctx), buf.index, buf.flags,
                     buf.m.offset, buf.bytesused, buf.length,
                     buf.timestamp.tv_sec, buf.timestamp.tv_usec);

                inc_counter(ctx, in_count);

                /* just exit (injector should be already ended and
                 * sink hopefully will)!
		 */
                if (buf.bytesused == 0)
                {
                    LOG ("%s: got EOS (End Of Stream) !\n", th_name(ctx));
		    set_event(END, MYSELF);
		    break;
                }

                if (ctx->arg_testCase < 3)
                {
		    /* just return to decoder! */
                    res = v4l2_queue_buf(ctx->v4l2cfid, &buf,
                                         V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                         V4L2_MEMORY_USERPTR);
                    if (res != 0)
                    {
                        ERROR ("%s: QBUF has failed, (%d: %s) quit!\n",
                               th_name(ctx), errno, strerror(errno));
                        set_event(QUIT, BROADCAST);
                        break;
                    }
                }
                else
                {
                    /* Output on display */
                    buf.field = V4L2_FIELD_NONE;

                    if (ctx->arg_disablePTS) {
                        /* display as soon as possible */
                        buf.timestamp.tv_usec = 0;
                        buf.timestamp.tv_sec = 0;
                    }

                    res = v4l2_queue_buf(ctx->v4l2ofid, &buf,
                                         V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                         V4L2_MEMORY_MMAP);
                    if (res != 0)
                    {
                        ERROR ("%s: QBUF has failed, (%d: %s) quit!\n",
                               th_name(ctx), errno, strerror(errno));
                        set_event(QUIT, BROADCAST);
                    }

                    /* At least a bugger must be queued to do STREAMON */
                    if (ctx->out_count == 0)
                    {
                        res = v4l2_stream_on(ctx->v4l2ofid,
                                             V4L2_BUF_TYPE_VIDEO_OUTPUT);
                        if (res != 0)
                        {
                            ERROR ("%s: V4L2 output STREAMON failed, (%d: %s) quit!\n",
                                   th_name(ctx), errno, strerror(errno));
                            set_event(QUIT, BROADCAST);
                        }
                        LOG("%s: STREAMON on output done!\n", th_name(ctx));
                    }
                }
                inc_counter(ctx, out_count);
                break;

            default:
                ERROR ("%s: Internal - unknown event <%4s>, quit!\n",
                       th_name(ctx), (char *)&levent);
                set_event(QUIT, BROADCAST);
        }

    } while ((levent != QUIT) && (levent != END));

    LOG ("%s: thread  exiting...\n", th_name(ctx));
    return (void *)&ctx->th;   /* return thread status */
}

void *
thread_sink (void *arg)
{
    struct thread_sink_s     *ctx;             /* working context */
    struct v4l2_buffer        buf;
    unsigned long             s_addr;
    int                       res;
    unsigned int	      levent;

    ctx = (struct thread_sink_s *) arg;
    gettimeofday(&ctx->start, NULL);
    th_name(ctx) = "-- th_sink";
    do {
	levent = get_event();
        switch (levent) {
            case INIT:
		/* nothing to do! */
		res = set_thread_scheduler(0, SCHED_RR, 40, th_name(ctx));
                if (res != 0)
		{
		    ERROR ("%s: cannot modify priority (%d: %s)!\n",
                           th_name(ctx), res, strerror(errno));
                    set_event(QUIT, BROADCAST);
		    break;
		}
		/* Show new scheduler attributes */
		if (show_thread_scheduler(0, th_name(ctx)) != 0)
		{
		    ERROR ("%s: error reading sched. params!\n", th_name(ctx));
                    set_event(QUIT, BROADCAST);
		    break;
		}
                set_event(RUNNING, MYSELF);
                break;

            case QUIT:
            case END:
                gettimeofday(&ctx->stop, NULL);
                break;

            case RUNNING:
                /* Try to get an empty buffer from display device ... */
                memset(&buf, '\0', sizeof(struct v4l2_buffer));
		/* try to dequeue a frame */
               res = v4l2_dequeue_buf(ctx->v4l2ofid, &buf,
                                      V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                      V4L2_MEMORY_MMAP,
                                      ref_event(), ctx->th.name);
                if (res != 0)
                {
		    /* check whether interrupted because of a EOS
                       in this case main sets the event to END and
                       than triggers a STREAMOFF
		    */
                    if (get_event() == END)
			break;

                    ERROR ("%s: DQBUF failed or interupted (%d: %s) quit!\n",
                           th_name(ctx), errno, strerror(errno));
                    set_event(QUIT, BROADCAST);
                    break;
                }

                /* queue back for capture device
                 * Be aware that output devices returns a an offset from
                 * start of memory (phys addrr) while capture vant a user
                 * virtual (V4L2_MEMORY_USERPTR)
                 */
                s_addr = buf.m.offset;
                buf.m.userptr = phys2user(s_addr);
                if (buf.m.userptr == 0)
                {
                    ERROR ("%s: Internal! phys2user has failed quit!\n",
                           th_name(ctx));
                    set_event(QUIT, BROADCAST);
                    break;
                }

 TRACE ("%s: QBUF frame [%d, 0x%x] @ 0x%lx, sz.0x%x user.0x%lx PTS[%lu/%lu]\n",
                     th_name(ctx), buf.flags,
                     buf.index, s_addr, buf.length, buf.m.userptr,

                     buf.timestamp.tv_sec, buf.timestamp.tv_usec);

                /* return buffer to capture device */
                res = v4l2_queue_buf(ctx->v4l2cfid, &buf,
                                     V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                     V4L2_MEMORY_USERPTR);
                if (res != 0)
                {
		    /* check whether interrupted because of a EOS
                       in this case main sets the event to END and
                       than triggers a STREAMOFF
		    */
                    if (get_event() == END)
			break;

                    ERROR ("%s: QBUF failed, (%d: %s) quit!\n",
                           th_name(ctx), errno, strerror(errno));
                    set_event(QUIT, BROADCAST);
                    break;
                }
                inc_counter(ctx, out_count);
                break;

            default:
                ERROR ("%s: Internal - unknown event <%4s>, quit!\n",
                       th_name(ctx), (char *)levent);
                set_event(QUIT, BROADCAST);
        }
    } while ((levent != QUIT) && (levent != END));

    return (void *)&ctx->th;   /* return thread status */
}

/*====================================================================== */
typedef enum grab_format_e {
	UNDEFINED,
        HD_720,
        HD_1080,
        SD_525,
        SD_625,
        DECIMATED,		/* QCIF ?*/
        INVALID			/* must be last	*/
} grab_format_t;

struct scr_format_s {
    grab_format_t  standard;
    unsigned int   width;
    unsigned int   height;
    unsigned int   pixelformat;
    char          *name;
}  _fmt[] = {
   [UNDEFINED]  = { UNDEFINED,    0,     0,  0,                 "UND?" },
   [HD_720]     = { HD_720,    1280,   720,  V4L2_PIX_FMT_NV12, "NV12" },
   [HD_1080]    = { HD_1080,   1920,  1088,  V4L2_PIX_FMT_NV12, "NV12" },
   [SD_525]     = { SD_525,     720,   480,  V4L2_PIX_FMT_NV12, "NV12" },
   [SD_625]     = { SD_625,     720,   576,  V4L2_PIX_FMT_NV12, "NV12" },
   [DECIMATED]  = { DECIMATED,    0,     0,  V4L2_PIX_FMT_NV12, "NV12" }, /*?*/
};

int define_format (int arg_pixf, int arg_resolution, struct v4l2_format *fmt)
{

    LOG ("Define fmt: resolution %d\n", arg_resolution);
    if ((arg_resolution <= UNDEFINED) || (arg_resolution > DECIMATED))
        return -1;

    fmt->fmt.pix.width  = _fmt[arg_resolution].width;
    fmt->fmt.pix.height = _fmt[arg_resolution].height;
    /* NOTE: must match native format */
    fmt->fmt.pix.pixelformat = arg_pixf;
    fmt->fmt.pix.field       = V4L2_FIELD_ANY;
    {
	char *m = "Buf type ???";
	if (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
	    m = "V4L2_BUF_TYPE_VIDEO_CAPTURE";
	else if (fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
	         m = "V4L2_BUF_TYPE_VIDEO_OUTPUT";
        TRACE ("Define fmt %s, w/h %5d/%5d, pix.fmt 0x%x, field %x\n", m,
               fmt->fmt.pix.width, fmt->fmt.pix.height,
               fmt->fmt.pix.pixelformat, fmt->fmt.pix.field);
    }
    return 0;
}

/*
 * check_output device:
 *   1. Checks if output device (display) support streaming I/O model
 *   2. should user set the output ?   (likely yes)
 *   3. Get and Set the output format (native video decode)
 * Returns < 0 when fails
 * Argumnets:
 *    v4l2ofid       in   - Output device (aleady open)
 *    arg_output     in   - Video output to set (ASCII name)
 *    arg_pixf       in   - used to set output pixel format
 *    arg_resolution in   - used to set output width/height format
 *
 * Note:
 *    likely output format may affect buffers allocation, so
 *    it should be set independently
 * See also player_set_stream_encoding in dvbtest
 */
int check_output (int    v4l2ofid,   /* V4L2 output video dev  */
		  char		     *arg_output,
                  int                 arg_pixf,
                  int                 arg_resolution)
{
    int res;

    /* Check if it support streaming I/O */
    {
        struct v4l2_capability      Capabilities;

        memset (&Capabilities, 0, sizeof (struct v4l2_capability));
        res = ioctl (v4l2ofid, VIDIOC_QUERYCAP, &Capabilities);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "output VIDIOC_QUERYCAP", strerror (errno));
            return res;
        }
        if ((Capabilities.capabilities & V4L2_CAP_STREAMING) == 0)
        {
            ERROR ("Output device does not support streaming!\n");
            return -1;
        }
    }

    /*
     * now we have to set the output, mean the display plane we
     * desire to use. In case of "zero copy" capture the video
     * format is "native" (NV12  STM 4:2:0 MB Raster), and this
     * format is supported by Video Planes.
     * This code is quite rough, it selects the output based on a
     * ASCII name, likely it would be better (but more complicate)
     * to capture the capabilities of each output and then to select
     */
    {
        struct v4l2_output   output;
	char                 name[MAX_PATH_SIZE];

        output.index = 0;
        while ( (res = ioctl (v4l2ofid, VIDIOC_ENUMOUTPUT, &output)) == 0)
        {
            LOG ("Video output %d: %s\n", output.index, output.name);
            if (strcmp (arg_output, (char *) output.name) == 0)
                break;
            output.index++;
        }
	LOG("output index = %d\n", output.index);

        if (res < 0)
        {
            /* mean the arg_output doesn't exists!  May be the
             * platform has changed and the application not updated
             */
            ERROR ("V4l2 output device does not include: %s!\n", name);
	    return(res);
        }
        /* seems ok, try to set this output */
        LOG ("video output: setting %d %s\n", output.index, output.name);
        res = ioctl (v4l2ofid, VIDIOC_S_OUTPUT, &output);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "VIDIOC_S_OUTPUT", strerror (errno));
	    return res;
        }
    }

    /* get and set the appropriate output fmt */
    {
        struct v4l2_format          fmt;

        memset (&fmt, 0, sizeof (struct v4l2_format));

        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        res = ioctl (v4l2ofid, VIDIOC_G_FMT, &fmt);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "output VIDIOC_G_FMT", strerror (errno));
            return res;
        }
        /* Compiler complains if pixelformat (4 ASCII chars ...) is
           defined as string:  LOG ("....., pixelfmt %.4s,   */
        TRACE ("Output: got fmt %s, w/h %5d/%5d, pix.fmt 0x%x, field %x\n",
              "V4L2_BUF_TYPE_VIDEO_OUTPUT",
              fmt.fmt.pix.width,
              fmt.fmt.pix.height,
              fmt.fmt.pix.pixelformat, fmt.fmt.pix.field);

        /* now set fmt with desired format ... */
        res = define_format(arg_pixf, arg_resolution, &fmt);
        if ( res < 0 )
        {
	    ERROR("unknown output format!\n");
            return res;
        }

        res = ioctl (v4l2ofid, VIDIOC_S_FMT, &fmt);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "output VIDIOC_S_FMT", strerror (errno));
            return(res);
        }

        /*
         * Seems it is necessary also to set the cropping window
         * otherwise QBUF miserely fails!
         */
	{
	    struct v4l2_crop	crop;

	    memset(&crop, '\0', sizeof(struct v4l2_crop));
            crop.type  =   V4L2_BUF_TYPE_VIDEO_OUTPUT;
            res = ioctl (v4l2ofid, VIDIOC_S_CROP, &crop);
            if ( res < 0 )
            {
                ERROR ("%s failed - %s\n", "output VIDIOC_G_CROP", strerror (errno));
                return res;
            }

            LOG("Window crop: left 0x%x, top 0x%x, width 0x%x, height 0x%x\n",
                crop.c.left, crop.c.top, crop.c.width, crop.c.height);
            res = ioctl (v4l2ofid, VIDIOC_S_CROP, &crop);
            if (res < 0)
            {
                ERROR ("%s failed - %s\n", "output VIDIOC_S_CROP", strerror (errno));
                return(res);
            }
        }
    }
    return 0;
}

/*
 * alloc_buffers
 * For time being ask the output device to allocate (user can't
 * allocate contiguous).
 * Query each buffer to get its reference and to map it in user.
 * Returns the number of allocated buffers (0 has failed)
 * TBD: this function should be simplified to allocate and map
 *      a single buffer
 * Argumnets:
 *    v4l2ofid  in     - Output device (aleady open)
 *    bufreq    in     - number of buffer requested
 *    size      in/out - minimal size requested (return can be >)
 *    bufs      out    - filled with buffer references (mapped in user
 *                       space) and size of each buffer.
 *
 * See also player_set_stream_encoding in dvbtest
 */
int alloc_buffers (int    v4l2ofid,   /* V4L2 output video dev  */
                   int                 bufreq,
		   unsigned int       *size,
                   struct buf_s       *bufs)
{
    int res;
    int granted_bufs = 0;
    struct v4l2_requestbuffers  reqbuf;

    /*  ask this device to provide buffers and map them */
    {
        memset (&reqbuf, 0, sizeof (struct v4l2_requestbuffers));
        reqbuf.count             = bufreq;
        reqbuf.type              = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        reqbuf.memory            = V4L2_MEMORY_MMAP;
        res = ioctl (v4l2ofid, VIDIOC_REQBUFS, &reqbuf);
        if (res < 0)
        {
            ERROR ("Buffer Request (VIDIOC_REQBUFS) failed (%s).\n",
                   strerror(errno));
	    if (res == EBUSY)
                ERROR ("... must be the first video output user!\n");
            return res;
        }
        LOG ("Video output device grants %d buffers!\n", reqbuf.count);

        granted_bufs = reqbuf.count;	/* remember this value */

	if (reqbuf.count < bufreq)
	{
	    /* assume the negotiation will be refused, hence release
             * reserved buffer */
	    reqbuf.count = 0;
            res = ioctl (v4l2ofid, VIDIOC_REQBUFS, &reqbuf);
	    if (res < 0) {
                ERROR ("buf release (VIDIOC_REQBUFS = %d) has failed (%s)\n",
                       reqbuf.count, strerror(errno));
		return(res);
	    }
	    /* return what is available */
	    return(granted_bufs);
	}

        /* query each buffer and map it in user space */
        {
            struct v4l2_buffer              buf;
            int                             i;

            memset (&buf, 0, sizeof (struct v4l2_buffer));
            buf.type               = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buf.memory             = V4L2_MEMORY_MMAP;

            for (i = 0; i < granted_bufs; i++, bufs++)
            {
                buf.index = i;
                res = ioctl (v4l2ofid, VIDIOC_QUERYBUF, &buf);
                if (res < 0)
                {
                    ERROR ("output VIDIOC_QUERYBUF of %d failed (%s).\n",
                          i, strerror (errno));
                    return res;
                }
                /* LOG("from output: Buf len 0x%x off. 0x%x\n", */
                /*      buf.length, buf.m.offset); */
		LOG("mmap: 0x%x  size %d (0x%x)\n", buf.m.offset, buf.length, buf.length);
                /* Although not accessed in user space, map these buffers */
                bufs->uaddr = mmap (0, buf.length, PROT_WRITE,
                                    MAP_SHARED, v4l2ofid, buf.m.offset);
                if (bufs->uaddr == MAP_FAILED)
                {
                    ERROR ("output Buf. mmap failed (%s).\n", strerror (errno));
                    return 0;	/* alloc or mapping is failed */
                }
                bufs->len    = buf.length;
                bufs->offset = buf.m.offset;

		/* return the length of last buffer (not precise but for
		 * time being all buffers have the same length) */
		*size        = buf.length;
            }
        }  /* end query */
    }
    /* assume all OK, return the n. of allocated bufs. */
    return(granted_bufs);
}

/*
 * check_capture:  a cascade set of checks:
 *   whether supports USERPTR streaming I/O model.
 *   whether the given input exists
 *   whether supports capture with the defined format
 * In input Capture device is already open.
 * Argumnets:
 *    v4l2cfid    in  - the V4L2 capture device (required for capabilities)
 *    arg_video   in  - N. of LinuxDVB decoder (to select capture input)
 *    arg_pixf    in  - capture pixel format
 *    arg_resolution  - used to zise and quantify frame buffers
 *
 * Returns 0 when all OK, < 0 in case of error
 */
int check_capture (int    v4l2cfid,      /* V4L2 capture device    */
                   int    arg_video,     /* N. of V4l2 capture dev */
                   int    arg_pixf,	 /* desired frame format   */
                   int    arg_resolution)
{
    int res = -1;

    /*
     * get device capabilities and checks for STREAMING I/O model
     */
    {
        struct v4l2_capability      Capabilities;

        memset (&Capabilities, 0, sizeof (struct v4l2_capability));
        res = ioctl (v4l2cfid, VIDIOC_QUERYCAP, &Capabilities);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "capture VIDIOC_QUERYCAP", strerror (errno));
            return res;
        }

        LOG ("HW Type: Driver %s,\n\t Card %s,\n\t BusInfo %s,\n\t Vers. %d, Cap. %x\n",
              Capabilities.driver, Capabilities.card,
              Capabilities.bus_info, Capabilities.version,
              Capabilities.capabilities);

        if ((Capabilities.capabilities & V4L2_CAP_STREAMING) == 0)
        {
            ERROR ("Capture device does not support streaming!\n");
            return -1;
        }
        /* shoud we check USERPTR here ? or later */
    }

    /*
     * Select the input (which decode)
     */
    {
        struct v4l2_input   input;
	char                name[MAX_PATH_SIZE];
        sprintf (name, "dvb0.video%d", arg_video);

        input.index = 0;
        while ( (res = ioctl (v4l2cfid, VIDIOC_ENUMINPUT, &input)) == 0)
        {
            if (strcmp (name, (char *) input.name) == 0)
                break;
            input.index++;
        }
        if (res < 0)
        {
            /* there is a problem! ...
               ... trace all the input available before failing */
            input.index = 0;
            while (ioctl (v4l2cfid, VIDIOC_ENUMINPUT, &input) == 0)
            {
                LOG ("capture input %d: %s\n", input.index, input.name);
                input.index++;
            }
            ERROR ("Capture device does not have the input: %s!\n", name);
            return res;
        }
        /* seems ok, try to set this input */
        LOG ("capture: setting input %d: %s\n", input.index, input.name);
        res = ioctl (v4l2cfid, VIDIOC_S_INPUT, &input);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "capture VIDIOC_S_INPUT", strerror (errno));
	    return res;
        }
    }

    /*
     * Get and Set the capture format
     */
    {
        struct v4l2_format          fmt;
        memset (&fmt, 0, sizeof (struct v4l2_format));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

#ifdef V4L2_COMPLIANT_ADAPTATION
	/*
         * for time being don't GET or TRY FMT, adaptation accepts
         * only a set format as a first call!
         */
        res = ioctl (v4l2cfid, VIDIOC_G_FMT, &fmt);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "capture VIDIOC_G_FMT", strerror (errno));
            return res;
        }
        LOG ("Capture: got fmt %s, w/h %5d/%5d, pix.fmt 0x%x, field %x\n",
              "V4L2_BUF_TYPE_VIDEO_CAPTURE",
              fmt.fmt.pix.width,
              fmt.fmt.pix.height,
              fmt.fmt.pix.pixelformat, fmt.fmt.pix.field);
#endif
        /* now set fmt with desired format ... */
        res = define_format(arg_pixf, arg_resolution, &fmt);
        if ( res < 0 )
        {
	    ERROR("unknown format!\n");
            return res;
        }

        res = ioctl (v4l2cfid, VIDIOC_S_FMT, &fmt);
        if (res < 0)
        {
            ERROR ("%s failed - %s\n", "capture VIDIOC_S_FMT", strerror (errno));
            return res;
        }
    }
        /*
	 * To be completed:
         *    Once video output has provided buffers (memory mapped)
         *    the same set has to be provided as USERPTR to capture device.
        bufreq.count             = ...
        bufreq.type              = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufreq.memory            = V4L2_MEMORY_USERPTR;
         */
    return res;
}
/*======================================================================*/
void log_term_status(struct thread_s *th)
{
    char *m;
    if (th->event == END)
    {
	/* Likely got an EOS frame */
	m = "succesfully";
    } else {
	m = "because of error";
    }
    LOG ("Thread %s terminate %s! (%.4s)\n", th->name, m, (char *)&th->event);
}

/* test case 1: Internal only, used to check the attach detach
 *              performed at STLinuxTV level, between Streaming
 *              Engine and STLinuxTV itself
 *              No use of threads.
 */
int test_case_01(int arg_testCase, int v4l2cfid)
{
    int res = 0;

    /* do nothing if the use_case is 0 */
    if (arg_testCase == 1)
    {
        /* do STREAMON on capture .... */
        res = v4l2_stream_on(v4l2cfid, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        if (res != 0)
	{
	    ERROR ("V4L2 capture STREAMON failed, (%s) quit!\n", strerror(errno));
	    return res;
	}
        LOG("STREAMON on capture done!\n");

        res = v4l2_stream_off(v4l2cfid, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        if (res != 0)
	{
            ERROR ("V4L2 capture STREAMOFF failed, (%s)!\n", strerror(errno));
	    return res;
	}
            /*this should release also the sync thread... */
        LOG("STREAMOFF on capture done!\n");
    }
    return res;
}


/*****************************************************************
 * test case 2: Internal only, used to check the frame production
 *		and the correct Push from SE to STLinuxTV
 *              Uses only the injection thread.
 */
int test_case_02(int arg_testCase, int v4l2cfid, int v4l2ofid)
{
    int res = 0;
    struct thread_s *th;

    /*
     *  - create the injector (write to LinuxDVB video device)
     *  - main thread captures the frames and returns immediatelly
     *    to SE decoder.
     *    Display device not used.
    */
    {
	set_event(INIT, BROADCAST);

	/* enable the injector as last thread (runs in "best effort") */
        LOG ("starting injection thread\n");
        res = pthread_create (&(th_inj.th.id), NULL,
                              thread_inject, &th_inj);
        if (res != 0)
	    goto thread_create_error;
    }

    /*****************************************************************/
    /* capture loop:                                                 */
    /*    exit because of interrupt or because of EOS                */
    /*    capture does also the STREAMON on both devices             */

    th_grab.arg_testCase = arg_testCase;
    th = thread_capturer((void *) &th_grab);

    log_term_status(th);

    /*****************************************************************/
    /* Start the close down sequence!                                */
    /* AA. join injector thread:                                     */
    {
	struct thread_s  *th;

	/* injector should always terminate first */
        res = pthread_join (th_inj.th.id, (void *)&th);
        if (res != 0)
	    goto thread_join_error;
	log_term_status(th);
    }

    /*******************************************************************/
    /* BC.  Do a STREAMOFF on capture        !                          */
    {
        res = v4l2_stream_off(v4l2cfid, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        if (res != 0)
            ERROR ("V4L2 STREAMOFF failed on capture, (%s)!\n",
	           strerror(errno));
        /* this should release also the sync thread... */
        LOG("STREAMOFF on capture done!\n");
    }
    return res;

thread_create_error:
    ERROR ("pthread_create error: %s\n", strerror (errno));
    return res;
thread_join_error:
    ERROR ("pthread_join error: %s\n", strerror (errno));
    return res;
}

/*****************************************************************
 * test case 3: User test (default)
 *	        A sink thread waits for empty frame from display
 *	        and return to the decoder
 *	        Inject thread, inject PES via LinuxDVB video device
 *	        The main thread performs the capture from decoder
 *	        and push to display.
 */
int test_case_03(int arg_testCase, int v4l2cfid, int v4l2ofid)
{
    int res = 0;
    struct thread_s *th;

    /*****************************************************************
     *  - create a sink thread (back to capture device empty grames)
     *  - create the injector (write to LinuxDVB video device)
     *  - capture and display loop is managed by main
    */
    {
	set_event(INIT, BROADCAST);

        LOG ("starting sink thread\n");
        res = pthread_create (&(th_sink.th.id), NULL,
                              thread_sink, &th_sink);
        if (res != 0)
	    goto thread_create_error;

	/* enable the injector as last thread (runs in "best effort") */
        LOG ("starting injection thread\n");
        res = pthread_create (&(th_inj.th.id), NULL,
                              thread_inject, &th_inj);
        if (res != 0)
	    goto thread_create_error;

    }

    /*****************************************************************/
    /* capture loop:                                                 */
    /*    exit because of interrupt or because of EOS                */
    /*    capture does also the STREAMON on both devices             */

    th_grab.arg_testCase = arg_testCase;
    th = thread_capturer((void *) &th_grab);

    if (th->event == END)
	/* Got an EOS: wait few milli to allow to sync all frames */
	my_msleep(100);

    log_term_status(th);

    /*****************************************************************/
    /* Start the close down sequence!                                */
    /* AA. join injector thread:                                     */
    {
	/* set_event(QUIT, BROADCAST);  not correct otherwise inject
 *         always terminates because of error */

	/* injector should always terminate first */
        res = pthread_join (th_inj.th.id, (void *)&th);
        if (res != 0)
	    goto thread_join_error;

        log_term_status(th);
    }


    /*******************************************************************/
    /* BA.  Do aSTREAMOFF on output first. Should release all buffers  */
    /*      and wakeup any waiting thread.                             */
    {
	/* thread sink will be interrupted
         * it always terminate with an error because interruped while
         * waiting on output (thread synchronization schema should be
         * improved, however the difficulty for sync thread to
         * understand when the frame flow terminates is real)
         */
	set_event(END, &th_sink.th);
        res = v4l2_stream_off(v4l2ofid, V4L2_BUF_TYPE_VIDEO_OUTPUT);
        if (res != 0)
            ERROR ("V4L2 STREAMOFF failed on output, (%s)!\n",
	           strerror(errno));
        /* this should release also the sync thread... */
        LOG("STREAMOFF on output done!\n");
    }

    /*******************************************************************/
    /* BB.  Do a STREAMOFF on capture        !                          */
    {
        res = v4l2_stream_off(v4l2cfid, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        if (res != 0)
            ERROR ("V4L2 STREAMOFF failed on capture, (%s)!\n",
	           strerror(errno));
        /* this should release also the sync thread... */
        LOG("STREAMOFF on capture done!\n");
    }

    /*******************************************************************/
    /* BC. join sink thread now:                                       */
    {
        res = pthread_join (th_sink.th.id, (void *)&th);
        if (res != 0)
	    goto thread_join_error;
        log_term_status(th);
    }

    return res;

thread_create_error:
    ERROR ("pthread_create error: %s\n", strerror (errno));
    return res;
thread_join_error:
    ERROR ("pthread_join error: %s\n", strerror (errno));
    return res;
}

void close_all(int v4l2cfid, int v4l2ofid,
               int vdataid,  int dvbvid)
{
    if (v4l2ofid > 0)
        close(v4l2ofid);
    if (v4l2cfid > 0)
        close(v4l2cfid);
    /*  close data input and LinuxDVB video device */
    if (vdataid > 0)
        close(vdataid);
    if (dvbvid > 0)
        close(dvbvid);
}

/*======================================================================*/
/* this helper is here only for debug, remove it asap! */

void SigactionHandler (int signum)
{
    LOG ("Main terminated because of signal!\n");
    /* simply, try to send a quit command to all threads */
    set_event(QUIT, BROADCAST);
}


/*======================================================================*/
int
main (int argc, char *argv[])
{
    char *input_file_name = NULL;

    int i, res;
    int opt;

    /* Optional args: set defaults */
    int arg_video = 0;
    int arg_codec = VIDEO_ENCODING_H264;
    int arg_pixf  = V4L2_PIX_FMT_NV12;
    int arg_resolution = HD_1080;    /* picture resolution = HD            */
/*    int arg_keyboard = 0;             not implemented                    */
    int arg_capturer_blocking = 0;   /* NO BLOCK by default                */
    int arg_sink_blocking = 0;       /* NO BLOCK by default                */
    int arg_disablePTS = 0;          /* false: by default uses PTS         */
    int arg_userBufs = 0;            /* user reserved buffers              */
    int arg_delay = 0;               /* def. no injection delay (in milli) */
    int arg_testCase = 3;            /* first meaningful test (default)
                                        1, 2 are internal only)
				     */
/*    int arg_rate = 0;                 injection rate (not implemented
                                        use best effort                    */
    char *arg_output = NULL;         /* name of Video output device        */

    int vdataid;	    /* video data input file ID */
    int dvbvid = -1;	    /* LinuxDVB video file ID   */
    int v4l2cfid = -1;      /* V4L2 capture file ID     */
    int v4l2ofid = -1;      /* V4L2 output file ID      */

    arg_verbose = 0;        /*  OFF */

    /*------------------ */
    if (argc < 2)
        usage (argc, argv);

    while ((opt = getopt (argc, argv, option_list)) != -1)
    {
        switch (opt)
        {
            case 'V':   /* enable additional TRACE */
                arg_verbose = 1;
                break;
            case 'b':   /* enable capture to work in BLOCKING mode */
                arg_capturer_blocking = 1;
                break;
            case 'B':   /* enable sink to work in BLOCKING mode */
                arg_sink_blocking = 1;
                break;
            case 'P':   /* disable use of PTS (goes in free running) */
                arg_disablePTS = 1;
                break;
            case 'T':   /* Test case number 1,2 reserved */
                arg_testCase = atoi(optarg);
                break;

            case 'u':   /* Number of extra buffers reserved for user */
                arg_userBufs = atoi(optarg);
                break;

            case 'h':   /* picture resolution HD */
                arg_resolution = HD_720;
                break;
            case 'H':   /* picture resolution HD (default) */
                arg_resolution = HD_1080;
                break;
            case 's':   /* picture resolution SD */
                arg_resolution = SD_525;
                break;
            case 'S':   /* picture resolution SD */
                arg_resolution = SD_625;
                break;
            case 'd':   /* interleave injection with delay */
                arg_delay = atoi(optarg);
                break;

            case 'c':  /* select codec type */
		arg_codec = opt_ascii2id(optarg, encoding_std);
                if (arg_codec == -1) {
                    ERROR ("-c must be followed by a known codec type!\n");
                    usage (argc, argv);
                }
                break;

            case 'v':   /* LinuxDVB Video device (number) */
                arg_video = atoi(optarg);
                if ((arg_video <0) || (arg_video > 7)) {
                    ERROR ("video input '%d' not in range [0-%d]\n",
                            arg_video, (MAX_GRAB_INPUT - 1));
                    usage (argc, argv);
                }
                break;

            case 'o':   /* output Video device (name of output plane) */
                arg_output = optarg;
                break;

            case 'f':   /* grab pixel format */
		arg_pixf   = opt_ascii2id(optarg, pix_formats);
                if (arg_pixf == -1) {
                    ERROR ("-f must be followed by a known format!\n");
                    usage (argc, argv);
                }
                break;

#if 0
	// not implemented yet (so far works in best effort only and
	// window always at 0:0)
            case 'r':  /* set a rough bit rate: between 1 and 60 Mbits */
                arg_rate = atoi(optarg);
                if ((arg_rate < 1) && (arg_rate > 60)) {
                    ERROR ("-r bit rate [1-60] in mega must follow!\n");
                    usage (argc, argv);
                }
                break;
            case 'w':  //... set output window position...
#endif
            case '?':
            default:
                usage (argc, argv);
        }
    }

    if ((arg_video == -1) || (arg_codec == -1) || (arg_pixf == -1))
    {
        ERROR ("Mandatory argument is missed!\n");
        usage(argc, argv);
    }
    arg_output = opt_ascii_check(arg_output, output_devices_tab);
    if (arg_output == NULL)
    {
	ERROR ("Invalid option <output>!\n");
        usage(argc, argv);
    }


    /* look for input file name */
    if (optind == (argc-1))
	input_file_name = argv[optind];
    else {
        ERROR ("incomplete argument list!\n");
        usage (argc, argv);
    }
    /*****************************************************************/
    /* set signal handler
     * for all signals assume to simply quit!
    */
    {
	struct sigaction    SigAction;

        SigAction.sa_handler  = SigactionHandler;
	sigfillset(&SigAction.sa_mask);
/*        sigemptyset (&SigAction.sa_mask); */
        SigAction.sa_flags    = 0;

        sigaction (SIGUSR1, &SigAction, NULL);
    }

    /*****************************************************************/
    /* reset thread's working contexts (not a must but ...)
    */
    memset(&th_inj,  '\0', sizeof (struct thread_inject_s));
    memset(&th_grab, '\0', sizeof (struct thread_capturer_s));
    memset(&th_sink, '\0', sizeof (struct thread_sink_s));

    /* Pass some args to threads */
    th_grab.arg_disablePTS = arg_disablePTS;
    th_sink.arg_disablePTS = arg_disablePTS;

    th_inj.arg_delay = arg_delay;
    /*****************************************************************/
    /*
     * Open input file and devices
     *  1. Open the input PES Video file (could be done by inject
     *     thread providing it clean all properly in case of error)
     */
    {

        if ((vdataid = open(input_file_name, O_RDONLY)) < 0)
        {
            ERROR ("wrong input file name: %s\n", input_file_name);
            goto abort;
        }
        /* inject thread need this */
        th_inj.input    = FROM_PES_FILE;
        th_inj.vdataid  = vdataid;
    }
    /*
     * 2. Open the LinuxDVB Video device (decoder):
     */
    {
        char name[MAX_PATH_SIZE];

        sprintf(name, "%s%1d", LinuxDVB_INPUT_VIDEO_DEVICE_NAME, arg_video);
        dvbvid = open (name, O_RDWR | O_NONBLOCK);
        if (dvbvid < 0)
        {
            ERROR ("failed to open %s\n", name);
            goto abort;
        }
        LOG ("main: open LinuxDVB %s\n", name);
        /* injection thread needs DVB device to inject */
        th_inj.dvbvid = dvbvid;
        /* Video capture thread needs DVB video N. to
         * select the relevant decode to capture from */
        th_grab.videoN = arg_video;
    }

    /*
     *  3. Open V4L2 capture device
     *     Open V4L2 video output device (to TV)
     */
    {
        char name[MAX_PATH_SIZE];

        sprintf(name, "%s", V4L2_CAPTURE_DEVICE_NAME);
	if (arg_capturer_blocking == 1)
            v4l2cfid = v4l2_open_by_name (name, V4L2_STM_CARD_NAME,
					  O_RDWR);
        else
            v4l2cfid = v4l2_open_by_name (name, V4L2_STM_CARD_NAME,
					  (O_RDWR | O_NONBLOCK));
        if (v4l2cfid < 0)
        {
            ERROR ("open of <%s> has failed (%s)\n", name, strerror (errno));
            goto abort;
        }
        LOG ("main: open video capture device %s (bloking = %s; fileID %d)\n",
              name, ((arg_capturer_blocking) ? "true" : "false"), v4l2cfid);
        /* both threads need v4l2 video input device */
        th_grab.v4l2cfid	= v4l2cfid;
        th_sink.v4l2cfid        = v4l2cfid;

        sprintf(name, "%s", V4L2_DISPLAY_DRIVER_NAME);
        /* sink (display) thread always works in blocking mode! */
	if (arg_sink_blocking == 1)
            v4l2ofid = v4l2_open_by_name (name, V4L2_STM_CARD_NAME,
					  O_RDWR);
	else
            v4l2ofid = v4l2_open_by_name (name, V4L2_STM_CARD_NAME,
					  (O_RDWR | O_NONBLOCK));
        if (v4l2ofid < 0)
        {
            ERROR ("open of <%s> has failed (%s)\n", name, strerror (errno));
            goto abort;
        }
        LOG ("main: open output display %s (blocking = %s; fileID %d)\n",
             name, ((arg_sink_blocking) ? "true" : "false"), v4l2ofid);

        /* both threads need v4l2 video output device */
        th_sink.v4l2ofid = v4l2ofid;
        th_grab.v4l2ofid = v4l2ofid;

        /* sink thread needs screen portion to display
         * (can be provided via input args or using split
         * strategy as in grab_all tool)
         */
        th_sink.tv_pos_w = 1;
        th_sink.tv_pos_h = 1;
    }

    /*
     * 4. back to LinuxDVB:
     *    set codec type and create the player.
     * would be interesting to create player (VIDEO_PLAY) later
     *  - after check_capture
     *  - after negotiation (request of # of buffer should fail)
     *
     */
    {
	struct video_command        VideoCommand;

	/*
         * Set the stream type to play (for time being we inject video
         * PES from memory)
         */
        res = ioctl (dvbvid, VIDEO_SET_STREAMTYPE, STREAM_TYPE_PES);
        if (res != 0)
        {
            ERROR ("%s failed - %s \n", "VIDEO_SET_STREAMTYPE (pes)", strerror (errno));
            goto abort;
        }
        LOG ("LinuxDVB: VIDEO_SET_STREAMTYPE to %s\n", "PES");

	/* for time being start to test the "zero copy" capture
         * injecting from memory (i.e. PES via /dev/dvb/adapter/videoX)
         */
        res = ioctl (dvbvid, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY);
        if (res != 0)
        {
            ERROR ("%s failed - %s \n", "VIDEO_SELECT_SOURCE", strerror (errno));
            goto abort;
        }
        LOG ("LinuxDVB: VIDEO_SELECT_SOURCE, to %s\n", "MEMORY");

	/* Inform the player we want to grab native frames (by default
         * player pushes decimated frames!)
         * Not clear what "dvbtest" captures since (apparently) it doesn't
         * perform this command !?
         */
	memset(&VideoCommand, '\0', sizeof(VideoCommand));
        VideoCommand.cmd              = VIDEO_CMD_SET_OPTION;
	VideoCommand.option.option    = DVB_OPTION_DECIMATE_DECODER_OUTPUT;
	VideoCommand.option.value     = DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED;
        res = ioctl (dvbvid, VIDEO_COMMAND, (void *)&VideoCommand);
        if (res != 0)
        {
            ERROR ("%s failed - %s \n", "VIDEO_COMMAND disable decimated", strerror (errno));
            goto abort;
        }
        LOG ("LinuxDVB: VIDEO_CMD_SET_OPTION, to %s\n", "DECIMATED output disabled");
        /* enable zero copy... */
        arg_codec |= VIDEO_ENCODING_USER_ALLOCATED_FRAMES;

        /* arg_codec |= STM_USER_ALLOCATED_DECODE_BUFFERS; */

        res = ioctl (dvbvid, VIDEO_SET_ENCODING, (arg_codec));
        if (res != 0)
        {
            ERROR ("%s failed - %s 0x%x \n", "VIDEO_SET_ENCODING",
                   strerror (errno), (unsigned int)arg_codec);
            goto abort;
        }
        LOG("LinuxDVB: VIDEO_SET_ENCODING to 0x%x\n", (unsigned int)arg_codec);


	/* also disable A/V sync  (by def. is not disabled) */
	if (arg_disablePTS)
        {
            struct video_command  cmd;
	    memset(&cmd, '\0', sizeof(cmd));
            cmd.cmd           = VIDEO_CMD_SET_OPTION;
            cmd.option.option = DVB_OPTION_AV_SYNC;
	    cmd.option.value  = DVB_OPTION_VALUE_DISABLE;
            res = ioctl (dvbvid, VIDEO_COMMAND, (void *)&cmd);
            if (res != 0)
            {
                ERROR ("%s failed - %s cmd: %s\n", "VIDEO_COMMAND",
			"a/v sync disable", strerror (errno));
                goto abort;
            }
            LOG("LinuxDVB: VIDEO_CMD_SET_OPTION to %s\n", "A/V SYNC disabled");
        }

        /* enable video player (adaptation creates a play stream) */
        res = ioctl (dvbvid, VIDEO_PLAY,  NULL);
        if (res != 0)
        {
            ERROR ("%s failed - %s \n", "VIDEO_PLAY", strerror (errno));
            goto abort;
        }
        LOG("LinuxDVB: VIDEO_PLAY done ...\n");

    }

    /*
     * 5.  Now check capture device
     *     sets the input and format to capture
     */
    if (check_capture(v4l2cfid, arg_video,
                      arg_pixf, arg_resolution) < 0)
	goto abort;

    /*
     * 6.  Now check video output device and sets the output
     *     device and the output format
     */
    if (check_output(v4l2ofid, arg_output,
                     arg_pixf, arg_resolution) < 0)
	goto abort;

    /* 7. asks player how many frame buffers it needs ... */
    {
	struct v4l2_control		ctrl[2];

        memset (&ctrl, '\0', sizeof (ctrl));
	ctrl[0].id           = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
	ctrl[1].id           = V4L2_CID_MPEG_STM_FRAME_SIZE;

        res = ioctl (v4l2cfid, VIDIOC_G_CTRL, &(ctrl[0]));
        if (res != 0)
        {
            ERROR ("%s failed - %s \n", "VIDIOC_G_CTRL MIN_BUFFERS_FOR_CAPTURE", strerror (errno));
            goto abort;
        }

        res = ioctl (v4l2cfid, VIDIOC_G_CTRL, &(ctrl[1]));
        if (res != 0)
        {
            ERROR ("%s failed - %s \n", "VIDIOC_G_CTRL DECODER_FRAME_SIZE", strerror (errno));
            goto abort;
        }

        LOG("Player requires: %d decode buffers; size %d/0x%x (MB %d.%d)\n",
            ctrl[0].value, ctrl[1].value, ctrl[1].value,
            (ctrl[1].value / MEGA), ((ctrl[1].value % MEGA) / 1024));

        /* Assume (at least) one extra buffer to compensate HW and SW
         * requirements:
         *  - HW: Orly FVDP Display continuously holds a buffer in its pipe!
         *  - SW: to guarantee smooth Video decoding the player must always
         *        have free buffers where to produce (regardless the life
         *        time of user shared buffers).
         *        A way to guarantee this is to dimension the pool:
         *            min # for codec + min # to share with users
         */
        bufs.user  = arg_userBufs;       /* keep it as lower as possible */
        bufs.rbp   = ctrl[0].value;
        bufs.size  = ctrl[1].value;
    }

    /*
     * 8. allocates buffers
     *    For time being, since a contiguous allocator doesn't exists
     *    in user mode, borrows from output device and mmap in user
     *    space.
     *    Buffers to allocate: rbp (request by player) + user reserved (now
     *    arbitrary and = 1)
     */
    {
	unsigned int size = 0;
        memset(bufs.list, '\0', sizeof(bufs.list));
	size = bufs.size;	/* desired size (min) */
        bufs.count = alloc_buffers (v4l2ofid,
                                   (bufs.rbp + bufs.user),
				    &size,  /* returned size can be > */
				    bufs.list);
        if (bufs.count < 0)
        {
	    /* frame buffer allocation has failed with an I/O error */
	    goto abort;
        }
        if (bufs.count < (bufs.rbp + bufs.user))
        {
	    /* should we retry the negotiation ? (not yet supported) */
            ERROR (" %d buffers are not enough to decode (quit)!\n", bufs.count);
	    goto abort;
        }

	/* adjust length, ... just in case */
	bufs.size = size;

        LOG("Buffers: allocated %d, usr reserved %d (size 0x%x)\n",
            bufs.count, bufs.user, bufs.size);
	for (i = 0; bufs.list[i].uaddr; i++)
            TRACE("\t [%d] user %p   kern(off) 0x%lx  len 0x%x\n", i,
                bufs.list[i].uaddr, bufs.list[i].offset, bufs.list[i].len);
    }

    /*
     *  9. informs capture device: buffers are avilable ...
     */
    {
        struct v4l2_requestbuffers  reqbuf;

        /*  inform capture of user provided buffers */
        memset (&reqbuf, 0, sizeof (struct v4l2_requestbuffers));
        reqbuf.count             = bufs.count;
        reqbuf.type              = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        reqbuf.memory            = V4L2_MEMORY_USERPTR;
        res = ioctl (v4l2cfid, VIDIOC_REQBUFS, &reqbuf);
        if (res < 0)
        {
            ERROR ("capture VIDIOC_REQBUFS failed (%s).\n", strerror (errno));
            goto abort;
        }
    }


    /* 10: provide buffers to SE:
     *     This is achieved with a number of preliminary v4l2 QBUF of empty
     *     buffers. LinuxTV adaptation collects all negotiated buffers
     *     and provid them to SE in one shot.
     */
    {
        struct v4l2_buffer buf;
        int    i;

        for (i = 0; i < bufs.count; i++)
        {
            memset(&buf, '\0', sizeof(struct v4l2_buffer));
	    buf.index     = i;
	    buf.type      = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory    = V4L2_MEMORY_USERPTR;
	    buf.m.userptr = (unsigned long)bufs.list[i].uaddr;
            buf.length    = bufs.list[i].len;

            res = ioctl (v4l2cfid, VIDIOC_QBUF, &buf);
	    if (res < 0)
	    {
                ERROR ("sending buffer %d to player (%s).\n", i, strerror (errno));
                goto abort;
	    }
	    bufs.list[i].status = TO_DECODER;
	}
        LOG("Sent %d frame buffers sent to player:\n", bufs.count);
    }

    /*****************************************************************/
    /*  Now execute the selected use case ....                       */
    LOG("Use Case #%d:\n", arg_testCase);
    switch (arg_testCase) {
        case 0:
            LOG("\tDo only setup and close on capture. Don't do\n");
            LOG("\tSTREAMON / STREAMOFF (hence no attach/detach).\n");
            LOG("\tDo nothing on output, only the close\n");
	    res = test_case_01(arg_testCase, v4l2cfid);
            break;

        case 1:
            LOG("\tDo STREAMON and STREAMOFF on capture\n");
            LOG("\tand then close.\n");
            LOG("\tDo nothing on output, only the close\n");
	    res = test_case_01(arg_testCase, v4l2cfid);
            break;

        case 2:
            LOG("\tStart an injection thread (64k granularity\n");
            LOG("\twhile main thread loops to capture all frames and\n");
            LOG("\treturn to decoder. Don't display\n");
            LOG("\tQBUF/DQBUF sequentialized on a single device\n");
	    res = test_case_02(arg_testCase, v4l2cfid, v4l2ofid);
            break;

        case 3:
            LOG("\tStart an injection thread (64k granularity) and a\n");
            LOG("\tthread to return empty frames to capture device.\n");
            LOG("\tMain thread loops waiting for decoded frames and\n");
            LOG("\tpushes to display (QBUF/DQBUF simultaneous on two\n");
            LOG("\tdevices\n");
	    res = test_case_03(arg_testCase, v4l2cfid, v4l2ofid);
            break;

        default:
            LOG("\tUnknown use case U %d\n", arg_testCase);
	    break;
    }

#ifdef NOTUSED
    /* deinitialize a unique direct framebuffer */
    if (nbr_grab != 0)
        dfb_release ();

    LOG ("display/write output buffer counts: ");
    if (output_file_name != NULL)
    {
        FILE *file = fopen (output_file_name, "w");
        output_buffer_counts (file, tinfo, 0);
        fclose (file);
    }
    else
    {
        output_buffer_counts (stdout, tinfo, arg_mode_display);
    }
#endif
    if (res)
	goto abort;

    close_all(v4l2cfid, v4l2ofid, vdataid, dvbvid);
    LOG ("The End!\n");
    exit(0);

abort:
    close_all(v4l2cfid, v4l2ofid, vdataid, dvbvid);
    ERROR ("aborted!\n");
    exit(1);
}
