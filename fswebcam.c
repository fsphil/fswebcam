/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2019 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <gd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "log.h"
#include "src.h"
#include "effects.h"
#include "parse.h"

#define CLIP(val, min, max) (val > max) ? max : ((val < min) ? min : val)

#define ALIGN_LEFT   (0)
#define ALIGN_CENTER (1)
#define ALIGN_RIGHT  (2)

#define NO_BANNER     (0)
#define TOP_BANNER    (1)
#define BOTTOM_BANNER (2)

#define FORMAT_JPEG (0)
#define FORMAT_PNG  (1)

#define OPTBASE (128)
#define OPT_VERSION         (OPTBASE + 0)
#define OPT_PID             (OPTBASE + 1)
#define OPT_OFFSET          (OPTBASE + 2)
#define OPT_LIST_INPUTS     (OPTBASE + 3)
#define OPT_LIST_TUNERS     (OPTBASE + 4)
#define OPT_LIST_FORMATS    (OPTBASE + 5)
#define OPT_LIST_CONTROLS   (OPTBASE + 6)
#define OPT_LIST_FRAMESIZES (OPTBASE + 7)
#define OPT_LIST_FRAMERATES (OPTBASE + 8)
#define OPT_BRIGHTNESS      (OPTBASE + 9)
#define OPT_HUE             (OPTBASE + 10)
#define OPT_COLOUR          (OPTBASE + 11)
#define OPT_CONTRAST        (OPTBASE + 12)
#define OPT_WHITENESS       (OPTBASE + 13)
#define OPT_REVERT          (OPTBASE + 14)
#define OPT_FLIP            (OPTBASE + 15)
#define OPT_CROP            (OPTBASE + 16)
#define OPT_SCALE           (OPTBASE + 17)
#define OPT_ROTATE          (OPTBASE + 18)
#define OPT_DEINTERLACE     (OPTBASE + 19)
#define OPT_INVERT          (OPTBASE + 20)
#define OPT_GREYSCALE       (OPTBASE + 21)
#define OPT_NO_BANNER       (OPTBASE + 22)
#define OPT_TOP_BANNER      (OPTBASE + 23)
#define OPT_BOTTOM_BANNER   (OPTBASE + 24)
#define OPT_BG_COLOUR       (OPTBASE + 25)
#define OPT_BL_COLOUR       (OPTBASE + 26)
#define OPT_FG_COLOUR       (OPTBASE + 27)
#define OPT_FONT            (OPTBASE + 28)
#define OPT_NO_SHADOW       (OPTBASE + 29)
#define OPT_SHADOW          (OPTBASE + 30)
#define OPT_TITLE           (OPTBASE + 31)
#define OPT_NO_TITLE        (OPTBASE + 32)
#define OPT_SUBTITLE        (OPTBASE + 33)
#define OPT_NO_SUBTITLE     (OPTBASE + 34)
#define OPT_TIMESTAMP       (OPTBASE + 35)
#define OPT_NO_TIMESTAMP    (OPTBASE + 36)
#define OPT_GMT             (OPTBASE + 37)
#define OPT_INFO            (OPTBASE + 38)
#define OPT_NO_INFO         (OPTBASE + 39)
#define OPT_UNDERLAY        (OPTBASE + 40)
#define OPT_NO_UNDERLAY     (OPTBASE + 41)
#define OPT_OVERLAY         (OPTBASE + 42)
#define OPT_NO_OVERLAY      (OPTBASE + 43)
#define OPT_JPEG            (OPTBASE + 44)
#define OPT_PNG             (OPTBASE + 45)
#define OPT_SAVE            (OPTBASE + 46)
#define OPT_EXEC            (OPTBASE + 47)
#define OPT_DUMPFRAME       (OPTBASE + 48)
#define OPT_FPS             (OPTBASE + 49)

typedef struct {
	
	/* List of options. */
	char *opts;
	const struct option *long_opts;
	
	/* When reading from the command line. */
	int opt_index;
	
	/* When reading from a configuration file. */
	char *filename;
	FILE *f;
	size_t line;
	
} fswc_getopt_t;

typedef struct {
	uint16_t id;
	char    *options;
} fswebcam_job_t;

typedef struct {
	
	/* General options. */
	unsigned long loop;
	signed long offset;
	unsigned char background;
	char *pidfile;
	char *logfile;
	char gmt;
	
	/* Capture start time. */
	time_t start;
	
	/* Device options. */
	char *device;
	char *input;
	unsigned char tuner;
	unsigned long frequency;
	unsigned long delay;
	char use_read;
	uint8_t list;
	
	/* Image capture options. */
	unsigned int width;
	unsigned int height;
	unsigned int frames;
	unsigned int fps;
	unsigned int skipframes;
	int palette;
	src_option_t **option;
	char *dumpframe;
	
	/* Job queue. */
	uint8_t jobs;
	fswebcam_job_t **job;
	
	/* Banner options. */
	char banner;
	uint32_t bg_colour;
	uint32_t bl_colour;
	uint32_t fg_colour;
	char *title;
	char *subtitle;
	char *timestamp;
	char *info;
	char *font;
	int fontsize;
	char shadow;
	
	/* Overlay options. */
	char *underlay;
	char *overlay;
	
	/* Output options. */
	char *filename;
	char format;
	char compression;
	
} fswebcam_config_t;

#ifdef USE_32BIT_BUFFER

typedef uint32_t avgbmp_t;
#define MAX_FRAMES (UINT32_MAX >> 8)

#else

typedef uint16_t avgbmp_t;
#define MAX_FRAMES (UINT16_MAX >> 8)

#endif

volatile char received_sigusr1 = 0;
volatile char received_sighup  = 0;
volatile char received_sigterm = 0;

void fswc_signal_usr1_handler(int signum)
{
	/* Catches SIGUSR1 */
	INFO("Caught signal SIGUSR1.");
	received_sigusr1 = 1;
}

void fswc_signal_hup_handler(int signum)
{
	/* Catches SIGHUP */
	INFO("Caught signal SIGHUP.");
	received_sighup = 1;
}

void fswc_signal_term_handler(int signum)
{
	char *signame;
	
	/* Catches SIGTERM and SIGINT */
	switch(signum)
	{
	case SIGTERM: signame = "SIGTERM"; break;
	case SIGINT:  signame = "SIGINT"; break;
	default:      signame = "Unknown"; break;
	}
	
	INFO("Caught signal %s", signame);
	received_sigterm = 1;
}

int fswc_setup_signals()
{
	signal(SIGUSR1, fswc_signal_usr1_handler);
	signal(SIGHUP,  fswc_signal_hup_handler);
	signal(SIGTERM, fswc_signal_term_handler);
	signal(SIGINT,  fswc_signal_term_handler);
	
	return(0);
}

char *fswc_strftime(char *dst, size_t max, char *src,
                    time_t timestamp, int gmt)
{
	struct tm tm_timestamp;
	
	/* Set the time structure. */
	if(gmt) gmtime_r(&timestamp, &tm_timestamp);
	else localtime_r(&timestamp, &tm_timestamp);
	
	*dst = '\0';
	strftime(dst, max, src, &tm_timestamp);
	
	return(dst);
}

char *fswc_strduptime(char *src, time_t timestamp, int gmt)
{
	struct tm tm_timestamp;
	char *dst;
	size_t l;
	
	if(!src) return(NULL);
	
	/* Set the time structure. */
	if(gmt) gmtime_r(&timestamp, &tm_timestamp);
	else localtime_r(&timestamp, &tm_timestamp);
	
	dst = NULL;
	l = strlen(src) * 2;
	
	while(1)
	{
		size_t r;
		char *t = realloc(dst, l);
		
		if(!t)
		{
			free(dst);
			return(NULL);
		}
		
		dst = t;
		
		*dst = 1;
		r = strftime(dst, l, src, &tm_timestamp);
		
		if(r > 0 && r < l) return(dst);
		if(r == 0 && *dst == '\0') return(dst);
		
		l *= 2;
	}
	
	return(NULL);
}

