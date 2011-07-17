/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2011 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifndef INC_FSWC_H
#define INC_FSWC_H

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Define the bitmap type */
#ifdef USE_32BIT_BUFFER

typedef uint32_t avgbmp_t;
#define MAX_FRAMES (UINT32_MAX >> 8)

#else

typedef uint16_t avgbmp_t;
#define MAX_FRAMES (UINT16_MAX >> 8)

#endif
/*----*/

#define CLIP(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

#endif
