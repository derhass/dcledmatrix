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

#include "dclm_image.h"

/****************************************************************************
 * Creation, Destruction                                                    *
 ****************************************************************************/ 

extern DCLMImage *
dclmImageCreate(size_t width, size_t height, void *dataptr)
{
	DCLMImage *img;
	size_t size;

	size=(dataptr)?sizeof(*img):sizeof(*img)+width*height;

	img=malloc(size);
	if (img) {
		img->dims[0]=width;
		img->dims[1]=height;
		img->size=width*height;
		img->data=(dataptr)?(uint8_t*)dataptr:(uint8_t*)(img+1);
	}

	return img;
}

extern void
dclmImageDestroy(DCLMImage *img)
{
	free(img);
}

/****************************************************************************
 * Access                                                                   *
 ****************************************************************************/ 

extern void
dclmImageFill(DCLMImage *img, uint8_t value)
{
	size_t i;

	for (i=0; i<img->size; i++)
		img->data[i]=value;
}

extern void
dclmImageClear(DCLMImage *img)
{
	dclmImageFill(img, 0);
}

extern void
dclmImageInvert(DCLMImage *img)
{
	size_t i;

	for (i=0; i<img->size; i++)
		img->data[i]=~img->data[i];
}

extern int
dclmImageIsPixelValid(const DCLMImage *img, size_t x, size_t y)
{
	if (!img || !img->data || x < 0 || y < 0 || x >= img->dims[0] || y >= img->dims[1])
		return 0;
	return 1;
}
		
extern void
dclmImageSetPixel(DCLMImage *img, size_t x, size_t y, uint8_t value)
{
	if (!dclmImageIsPixelValid(img,x,y))
		return;
	*DCLM_IMG_PIXEL(img,x,y)=value;
}

extern uint8_t
dclmImageGetPixel(const DCLMImage *img, size_t x, size_t y)
{
	if (!dclmImageIsPixelValid(img,x,y))
		return 0;
	return *DCLM_IMG_PIXEL(img,x,y);
}