void fswc_DrawText(gdImagePtr im, char *font, double size,
                   int x, int y, char align,
                   uint32_t colour, char shadow, char *text)
{
	char *err;
	int brect[8];
	
	if(!text) return;
	
	if(shadow)
	{
		uint32_t scolour = colour & 0xFF000000;
		
		fswc_DrawText(im, font, size, x + 1, y + 1,
		              align, scolour, 0, text);
	}
	
	/* Correct alpha value for GD. */
	colour = (((colour & 0xFF000000) / 2) & 0xFF000000) +
	         (colour & 0xFFFFFF);
	
	/* Pre-render the text. We use the results during alignment. */
	err = gdImageStringFT(NULL, &brect[0], colour, font, size, 0.0, 0, 0, text);
	if(err)
	{
		WARN("%s", err);
		return;
	}
	
	/* Adjust the coordinates according to the alignment. */
	switch(align)
	{
	case ALIGN_CENTER: x -= brect[4] / 2; break;
	case ALIGN_RIGHT:  x -= brect[4];     break;
	}
	
	/* Render the text onto the image. */
	gdImageStringFT(im, NULL, colour, font, size, 0.0, x, y, text);
}

int fswc_add_image_png(src_t *src, avgbmp_t *abitmap)
{
	uint32_t x, y;
	gdImage *im;
	
	im = gdImageCreateFromPngPtr(src->length, src->img);
	if(!im) return(-1);
	
	for(y = 0; y < src->height; y++)
		for(x = 0; x < src->width; x++)
		{
			int c = gdImageGetPixel(im, x, y);
			
			*(abitmap++) += (c & 0xFF0000) >> 16;
			*(abitmap++) += (c & 0x00FF00) >> 8;
			*(abitmap++) += (c & 0x0000FF);
		}
	
	gdImageDestroy(im);
	
	return(0);
}

int fswc_verify_jpeg_dht(uint8_t *src,  uint32_t lsrc,
                         uint8_t **dst, uint32_t *ldst)
{
	/* This function is based on a patch provided by Scott J. Bertin. */
	
	static unsigned char dht[] =
	{
		0xff, 0xc4, 0x01, 0xa2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02,
		0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d,
		0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31,
		0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32,
		0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52,
		0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
		0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
		0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
		0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57,
		0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83,
		0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
		0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
		0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
		0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
		0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
		0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
		0xf9, 0xfa, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
		0x0b, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
		0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01,
		0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41,
		0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14,
		0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
		0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25,
		0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a,
		0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46,
		0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
		0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83,
		0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
		0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
		0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
		0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
		0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
	};
	uint8_t *p, *i = NULL;
	
	/* By default we simply return the source image. */
	*dst = src;
	*ldst = lsrc;
	
	/* Scan for an existing DHT segment or the first SOS segment. */
	for(p = src + 2; p - src < lsrc - 3 && i == NULL; )
	{
		if(*(p++) != 0xFF) continue;
		
		if(*p == 0xD9) break;           /* JPEG_EOI */
		if(*p == 0xC4) return(0);       /* JPEG_DHT */
		if(*p == 0xDA && !i) i = p - 1; /* JPEG_SOS */
		
		/* Move to next segment. */
		p += (p[1] << 8) + p[2];
	}
	
	/* If no SOS was found, insert the DHT directly after the SOI. */
	if(i == NULL) i = src + 2;
	
	DEBUG("Inserting DHT segment into JPEG frame.");
	
	*ldst = lsrc + sizeof(dht);
	*dst  = malloc(*ldst);
	if(!*dst)
	{
		ERROR("Out of memory.");
		return(-1);
	}
	
	/* Copy the JPEG data, inserting the DHT segment. */
	memcpy((p  = *dst), src, i - src);
	memcpy((p += i - src), dht, sizeof(dht));
	memcpy((p += sizeof(dht)), i, lsrc - (i - src));
	
	return(1);
}

int fswc_add_image_jpeg(src_t *src, avgbmp_t *abitmap)
{
	uint32_t x, y, hlength;
	uint8_t *himg = NULL;
	gdImage *im;
	int i;
	
	/* MJPEG data may lack the DHT segment required for decoding... */
	i = fswc_verify_jpeg_dht(src->img, src->length, &himg, &hlength);
	
	im = gdImageCreateFromJpegPtr(hlength, himg);
	if(i == 1) free(himg);
	
	if(!im) return(-1);
	
	for(y = 0; y < src->height; y++)
		for(x = 0; x < src->width; x++)
		{
			int c = gdImageGetPixel(im, x, y);
			
			*(abitmap++) += (c & 0xFF0000) >> 16;
			*(abitmap++) += (c & 0x00FF00) >> 8;
			*(abitmap++) += (c & 0x0000FF);
		}
	
	gdImageDestroy(im);
	
	return(0);
}

int fswc_add_image_rgb32(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length << 2 < i) return(-1);
	
	while(i-- > 0)
	{
		*(abitmap++) += *(img++);
		*(abitmap++) += *(img++);
		*(abitmap++) += *(img++);
		img++;
	}
	
	return(0);
}

int fswc_add_image_bgr32(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t p, i = src->width * src->height;
	
	if(src->length << 2 < i) return(-1);
	
	for(p = 0; p < i; p += 4)
	{
		abitmap[0] += img[2];
		abitmap[1] += img[1];
		abitmap[2] += img[0];
		abitmap += 3;
		img += 4;
	}
	
	return(0);
}

int fswc_add_image_rgb24(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t i = src->width * src->height * 3;
	
	if(src->length < i) return(-1);
	while(i-- > 0) *(abitmap++) += *(img++);
	
	return(0);
}

int fswc_add_image_bgr24(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = (uint8_t *) src->img;
	uint32_t p, i = src->width * src->height * 3;
	
	if(src->length < i) return(-1);
	
	for(p = 0; p < src->length; p += 3)
	{
		abitmap[0] += img[2];
		abitmap[1] += img[1];
		abitmap[2] += img[0];
		abitmap += 3;
		img += 3;
	}
	
	return(0);
}

int fswc_add_image_rgb565(src_t *src, avgbmp_t *abitmap)
{
	uint16_t *img = (uint16_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length >> 1 < i) return(-1);
	
	while(i-- > 0)
	{
		uint8_t r, g, b;
		
		r = (*img & 0xF800) >> 8;
		g = (*img &  0x7E0) >> 3;
		b = (*img &   0x1F) << 3;
		
		*(abitmap++) += r + (r >> 5);
		*(abitmap++) += g + (g >> 6);
		*(abitmap++) += b + (b >> 5);
		
		img++;
	}
	
	return(0);
}

int fswc_add_image_rgb555(src_t *src, avgbmp_t *abitmap)
{
	uint16_t *img = (uint16_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length >> 1 < i) return(-1);
	
	while(i-- > 0)
	{
		uint8_t r, g, b;
		
		r = (*img & 0x7C00) >> 7;
		g = (*img &  0x3E0) >> 2;
		b = (*img &   0x1F) << 3;
		
		*(abitmap++) += r + (r >> 5);
		*(abitmap++) += g + (g >> 5);
		*(abitmap++) += b + (b >> 5);
		
		img++;
	}
	
	return(0);
}

