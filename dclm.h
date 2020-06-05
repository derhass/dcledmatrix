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

