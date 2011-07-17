/* fswebcam - Small and simple webcam for *nix                */
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
#include <gd.h>
#include "fswebcam.h"
#include "src.h"

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