int fswc_add_image_bayer(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *img = src->img;
	uint32_t x = 0, y = 0;
	uint32_t w = src->width, h = src->height;
	uint32_t i = w * h;
	
	if(src->length < i) return(-1);
	
	/* SBGGR8 bayer pattern:
	 * 
	 * BGBGBGBGBG
	 * GRGRGRGRGR
	 * BGBGBGBGBG
	 * GRGRGRGRGR
	 * 
	 * SGBRG8 bayer pattern:
	 * 
	 * GBGBGBGBGB
	 * RGRGRGRGRG
	 * GBGBGBGBGB
	 * RGRGRGRGRG
	 *
	 * SGRBG8 bayer pattern:
	 *
	 * GRGRGRGRGR
	 * BGBGBGBGBG
	 * GRGRGRGRGR
	 * BGBGBGBGBG
	*/
	
	while(i-- > 0)
	{
		uint8_t *p[8];
		uint8_t hn, vn, di;
		uint8_t r, g, b;
		int mode;
		
		/* Setup pointers to this pixel's neighbours. */
		p[0] = img - w - 1;
		p[1] = img - w;
		p[2] = img - w + 1;
		p[3] = img - 1;
		p[4] = img + 1;
		p[5] = img + w - 1;
		p[6] = img + w;
		p[7] = img + w + 1;
		
		/* Juggle pointers if they are out of bounds. */
		if(!y)              { p[0]=p[5]; p[1]=p[6]; p[2]=p[7]; }
		else if(y == h - 1) { p[5]=p[0]; p[6]=p[1]; p[7]=p[2]; }
		if(!x)              { p[0]=p[2]; p[3]=p[4]; p[5]=p[7]; }
		else if(x == w - 1) { p[2]=p[0]; p[4]=p[3]; p[7]=p[5]; }
		
		/* Average matching neighbours. */
		hn = (*p[3] + *p[4]) / 2;
		vn = (*p[1] + *p[6]) / 2;
		di = (*p[0] + *p[2] + *p[5] + *p[7]) / 4;
		
		/* Calculate RGB */
		if(src->palette == SRC_PAL_BAYER) mode = (x + y) & 0x01;
		else mode = ~(x + y) & 0x01;
		
		if(mode)
		{
			g = *img;
			if(y & 0x01) { r = hn; b = vn; }
			else         { r = vn; b = hn; }
		}
		else if(y & 0x01) { r = *img; g = (vn + hn) / 2; b = di; }
		else              { b = *img; g = (vn + hn) / 2; r = di; }
		
		if(src->palette == SRC_PAL_SGRBG8)
		{
			uint8_t t = r;
			r = b;
			b = t;
		}
		
		*(abitmap++) += r;
		*(abitmap++) += g;
		*(abitmap++) += b;
		
		/* Move to the next pixel (or line) */
		if(++x == w) { x = 0; y++; }
		img++;
	}
	
	return(0);
}

/* The following YUV functions are based on code by Vincent Hourdin.
 * http://vinvin.dyndns.org/projects/
 *
 * Faster integer maths from camE by Tom Gilbert.
 * http://linuxbrit.co.uk/camE/
*/

int fswc_add_image_yuyv(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *ptr;
	uint32_t x, y, z;
	
	if(src->length < (src->width * src->height * 2)) return(-1);
	
	/* YUYV and UYVY are very similar and so  *
	 * are both handled by this one function. */
	
	ptr = (uint8_t *) src->img;
	z = 0;
	
	for(y = 0; y < src->height; y++)
	{
		for(x = 0; x < src->width; x++)
		{
			int r, g, b;
			int y, u, v;
			
			if(src->palette == SRC_PAL_UYVY)
			{
				if(!z) y = ptr[1] << 8;
				else   y = ptr[3] << 8;
				
				u = ptr[0] - 128;
				v = ptr[2] - 128;
			}
			else /* SRC_PAL_YUYV */
			{
				if(!z) y = ptr[0] << 8;
				else   y = ptr[2] << 8;
				
				u = ptr[1] - 128;
				v = ptr[3] - 128;
			}
			
			r = (y + (359 * v)) >> 8;
			g = (y - (88 * u) - (183 * v)) >> 8;
			b = (y + (454 * u)) >> 8;
			
			*(abitmap++) += CLIP(r, 0x00, 0xFF);
			*(abitmap++) += CLIP(g, 0x00, 0xFF);
			*(abitmap++) += CLIP(b, 0x00, 0xFF);
			
			if(z++)
			{
				z = 0;
				ptr += 4;
			}
		}
	}
	
        return(0);
}

int fswc_add_image_yuv420p(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *yptr, *uptr, *vptr;
	uint32_t x, y, p, o;
	
	if(src->length < (src->width * src->height * 3) / 2) return(-1);
	
	/* Setup pointers to Y, U and V buffers. */
	yptr = (uint8_t *) src->img;
	uptr = yptr + (src->width * src->height);
	vptr = uptr + (src->width * src->height / 4);
	o = 0;
	p = 0;
	
	for(y = 0; y < src->height; y++)
	{
		for(x = 0; x < src->width; x++)
		{
			int r, g, b;
			int y, u, v;
			
			y = *(yptr++) << 8;
			u = uptr[p] - 128;
			v = vptr[p] - 128;
			
			r = (y + (359 * v)) >> 8;
			g = (y - (88 * u) - (183 * v)) >> 8;
			b = (y + (454 * u)) >> 8;
			
			*(abitmap++) += CLIP(r, 0x00, 0xFF);
			*(abitmap++) += CLIP(g, 0x00, 0xFF);
			*(abitmap++) += CLIP(b, 0x00, 0xFF);
			
			if(x & 1) p++;
		}
		
		if(!(y & 1)) p -= src->width / 2;
	}
	
	return(0);
}

int fswc_add_image_nv12mb(src_t *src, avgbmp_t *abitmap)
{
	uint32_t x, y;
	uint32_t bw;
	
	if(src->length != (src->width * src->height * 3) / 2) return(-1);
	
	bw = src->width >> 4;
	
	for(y = 0; y < src->height; y++)
	{
		for(x = 0; x < src->width; x++)
		{
			uint32_t bx, by;
			int cy, cu, cv;
			int cr, cg, cb;
			uint8_t *py, *puv;
			
			bx = x >> 4;
			by = y >> 4;
			
			py  = src->img;
			py += ((by * bw) + bx) * 0x100;
			py += ((y - (by << 4)) * 0x10) + (x - (bx << 4));
			
			by /= 2;
			
			puv  = src->img + (src->width * src->height);
			puv += ((by * bw) + bx) * 0x100;
			puv += (((y / 2) - (by << 4)) * 0x10) + ((x - (bx << 4)) &~ 1);
			
			cy = *py << 8;
			cu = puv[0] - 128;
			cv = puv[1] - 128;
			
			cr = (cy + (359 * cv)) >> 8;
			cg = (cy - (88 * cu) - (183 * cv)) >> 8;
			cb = (cy + (454 * cu)) >> 8;
			
			*(abitmap++) += CLIP(cr, 0x00, 0xFF);
			*(abitmap++) += CLIP(cg, 0x00, 0xFF);
			*(abitmap++) += CLIP(cb, 0x00, 0xFF);
		}
	}
	
	return(0);
}

int fswc_add_image_grey(src_t *src, avgbmp_t *abitmap)
{
	uint8_t *bitmap = (uint8_t *) src->img;
	uint32_t i = src->width * src->height;
	
	if(src->length < i) return(-1);
	
	while(i-- > 0)
	{
		*(abitmap++) += *bitmap;
		*(abitmap++) += *bitmap;
		*(abitmap++) += *(bitmap++);
	}
	
	return(0);
}

int fswc_draw_overlay(fswebcam_config_t *config, char *filename, gdImage *image){
	FILE *f;
	gdImage *overlay;
	
	if(!filename) return(-1);
	
	f = fopen(filename, "rb");
	if(!f)
	{
		ERROR("Unable to open '%s'", filename);
		ERROR("fopen: %s", strerror(errno));
		return(-1);
	}
	
	overlay = gdImageCreateFromPng(f);
	fclose(f);
	
	if(!overlay)
	{
		ERROR("Unable to read '%s'. Not a PNG image?", filename);
		return(-1);
	}
	
	gdImageCopy(image, overlay, 0, 0, 0, 0, overlay->sx, overlay->sy);
	gdImageDestroy(overlay);
	
	return(0);
}

