#ifndef DCLM_H
#define DCLM_H

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * DATA TYPES                                                               *
 ****************************************************************************/ 

/* status codes */
typedef enum {
	DCLM_OK=0,
	DCLM_OUT_OF_MEMORY,
	DCLM_NO_CONTEXT,
	DCLM_NO_SCREEN,
	DCLM_USB_ERROR,
	DCLM_NOT_OPEN,
	DCLM_ALREADY_OPEN,
	DCLM_NO_DEVICE,
	DCLM_USB_OPEN_FAILED,
	DCLM_INVALID_CONFIG,
	DCLM_INVALID_INTERFACE,
	DCLM_FAILED_CONFIG,
	DCLM_FAILED_CLAIM,
	DCLM_FAILED_DETACH,
	DCLM_FAILED_RELEASE,
	DCLM_FAILED_CTRL_MSG	
} DCLEDMatrixError;

/* abstract data types */
typedef struct DCLEDMatrix_s DCLEDMatrix;
typedef struct DCLEDMatrixScreen_s DCLEDMatrixScreen;

/****************************************************************************
 * DCLEDMatrixScreen                                                        *
 ****************************************************************************/ 

extern void
dclmScrSetBrightness(DCLEDMatrixScreen *scr, int brightness);

extern DCLEDMatrixScreen *
dclmScrCreate(DCLEDMatrix *dclm);

extern void
dclmScrDestroy(DCLEDMatrixScreen *scr);

extern void
dclmScrSetPixel(DCLEDMatrixScreen *scr, unsigned int x, unsigned int y, int value);

/****************************************************************************
 * DCLM API                                                                 *
 ****************************************************************************/ 

extern DCLEDMatrix *
dclmOpen(const char *options);

extern DCLEDMatrixError
dclmGetError(const DCLEDMatrix *dclm);

extern DCLEDMatrixError
dclmSendScreen(DCLEDMatrixScreen *scr);

extern DCLEDMatrixError 
dclmClose(DCLEDMatrix *dclm);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLM_H */

