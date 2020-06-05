#ifndef DCLM_IMAGE_H
#define DCLM_IMAGE_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * DATA TYPES                                                               *
 ****************************************************************************/ 

typedef struct {
	size_t dims[2]; /* width and height in pixels */
	size_t size; /* total size in bytes == pixels */

	/* image data is stored as tightly packed byte array,
	 * we start at top left and go left-to-right, top-to-bottom
	 * NOTE: usually we use a single chunk of memory for this
	 *       header and the data */
	uint8_t *data; 
} DCLMImage;

/****************************************************************************
 * Creation, Destruction                                                    *
 ****************************************************************************/ 

extern DCLMImage *
dclmImageCreate(size_t width, size_t height, void *dataptr);

extern void
dclmImageDestroy(DCLMImage *img);

/****************************************************************************
 * Access                                                                   *
 ****************************************************************************/ 

#define DCLM_IMG_PIXEL(img,x,y) ((img)->data + (y) * (img)->dims[0] + (x))

extern void
dclmImageFill(DCLMImage *img, uint8_t value);

extern void
dclmImageClear(DCLMImage *img);

extern void
dclmImageInvert(DCLMImage *img);

extern int
dclmImageIsPixelValid(const DCLMImage *img, size_t x, size_t y);
		
extern void
dclmImageSetPixel(DCLMImage *img, size_t x, size_t y, uint8_t value);

extern uint8_t
dclmImageGetPixel(const DCLMImage *img, size_t x, size_t y);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLM_IMAGE_H */