int fswc_draw_banner(fswebcam_config_t *config, gdImage *image)
{
	char timestamp[200];
	int w, h;
	int height;
	int spacing;
	int top;
	int y;
	
	w = gdImageSX(image);
	h = gdImageSY(image);
	
	/* Create the timestamp text. */
	fswc_strftime(timestamp, 200, config->timestamp,
	              config->start, config->gmt);
	
	/* Calculate the position and height of the banner. */
	spacing = 4;
	height = config->fontsize + (spacing * 2);
	
	if(config->subtitle || config->info)
		height += config->fontsize * 0.8 + spacing;
	
	top = 0;
	if(config->banner == BOTTOM_BANNER) top = h - height;
	
	/* Draw the banner line. */
	if(config->banner == TOP_BANNER)
	{
		gdImageFilledRectangle(image,
		                       0, height + 1,
		                       w, height + 2,
		                       config->bl_colour);
	}
	else
	{
		gdImageFilledRectangle(image,
		                       0, top - 2,
		                       w, top - 1,
		                       config->bl_colour);
	}
	
	/* Draw the background box. */
	gdImageFilledRectangle(image,
	   0, top,
	   w, top + height,
	   config->bg_colour);
	
	y = top + spacing + config->fontsize;
	
	/* Draw the title. */
	fswc_DrawText(image, config->font, config->fontsize,
	              spacing, y, ALIGN_LEFT,
	              config->fg_colour, config->shadow, config->title);
	
	/* Draw the timestamp. */
	fswc_DrawText(image, config->font, config->fontsize * 0.8,
	              w - spacing, y, ALIGN_RIGHT,
	              config->fg_colour, config->shadow, timestamp);
	
	y += spacing + config->fontsize * 0.8;
	
	/* Draw the sub-title. */
	fswc_DrawText(image, config->font, config->fontsize * 0.8,
	              spacing, y, ALIGN_LEFT,
	              config->fg_colour, config->shadow, config->subtitle);
	
	/* Draw the info text. */
	fswc_DrawText(image, config->font, config->fontsize * 0.7,
	              w - spacing, y, ALIGN_RIGHT,
	              config->fg_colour, config->shadow, config->info);
	
	return(0);
}

gdImage* fswc_gdImageDuplicate(gdImage* src)
{
	gdImage *dst;
	
	dst = gdImageCreateTrueColor(gdImageSX(src), gdImageSY(src));
	if(!dst) return(NULL);
	
	gdImageCopy(dst, src, 0, 0, 0, 0, gdImageSX(src), gdImageSY(src));
	
	return(dst);
}

int fswc_output(fswebcam_config_t *config, char *name, gdImage *image)
{
	char filename[FILENAME_MAX];
	gdImage *im;
	FILE *f;
	
	if(!name) return(-1);
	if(!strncmp(name, "-", 2) && config->background)
	{
		ERROR("stdout is unavailable in background mode.");
		return(-1);
	}
	
	fswc_strftime(filename, FILENAME_MAX, name,
	              config->start, config->gmt);
	
	/* Create a temporary image buffer. */
	im = fswc_gdImageDuplicate(image);
	if(!im)
	{
		ERROR("Out of memory.");
		return(-1);
	}
	
	/* Draw the underlay. */
	fswc_draw_overlay(config, config->underlay, im);
	
	/* Draw the banner. */
	if(config->banner != NO_BANNER)
	{
		char *err;
		
		/* Check if drawing text works */
		err = gdImageStringFT(NULL, NULL, 0, config->font, config->fontsize, 0.0, 0, 0, "");
		
		if(!err) fswc_draw_banner(config, im);
		else
		{
			/* Can't load the font - display a warning */
			WARN("Unable to load font '%s': %s", config->font, err);
			WARN("Disabling the the banner.");
		}
	}
	
	/* Draw the overlay. */
	fswc_draw_overlay(config, config->overlay, im);
	
	/* Write to a file if a filename was given, otherwise stdout. */
	if(strncmp(name, "-", 2)) f = fopen(filename, "wb");
	else f = stdout;
	
	if(!f)
	{
		ERROR("Error opening file for output: %s", filename);
		ERROR("fopen: %s", strerror(errno));
		gdImageDestroy(im);
		return(-1);
	}
	
	/* Write the compressed image. */
	switch(config->format)
	{
	case FORMAT_JPEG:
		MSG("Writing JPEG image to '%s'.", filename);
		gdImageJpeg(im, f, config->compression);
		break;
	case FORMAT_PNG:
		MSG("Writing PNG image to '%s'.", filename);
		gdImagePngEx(im, f, config->compression);
		break;
	}
	
	if(f != stdout) fclose(f);
	
	gdImageDestroy(im);
	
	return(0);
}

int fswc_exec(fswebcam_config_t *config, char *cmd)
{
	char *cmdline;
	FILE *p;
	
	cmdline = fswc_strduptime(cmd, config->start, config->gmt);
	if(!cmdline) return(-1);
	
	MSG("Executing '%s'...", cmdline);
	
	p = popen(cmdline, "r");
	free(cmdline);
	
	if(p)
	{
		while(!feof(p))
		{
			char *n;
			char line[1024];
			
			if(!fgets(line, 1024, p)) break;
			
			while((n = strchr(line, '\n'))) *n = '\0';
			MSG("%s", line);
		}
		
		pclose(p);
	}
	else
	{
		ERROR("popen: %s", strerror(errno));
		return(-1);
	}
	
	return(0);
}

