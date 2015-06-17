/***********************************************************************
 *
 * File: linux/tests/v4l2gam/v4l2gam.c
 * Copyright (c) 2005-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

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

#include "linux/include/linux/dvb/dvb_v4l2_export.h"
#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

#define unlikely(x)     __builtin_expect(!!(x),0)
#define likely(x)       __builtin_expect(!!(x),1)
#define abs(x)  ({				\
		  long long __x = (x);		\
		  (__x < 0) ? -__x : __x;	\
		})

#define printf_standard(std)\
          fprintf(stderr,"\t                 %#.16llx - %s\n", std, #std)

#ifndef __cplusplus
#ifndef bool
#define bool    unsigned char
#define false   0
#define true    1
#endif
#endif

typedef struct {
	int x;
	int y;
	int width;
	int height;
} video_window_t;
/*
 * This is a simple program to test V4L2 buffer queueing and presentation time from a gam file.
 *
 * The code is deliberately written in a linear, straight forward, way to show
 * the step by step usage of V4L2 to display video frames.
 */

typedef struct GamPictureHeader_s {
	unsigned short HeaderSize;
	unsigned short Signature;
	unsigned short Type;
	unsigned short Properties;
	unsigned int PictureWidth;	/* With 4:2:0 R2B this is a OR between Pitch and Width */
	unsigned int PictureHeight;
	unsigned int LumaSize;
	unsigned int ChromaSize;
} GamPictureHeader_t;

enum gamfile_type {
	GAMFILE_UNKNOWN = 0x0000,
	GAMFILE_RGB565 = 0x0080,
	GAMFILE_RGB888 = 0x0081,
	GAMFILE_xRGB8888 = 0x0082,
	GAMFILE_ARGB8565 = 0x0084,
	GAMFILE_ARGB8888 = 0x0085,
	GAMFILE_ARGB1555 = 0x0086,
	GAMFILE_ARGB4444 = 0x0087,
	GAMFILE_CLUT1 = 0x0088,
	GAMFILE_CLUT2 = 0x0089,
	GAMFILE_CLUT4 = 0x008a,
	GAMFILE_CLUT8 = 0x008b,
	GAMFILE_ACLUT44 = 0x008c,
	GAMFILE_ACLUT88 = 0x008d,
	GAMFILE_CLUT8_4444 = 0x008e,
	GAMFILE_YCBCR888 = 0x0090,
	GAMFILE_YCBCR101010 = 0x0091,
	GAMFILE_YCBCR422R = 0x0092,
	GAMFILE_YCBCR420MB = 0x0094,
	GAMFILE_YCBCR422MB = 0x0095,
	GAMFILE_AYCBCR8888 = 0x0096,
	GAMFILE_A1 = 0x0098,
	GAMFILE_A8 = 0x0099,
	GAMFILE_I420 = 0x009e,
	GAMFILE_YCBCR420R2B = 0x00b0,
	GAMFILE_YCBCR422R2B = 0x00b1,
	GAMFILE_XY = 0x00c8,
	GAMFILE_XYL = 0x00c9,
	GAMFILE_XYC = 0x00cA,
	GAMFILE_XYLC = 0x00cb,
	GAMFILE_RGB101010 = 0x0101,
};

static void usage_begin_decode(void)
{
  fprintf(stderr,"Usage: v4l2gam file_name [options]\n");
  fprintf(stderr,"\ta         - Adjust time to minimise presentation delay\n");
  fprintf(stderr,"\tb num     - Number of V4L buffers to use (default is 5. Value below 2 blocks display)\n");
  fprintf(stderr,"\td num     - Presentation delay on first buffer by num seconds, default is 1\n");
  fprintf(stderr,"\ti num     - Do num interations, default is 200\n");
  fprintf(stderr,"\tI         - Force Interlaced buffers\n");
  fprintf(stderr,"\th         - Scale the buffer down to half size\n");
  fprintf(stderr,"\th1          - Scale the buffer down to half horizontal size and put it in the left area\n");
  fprintf(stderr,"\th2          - Scale the buffer down to half horizontal size and put it in the right area\n");
  fprintf(stderr,"\tq         - Scale the buffer down to quarter size\n");
  fprintf(stderr,"\tv num     - Open V4L2 device /dev/videoNUM, default is 0\n");
  fprintf(stderr,"\tw name    - select plane by name on output, default is first plane\n");
  fprintf(stderr,"\tt std     - select V4L2_STD_... video standard\n");
  fprintf(stderr,"\t            here's a selection of some possible standards:\n");
    printf_standard(V4L2_STD_VGA_60);
    printf_standard(V4L2_STD_VGA_59_94);
    printf_standard(V4L2_STD_480P_60);
    printf_standard(V4L2_STD_480P_59_94);
    printf_standard(V4L2_STD_576P_50);
    printf_standard(V4L2_STD_1080P_60);
    printf_standard(V4L2_STD_1080P_59_94);
    printf_standard(V4L2_STD_1080P_50);
    printf_standard(V4L2_STD_1080P_23_98);
    printf_standard(V4L2_STD_1080P_24);
    printf_standard(V4L2_STD_1080P_25);
    printf_standard(V4L2_STD_1080P_29_97);
    printf_standard(V4L2_STD_1080P_30);
    printf_standard(V4L2_STD_1080I_60);
    printf_standard(V4L2_STD_1080I_59_94);
    printf_standard(V4L2_STD_1080I_50);
    printf_standard(V4L2_STD_720P_60);
    printf_standard(V4L2_STD_720P_59_94);
    printf_standard(V4L2_STD_720P_50);
  fprintf(stderr,"\tx num     - Crop the source image at starting at the given pixel number\n");
  fprintf(stderr,"\ty num     - Crop the source image at starting at the given line number\n");
  fprintf(stderr,"\tz         - Do a dynamic horizontal zoom out\n");
  fprintf(stderr,"\te         - vErbose\n");
}

