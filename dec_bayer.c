/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2010 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include "fswebcam.h"
#include "src.h"

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