int fswc_grab(fswebcam_config_t *config)
{
	uint32_t frame;
	uint32_t x, y;
	avgbmp_t *abitmap, *pbitmap;
	gdImage *image, *original;
	uint8_t modified;
	src_t src;
	
	/* Record the start time. */
	config->start = time(NULL);
	
	/* Set source options... */
	memset(&src, 0, sizeof(src));
	src.input      = config->input;
	src.tuner      = config->tuner;
	src.frequency  = config->frequency;
	src.delay      = config->delay;
	src.timeout    = 10; /* seconds */
	src.use_read   = config->use_read;
	src.list       = config->list;
	src.palette    = config->palette;
	src.width      = config->width;
	src.height     = config->height;
	src.fps        = config->fps;
	src.option     = config->option;
	
	HEAD("--- Opening %s...", config->device);
	
	if(src_open(&src, config->device) == -1) return(-1);
	
	/* The source may have adjusted the width and height we passed
	 * to it. Update the main config to match. */
	config->width  = src.width;
	config->height = src.height;
	
	/* Allocate memory for the average bitmap buffer. */
	abitmap = calloc(config->width * config->height * 3, sizeof(avgbmp_t));
	if(!abitmap)
	{
		ERROR("Out of memory.");
		return(-1);
	}
	
	if(config->frames == 1) HEAD("--- Capturing frame...");
	else HEAD("--- Capturing %i frames...", config->frames);
	
	if(config->skipframes == 1) MSG("Skipping frame...");
	else if(config->skipframes > 1) MSG("Skipping %i frames...", config->skipframes);
	
	/* Grab (and do nothing with) the skipped frames. */
	for(frame = 0; frame < config->skipframes; frame++)
		if(src_grab(&src) == -1) break;
	
	/* If frames where skipped, inform when normal capture begins. */
	if(config->skipframes) MSG("Capturing %i frames...", config->frames);
	
	/* Grab the requested number of frames. */
	for(frame = 0; frame < config->frames; frame++)
	{
		if(src_grab(&src) == -1) break;
		
		if(!frame && config->dumpframe)
		{
			/* Dump the raw data from the first frame to file. */
			FILE *f;
			
			MSG("Dumping raw frame to '%s'...", config->dumpframe);
			
			f = fopen(config->dumpframe, "wb");
			if(!f) ERROR("fopen: %s", strerror(errno));
			else
			{
				size_t l;
				l = fwrite(src.img, 1, src.length, f);
				fclose(f);
			}
		}
		
		/* Add frame to the average bitmap. */
		switch(src.palette)
		{
		case SRC_PAL_PNG:
			fswc_add_image_png(&src, abitmap);
			break;
		case SRC_PAL_JPEG:
		case SRC_PAL_MJPEG:
			fswc_add_image_jpeg(&src, abitmap);
			break;
		case SRC_PAL_RGB32:
			fswc_add_image_rgb32(&src, abitmap);
			break;
		case SRC_PAL_BGR32:
			fswc_add_image_bgr32(&src, abitmap);
			break;
		case SRC_PAL_RGB24:
			fswc_add_image_rgb24(&src, abitmap);
			break;
		case SRC_PAL_BGR24:
			fswc_add_image_bgr24(&src, abitmap);
			break;
		case SRC_PAL_BAYER:
		case SRC_PAL_SGBRG8:
		case SRC_PAL_SGRBG8:
			fswc_add_image_bayer(&src, abitmap);
			break;
		case SRC_PAL_YUYV:
		case SRC_PAL_UYVY:
			fswc_add_image_yuyv(&src, abitmap);
			break;
		case SRC_PAL_YUV420P:
			fswc_add_image_yuv420p(&src, abitmap);
			break;
		case SRC_PAL_NV12MB:
			fswc_add_image_nv12mb(&src, abitmap);
			break;
		case SRC_PAL_RGB565:
			fswc_add_image_rgb565(&src, abitmap);
			break;
		case SRC_PAL_RGB555:
			fswc_add_image_rgb555(&src, abitmap);
			break;
		case SRC_PAL_GREY:
			fswc_add_image_grey(&src, abitmap);
			break;
		}
	}
	
	/* We are now finished with the capture card. */
	src_close(&src);
	
	/* Fail if no frames where captured. */
	if(!frame)
	{
		ERROR("No frames captured.");
		free(abitmap);
		return(-1);
	}
	
	HEAD("--- Processing captured image...");
	
	/* Copy the average bitmap image to a gdImage. */
	original = gdImageCreateTrueColor(config->width, config->height);
	if(!original)
	{
		ERROR("Out of memory.");
		free(abitmap);
		return(-1);
	}
	
	pbitmap = abitmap;
	for(y = 0; y < config->height; y++)
		for(x = 0; x < config->width; x++)
		{
			int px = x;
			int py = y;
			int colour;
			
			colour  = (*(pbitmap++) / config->frames) << 16;
			colour += (*(pbitmap++) / config->frames) << 8;
			colour += (*(pbitmap++) / config->frames);
			
			gdImageSetPixel(original, px, py, colour);
		}
	
	free(abitmap);
	
	/* Make a copy of the original image. */
	image = fswc_gdImageDuplicate(original);
	if(!image)
	{
		ERROR("Out of memory.");
		gdImageDestroy(image);
		return(-1);
	}
	
	/* Set the default values for this run. */
	if(config->font) free(config->font);
	if(config->title) free(config->title);
	if(config->subtitle) free(config->subtitle);
	if(config->timestamp) free(config->timestamp);
	if(config->info) free(config->info);
	if(config->underlay) free(config->underlay);
	if(config->overlay) free(config->overlay);
	if(config->filename) free(config->filename);
	
	config->banner       = BOTTOM_BANNER;
	config->bg_colour    = 0x40263A93;
	config->bl_colour    = 0x00FF0000;
	config->fg_colour    = 0x00FFFFFF;
	config->font         = strdup("luxisr");
	config->fontsize     = 10;
	config->shadow       = 1;
	config->title        = NULL;
	config->subtitle     = NULL;
	config->timestamp    = strdup("%Y-%m-%d %H:%M (%Z)");
	config->info         = NULL;
	config->underlay     = NULL;
	config->overlay      = NULL;
	config->filename     = NULL;
	config->format       = FORMAT_JPEG;
	config->compression  = -1;
	
	modified = 1;
	
	/* Run through the jobs list. */
	for(x = 0; x < config->jobs; x++)
	{
		uint16_t id   = config->job[x]->id;
		char *options = config->job[x]->options;
		
		switch(id)
		{
		case 1: /* A non-option argument: a filename. */
		case OPT_SAVE:
			fswc_output(config, options, image);
			modified = 0;
			break;
		case OPT_EXEC:
			fswc_exec(config, options);
			break;
		case OPT_REVERT:
			modified = 1;
			gdImageDestroy(image);
			image = fswc_gdImageDuplicate(original);
			break;
		case OPT_FLIP:
			modified = 1;
			image = fx_flip(image, options);
			break;
		case OPT_CROP:
			modified = 1;
			image = fx_crop(image, options);
			break;
		case OPT_SCALE:
			modified = 1;
			image = fx_scale(image, options);
			break;
		case OPT_ROTATE:
			modified = 1;
			image = fx_rotate(image, options);
			break;
		case OPT_DEINTERLACE:
			modified = 1;
			image = fx_deinterlace(image, options);
			break;
		case OPT_INVERT:
			modified = 1;
			image = fx_invert(image, options);
			break;
		case OPT_GREYSCALE:
			modified = 1;
			image = fx_greyscale(image, options);
			break;
		case OPT_NO_BANNER:
			modified = 1;
			MSG("Disabling banner.");
			config->banner = NO_BANNER;
			break;
		case OPT_TOP_BANNER:
			modified = 1;
			MSG("Putting banner at the top.");
			config->banner = TOP_BANNER;
			break;
		case OPT_BOTTOM_BANNER:
			modified = 1;
			MSG("Putting banner at the bottom.");
			config->banner = BOTTOM_BANNER;
			break;
		case OPT_BG_COLOUR:
			modified = 1;
			MSG("Setting banner background colour to %s.", options);
			if(sscanf(options, "#%X", &config->bg_colour) != 1)
				WARN("Bad background colour: %s", options);
			break;
		case OPT_BL_COLOUR:
			modified = 1;
			MSG("Setting banner line colour to %s.", options);
			if(sscanf(options, "#%X", &config->bl_colour) != 1)
				WARN("Bad line colour: %s", options);
			break;
		case OPT_FG_COLOUR:
			modified = 1;
			MSG("Setting banner text colour to %s.", options);
			if(sscanf(options, "#%X", &config->fg_colour) != 1)
				WARN("Bad text colour: %s", options);
			break;
		case OPT_FONT:
			modified = 1;
			MSG("Setting font to %s.", options);
			if(parse_font(options, &config->font, &config->fontsize))
				WARN("Bad font: %s", options);
			break;
		case OPT_NO_SHADOW:
			modified = 1;
			MSG("Disabling text shadow.");
			config->shadow = 0;
			break;
		case OPT_SHADOW:
			modified = 1;
			MSG("Enabling text shadow.");
			config->shadow = 1;
			break;
		case OPT_TITLE:
			modified = 1;
			MSG("Setting title \"%s\".", options);
			if(config->title) free(config->title);
			config->title = strdup(options);
			break;
		case OPT_NO_TITLE:
			modified = 1;
			MSG("Clearing title.");
			if(config->title) free(config->title);
			config->title = NULL;
			break;
		case OPT_SUBTITLE:
			modified = 1;
			MSG("Setting subtitle \"%s\".", options);
			if(config->subtitle) free(config->subtitle);
			config->subtitle = strdup(options);
			break;
		case OPT_NO_SUBTITLE:
			modified = 1;
			MSG("Clearing subtitle.");
			if(config->subtitle) free(config->subtitle);
			config->subtitle = NULL;
			break;
		case OPT_TIMESTAMP:
			modified = 1;
			MSG("Setting timestamp \"%s\".", options);
			if(config->timestamp) free(config->timestamp);
			config->timestamp = strdup(options);
			break;
		case OPT_NO_TIMESTAMP:
			modified = 1;
			MSG("Clearing timestamp.");
			if(config->timestamp) free(config->timestamp);
			config->timestamp = NULL;
			break;
		case OPT_INFO:
			modified = 1;
			MSG("Setting info text \"%s\".", options);
			if(config->info) free(config->info);
			config->info = strdup(options);
			break;
		case OPT_NO_INFO:
			modified = 1;
			MSG("Clearing info text.");
			if(config->info) free(config->info);
			config->info = NULL;
			break;
		case OPT_UNDERLAY:
			modified = 1;
			MSG("Setting underlay image: %s", options);
			if(config->underlay) free(config->underlay);
			config->underlay = strdup(options);
			break;
		case OPT_NO_UNDERLAY:
			modified = 1;
			MSG("Clearing underlay.");
			if(config->underlay) free(config->underlay);
			config->underlay = NULL;
			break;
		case OPT_OVERLAY:
			modified = 1;
			MSG("Setting overlay image: %s", options);
			if(config->overlay) free(config->overlay);
			config->overlay = strdup(options);
			break;
		case OPT_NO_OVERLAY:
			modified = 1;
			MSG("Clearing overlay image.");
			if(config->overlay) free(config->overlay);
			config->overlay = NULL;
			break;
		case OPT_JPEG:
			modified = 1;
			MSG("Setting output format to JPEG, quality %i", atoi(options));
			config->format = FORMAT_JPEG;
			config->compression = atoi(options);
			break;
		case OPT_PNG:
			modified = 1;
			MSG("Setting output format to PNG, quality %i", atoi(options));
			config->format = FORMAT_PNG;
			config->compression = atoi(options);
			break;
		}
	}
	
	gdImageDestroy(image);
	gdImageDestroy(original);
	
	if(modified) WARN("There are unsaved changes to the image.");
	
	return(0);
}

