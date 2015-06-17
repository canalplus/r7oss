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

IDirectFB            *DirectFBHandle;
DFBSurfaceDescription SurfaceDescription;

static void init_dfb(IDirectFBSurface     **SurfaceHandle, int clut)
{
  if (!DirectFBHandle) {
    DFBCHECK (DirectFBInit (0, NULL));
    DFBCHECK (DirectFBCreate (&DirectFBHandle));
  }

  SurfaceDescription.width  = 1280;
  SurfaceDescription.height = 720;
  SurfaceDescription.pixelformat = clut ? DSPF_LUT8 : DSPF_ARGB;
  SurfaceDescription.caps   = DSCAPS_VIDEOONLY;
  SurfaceDescription.flags  = (DSDESC_CAPS
			       | DSDESC_PIXELFORMAT
			       | DSDESC_WIDTH
			       | DSDESC_HEIGHT);
  
  DFBCHECK(DirectFBHandle->CreateSurface( DirectFBHandle, &SurfaceDescription, SurfaceHandle ));  
}

static void deinit_dfb()
{
  DirectFBHandle->Release ( DirectFBHandle );  
}


static void init_v4l(int fd,int left,int top,int width,int height,int clut)
{
  struct v4l2_requestbuffers reqbuf;
  struct v4l2_format         fmt;
  struct v4l2_crop           crop;

  memset(&reqbuf,0,sizeof(reqbuf));
  memset(&fmt,0,sizeof(fmt));

  fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width       = width;
  fmt.fmt.pix.height      = height;
  fmt.fmt.pix.pixelformat = clut ? V4L2_PIX_FMT_CLUT8 : V4L2_PIX_FMT_BGR32;
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
  
  //reqbuf.field     = V4L2_FIELD_ANY;  
  //reqbuf.bytesused = 1280*720*4;
}

static void zorder(int fd, int value)
{
  struct v4l2_control cntrl;

  cntrl.id     = V4L2_CID_STM_Z_ORDER;
  cntrl.value  = value;

  if (ioctl(fd,VIDIOC_S_CTRL, &cntrl) == -1)
    perror("Zorder value OUT of range\n");
}

static void queue_buffer(int fd, void* ptr, void *clut,unsigned long long int pts)
{
  struct v4l2_buffer buf;
  
  memset(&buf,0,sizeof(buf));
  
  buf.type         = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  buf.memory       = V4L2_MEMORY_USERPTR;
  buf.m.userptr    = (unsigned long)ptr;
  buf.field        = V4L2_FIELD_NONE;
  buf.length       = 1280*720*4;

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
  
  buf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  if (ioctl (fd, VIDIOC_STREAMON, &buf.type) < 0)
    perror("Unable to start stream");
}

static void stream_off(int fd)
{
  struct v4l2_buffer buf;
  
  memset(&buf,0,sizeof(buf));
  
  buf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;

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

int main (int argc,char *argv[])
{
  int videofd1 = v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR);
  int videofd2 = v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR);
  IDirectFBSurface     *SurfaceHandle;
  IDirectFBSurface     *SurfaceHandle2;
  void *ptr,*ptr2;
  int pitch,pitch2;
  int colour_palette[256];
  int colour_palette2_real[256]; /* random... */
  int *colour_palette2 = colour_palette2_real;
  int is_lut8;

  is_lut8 = (argc > 1 && !strcmp (argv[1], "both"));

  memset(&colour_palette,0x0,4*256);
  //memset(&colour_palette[1],0xff,255*4);

  colour_palette[0] = 0xff000000; // black
  colour_palette[1] = 0xffff0000; // red
  colour_palette[2] = 0xff00ff00; // green
  colour_palette[3] = 0xff0000ff; // blue
  colour_palette[4] = 0xffffffff; // white
  colour_palette[5] = 0x80808000; // half-transp yellow
  colour_palette[6] = 0x00000000; // transp black

  if (videofd1 < 0)
    perror("Couldn't open video device 1\n");

  if (videofd2 < 0)
    perror("Couldn't open video device 2\n");
  
  v4l2_list_outputs (videofd1);

  fb_make_transparent();

  init_dfb(&SurfaceHandle,is_lut8);
  init_dfb(&SurfaceHandle2,1);

  v4l2_set_output_by_index (videofd1, 0);
  v4l2_set_output_by_index (videofd2, 1);
  init_v4l(videofd1,0,0,1280,720,is_lut8);
  init_v4l(videofd2,0,0,1280,720,1);

  printf("%s:%d\n",__FUNCTION__,__LINE__);


