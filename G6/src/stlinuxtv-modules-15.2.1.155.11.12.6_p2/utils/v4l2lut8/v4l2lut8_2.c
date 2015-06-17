#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <linux/fb.h>
#include <linux/videodev2.h>

#include <linux/stmfb.h>
#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

#include <directfb.h>

#define V4L2_DISPLAY_DRIVER_NAME	"Planes"
#define V4L2_DISPLAY_CARD_NAME		"STMicroelectronics"

#define DFBCHECK(x...)                                              \
  ( {                                                               \
    DFBResult err = x;                                              \
                                                                    \
    if (err != DFB_OK)                                              \
      {                                                             \
        typeof(errno) errno_backup = errno;                         \
        DirectFBError (#x, err);                                    \
        errno = errno_backup;                                       \
      }                                                             \
    err;                                                            \
  } )


static const unsigned int g_width = 1280;
static const unsigned int g_height = 720;

static IDirectFB             *DirectFBHandle;
static DFBSurfaceDescription  SurfaceDescription;


static void
init_dfb (IDirectFBSurface **SurfaceHandle)
{
  if (!DirectFBHandle) {
    DFBCHECK (DirectFBInit (0, NULL));
    DFBCHECK (DirectFBCreate (&DirectFBHandle));
  }

  SurfaceDescription.width  = g_width;
  SurfaceDescription.height = g_height;
  SurfaceDescription.pixelformat = DSPF_LUT8;
  SurfaceDescription.caps   = DSCAPS_VIDEOONLY;
  SurfaceDescription.flags  = (DSDESC_CAPS
			       | DSDESC_PIXELFORMAT
			       | DSDESC_WIDTH
			       | DSDESC_HEIGHT);
  
  DFBCHECK (DirectFBHandle->CreateSurface (DirectFBHandle,
					   &SurfaceDescription,
					   SurfaceHandle));  
}

static void
deinit_dfb (void)
{
  DFBCHECK (DirectFBHandle->Release (DirectFBHandle));
  DirectFBHandle = NULL;
}


static void
init_v4l (int fd, int left, int top, int width, int height)
{
  struct v4l2_requestbuffers reqbuf;
  struct v4l2_format         fmt;
  struct v4l2_crop           crop;

  memset(&reqbuf,0,sizeof(reqbuf));
  memset(&fmt,0,sizeof(fmt));

  fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width       = width;
  fmt.fmt.pix.height      = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_CLUT8;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  if (ioctl(fd,VIDIOC_S_FMT,&fmt))
    perror("Couldn't set height width or format: n");

  reqbuf.memory    = V4L2_MEMORY_USERPTR;
  reqbuf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if (ioctl(fd,VIDIOC_REQBUFS, &reqbuf) == -1)
    perror("Video display of user pointer streaming not supported: ");

  memset(&crop, 0, sizeof(struct v4l2_crop));
  crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  crop.c.left   = left;
  crop.c.top    = top;
  crop.c.width  = width;
  crop.c.height = height;

  if (ioctl(fd,VIDIOC_S_CROP,&crop) < 0)
    perror("Unable to set output crop");
}

static void queue_buffer(int fd, void* ptr, void *clut,unsigned long long int pts)
{
  struct v4l2_buffer buf;
  
  memset(&buf,0,sizeof(buf));
  
  buf.type         = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  buf.memory       = V4L2_MEMORY_USERPTR;
  buf.m.userptr    = (unsigned long)ptr;
  buf.field        = V4L2_FIELD_NONE;
  buf.length       = g_width * g_height;

  buf.reserved     = (__u32)clut;
  buf.flags        = V4L2_BUF_FLAG_NON_PREMULTIPLIED_ALPHA;
  
  if (ioctl(fd,VIDIOC_QBUF, &buf) == -1)
    perror("Couldn't queue user buffer\n");
}

static void dequeue_buffer(int fd)
{
  struct v4l2_buffer buf;
  
  memset(&buf,0,sizeof(buf));
  
  buf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  buf.memory    = V4L2_MEMORY_USERPTR;

  if (ioctl(fd,VIDIOC_DQBUF, &buf) == -1)
    perror("Couldn't de-queue user buffer\n");
} 

static void stream_on(int fd)
{
  struct v4l2_buffer buf;
  
  memset(&buf,0,sizeof(buf));
  
  buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if (ioctl (fd, VIDIOC_STREAMON, &buf.type) < 0)
    perror("Unable to start stream");
}

static void stream_off(int fd)
{
  struct v4l2_buffer buf;
  
  memset(&buf,0,sizeof(buf));
  
  buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if (ioctl (fd, VIDIOC_STREAMOFF, &buf.type) < 0)
    perror("Unable to start stream");
}

static void fb_make_transparent (void)
{
  int fbfd;
  struct stmfbio_var_screeninfo_ex screeninfo;


  if((fbfd  = open("/dev/fb0",   O_RDWR)) < 0)
    perror ("Unable to open frambuffer");

  screeninfo.layerid = 0;
  screeninfo.caps    = STMFBIO_VAR_CAPS_OPACITY;
  screeninfo.opacity = 0;
  ioctl(fbfd, STMFBIO_SET_VAR_SCREENINFO_EX, &screeninfo);

}

int
main (int argc, char *argv[])
{
  int videofd1 = v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR);
  int videofd2 = v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR);

  IDirectFBSurface *SurfaceHandle, *SurfaceHandle2;

  void *ptr, *ptr2;
  int   pitch, pitch2;
  u32   colour_palette[256];
  u32   colour_palette2[256];
  int   step, i;

  if (videofd1 < 0)
    perror ("Couldn't open video device 1\n");

  if (videofd2 < 0)
    perror ("Couldn't open video device 2\n");
  
  v4l2_list_outputs (videofd1);

  fb_make_transparent ();

  init_dfb (&SurfaceHandle);
  init_dfb (&SurfaceHandle2);

  v4l2_set_output_by_index (videofd1, 0);
  v4l2_set_output_by_index (videofd2, 1);
  init_v4l (videofd1, 0, 0, g_width, g_height);
  init_v4l (videofd2, 0, 0, g_width, g_height);


  colour_palette[0] = 0;
  colour_palette2[0] = 0;
  for (i = 1; i < 256; ++i) {
    /* some brown w/ changing alpha */
    colour_palette[i] = (i << 24) | 0xc0b090;
    /* some blue w/ changing alpha */
    colour_palette2[i] = ((255 - i) << 24) | 0x107ce8;
  }

  step = g_width / 255;

  SurfaceHandle->SetColorIndex (SurfaceHandle, 0);
  SurfaceHandle->FillRectangle (SurfaceHandle, 0, 0, g_width, g_height);
  for (i = 1; i < 255; ++i) {
    SurfaceHandle->SetColorIndex (SurfaceHandle, i);
    SurfaceHandle->FillRectangle (SurfaceHandle,
				  i * step, 0, step, g_height * 3 / 4);
  }

  SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0);
  SurfaceHandle2->FillRectangle (SurfaceHandle2, 0, 0, g_width, g_height);
  for (i = 1; i < 255; ++i) {
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, i);
    SurfaceHandle2->FillRectangle (SurfaceHandle2,
				   i * step, g_height / 4, step, g_height * 3 / 4);
  }

  DFBCHECK (SurfaceHandle->Lock (SurfaceHandle, DSLF_READ, &ptr, &pitch));
  DFBCHECK (SurfaceHandle2->Lock (SurfaceHandle2, DSLF_READ, &ptr2, &pitch2));

  queue_buffer (videofd1, ptr, colour_palette, 0);

  stream_on (videofd1);

  fprintf (stderr,"Press Return/Enter to show 2nd buffer\n");
  getchar ();

  queue_buffer (videofd2, ptr2, colour_palette2, 0);

  stream_on (videofd2);

  fprintf (stderr,"Press Return/Enter to quit\n");
  getchar ();

  // This seems to dequeue all buffers too, but that's what the spec says
  // so this is ok.
  stream_off (videofd1);
  stream_off (videofd2);

  close (videofd1);
  close (videofd2);

  DFBCHECK (SurfaceHandle->Unlock (SurfaceHandle));
  DFBCHECK (SurfaceHandle2->Unlock (SurfaceHandle2));

  DFBCHECK (SurfaceHandle->Release (SurfaceHandle));
  DFBCHECK (SurfaceHandle2->Release (SurfaceHandle2));

  deinit_dfb ();

  return 0;
}