int fswc_openlog(fswebcam_config_t *config)
{
	char *s;
	int r;
	
	/* Get the first part of the filename. */
	s = argdup(config->logfile, ":", 0, 0);
	if(!s)
	{
		ERROR("Out of memory.");
		return(-1);
	}
	
	if(!strcasecmp(s, "file"))
	{
		free(s);
		
		s = argdup(config->logfile, ":", 1, 0);
		if(!s)
		{
			ERROR("No log file was specified.");
			return(-1);
		}
	}
	else if(!strcasecmp(s, "syslog"))
	{
		free(s);
		log_syslog(1);
		return(0);
	}
	
	r = log_open(s);
	free(s);
	
	return(r);
}

int fswc_background(fswebcam_config_t *config)
{
	pid_t pid, sid;
	
	/* Silence the output if not logging to a file. */
	if(!config->logfile) log_set_fd(-1);
	
	/* Fork into the background. */
	pid = fork();
	
	if(pid < 0)
	{
		ERROR("Error going into the background.");
		ERROR("fork: %s", strerror(errno));
		return(-1);
	}
	
	/* Is this the parent process? If so, end it. */
	if(pid > 0) exit(0);
	
	umask(0);
	
	/* Create a new SID for the child process. */
	sid = setsid();
	if(sid < 0)
	{
		ERROR("Error going into the background.");
		ERROR("setsid: %s", strerror(errno));
		return(-1);
	}
	
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	
	return(0);
}

int fswc_savepid(fswebcam_config_t *config)
{
	FILE *fpid = fopen(config->pidfile, "wt");
	
	if(!fpid)
	{
		ERROR("Error saving PID to file '%s'",config->pidfile);
		ERROR("fopen: %s", strerror(errno));
		
		return(-1);
	}
	
	fprintf(fpid, "%i\n", getpid());
	fclose(fpid);
	
	return(0);
}

int fswc_find_palette(char *name)
{
	int i;
	
	/* Scan through the palette table until a match is found. */
	for(i = 0; src_palette[i].name != NULL; i++)
	{
		/* If a match was found, return the index. */
		if(!strcasecmp(src_palette[i].name, name)) return(i);
	}
	
	/* No match was found. */
	ERROR("Unrecognised palette format \"%s\". Supported formats:", name);
	
	for(i = 0; src_palette[i].name != NULL; i++)
		ERROR("%s", src_palette[i].name);
	
	return(-1);
}

int fswc_usage()
{
	printf("Usage: fswebcam [<options>] <filename> [[<options>] <filename> ... ]\n"
	       "\n"
	       " Options:\n"
	       "\n"
	       " -?, --help                   Display this help page and exit.\n"
	       " -c, --config <filename>      Load configuration from file.\n"
	       " -q, --quiet                  Hides all messages except for errors.\n"
	       " -v, --verbose                Displays extra messages while capturing\n"
	       "     --version                Displays the version and exits.\n"
	       " -l, --loop <seconds>         Run in loop mode.\n"
	       " -b, --background             Run in the background.\n"
	       " -o, --output <filename>      Output the log to a file.\n"
	       " -d, --device <name>          Sets the source to use.\n"
	       " -i, --input <number/name>    Selects the input to use.\n"
	       " -t, --tuner <number>         Selects the tuner to use.\n"
	       " -f, --frequency <number>     Selects the frequency use.\n"
	       " -p, --palette <name>         Selects the palette format to use.\n"
	       " -D, --delay <number>         Sets the pre-capture delay time. (seconds)\n"
	       " -r, --resolution <size>      Sets the capture resolution.\n"
	       "     --fps <framerate>        Sets the capture frame rate.\n"
	       " -F, --frames <number>        Sets the number of frames to capture.\n"
	       " -S, --skip <number>          Sets the number of frames to skip.\n"
	       "     --dumpframe <filename>   Dump a raw frame frame to file.\n"
	       " -s, --set <name>=<value>     Sets a control value.\n"
	       "     --revert                 Restores original captured image.\n"
	       "     --flip <direction>       Flips the image. (h, v)\n"
	       "     --crop <size>[,<offset>] Crop a part of the image.\n"
	       "     --scale <size>           Scales the image.\n"
	       "     --rotate <angle>         Rotates the image in right angles.\n"
	       "     --deinterlace            Reduces interlace artifacts.\n"
	       "     --invert                 Inverts the images colours.\n"
	       "     --greyscale              Removes colour from the image.\n"
	       "     --no-banner              Hides the banner.\n"
	       "     --top-banner             Puts the banner at the top.\n"
	       "     --bottom-banner          Puts the banner at the bottom. (Default)\n"
	       "     --banner-colour <colour> Sets the banner colour. (#AARRGGBB)\n"
	       "     --line-colour <colour>   Sets the banner line colour.\n"
	       "     --text-colour <colour>   Sets the text colour.\n"
	       "     --font <[name][:size]>   Sets the font and/or size.\n"
	       "     --no-shadow              Disables the text shadow.\n"
	       "     --shadow                 Enables the text shadow.\n"
	       "     --title <text>           Sets the main title. (top left)\n"
	       "     --no-title               Clears the main title.\n"
	       "     --subtitle <text>        Sets the sub-title. (bottom left)\n"
	       "     --no-subtitle            Clears the sub-title.\n"
	       "     --timestamp <format>     Sets the timestamp format. (top right)\n"
	       "     --no-timestamp           Clears the timestamp.\n"
	       "     --gmt                    Use GMT instead of local timezone.\n"
	       "     --info <text>            Sets the info text. (bottom right)\n"
	       "     --no-info                Clears the info text.\n"
	       "     --underlay <PNG image>   Sets the underlay image.\n"
	       "     --no-underlay            Clears the underlay.\n"
	       "     --overlay <PNG image>    Sets the overlay image.\n"
	       "     --no-overlay             Clears the overlay.\n"
	       "     --jpeg <factor>          Outputs a JPEG image. (-1, 0 - 95)\n"
	       "     --png <factor>           Outputs a PNG image. (-1, 0 - 10)\n"
	       "     --save <filename>        Save image to file.\n"
	       "     --exec <command>         Execute a command and wait for it to complete.\n"
	       "\n");
	
	return(0);
}