//  memcpy (colour_palette, colour_palette2, sizeof (colour_palette));
  colour_palette2 = colour_palette;

  {
    int coords = 60;
    int size   = 100;

    // clear
    if (!is_lut8) SurfaceHandle->Clear (SurfaceHandle, 0x00, 0x00, 0x00, 0x00);
    else {
      SurfaceHandle->SetColorIndex (SurfaceHandle, 0x6);
      SurfaceHandle->FillRectangle (SurfaceHandle, 0, 0, 600, 600);
    }
    // White
    if (is_lut8) SurfaceHandle->SetColorIndex (SurfaceHandle, 0x4);
    else SurfaceHandle->SetColor (SurfaceHandle, 0xff, 0xff, 0xff, 0xff);
    SurfaceHandle->FillRectangle (SurfaceHandle, coords, coords, size, size);
    coords += size;
    // Red
    if (is_lut8) SurfaceHandle->SetColorIndex (SurfaceHandle, 0x1);
    else SurfaceHandle->SetColor (SurfaceHandle, 0xff, 0x00, 0x00, 0xff);
    SurfaceHandle->FillRectangle (SurfaceHandle, coords, coords, size, size);
    coords += size;
    // Green
    if (is_lut8) SurfaceHandle->SetColorIndex (SurfaceHandle, 0x2);
    else SurfaceHandle->SetColor (SurfaceHandle, 0x00, 0xff, 0x00, 0xff);
    SurfaceHandle->FillRectangle (SurfaceHandle, coords, coords, size, size);
    coords += size;
    // Blue
    if (is_lut8) SurfaceHandle->SetColorIndex (SurfaceHandle, 0x3);
    else SurfaceHandle->SetColor (SurfaceHandle, 0x00, 0x00, 0xff, 0xff);
    SurfaceHandle->FillRectangle (SurfaceHandle, coords, coords, size, size);
    coords += size;
    // half transp yellow
    if (is_lut8) SurfaceHandle->SetColorIndex (SurfaceHandle, 0x5);
    else SurfaceHandle->SetColor (SurfaceHandle, 0x80, 0x80, 0x00, 0x80);
    SurfaceHandle->FillRectangle (SurfaceHandle, coords, coords, size, size);
    coords += size;
    // transp
    if (is_lut8) SurfaceHandle->SetColorIndex (SurfaceHandle, 0x6);
    else SurfaceHandle->SetColor (SurfaceHandle, 0x00, 0x00, 0x00, 0x00);
    SurfaceHandle->FillRectangle (SurfaceHandle, coords, coords, size, size);
    coords += size;
  }

  {
    int xcoords = 60 + (100 * 6), ycoords = 60;
    int size   = 100;

    // clear
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x6);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, 220, 220, 440, 440);
    // transp
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x6);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, xcoords, ycoords, size, size);
    xcoords -= size; ycoords += size;
    // half transp yellow
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x5);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, xcoords, ycoords, size, size);
    xcoords -= size; ycoords += size;
    // Blue
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x3);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, xcoords, ycoords, size, size);
    xcoords -= size; ycoords += size;
    // Green
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x2);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, xcoords, ycoords, size, size);
    xcoords -= size; ycoords += size;
    // Red
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x1);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, xcoords, ycoords, size, size);
    xcoords -= size; ycoords += size;
    // White
    SurfaceHandle2->SetColorIndex (SurfaceHandle2, 0x4);
    SurfaceHandle2->FillRectangle (SurfaceHandle2, xcoords, ycoords, size, size);
    xcoords -= size; ycoords += size;
  }
    
  DFBCHECK (SurfaceHandle->Lock (SurfaceHandle, DSLF_READ, &ptr, &pitch));
  DFBCHECK (SurfaceHandle2->Lock (SurfaceHandle2, DSLF_READ, &ptr2, &pitch2));

  printf("%s:%d\n",__FUNCTION__,__LINE__);

  zorder(videofd2,1);
  zorder(videofd1,2);

  queue_buffer( videofd1, ptr, (is_lut8 ? colour_palette : 0), 0ULL);

  stream_on( videofd1 );

  getchar();
  queue_buffer( videofd2, ptr2, colour_palette2, 0ULL); 

  printf("%s:%d\n",__FUNCTION__,__LINE__);

  stream_on( videofd2 );

  printf("%s:%d\n",__FUNCTION__,__LINE__);


  getchar();

  zorder(videofd2,2);
  zorder(videofd1,1);

  fprintf(stderr,"Press Return/Enter to quit\n");
  getchar();

  // This seems to dequeue all buffers too, but that's what the spec says
  // so this is ok.
  stream_off( videofd1 );
  stream_off( videofd2 );

  deinit_dfb();
  close(videofd1);  

  return 0;
}
