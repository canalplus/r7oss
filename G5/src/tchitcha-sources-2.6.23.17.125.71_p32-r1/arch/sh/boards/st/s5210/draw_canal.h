/*
 * arch/sh/boards/st/s5210/draw_canal.h
 *
 * Copyright (C) 2012 Canal +
 * Author: Antoine Maillard (Antoine.MAILLARD@canal-plus.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#define WIDTH 1280
#define HEIGHT 720

#define PLUS_WIDTH 14
#define PLUS_HEIGHT 44

#define BPP 32 //24 bits RGB

#define COLOR_QUAD1 24
#define COLOR_QUAD2 38
#define COLOR_QUAD3 65
#define COLOR_QUAD4 10
#define WHITE_COLOR 204


#define MAX(a,b) (a<b?b:a)
#define MIN(a,b) (a>b?b:a)

#define NORM_MAX 21474836

#define WELCOME_WIDTH 319
#define WELCOME_HEIGHT 43

#define WELCOME_X0 WIDTH/2+40
#define WELCOME_Y0 HEIGHT/2-63

static volatile unsigned char *framebuffer = (unsigned char *) fromSE(FBADDR);

static __initdata unsigned char *welcome = 
#include "welcome.raw.h"
;


static int mysqrt(int x){

	int _sqrt = 0;
	while(_sqrt*_sqrt<x){
		_sqrt++;	
	}
	
	if(x-(_sqrt-1)*(_sqrt-1) > _sqrt*_sqrt-x){
		return _sqrt;
	}else{
		return _sqrt-1;
	}
	
	return _sqrt;
}

static int m_z=155212;
static int m_w=-5454;
static int myrand(int max)
{
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return ((m_z << 16) + m_w)/NORM_MAX/max;
}

static void setPixel(int x, int y, int r, int g, int b){
// 	int indexR =  y * WIDTH*BPP/8 + x*BPP/8;
// 	int indexG = indexR + 1;
// 	int indexB = indexR + 2;

// 	framebuffer[indexR] = (unsigned char)r;
// 	framebuffer[indexG] = (unsigned char)g;
// 	framebuffer[indexB] = (unsigned char)b;
	
	((volatile u32 *) fromSE(FBADDR))[y * WIDTH + x] = r | (g<<8) | (b<<16);
}


static void add_noise(void){
	
	int i=0, j=0;
	for(i=0;i<WIDTH;i++){
		for(j=0;j<HEIGHT;j++){
			int _r = framebuffer[j * WIDTH*BPP/8 + i*BPP/8];
			int _g = framebuffer[j * WIDTH*BPP/8 + i*BPP/8 +1];
			int _b = framebuffer[j * WIDTH*BPP/8 + i*BPP/8 +2];
			int rand_r = myrand(40);
			int rand_g = myrand(40);
			int rand_b = myrand(40);
			framebuffer[j * WIDTH*BPP/8 + i*BPP/8] =  MIN(MAX(0, _r + rand_r), 255);
			framebuffer[j * WIDTH*BPP/8 + i*BPP/8 +1] = MIN(MAX(0, _g + rand_g), 255);
			framebuffer[j * WIDTH*BPP/8 + i*BPP/8 +2] =  MIN(MAX(0, _b + rand_b), 255);
		}
	}
	
}

static void draw_quads(void){
	
	int i=0, j=0;
	for(i=0;i<640;i++){
		for(j=0;j<360;j++){
			
			int distance = mysqrt(i*i+j*j)+myrand(10); // Note: pour éviter le banding, ajout d'un petit alea dans la distance
			int color_quad1 = COLOR_QUAD1*MAX(0, 640-distance)/640;
			int color_quad2 = COLOR_QUAD2*MAX(0, 640-distance)/640;
			int color_quad3 = COLOR_QUAD3*MAX(0, 640-distance)/640;
			int color_quad4 = COLOR_QUAD4*MAX(0, 640-distance)/640;

			setPixel(640-i-1, 360-j-1, color_quad1, color_quad1, color_quad1);
			setPixel(640+i, 360-j-1, color_quad2, color_quad2, color_quad2);
			setPixel(640-i-1, 360+j, color_quad3, color_quad3, color_quad3);
			setPixel(640+i, 360+j, color_quad4, color_quad4, color_quad4);
			
		}	
	}

}

static void draw_plus(void){
	int i,j,attn;
	for(j=0;j<PLUS_HEIGHT;j++){
		for(i=0;i<PLUS_WIDTH;i++){
			attn = 1;
			if((i==0 && j==0) || (i==PLUS_WIDTH-1 && j==0) || (i==0 && j==PLUS_HEIGHT-1) || (i==PLUS_WIDTH-1 && j==PLUS_HEIGHT-1))
				attn = 2;
			setPixel((WIDTH-PLUS_WIDTH)/2+i, (HEIGHT-PLUS_HEIGHT)/2+j, WHITE_COLOR/attn, WHITE_COLOR/attn, WHITE_COLOR/attn);
			setPixel((WIDTH-PLUS_HEIGHT)/2+j, (HEIGHT-PLUS_WIDTH)/2+i, WHITE_COLOR/attn, WHITE_COLOR/attn, WHITE_COLOR/attn);	
		}	
	}
	
}

static void draw_welcome(int x_0, int y_0){
	
	int i,j;
	for(i=0;i<WELCOME_WIDTH;i++){
		for(j=0;j<WELCOME_HEIGHT;j++){
		  framebuffer[(j+y_0) * WIDTH*BPP/8 + (i+x_0)*BPP/8] =  welcome[(j*WELCOME_WIDTH+i)];
		  framebuffer[(j+y_0) * WIDTH*BPP/8 + (i+x_0)*BPP/8 +1] = welcome[(j*WELCOME_WIDTH+i)];
		  framebuffer[(j+y_0) * WIDTH*BPP/8 + (i+x_0)*BPP/8 +2] =  welcome[(j*WELCOME_WIDTH+i)];
		}	
	}
		
}

static void draw_canal(void) {
	draw_quads();
	add_noise();	
	draw_plus();
	draw_welcome(WELCOME_X0, WELCOME_Y0);
}