int fswc_add_job(fswebcam_config_t *config, uint16_t id, char *options)
{
	fswebcam_job_t *job;
	void *n;
	
	job = malloc(sizeof(fswebcam_job_t));
	if(!job)
	{
		ERROR("Out of memory.");
		return(-1);
	}
	
	job->id = id;
	if(options) job->options = strdup(options);
	else job->options = NULL;
	
	/* Increase the size of the job queue. */
	n = realloc(config->job, sizeof(fswebcam_job_t *) * (config->jobs + 1));
	if(!n)
	{
		ERROR("Out of memory.");
		
		if(job->options) free(job->options);
		free(job);
		
		return(-1);
	}
	
	config->job = n;
	
	/* Add the new job to the queue. */
	config->job[config->jobs++] = job;
	
	return(0);
}

int fswc_free_jobs(fswebcam_config_t *config)
{
	int i;
	
	/* Scan through all the jobs and free the memory. */
	for(i = 0; i < config->jobs; i++)
	{
		if(config->job[i]->options) free(config->job[i]->options);
		free(config->job[i]);
	}
	
	/* Free the job queue, set settings to zero. */
	free(config->job);
	config->job = NULL;
	config->jobs = 0;
	
	return(0);
}

int fswc_set_option(fswebcam_config_t *config, char *option)
{
	char *name, *value;
	
	if(!option) return(-1);
	
	name = strdup(option);
	if(!name)
	{
		ERROR("Out of memory.");
		return(-1);
	}
	
	value = strchr(name, '=');
	if(value)
	{
		*(value++) = '\0';
		if(*value == '\0') value = NULL;
	}
	
	src_set_option(&config->option, name, value);
	
	free(name);
	
	return(0);
}

int fswc_getopt_file(fswc_getopt_t *s)
{
	char line[1024];
	char *arg, *val;
	struct option *opt;
	
	while(fgets(line, 1024, s->f))
	{
		s->line++;
		strtrim(line, WHITESPACE);
		arg = argdup(line, WHITESPACE, 0, 0);
		
		if(!arg) continue;
		if(*arg == '#') continue;
		
		/* Find argument in the list. */
		opt = (struct option *) s->long_opts;
		while(opt->name)
		{
			if(!strcasecmp(opt->name, arg)) break;
			opt++;
		}
		
		if(!opt->name)
		{
			ERROR("Unknown argument: %s", arg);
			WARN("%s,%i: %s", s->filename, s->line, line);
			free(arg);
			return(-2);
		}
		
		if(opt->val == 'c')
		{
			ERROR("You can't use config from a configuration file.");
			WARN("%s,%i: %s", s->filename, s->line, line);
			free(arg);
			return(-2);
		}
		
		if(opt->has_arg)
		{
			val = argdup(line, WHITESPACE, 1, 0);
			optarg = val;
		}
		
		free(arg);
		
		return(opt->val);
	}
	
	/* Have we reached the end of the file? */
	if(feof(s->f)) return(-1);
	
	/* No.. there has been an error. */
	ERROR("fread: %s", strerror(errno));
	
	return(-2);
}

int fswc_getopt(fswc_getopt_t *s, int argc, char *argv[])
{
	int c;
	
	/* If a conf file is opened, read from that. */
	if(s->f)
	{
		/* Read until we find an argument, error or EOF. */
		c = fswc_getopt_file(s);
		
		if(c < 0)
		{
			/* EOF or error. */
			fclose(s->f);
			s->f = NULL;
			
			if(c == -2) return('!');
		}
		else return(c);
	}
	
	/* Read from the command line. */
	c = getopt_long(argc, argv, s->opts, s->long_opts, &s->opt_index);
	
	return(c);
}

