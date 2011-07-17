/* fswebcam - FireStorm.cx's webcam generator                 */
/*============================================================*/
/* Copyright (C)2005-2011 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gd.h>
#include "parse.h"
#include "log.h"

/* These helper macros should maybe be moved elsewhere. */

#define RGB(r, g, b) ((r << 16) + (g << 8) + b)

#define R(c) ((c & 0xFF0000) >> 16)
#define G(c) ((c & 0xFF00) >> 8)
#define B(c) (c & 0xFF)

#define GREY(c) ((R(c) + G(c) + B(c)) / 3)
#define MIX(a, b, c) (a + (((b - a) * c) / 0xFF))

#define RGBMIX(c1, c2, d) \
  (RGB(MIX(R(c1), R(c2), d), MIX(G(c1), G(c2), d), MIX(B(c1), G(c2), d)))

gdImage *fx_flip(gdImage *src, char *options)
{
	int i;
	char d[32];
	
	i = 0;
	while(!argncpy(d, 32, options, ", \t", i++, 0))
	{
		if(*d == 'v')
		{
			int y, h;
			gdImage *line;
			
			MSG("Flipping image vertically.");
			
			line = gdImageCreateTrueColor(gdImageSX(src), 1);
			h = gdImageSY(src) / 2;
			
			for(y = 0; y < h; y++)
			{
				/* Copy bottom line into buffer. */
				gdImageCopy(line, src,
				   0, 0,
				   0, gdImageSY(src) - y - 1,
				   gdImageSX(src), 1);
				
				/* Copy the top line onto the bottom. */
				gdImageCopy(src, src,
				   0, gdImageSY(src) - y - 1,
				   0, y,
				   gdImageSX(src), 1);
				
				/* Copy the buffer into the top. */
				gdImageCopy(src, line,
				   0, y,
				   0, 0,
				   gdImageSX(src), 1);
			}
			
			gdImageDestroy(line);
		}
		else if(*d == 'h')
		{
			int x, w;
			gdImage *line;
			
			MSG("Flipping image horizontally.");
			
			line = gdImageCreateTrueColor(1, gdImageSY(src));
			w = gdImageSX(src) / 2;
			
			for(x = 0; x < w; x++)
			{
				/* Copy right line into buffer. */
				gdImageCopy(line, src,
				   0, 0,
				   gdImageSX(src) - x - 1, 0,
				   1, gdImageSY(src));
				
				/* Copy the left line onto the right. */
				gdImageCopy(src, src,
				   gdImageSX(src) - x - 1, 0,
				   x, 0,
				   1, gdImageSY(src));
				
				/* Copy the buffer into the left. */
				gdImageCopy(src, line,
				   x, 0,
				   0, 0,
				   1, gdImageSY(src));
			}
			
			gdImageDestroy(line);
		}
		else WARN("Unknown flip direction: %s", d);
	}
	
	return(src);
}

gdImage *fx_crop(gdImage *src, char *options)
{
	char arg[32];
	int w, h, x, y;
	gdImage *im;
	
	if(argncpy(arg, 32, options, ", \t", 0, 0))
	{
		WARN("Invalid area to crop: %s", arg);
		return(src);
	}
	
	w = argtol(arg, "x ", 0, 0, 10);
	h = argtol(arg, "x ", 1, 0, 10);
	
	if(w < 0 || h < 0)
	{
		WARN("Invalid area to crop: %s", arg);
		return(src);
	}
	
	/* Make sure crop area resolution is smaller than the source image. */
	if(w > gdImageSX(src) ||
	   h > gdImageSY(src))
	{
		WARN("Crop area is larger than the image!");
		return(src);
	}
	
	/* Get the offset. */
	x = -1;
	y = -1;
	
	if(!argncpy(arg, 32, options, ", \t", 1, 0))
	{
		x = argtol(arg, "x ", 0, 0, 10);
		y = argtol(arg, "x ", 1, 0, 10);
	}
	
	if(x < 0 || y < 0)
	{
		/* By default crop the center of the image. */
		x = (gdImageSX(src) - w) / 2;
		y = (gdImageSY(src) - h) / 2;
	}
	
	MSG("Cropping image from %ix%i [offset: %ix%i] -> %ix%i.",
	    gdImageSX(src), gdImageSY(src), x, y, w, h);
	
	im = gdImageCreateTrueColor(w, h);
	if(!im)
	{
		WARN("Out of memory.");
		return(src);
	}
	
	gdImageCopy(im, src, 0, 0, x, y, w, h);
	
	gdImageDestroy(src);
	
	return(im);
}

gdImage *fx_scale(gdImage *src, char *options)
{
	int w, h;
	gdImage *im;
	
	w = argtol(options, "x ", 0, 0, 10);
	h = argtol(options, "x ", 1, 0, 10);
	
	if(w < 0 || h < 0)
	{
		WARN("Invalid resolution: %s", options);
		return(src);
	}
	
	MSG("Scaling image from %ix%i -> %ix%i.",
	    gdImageSX(src), gdImageSY(src), w, h);
	
	im = gdImageCreateTrueColor(w, h);
	if(!im)
	{
		WARN("Out of memory.");
		return(src);
	}
	
	gdImageCopyResampled(im, src, 0, 0, 0, 0,
	   w, h, gdImageSX(src), gdImageSY(src));
	
	gdImageDestroy(src);
	
	return(im);
}