static void usage_during_playback(void)
{
  fprintf(stderr,"\n");
  fprintf(stderr,"The following commands can be issued during playback by pressing the\n");
  fprintf(stderr,"(b)racketed key and pressing return:\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"  Plane (a)lpha mode       Set plane alpha.\n");
  fprintf(stderr,"  Asp(e)ctRatioConvMode    Change aspect ratio conversion mode:\n");
  fprintf(stderr,"                               0=letter box\n");
  fprintf(stderr,"                               1=pan&scan\n");
  fprintf(stderr,"                               2=combined\n");
  fprintf(stderr,"                               3=ignore\n");
  fprintf(stderr,"  OutputDisplay            Change output display aspect ratio:\n");
  fprintf(stderr,"    Asp(E)ctRatio              0=16/9\n");
  fprintf(stderr,"                               1=4/3\n");
  fprintf(stderr,"                               2=14/9\n");
  fprintf(stderr,"  (h)elp                   Display this help text.\n");
  fprintf(stderr,"  (j)window mode           Set output in auto(1)/manual(0) mode or switch if no parameter provided.\n");
  fprintf(stderr,"  (J)window mode           Set input in auto(1)/manual(0) mode or switch if no parameter provided.\n");
  fprintf(stderr,"  (w)output window value   Resize output display window.\n");
  fprintf(stderr,"  (W)input window value    Resize input window crop.\n");
  fprintf(stderr,"  e(x)it                   Exit dvbplay.\n");
}

static void usage(void)
{
  usage_begin_decode();
  usage_during_playback();
  exit(1);
}

static FILE *read_gam_header(char *file_name, int *pixfmt_p,
			     GamPictureHeader_t * GamPictureHeader_p,
			     unsigned int *pitch_p,
			     unsigned int *frameBufferSize_p,
			     enum v4l2_colorspace *cspace_p)
{
	FILE *fp;
	int readBytes;
	unsigned int fileSize;

	fp = fopen(file_name, "rb");

	if (fp == NULL) {
		printf("Error! File not found!\n");
		return 0;
	}

	/* Obtain file size */
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Read Gam header */
	readBytes =
	    fread(GamPictureHeader_p, 1, sizeof(GamPictureHeader_t), fp);
	if (readBytes != sizeof(GamPictureHeader_t)) {
		printf("Error while reading the GAM Header!\n");
		return 0;
	}

	/* To avoid memory crash, we should check data consistency:
	   For MB pictures, we should have "fileSize = header size + Luma size + Chroma size"
	   For Raster Progressive pictures, it seems to be more complex... We can find GAM Raster pictures with the following rules:
	   "fileSize = header size + 2 * Luma size" (Chroma size = 0)
	   "fileSize = header size + Luma size + Chroma size"
	 */
	if ((fileSize !=
	     (GamPictureHeader_p->LumaSize + GamPictureHeader_p->ChromaSize +
	      sizeof(GamPictureHeader_t)))
	    && (fileSize !=
		(2 * GamPictureHeader_p->LumaSize +
		 sizeof(GamPictureHeader_t))))
		if (readBytes != sizeof(GamPictureHeader_t)) {
			printf("Invalid GAM Header!\n");
			return 0;
		}

	*frameBufferSize_p = fileSize - sizeof(GamPictureHeader_t);

	switch (GamPictureHeader_p->Type) {
	case GAMFILE_ARGB8888:
		/* 0x85 = ARGB 8888 */
		*pixfmt_p = V4L2_PIX_FMT_BGR32;
		*pitch_p = 4 * GamPictureHeader_p->PictureWidth;
		*cspace_p = V4L2_COLORSPACE_SRGB;
		printf("Pixel format is ARGB 8888\n");
		break;
	case GAMFILE_RGB888:
		/* 0x85 = RGB 888 */
		*pixfmt_p = V4L2_PIX_FMT_BGR24;
		*pitch_p = 3 * GamPictureHeader_p->PictureWidth;
		*cspace_p = V4L2_COLORSPACE_SRGB;
		printf("Pixel format is RGB 888\n");
		break;
	case GAMFILE_ARGB4444:
		/* 0x87 = ARGB 4444 */
		*pixfmt_p = V4L2_PIX_FMT_BGRA4444;
		*pitch_p = 2 * GamPictureHeader_p->PictureWidth;
		*cspace_p = V4L2_COLORSPACE_SRGB;
		printf("Pixel format is ARGB 4444\n");
		break;
	case GAMFILE_RGB565:
		/* 0x80 = RGB 565 */
		*pixfmt_p = V4L2_PIX_FMT_RGB565;
		*pitch_p = 2 * GamPictureHeader_p->PictureWidth;
		*cspace_p = V4L2_COLORSPACE_SRGB;
		printf("Pixel format is RGB 565\n");
		break;
	case GAMFILE_ARGB8565:
		*pixfmt_p = V4L2_PIX_FMT_ARGB8565;
		*pitch_p = 3 * GamPictureHeader_p->PictureWidth;
		*cspace_p = V4L2_COLORSPACE_SRGB;
		printf("Pixel format is ARGB 8565\n");
		break;
	case GAMFILE_AYCBCR8888:
		*pixfmt_p = V4L2_PIX_FMT_YUV32;
		*pitch_p = 4 * GamPictureHeader_p->PictureWidth;
		*cspace_p = V4L2_COLORSPACE_REC709;
		printf("Pixel format is AYCbCr 8888\n");
		break;
	case GAMFILE_YCBCR422R:
		/* 0x92 = YcbCr 4:2:2 Raster */
		/* ColorFmt = SURF_YCBCR422R; */
		*pixfmt_p = V4L2_PIX_FMT_UYVY;
		*pitch_p = 2 * GamPictureHeader_p->PictureWidth;
		if (GamPictureHeader_p->ChromaSize == 0)
			GamPictureHeader_p->LumaSize *= 2;	/* We use only luma as chroma is interleaved so update the size accordingly */
		else {
			GamPictureHeader_p->LumaSize +=
			    GamPictureHeader_p->ChromaSize;
			GamPictureHeader_p->ChromaSize = 0;
		}
		printf("Pixel format is YCbCr 4:2:2 Raster single buffer\n");
		break;

	case GAMFILE_YCBCR420R2B:
		/* 0xB0 = YcbCr 4:2:0 Raster Double Buffer */
		/* ColorFmt = SURF_YCbCr420R2B; */
		*pixfmt_p = V4L2_PIX_FMT_NV12;
		/* With 4:2:0 R2B, PictureWidth is a OR between Pitch and Width (each one on 16 bits) */
		*pitch_p = GamPictureHeader_p->PictureWidth >> 16;
		GamPictureHeader_p->PictureWidth =
		    GamPictureHeader_p->PictureWidth & 0xFFFF;
		if (*pitch_p == 0)
			*pitch_p = GamPictureHeader_p->PictureWidth;
		/* Patch the chroma size to the real value */
		GamPictureHeader_p->ChromaSize =
		    GamPictureHeader_p->LumaSize / 2;
		printf("Pixel format is 4:2:0 Raster double Buffer\n");
		break;

	case GAMFILE_YCBCR420MB:
		/* 0x94 = OMEGA2 4:2:0 (= Macroblock) */
		/* ColorFmt = SURF_YCBCR420MB; */
		*pixfmt_p = V4L2_PIX_FMT_STM420MB;
		*pitch_p = ((GamPictureHeader_p->PictureWidth + 15) / 16) * 16;
		printf("Pixel format is 4:2:0 Macroblock\n");
		break;

	case GAMFILE_YCBCR422MB:
		/* 0x95 = OMEGA2 4:2:2 (= Macroblock) */
		/* ColorFmt = SURF_YCBCR422MB; */
		*pixfmt_p = V4L2_PIX_FMT_STM422MB;
		*pitch_p = ((GamPictureHeader_p->PictureWidth + 15) / 16) * 16;
		printf("Pixel format is 4:2:2 Macroblock\n");
		break;

	case GAMFILE_YCBCR422R2B:
		/* 0xB1 = YcbCr 4:2:2 Raster Double Buffer */
		/* ColorFmt = SURF_YCbCr422R2B; */
		*pixfmt_p = V4L2_PIX_FMT_NV16;
		/* With 4:2:2 R2B, PictureWidth is a OR between Pitch and Width (each one on 16 bits) */
		*pitch_p = GamPictureHeader_p->PictureWidth >> 16;
		GamPictureHeader_p->PictureWidth =
		    GamPictureHeader_p->PictureWidth & 0xFFFF;
		if (*pitch_p == 0)
			*pitch_p = GamPictureHeader_p->PictureWidth;
		/* Patch the chroma size to the real value */
		GamPictureHeader_p->ChromaSize = GamPictureHeader_p->LumaSize;
		printf("Pixel format is 4:2:2 Raster double Buffer\n");
		break;

	default:
		printf("Gam format not supported!\n");
		return 0;
	}

	printf("Gam width: %u\n", GamPictureHeader_p->PictureWidth);
	printf("Gam height: %u\n", GamPictureHeader_p->PictureHeight);
	printf("Gam pitch: %u\n", *pitch_p);

	printf("Gam Luma size: %u\n", GamPictureHeader_p->LumaSize);
	printf("Gam Chroma size: %u\n", GamPictureHeader_p->ChromaSize);
	printf("Gam Frame buffer size in file: %u\n", *frameBufferSize_p);

	if (GamPictureHeader_p->LumaSize + GamPictureHeader_p->ChromaSize >
	    *frameBufferSize_p) {
		printf
		    ("Gam Frame buffer luma+chroma size: %u exceeding file size.\n",
		     GamPictureHeader_p->LumaSize +
		     GamPictureHeader_p->ChromaSize);
		GamPictureHeader_p->ChromaSize =
		    *frameBufferSize_p - GamPictureHeader_p->LumaSize;
		printf("Limiting chroma size to %u\n",
		       GamPictureHeader_p->ChromaSize);

	}
	return fp;
}

static int read_gam_buffer(FILE * fp, char *buffer,
			   GamPictureHeader_t GamPictureHeader,
			   unsigned int srcWidth, struct v4l2_pix_format pix)
{
	unsigned int dstWidth = pix.bytesperline;

	char *lumaBufferAddr = buffer;
	char *chromaBufferAddr = buffer + pix.priv;

	unsigned int lumaBufferSize =
	    (pix.priv == 0 ? pix.sizeimage : pix.priv);
	unsigned int chromaBufferSize = pix.sizeimage - lumaBufferSize;

	unsigned readBytes, copiedBytes = 0;

	if (fp == NULL) {
		printf("Error! Invalid file !\n");
		return -1;
	}

	/* initialize buffer to zero */
	memset(lumaBufferAddr, 0x00, pix.sizeimage);

	switch (GamPictureHeader.Type) {
	case GAMFILE_AYCBCR8888:
		{
			// We need to invert Y and Cb components (first 2 bytes of each pixel)
			printf
			    ("Reading file content and correcting components positions...\n");
			unsigned int currentDest = 0;
			char reads[4];
			char tmp = 0;
			do {
				readBytes = fread(&reads, 1, 4, fp);

				if (readBytes != 0) {
					tmp = reads[0];
					reads[0] = reads[1];
					reads[1] = tmp;
					memcpy(lumaBufferAddr + currentDest,
					       &reads, 4 * sizeof(char));
					currentDest += 4;
				}
			} while ((readBytes != 0)
				 && currentDest < lumaBufferSize);
			break;
		}
	case GAMFILE_ARGB8565:
	case GAMFILE_RGB888:
	case GAMFILE_YCBCR888:
		{
			// The file format is 24bits aligned on 32, we have to ignore 1 byte out of 4
			printf("Reading file content and realigning...\n");
			unsigned int currentDest = 0;
			char reads[4];
			do {
				readBytes = fread(&reads, 1, 4, fp);

				if (readBytes != 0) {
					memcpy(lumaBufferAddr + currentDest,
					       &reads, 3 * sizeof(char));
					currentDest += 3;
				}
			} while ((readBytes != 0)
				 && currentDest < lumaBufferSize);
			break;
		}
	default:
		if ((dstWidth == srcWidth
		     || (GamPictureHeader.Type >= GAMFILE_RGB565
			 && GamPictureHeader.Type <= GAMFILE_YCBCR888)
		     || GamPictureHeader.Type == GAMFILE_AYCBCR8888)) {
			/* luma and chroma are read to buffer directly */
			printf("Reading whole file into the buffer...\n");
			fread(lumaBufferAddr, 1, lumaBufferSize, fp);
			/* handle gam files which LumaSize doesn't equal to lumaBufferSize */
			fseek(fp,
			      (sizeof(GamPictureHeader_t) +
			       GamPictureHeader.LumaSize), SEEK_SET);
			fread(chromaBufferAddr, 1, chromaBufferSize, fp);
		} else {
			/* read luma data to buffere line by line */
			do {
				readBytes =
				    fread(lumaBufferAddr, 1, srcWidth, fp);
				copiedBytes += dstWidth;
				lumaBufferAddr += dstWidth;

			} while ((readBytes != 0)
				 && (copiedBytes < lumaBufferSize));

			printf("Luma copiedBytes = %d\n", copiedBytes);

			if (GamPictureHeader.ChromaSize) {
				copiedBytes = 0;
				/* seek the read pointer of the beginning of the chroma data */
				fseek(fp,
				      (sizeof(GamPictureHeader_t) +
				       GamPictureHeader.LumaSize), SEEK_SET);

				/* read chroma data to buffer line by line */
				do {
					readBytes =
					    fread(chromaBufferAddr, 1, srcWidth,
						  fp);
					copiedBytes += dstWidth;
					chromaBufferAddr += dstWidth;
				} while ((readBytes != 0)
					 && (copiedBytes < chromaBufferSize));
				printf("Chroma copiedBytes = %d\n",
				       copiedBytes);
			}
		}
	}
	printf("Read completed\n");

	fclose(fp);

	return 0;
}

static unsigned int Parameters[16];
static unsigned int ParameterCount;

static char GetCommand(void)
{
	char Key;
	char line[40];

	ParameterCount = 0;

	if (fgets(line, sizeof(line), stdin) != NULL) {
		ParameterCount =
		    sscanf(line, "%c %d %d %d %d\n", &Key, &Parameters[0],
			   &Parameters[1], &Parameters[2], &Parameters[3]);
		ParameterCount--;
		return Key;
	}

	return 'x';

}

static bool CheckKey(int v4lfd)
{
	bool Running = true;
	char Key;
	static unsigned int AspectRatioConversionMode = 0;
	static enum _V4L2_CID_STM_OUTPUT_DISPLAY_ASPECT_RATIO
	    OutputDisplayAspectRatio = VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9;
	struct v4l2_control Control;
	struct v4l2_crop crop;

	ParameterCount = 0;
	Key = GetCommand();
	switch (Key) {
        case 'a':
            if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_OUTPUT_ALPHA, &Parameters[0])) < 0) {
                perror("VIDIOC_S_CTRL VIDIOC_S_OUTPUT_ALPHA failed\n");
                return(false);
            }
            break;
	case 'e':
		// Switch to next incremental mode
		AspectRatioConversionMode++;
		if (AspectRatioConversionMode > 3)
			AspectRatioConversionMode = 0;
		// If Parameter given apply the Parameter mode
		if (ParameterCount != 0) {
			AspectRatioConversionMode = Parameters[0];
			ParameterCount = 0;	// Disable Parameter mode for next time
		}

		Control.id = V4L2_CID_STM_ASPECT_RATIO_CONV_MODE;
		Control.value = AspectRatioConversionMode;
		printf("VIDIOC_S_CTRL V4L2_CID_STM_ASPECT_RATIO_CONV_MODE=%d\n",
		       AspectRatioConversionMode);
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CTRL, &Control)) < 0) {
			perror
			    ("VIDIOC_S_CTRL V4L2_CID_STM_ASPECT_RATIO_CONV_MODE failed\n");
			return (false);
		}
		Control.id = V4L2_CID_STM_ASPECT_RATIO_CONV_MODE;
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_G_CTRL, &Control)) < 0) {
			perror
			    ("VIDIOC_G_CTRL V4L2_CID_STM_ASPECT_RATIO_CONV_MODE failed\n");
			return (false);
		}
		if (AspectRatioConversionMode != Control.value) {
			perror
			    ("Plane aspect ratio conversion mode set and get are different.\n");
		}
		break;
	case 'E':
		// Switch to next output aspect ratio value.
		OutputDisplayAspectRatio++;
		if (OutputDisplayAspectRatio <
		    VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9
		    || OutputDisplayAspectRatio >
		    VCSOUTPUT_DISPLAY_ASPECT_RATIO_14_9)
			OutputDisplayAspectRatio =
			    VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9;
		// If Parameter given apply the Parameter mode
		if (ParameterCount != 0) {
			OutputDisplayAspectRatio = Parameters[0];
			ParameterCount = 0;	// Disable Parameter mode for next time
		}

		Control.id = V4L2_CID_DV_STM_TX_ASPECT_RATIO;
		Control.value = OutputDisplayAspectRatio;
		printf("VIDIOC_S_CTRL V4L2_CID_DV_STM_TX_ASPECT_RATIO=%d\n",
		       OutputDisplayAspectRatio);
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CTRL, &Control)) < 0) {
			perror
			    ("VIDIOC_S_CTRL V4L2_CID_DV_STM_TX_ASPECT_RATIO failed\n");
			return (false);
		}

		Control.id = V4L2_CID_DV_STM_TX_ASPECT_RATIO;
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_G_CTRL, &Control)) < 0) {
			perror
			    ("VIDIOC_G_CTRL V4L2_CID_DV_STM_TX_ASPECT_RATIO failed\n");
			return (false);
		}

		if (OutputDisplayAspectRatio != Control.value) {
			perror
			    ("Output display aspect ratio set and get are different.\n");
		}
		break;
	case 'f':
		Control.id = V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE;
		Control.value = Parameters[0];
		printf("V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE=%d\n",
		       Parameters[0]);

		if (LTV_CHECK (ioctl(v4lfd, VIDIOC_S_CTRL, &Control)) < 0) {
			perror
			    ("V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE failed\n");
			return (false);
		}
		break;
	case 'F':
		Control.id = V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE;
		Control.value = Parameters[0];
		printf("V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE=%d\n",
		       Parameters[0]);

		if (LTV_CHECK (ioctl(v4lfd, VIDIOC_S_CTRL, &Control)) < 0) {
			perror
			    ("V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE failed\n");
			return (false);
		}
		break;
	case 'j':
		Control.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
		if (ParameterCount == 0) {
			if (LTV_CHECK(ioctl(v4lfd, VIDIOC_G_CTRL, &Control)) <
			    0) {
				perror
				    ("VIDIOC_G_CTRL V4L2_CID_STM_OUTPUT_WINDOW_MODE failed\n");
				return (false);
			}

			/* Switch mode */
			Control.value =
			    ((Control.value ==
			      VCSPLANE_AUTO_MODE) ? VCSPLANE_MANUAL_MODE :
			     VCSPLANE_AUTO_MODE);
		} else if (ParameterCount == 1) {
			Control.value = Parameters[0];
		}

		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CTRL, &Control)) < 0) {
			perror
			    ("VIDIOC_S_CTRL V4L2_CID_STM_OUTPUT_WINDOW_MODE failed\n");
			return (false);
		}

		if (Control.value == VCSPLANE_AUTO_MODE) {
			printf("Setting output window mode to AUTOMATIC\n");
		} else {
			printf("Setting output window mode to MANUAL\n");
		}

		break;

	case 'J':
		Control.id = V4L2_CID_STM_INPUT_WINDOW_MODE;
		if (ParameterCount == 0) {
			if (LTV_CHECK(ioctl(v4lfd, VIDIOC_G_CTRL, &Control)) <
			    0) {
				perror
				    ("VIDIOC_G_CTRL V4L2_CID_STM_INPUT_WINDOW_MODE failed\n");
				return (false);
			}

			/* Switch mode */
			Control.value =
			    ((Control.value ==
			      VCSPLANE_AUTO_MODE) ? VCSPLANE_MANUAL_MODE :
			     VCSPLANE_AUTO_MODE);
		} else if (ParameterCount == 1) {
			Control.value = Parameters[0];
		}

		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CTRL, &Control)) < 0) {
			perror
			    ("VIDIOC_S_CTRL V4L2_CID_STM_INPUT_WINDOW_MODE failed\n");
			return (false);
		}

		if (Control.value == VCSPLANE_AUTO_MODE) {
			printf("Setting input window mode to AUTOMATIC\n");
		} else {
			printf("Setting input window mode to MANUAL\n");
		}

		break;
	case 'h':
		usage_during_playback();
		break;
	case 'i':
		//Status  = player_flip_display (Player);
		break;
	case 'w':
		if (ParameterCount == 4) {
			memset(&crop, 0, sizeof(struct v4l2_crop));
			crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			crop.c.left = Parameters[0];
			crop.c.top = Parameters[1];
			crop.c.width = Parameters[2];
			crop.c.height = Parameters[3];
			printf("Output Image Dimensions (%d,%d) (%d x %d)\n",
			       crop.c.left, crop.c.top, crop.c.width,
			       crop.c.height);

			/* set output crop */
			if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CROP, &crop)) < 0) {
				perror("VIDIOC_S_CROP failed\n");
			}
		}
		break;
	case 'W':
		if (ParameterCount == 4) {
			memset(&crop, 0, sizeof(struct v4l2_crop));
			crop.type = V4L2_BUF_TYPE_PRIVATE;
			crop.c.left = Parameters[0];
			crop.c.top = Parameters[1];
			crop.c.width = Parameters[2];
			crop.c.height = Parameters[3];

			printf("Source Image Dimensions (%d,%d) (%d x %d)\n",
			       crop.c.left, crop.c.top, crop.c.width,
			       crop.c.height);

			/* set source crop */
			if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CROP, &crop)) < 0) {
				perror("VIDIOC_S_CROP failed\n");
				return false;
			}
		}
		break;
	case 'x':
		printf("Exiting from v4l2gam\n");
		Running = false;
		break;
	}

	return Running;
}