int fswc_getopts(fswebcam_config_t *config, int argc, char *argv[])
{
	int c;
	fswc_getopt_t s;
	static struct option long_opts[] =
	{
		{"help",            no_argument,       0, '?'},
		{"config",          required_argument, 0, 'c'},
		{"quiet",           no_argument,       0, 'q'},
		{"verbose",         no_argument,       0, 'v'},
		{"version",         no_argument,       0, OPT_VERSION},
		{"loop",            required_argument, 0, 'l'},
		{"offset",          required_argument, 0, OPT_OFFSET},
		{"background",      no_argument,       0, 'b'},
		{"pid",             required_argument, 0, OPT_PID},
		{"log",             required_argument, 0, 'L'},
		{"device",          required_argument, 0, 'd'},
		{"input",           required_argument, 0, 'i'},
		{"list-inputs",     no_argument,       0, OPT_LIST_INPUTS},
		{"tuner",           required_argument, 0, 't'},
		{"list-tuners",     no_argument,       0, OPT_LIST_TUNERS},
		{"frequency",       required_argument, 0, 'f'},
		{"delay",           required_argument, 0, 'D'},
		{"resolution",      required_argument, 0, 'r'},
		{"fps",	            required_argument, 0, OPT_FPS},
		{"list-framesizes", no_argument,       0, OPT_LIST_FRAMESIZES},
		{"list-framerates", no_argument,       0, OPT_LIST_FRAMERATES},
		{"frames",          required_argument, 0, 'F'},
		{"skip",            required_argument, 0, 'S'},
		{"palette",         required_argument, 0, 'p'},
		{"dumpframe",       required_argument, 0, OPT_DUMPFRAME},
		{"read",            no_argument,       0, 'R'},
		{"list-formats",    no_argument,       0, OPT_LIST_FORMATS},
		{"set",             required_argument, 0, 's'},
		{"list-controls",   no_argument,       0, OPT_LIST_CONTROLS},
		{"revert",          no_argument,       0, OPT_REVERT},
		{"flip",            required_argument, 0, OPT_FLIP},
		{"crop",            required_argument, 0, OPT_CROP},
		{"scale",           required_argument, 0, OPT_SCALE},
		{"rotate",          required_argument, 0, OPT_ROTATE},
		{"deinterlace",     no_argument,       0, OPT_DEINTERLACE},
		{"invert",          no_argument,       0, OPT_INVERT},
		{"greyscale",       no_argument,       0, OPT_GREYSCALE},
		{"no-banner",       no_argument,       0, OPT_NO_BANNER},
		{"top-banner",      no_argument,       0, OPT_TOP_BANNER},
		{"bottom-banner",   no_argument,       0, OPT_BOTTOM_BANNER},
		{"banner-colour",   required_argument, 0, OPT_BG_COLOUR},
		{"line-colour",     required_argument, 0, OPT_BL_COLOUR},
		{"text-colour",     required_argument, 0, OPT_FG_COLOUR},
		{"font",            required_argument, 0, OPT_FONT},
		{"no-shadow",       no_argument,       0, OPT_NO_SHADOW},
		{"shadow",          no_argument,       0, OPT_SHADOW},
		{"title",           required_argument, 0, OPT_TITLE},
		{"no-title",        no_argument,       0, OPT_NO_TITLE},
		{"subtitle",        required_argument, 0, OPT_SUBTITLE},
		{"no-subtitle",     no_argument,       0, OPT_NO_SUBTITLE},
		{"timestamp",       required_argument, 0, OPT_TIMESTAMP},
		{"no-timestamp",    no_argument,       0, OPT_NO_TIMESTAMP},
		{"gmt",             no_argument,       0, OPT_GMT},
		{"info",            required_argument, 0, OPT_INFO},
		{"no-info",         no_argument,       0, OPT_NO_INFO},
		{"underlay",        required_argument, 0, OPT_UNDERLAY},
		{"no-underlay",     no_argument,       0, OPT_NO_UNDERLAY},
		{"overlay",         required_argument, 0, OPT_OVERLAY},
		{"no-overlay",      no_argument,       0, OPT_NO_OVERLAY},
		{"jpeg",            required_argument, 0, OPT_JPEG},
		{"png",             required_argument, 0, OPT_PNG},
		{"save",            required_argument, 0, OPT_SAVE},
		{"exec",            required_argument, 0, OPT_EXEC},
		{0, 0, 0, 0}
	};
	char *opts = "-qc:vl:bL:d:i:t:f:D:r:F:s:S:p:R";
	
	s.opts      = opts;
	s.long_opts = long_opts;
	s.opt_index = 0;
	s.filename  = NULL;
	s.f         = NULL;
	s.line      = 0;
	
	/* Set the defaults. */
	config->loop = 0;
	config->offset = 0;
	config->background = 0;
	config->pidfile = NULL;
	config->logfile = NULL;
	config->gmt = 0;
	config->start = 0;
	config->device = strdup("/dev/video0");
	config->input = NULL;
	config->tuner = 0;
	config->frequency = 0;
	config->delay = 0;
	config->use_read = 0;
	config->list = 0;
	config->width = 384;
	config->height = 288;
	config->fps = 0;
	config->frames = 1;
	config->skipframes = 0;
	config->palette = SRC_PAL_ANY;
	config->option = NULL;
	config->dumpframe = NULL;
	config->jobs = 0;
	config->job = NULL;
	
	/* Don't report errors. */
	opterr = 0;
	
	/* Reset getopt to ensure parsing begins at the first argument. */
	optind = 0;
	
	/* Parse the command line and any config files. */
	while((c = fswc_getopt(&s, argc, argv)) != -1)
	{
		switch(c)
		{
		case '?': fswc_usage(); /* Command line error. */
		case '!': return(-1);   /* Conf file error. */
		case 'c':
			INFO("Reading configuration from '%s'...", optarg);
			s.f = fopen(optarg, "rt");
			if(!s.f)
			{
				ERROR("fopen: %s", strerror(errno));
				return(-1);
			}
			s.line = 0;
			free(s.filename);
			s.filename = strdup(optarg);
			break;
		case OPT_VERSION:
			fprintf(stderr, "fswebcam %s\n", PACKAGE_VERSION);
			return(-1);
		case 'q':
			log_quiet(-1);
			log_verbose(0);
			break;
		case 'v':
			log_quiet(0);
			log_verbose(-1);
			break;
		case 'l':
			config->loop = atol(optarg);
			break;
		case OPT_OFFSET:
			config->offset = atol(optarg);
			break;
		case 'b':
			config->background = -1;
			break;
		case OPT_PID:
			if(config->pidfile) free(config->pidfile);
			config->pidfile = strdup(optarg);
			break;
		case 'L':
			if(config->logfile) free(config->logfile);
			config->logfile = strdup(optarg);
			break;
		case 'd':
			if(config->device) free(config->device);
			config->device = strdup(optarg);
			break;
		case 'i':
			if(config->input) free(config->input);
			config->input = strdup(optarg);
			break;
		case OPT_LIST_INPUTS:
			config->list |= SRC_LIST_INPUTS;
			break;
		case 't':
			config->tuner = atoi(optarg);
			break;
		case OPT_LIST_TUNERS:
			config->list |= SRC_LIST_TUNERS;
			break;
		case 'f':
			config->frequency = atof(optarg) * 1000;
			break;
		case 'D':
			config->delay = atoi(optarg);
			break;
		case 'r':
	 		config->width  = argtol(optarg, "x ", 0, 0, 10);
			config->height = argtol(optarg, "x ", 1, 0, 10);
			break;
		case OPT_FPS:
			config->fps = atoi(optarg);
			break;
		case 'F':
			config->frames = atoi(optarg);
			break;
		case 'S':
			config->skipframes = atoi(optarg);
			break;
		case 's':
			fswc_set_option(config, optarg);
			break;
		case OPT_LIST_CONTROLS:
			config->list |= SRC_LIST_CONTROLS;
			break;
		case 'p':
			config->palette = fswc_find_palette(optarg);
			if(config->palette == -1) return(-1);
			break;
		case 'R':
			config->use_read = -1;
			break;
		case OPT_LIST_FORMATS:
			config->list |= SRC_LIST_FORMATS;
			break;
		case OPT_GMT:
			config->gmt = -1;
			break;
		case OPT_DUMPFRAME:
			free(config->dumpframe);
			config->dumpframe = strdup(optarg);
			break;
		default:
			/* All other options are added to the job queue. */
			fswc_add_job(config, c, optarg);
			break;
		}
	}
	
	/* Do a sanity check on the options. */
	if(config->frequency < 0)       config->frequency = 0;
	if(config->width < 1)           config->width = 1;
	if(config->height < 1)          config->height = 1;
	if(config->frames < 1)          config->frames = 1;
	if(config->frames > MAX_FRAMES)
	{
		WARN("Requested %u frames, maximum is %u. Using that.",
		   config->frames, MAX_FRAMES);
		
		config->frames = MAX_FRAMES;
	}
	
	/* Correct offset if negative or out of range. */
	if(config->offset && (config->offset %= (signed long) config->loop) < 0)
		config->offset += config->loop;
	
	/* Free the config filename if set. */
	free(s.filename);
	
	return(0);
}

int fswc_free_config(fswebcam_config_t *config)
{
	free(config->pidfile);
	free(config->logfile);
	free(config->device);
	free(config->input);
	
	free(config->dumpframe);
        free(config->title);
	free(config->subtitle);
	free(config->timestamp);
	free(config->info);
	free(config->font);
	free(config->underlay);
	free(config->overlay);
	free(config->filename);
	
	src_free_options(&config->option);
	fswc_free_jobs(config);
	
	memset(config, 0, sizeof(fswebcam_config_t));
	
	return(0);
}

int main(int argc, char *argv[])
{
	fswebcam_config_t *config;
	
	/* Prepare the configuration structure. */
	config = calloc(sizeof(fswebcam_config_t), 1);
	if(!config)
	{
		WARN("Out of memory.");
		return(-1);
	}
	
	/* Set defaults and parse the command line. */
	if(fswc_getopts(config, argc, argv)) return(-1);
	
	/* Open the log file if one was specified. */
	if(config->logfile && fswc_openlog(config)) return(-1);
	
	/* Go into the background if requested. */
	if(config->background && fswc_background(config)) return(-1);
	
	/* Save PID of requested. */
	if(config->pidfile && fswc_savepid(config)) return(-1);
	
	/* Setup signal handlers. */
	if(fswc_setup_signals()) return(-1);
	
	/* Capture the image(s). */
	if(!config->loop) fswc_grab(config);
	else
	{
		/* Loop mode ... keep capturing images until terminated. */
		while(1 == 1)
		{
			time_t capturetime = time(NULL);
			char timestamp[32];
			
			/* Calculate when the next image is due. */
			capturetime -= (capturetime % config->loop);
			capturetime += config->loop + config->offset;
			
			/* Correct the capturetime if the offset pushes
			 * it to far into the future. */
			if(capturetime - time(NULL) > config->loop)
				capturetime -= config->loop;
			
			fswc_strftime(timestamp, 32, "%Y-%m-%d %H:%M:%S (%Z)",
			              capturetime, config->gmt);
			
			MSG(">>> Next image due: %s", timestamp);
			
			/* Wait until that time comes. */
			while(time(NULL) < capturetime)
			{
				usleep(250000);
				if(received_sighup)
				{
					/* Reload configuration. */
					MSG("Received HUP signal... reloading configuration.");
					fswc_free_config(config);
					fswc_getopts(config, argc, argv);
					
					/* Clear hup signal. */
					received_sighup = 0;
				}
				
				if(received_sigusr1)
				{
					MSG("Received USR1 signal... Capturing image.");
					break;
				}
				
				if(received_sigterm)
				{
					MSG("Received TERM signal... exiting.");
					break;
				}
			}
			
			if(received_sigterm) break;
			
			/* Clear usr1 signal. */
			received_sigusr1 = 0;
			
			/* Capture the image. */
			fswc_grab(config);
		}
	}
	
	/* Close the log file. */
	if(config->logfile) log_close();
	
	/* Free all used memory. */
	fswc_free_config(config);
	free(config);
	
	return(0);
}