gdImage *fx_rotate(gdImage *src, char *options)
{
	int x, y;
	gdImage *im;
	int angle = atoi(options);
	
	/* Restrict angle to 0-360 range. */
	if((angle %= 360) < 0) angle += 360;
	
	/* Round to nearest right angle. */
	x = (angle + 45) % 360;
	angle = x - (x % 90);
	
	/* Not rotating 0 degrees. */
	if(angle == 0)
	{
		MSG("Not rotating 0 degrees.");
		return(src);
	}
	
	/* 180 can be done with two flips. */
	if(angle == 180)
	{
		fx_flip(src, "h,v");
		return(src);
	}
	
	MSG("Rotating image %i degrees. %ix%i -> %ix%i.", angle,
	    gdImageSX(src), gdImageSY(src),
	    gdImageSY(src), gdImageSX(src));
	
	/* Create rotated image. */
	im = gdImageCreateTrueColor(gdImageSY(src), gdImageSX(src));
	if(!im)
	{
		WARN("Out of memory.");
		return(src);
	}
	
	for(y = 0; y < gdImageSY(src); y++)
		for(x = 0; x < gdImageSX(src); x++)
		{
			int c = gdImageGetPixel(src, x, y);
			
			if(angle == 90)
			{
				gdImageSetPixel(im, gdImageSX(im) - y - 1, x, c);
			}
			else
			{
				gdImageSetPixel(im, y, gdImageSY(im) - x - 1, c);
			}
		}
	
	gdImageDestroy(src);
	
	return(im);
}

/*
 * This is a very simple and experimental deinterlacer,
 * there is a lot of room for improvement here.
 *
 * (For example: Making it work)
 *
*/
gdImage *fx_deinterlace(gdImage *src, char *options)
{
	int x, y;
	
	MSG("Deinterlacing image.");
	
	for(y = 1; y < gdImageSY(src) - 1; y += 2)
	{
		for(x = 0; x < gdImageSX(src); x++)
		{
			int c, cu, cd, d;
			
			c  = gdImageGetPixel(src, x, y);
			cu = gdImageGetPixel(src, x, y - 1);
			cd = gdImageGetPixel(src, x, y + 1);
			
			/* Calculate the difference of the pixel (x,y) from
			 * the average of it's neighbours above and below. */
			d = 0xFF - abs(GREY(cu) - GREY(cd));
			d = (abs(GREY(cu) - (0xFF - GREY(c)) - GREY(cd)) * d) / 0xFF;
			
			c = RGBMIX(c, RGBMIX(cu, cd, 128), d);
			
			gdImageSetPixel(src, x, y, c);
		}
	}
	
	return(src);
}

gdImage *fx_invert(gdImage *src, char *options)
{
	int x, y;
	
	MSG("Inverting image.");
	
	for(y = 0; y < gdImageSY(src); y++)
		for(x = 0; x < gdImageSX(src); x++)
		{
			/* Overwrite the pixel with a negative of its value. */
			gdImageSetPixel(src, x, y,
			   0xFFFFFF - gdImageGetPixel(src, x, y));
		}
	
	return(src);
}

gdImage *fx_greyscale(gdImage *src, char *options)
{
	int x, y;
	
	MSG("Greyscaling image.");
	
	for(y = 0; y < gdImageSY(src); y++)
		for(x = 0; x < gdImageSX(src); x++)
		{
			uint8_t c = GREY(gdImageGetPixel(src, x, y));
			gdImageSetPixel(src, x, y, RGB(c, c, c));
		}
	
	return(src);
}

gdImage *fx_swapchannels(gdImage *src, char *options)
{
	int mode;
	int x, y;
	
	if(strlen(options) != 2)
	{
		WARN("You can only swap two channels: %s", options);
		return(src);
	}
	
	for(mode = 0, x = 0; x < 2; x++)
	{
		char c = toupper(options[x]);
		if(c == 'R') mode += 0;
		else if(c == 'G') mode += 1;
		else if(c == 'B') mode += 2;
		else mode += 4;
	}
	
	if(mode < 1 || mode > 3)
	{
		WARN("Cannot swap colour channels '%s'", options);
		return(src);
	}
	
	MSG("Swapping colour channels %c <> %c",
		toupper(options[0]), toupper(options[1]));
	
	for(y = 0; y < gdImageSY(src); y++)
		for(x = 0; x < gdImageSX(src); x++)
		{
			int c = gdImageGetPixel(src, x, y);
			
			if(mode == 1) c = RGB(G(c), R(c), B(c));
			else if(mode == 2) c = RGB(B(c), G(c), R(c));
			else if(mode == 3) c = RGB(R(c), B(c), G(c));
			
			gdImageSetPixel(src, x, y, c);
		}
	
	return(src);
}