int main(int argc, char **argv)
{
	bool Running = true;
	int verbose;
	int v4lfd;
	int viddevice;
	char vidname[32];
	const char *plane_name = "Main-VID";
	int progressive;
	int iterations;
	int delay;
	int pixfmt;
	int halfsize;
	int left_half, right_half;
	int quartersize;
	int adjusttime;
	int bufferNb;
	int i;
	unsigned int image_width;
	unsigned int image_height;
	video_window_t Window;
	int zoom;

	unsigned long long frameduration;

	struct v4l2_format fmt;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_crop crop;
	v4l2_std_id stdid = V4L2_STD_UNKNOWN;
	struct v4l2_standard standard;
	struct v4l2_cropcap cropcap;
	struct v4l2_buffer buffer[20];
	char *bufferdata[20];

	struct timespec currenttime;
	struct timeval ptime;

	char gamFileName[250];
	FILE *fp;
	unsigned int frameBufferSize;
	unsigned int pitch;
	GamPictureHeader_t GamPictureHeader;

	enum v4l2_colorspace cspace = V4L2_COLORSPACE_SRGB;

	argc--;
	argv++;

	verbose = 0;
	adjusttime = 0;
	viddevice = 1;		/* Default stmvout device is now video1 */
	pixfmt = V4L2_PIX_FMT_UYVY;
	halfsize = 0;
	left_half = right_half = 0;
	quartersize = 0;
	delay = 1;
	iterations = 20;	/* Default value to get ~1s of display (20 iteration * 3 buffers * 1/60s in ntsc) */
	bufferNb = 3;
	Window.x = 0;
	Window.y = 0;
	zoom = 0;
	image_width = 0;
	image_height = 0;
	progressive = 1;

	/* The next argument contains the name of the GAM file to load */
	if (argv[0] != NULL) {
		strncpy(gamFileName, argv[0], sizeof(gamFileName));
		argc--;
		argv++;
	}

	while (argc > 0) {
		switch (**argv) {
		case 'e':
			verbose = 1;
			break;
		case 'a':
			adjusttime = 1;
			break;
		case 'd':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing delay\n");
				usage();
			}

			delay = atoi(*argv);
			if (delay < 1) {
				fprintf(stderr,
					"Require a minimum of 1sec delay\n");
				usage();
			}

			break;
		case 'i':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing iterations\n");
				usage();
			}

			iterations = atoi(*argv);
			if (iterations < 5) {
				fprintf(stderr,
					"Require a minimum of 5 iterations\n");
				usage();
			}

			break;
		case 'I':
			progressive = 0;
			break;
		case 'h':
			if ((*argv)[1] == '1')
				left_half = 1;
			else if ((*argv)[1] == '2')
				right_half = 1;
			halfsize = 1;
			break;
		case 'q':
			quartersize = 1;
			break;
		case 'v':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr,
					"Missing video device number\n");
				usage();
			}

			viddevice = atoi(*argv);
			if (viddevice < 0 || viddevice > 255) {
				fprintf(stderr,
					"Video device number out of range\n");
				usage();
			}

			break;
		case 'w':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing plane name\n");
				usage();
			}

			plane_name = *argv;

			break;
		case 't':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing standard id\n");
				usage();
			}

			stdid = (v4l2_std_id) strtoull(*argv, NULL, 0);
			break;
		case 'b':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing number of buffers\n");
				usage();
			}

			bufferNb = atoi(*argv);
			if (bufferNb < 0 || bufferNb > 20) {
				fprintf(stderr,
					"Number of buffers out of range\n");
				usage();
			}
			break;
		case 'x':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing pixel number\n");
				usage();
			}

			Window.x = atoi(*argv);
			break;
		case 'y':
			argc--;
			argv++;

			if (argc <= 0) {
				fprintf(stderr, "Missing line number\n");
				usage();
			}

			Window.y = atoi(*argv);
			break;
		case 'z':
			zoom = 1;
			break;
		default:
			fprintf(stderr, "Unknown option '%s'\n", *argv);
			usage();
		}

		argc--;
		argv++;
	}

	/*
	 * Open the requested V4L2 device
	 */
	snprintf(vidname, sizeof(vidname), "/dev/video%d", viddevice);
	if ((v4lfd = open(vidname, O_RDWR)) < 0) {
		perror("Unable to open video device");
		goto exit;
	}

	v4l2_list_outputs(v4lfd);
	if (!plane_name) {
		i = v4l2_set_output_by_index(v4lfd, 0);
	} else
		i = v4l2_set_output_by_name(v4lfd, plane_name);

	if (i == -1) {
		perror("Unable to set video output");
		goto exit;
	}

	if (stdid == V4L2_STD_UNKNOWN) {
		/* if no standard was requested... */
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_G_OUTPUT_STD, &stdid)) < 0)
			goto exit;
		if (stdid == V4L2_STD_UNKNOWN) {
			/* and no standard is currently set on the display pipeline, we just
			   pick the first standard listed, whatever that might be. */
			standard.index = 0;
			if (LTV_CHECK
			    (ioctl(v4lfd, VIDIOC_ENUM_OUTPUT_STD, &standard)) <
			    0)
				goto exit;
			stdid = standard.id;
		}
	}
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_OUTPUT_STD, &stdid)) < 0)
		goto exit;

	standard.index = 0;
	standard.id = V4L2_STD_UNKNOWN;
	do {
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_ENUM_OUTPUT_STD, &standard)) <
		    0)
			goto exit;

		++standard.index;
	} while ((standard.id & stdid) != stdid);

	printf("Current display standard is '%s'\n", standard.name);

	memset(&cropcap, 0, sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_CROPCAP, &cropcap)) < 0)
		goto exit;

	frameduration =
	    ((double)standard.frameperiod.numerator /
	     (double)standard.frameperiod.denominator) * 1000000.0;

	printf("Frameduration = %lluus\n", frameduration);

	fp = read_gam_header(gamFileName, &pixfmt, &GamPictureHeader, &pitch,
			     &frameBufferSize, &cspace);
	if (fp == NULL) {
		perror("Unable to read gam header");
		goto exit;
	}
	image_width = GamPictureHeader.PictureWidth;
	image_height = GamPictureHeader.PictureHeight;

	/*
	 * Set display sized buffer format
	 */
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.pixelformat = pixfmt;
	fmt.fmt.pix.colorspace = cspace;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	fmt.fmt.pix.width = image_width;
	fmt.fmt.pix.height = image_height;

	/* set buffer format */
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_FMT, &fmt)) < 0)
		goto exit;

	printf("Selected V4L2 pixel format:\n");

	printf("Colorspace           = %d\n", fmt.fmt.pix.colorspace);
	printf("Width                = %d\n", fmt.fmt.pix.width);
	printf("Height               = %d\n", fmt.fmt.pix.height);
	printf("BytesPerLine         = %d\n", fmt.fmt.pix.bytesperline);
	printf("SizeImage            = %d\n", fmt.fmt.pix.sizeimage);
	printf("Priv (Chroma Offset) = %d\n\n", fmt.fmt.pix.priv);

	/*
	 * Request buffers to be allocated on the V4L2 device
	 */
	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = bufferNb;

	/* request buffers */
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_REQBUFS, &reqbuf)) < 0)
		goto exit;

	if (reqbuf.count < bufferNb) {
		perror("Unable to allocate all buffers");
		goto exit;
	}

	/*
	 * Query all the buffers from the driver.
	 */
	for (i = 0; i < bufferNb; i++) {
		memset(&buffer[i], 0, sizeof(struct v4l2_buffer));
		buffer[i].index = i;
		buffer[i].type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

		/* query buffer */
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_QUERYBUF, &buffer[i])) < 0)
			goto exit;

		buffer[i].field =
		    progressive ? V4L2_FIELD_NONE : V4L2_FIELD_INTERLACED;
		bufferdata[i] =
		    (char *)mmap(0, buffer[i].length, PROT_READ | PROT_WRITE,
				 MAP_SHARED, v4lfd, buffer[i].m.offset);

		if (bufferdata[i] == ((void *)-1)) {
			perror("mmap buffer failed\n");
			goto exit;
		}

		if (verbose)
			printf("creating buffer %d\n", i);

		if (i == 0) {
			read_gam_buffer(fp, bufferdata[i], GamPictureHeader,
					pitch, fmt.fmt.pix);
		} else {	/* For next buffers just copy content from first one */
			printf("Copying buffer %d\n", i);
			memcpy(bufferdata[i], bufferdata[0],
			       fmt.fmt.pix.sizeimage);
		}
	}

	/*
	 * Set the output size and position if downscale requested
	 */
	memset(&crop, 0, sizeof(struct v4l2_crop));
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (halfsize) {
		if (left_half) {
			crop.c.left = 0;
			crop.c.top = 0;
			crop.c.height = cropcap.bounds.height;
		} else if (right_half) {
			crop.c.left = cropcap.bounds.width / 2;
			crop.c.top = 0;
			crop.c.height = cropcap.bounds.height;
		} else {
			crop.c.left = cropcap.bounds.width / 4;
			crop.c.top = cropcap.bounds.height / 4;
			crop.c.height = cropcap.bounds.height / 2;
		}
		crop.c.width = cropcap.bounds.width / 2;
	} else if (quartersize) {
		crop.c.left = cropcap.bounds.width / 3;
		crop.c.top = cropcap.bounds.height / 3;
		crop.c.width = cropcap.bounds.width / 4;
		crop.c.height = cropcap.bounds.height / 4;
	} else {
		crop.c.left = 0;
		crop.c.top = 0;
		crop.c.width = cropcap.bounds.width;
		crop.c.height = cropcap.bounds.height;
	}
	printf("Output Image Dimensions (%d,%d) (%d x %d)\n", crop.c.left,
	       crop.c.top, crop.c.width, crop.c.height);

	/* set output crop */
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CROP, &crop)) < 0)
		goto exit;

	/*
	 * Set the buffer crop to be the full image.
	 */
	memset(&crop, 0, sizeof(struct v4l2_crop));
	crop.type = V4L2_BUF_TYPE_PRIVATE;
	crop.c.left = Window.x;
	crop.c.top = Window.y;
	crop.c.width = image_width - Window.x;
	crop.c.height = image_height - Window.y;

	printf("Source Image Dimensions (%d,%d) (%d x %d)\n", Window.x,
	       Window.y, image_width, image_height);

	/* set source crop */
	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_S_CROP, &crop)) < 0)
		goto exit;

	/*
	 * Work out the presentation time of the first buffer to be queued.
	 * Note the use of the posix monotomic clock NOT the adjusted wall clock.
	 */
	clock_gettime(CLOCK_MONOTONIC, &currenttime);

	if (verbose)
		printf("Current time = %ld.%ld\n", currenttime.tv_sec,
		       currenttime.tv_nsec / 1000);
	ptime.tv_sec = currenttime.tv_sec + delay;
	ptime.tv_usec = currenttime.tv_nsec / 1000;

	/*
	 * Queue all the buffers up for display.
	 */
	for (i = 0; i < bufferNb; i++) {
		buffer[i].timestamp = ptime;

		/* queue buffer */
		if (LTV_CHECK(ioctl(v4lfd, VIDIOC_QBUF, &buffer[i])) < 0)
			goto exit;

		ptime.tv_usec += frameduration;
		while (ptime.tv_usec >= 1000000L) {
			ptime.tv_sec++;
			ptime.tv_usec -= 1000000L;
		}

	}

	/* start stream */
	if (verbose)
		printf("Starting stream \n");

	if (LTV_CHECK(ioctl(v4lfd, VIDIOC_STREAMON, &buffer[0].type)) < 0)
		goto exit;

	while (Running) {
		for (i = 0; i < iterations; i++) {
			struct v4l2_buffer dqbuffer;
			long long time1;
			long long time2;
			long long timediff;

			/*
			 * To Dqueue a streaming memory buffer you must set the buffer type
			 * AND and the memory type correctly, otherwise the API returns an error.
			 */
			dqbuffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			dqbuffer.memory = V4L2_MEMORY_MMAP;

			/*
			 * We didn't open the V4L2 device non-blocking so dequeueing a buffer will
			 * sleep until a buffer becomes available.
			 */
			if (verbose)
				printf("dequeueing buffer\n");

			if (bufferNb < 3)
				printf
				    ("Warning ! We use only %d buffer(s) so dequeueing will block forever\nPress CTRL+C to exit program !!!\n",
				     bufferNb);

			if (LTV_CHECK(ioctl(v4lfd, VIDIOC_DQBUF, &dqbuffer)) <
			    0)
				goto streamoff;

			time1 =
			    (buffer[dqbuffer.index].timestamp.tv_sec *
			     1000000LL) +
			    buffer[dqbuffer.index].timestamp.tv_usec;
			time2 =
			    (dqbuffer.timestamp.tv_sec * 1000000LL) +
			    dqbuffer.timestamp.tv_usec;

			timediff = time2 - time1;

			if (verbose)
				printf
				    ("Buffer%d: wanted %ld.%06ld, actual %ld.%06ld, diff = %lld us\n",
				     dqbuffer.index,
				     buffer[dqbuffer.index].timestamp.tv_sec,
				     buffer[dqbuffer.index].timestamp.tv_usec,
				     dqbuffer.timestamp.tv_sec,
				     dqbuffer.timestamp.tv_usec, timediff);

			ptime.tv_usec += frameduration;
			if (unlikely(i == 0)) {
				/* after the first frame came back, we know when exactly the VSync is
				   happening, so adjust the presentation time - only once, though! */
				ptime.tv_usec += timediff;
			}
			if (adjusttime
			    && unlikely((dqbuffer.index == (bufferNb - 1))
					&& (i >= (bufferNb * 2) - 1))) {
				if (abs(timediff) > 500) {
					printf
					    ("Adjusting presentation time by %lld us\n",
					     timediff / 2);

					ptime.tv_usec += timediff / 2;
				}
			}
			while (ptime.tv_usec >= 1000000L) {
				ptime.tv_sec++;
				ptime.tv_usec -= 1000000L;
			}

			buffer[dqbuffer.index].timestamp = ptime;

			if (zoom) {
				memset(&crop, 0, sizeof(struct v4l2_crop));
				crop.type = V4L2_BUF_TYPE_PRIVATE;
				crop.c.left = i % (image_width / 4);
				crop.c.top = Window.y;
				crop.c.width = image_width - (crop.c.left * 2);
				crop.c.height = image_height - Window.y;

				/* set source crop */
				if (LTV_CHECK
				    (ioctl(v4lfd, VIDIOC_S_CROP, &crop)) < 0)
					goto streamoff;
			}

			/* queue buffer */
			if (LTV_CHECK
			    (ioctl(v4lfd, VIDIOC_QBUF, &buffer[dqbuffer.index]))
			    < 0)
				goto streamoff;
		}

		Running = CheckKey(v4lfd);

	}

streamoff:
	if (verbose)
		printf("Stream off \n");

	LTV_CHECK(ioctl(v4lfd, VIDIOC_STREAMOFF, &buffer[0].type));

exit:

  close(v4lfd);
  return 0;
}
