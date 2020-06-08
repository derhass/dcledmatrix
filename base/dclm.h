/*
 * Copyright (C) 2011 - 2020 by derhass <derhass@arcor.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DCLM_H
#define DCLM_H

#include "dclm_error.h"
#include "dclm_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * DATA TYPES                                                               *
 ****************************************************************************/ 

/* parameters */
typedef enum {
	DCLM_PARAM_ROWS=0,
	DCLM_PARAM_COLUMNS
} DCLEDMatrixParam;

/* abstract data types */
typedef struct DCLEDMatrix_s DCLEDMatrix;
typedef struct DCLEDMatrixScreen_s DCLEDMatrixScreen;

/****************************************************************************
 * DCLEDMatrixScreen                                                        *
 ****************************************************************************/ 

extern DCLEDMatrixScreen *
dclmScrCreate(DCLEDMatrix *dclm);

extern void
dclmScrDestroy(DCLEDMatrixScreen *scr);

extern void
dclmScrSetBrightness(DCLEDMatrixScreen *scr, int brightness);

extern void
dclmScrClear(DCLEDMatrixScreen *scr, int value);

extern void
dclmScrSetPixel(DCLEDMatrixScreen *scr, unsigned int x, unsigned int y, int value);

extern DCLMImage *
dclmImageCreateFit(const DCLEDMatrix *dclm);

extern void
dclmScrFromImg(DCLEDMatrixScreen *scr, const DCLMImage *img);

extern void
dclmScrToiImg(const DCLEDMatrixScreen *scr, DCLMImage *img);

extern void
dclmScrFromImgBlit(DCLEDMatrixScreen *scr, const DCLMImage *img,
                   size_t from_x, size_t from_y,
                   int to_x, int to_y, int w, int h);

/****************************************************************************
 * DCLM API                                                                 *
 ****************************************************************************/ 

extern DCLEDMatrix *
dclmOpen(const char *options);

extern DCLEDMatrixError
dclmGetError(const DCLEDMatrix *dclm);

extern int
dclmGetInt(const DCLEDMatrix *dclm, DCLEDMatrixParam param);

extern DCLEDMatrixError
dclmSendScreen(DCLEDMatrixScreen *scr);

extern DCLEDMatrixError
dclmBlankScreen(DCLEDMatrix *dclm);

extern DCLEDMatrixError 
dclmClose(DCLEDMatrix *dclm);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLM_H */

