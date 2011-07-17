/* fswebcam - FireStorm.cx's webcam generator                 */
/*============================================================*/
/* Copyright (C)2005-2011 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifndef INC_EFFECTS_H
#define INC_EFFECTS_H

extern gdImage *fx_flip(gdImage *src, char *options);
extern gdImage *fx_crop(gdImage *src, char *options);
extern gdImage *fx_scale(gdImage *src, char *options);
extern gdImage *fx_rotate(gdImage *src, char *options);
extern gdImage *fx_deinterlace(gdImage *src, char *options);
extern gdImage *fx_invert(gdImage *src, char *options);
extern gdImage *fx_blur(gdImage *src, char *options);
extern gdImage *fx_greyscale(gdImage *src, char *options);
extern gdImage *fx_swapchannels(gdImage *src, char *options);

#endif

