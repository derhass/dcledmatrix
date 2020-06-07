#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "dclm.h"
#include "dclm_internal.h"

/****************************************************************************
 * ERROR HANDLER                                                            *
 ****************************************************************************/ 

static DCLEDMatrixError
dclmError(DCLEDMatrix *dclm, DCLEDMatrixError err, const char *template, ...)
{
	va_list args;

	if (dclm) {
		dclm->error_state=err;
	}

	fprintf(stderr,"dclm: error %d encountered: ",err);
	va_start(args, template);
	vfprintf(stderr, template, args);
	va_end(args);
	fputc('\n',stderr);

	return err;
}

/****************************************************************************
 * LIBHIDAPI STUFF                                                          *
 ****************************************************************************/ 

static DCLEDMatrixError
dclmOpenHID(DCLEDMatrix *dclm)
{
	if (!dclm) {
		return dclmError(NULL, DCLM_NO_CONTEXT, "OpenUSB");
	}
	if (dclm->flags & DCLM_OPEN) {
		return 	dclmError(dclm, DCLM_ALREADY_OPEN, "OpenUSB");
	}

	if (hid_init()) {
		return dclmError(dclm, DCLM_FAILED_HIDAPI, "initialize libhidapi");
	}

	dclm->dev = hid_open(dclm->idVendor, dclm->idProduct, NULL);
	if (!dclm->dev) {
		hid_exit();
		return DCLM_HID_OPEN_FAILED;
	}

	/* sucessfully openend the device */
	dclm->flags |= DCLM_OPEN;
	return DCLM_OK;
}

static DCLEDMatrixError
dclmCloseHIDInternal(DCLEDMatrix *dclm)
{
	DCLEDMatrixError err=DCLM_OK;
	if (dclm->dev) {
		hid_close(dclm->dev);
		dclm->dev=NULL;
	}

	hid_exit();

	dclm->flags &= ~DCLM_OPEN;
	return err;
}

static DCLEDMatrixError
dclmCloseHID(DCLEDMatrix *dclm)
{
	if (!dclm) {
		return dclmError(NULL, DCLM_NO_CONTEXT, "CloseUSB");
	}
	if (!(dclm->flags & DCLM_OPEN)) {
		return 	dclmError(dclm, DCLM_NOT_OPEN, "CloseUSB");
	}

	return dclmCloseHIDInternal(dclm);
}

static DCLEDMatrixError
dclmSendReport(DCLEDMatrix *dclm, const uint8_t *buffer, int size)
{
	int len;

	assert(dclm);
	assert(dclm->flags & DCLM_OPEN);
	assert(dclm->dev);
	assert(size <= 4096);

	len=hid_write(dclm->dev, buffer, size);

	if (len != size) {
		return dclmError(dclm,DCLM_FAILED_REPORT,"failed to send USB HID report packet");
	}
	return DCLM_OK;
}

static DCLEDMatrixError
dclmSendScreenHID(DCLEDMatrix *dclm, const DCLEDMatrixScreen *scr)
{
	DCLEDMatrixError err = DCLM_OK;
	int i;

	assert(dclm);
	assert(scr);
	assert(dclm == scr->dclm);

	for (i=0; i<DCLM_DATA_ROWS; i++) {
		DCLEDMatrixError res=dclmSendReport(dclm,&scr->data[i][0],DCLM_DATA_COLS);
		if (res) {
 			err=res;
		}
	}

	return err;
}

/****************************************************************************
 * DCLEDMatrixScreen                                                        *
 ****************************************************************************/ 

static void
dclmScrInit(DCLEDMatrixScreen *scr)
{
	int i,j;

	for (i=0; i<DCLM_DATA_ROWS; i++) {
		scr->data[i][1]=(uint8_t)(i*2);
		for (j=2; j<DCLM_DATA_COLS; j++) {
			scr->data[i][j]=0xff;
		}
	}
	dclmScrSetBrightness(scr,0);
}

extern DCLEDMatrixScreen *
dclmScrCreate(DCLEDMatrix *dclm)
{
	DCLEDMatrixScreen *scr;

	if (!dclm) {
		dclmError(NULL, DCLM_NO_CONTEXT, "ScrCreate");
		return NULL;
	}

	if (!(dclm->flags & DCLM_OPEN)) {
		dclmError(dclm, DCLM_NOT_OPEN,"ScrCreate");
		return NULL;	
	}

	scr=malloc(sizeof(*scr));
	if (!scr) {
		dclmError(dclm, DCLM_OUT_OF_MEMORY,"ScrCreate");
		return NULL;	
	}

	scr->dclm=dclm;
	dclmScrInit(scr);
	return scr;
}

extern void
dclmScrDestroy(DCLEDMatrixScreen *scr)
{
	if (scr) {
		free(scr);
	}
}

extern void
dclmScrSetBrightness(DCLEDMatrixScreen *scr, int brightness)
{
	int i;
	uint8_t b;

	/* convert range 0 ... max_brightness to 
	 * max_brightness ... 0. like the HW expects it */
	if (brightness >= scr->dclm->max_brightness) {
		b=1; /* maximum brightness */
	} else if (brightness < 1) {
		b=scr->dclm->max_brightness; /* minimum brightness */
	} else {
		b=(uint8_t)(scr->dclm->max_brightness - brightness);
	}

	/* TODO: brightness doesn't work, use value 2*/
	b = 2;
	for (i=0; i<DCLM_DATA_ROWS; i++) {
		scr->data[i][0]=b;
	}
}

extern void
dclmScrClear(DCLEDMatrixScreen *scr, int value)
{
	int i;
	uint8_t b = (value)?0x00:0xff;
	for (i=0; i<DCLM_DATA_ROWS; i++) {
		memset(&scr->data[i][2], b, (DCLM_DATA_COLS - 2));
	}
}

extern void
dclmScrSetPixel(DCLEDMatrixScreen *scr, unsigned int x, unsigned int y, int value)
{
	uint8_t *data;
	unsigned int bit;

	if (y >= (unsigned)scr->dclm->rows) {
		return;
	}

	data=&scr->data[0][2] + DCLM_DATA_COLS * (y >> 1) + 3 * (y & 1);
	if (x >= (unsigned)scr->dclm->cols) {
		return;
	}

	data += (DCLM_DATA_COLS - 2)/2 - 1 - (x>>3);

	bit=(x & 0x07);
	if (value == 0) {
		/* clear LED */
		data[0] |= (1<<bit);
	} else if (value == 1) {
		/* set LED */
		data[0] &= ~(1<<bit);
	} else {
		/* toggle LED */
	}
}

extern DCLMImage *
dclmImageCreateFit(const DCLEDMatrix *dclm)
{
	if (!dclm || dclm->cols < 1 || dclm->rows<1)
		return NULL;

	return dclmImageCreate(dclm->cols, dclm->rows, NULL);
}

static void
scr_set_row(uint8_t *scrdata, const uint8_t *imgdata)
{
	int byte=(DCLM_DATA_COLS-2)/2-1;
	int bit=0;
	int col;
	uint8_t mask;

	for (col=0; col < DCLM_COLS; col++) {
		mask=1<<bit;
		scrdata[byte]=(~mask & scrdata[byte]) | (mask & ( (~imgdata[col]) >> (7-bit)) );
		byte-=(++bit)>>3;
		bit &= 7;
	}
}

extern void
dclmScrFromImg(DCLEDMatrixScreen *scr, const DCLMImage *img)
{
	int row;
	uint8_t *scrdata;
	const uint8_t *imgdata;

	assert(scr);
	assert(scr->dclm);
	assert(img);
	assert(img->data);
	assert(img->dims[0] >= (size_t)scr->dclm->cols);
	assert(img->dims[1] >= (size_t)scr->dclm->rows);

	scrdata=&scr->data[0][2];
	imgdata=img->data;
	for (row=0; row < DCLM_ROWS; row++) {
		scr_set_row(scrdata,imgdata);
		scrdata+= (DCLM_DATA_COLS-2)/2 + ((row & 1)<<1);
		imgdata+=img->dims[0];
	}
}

static void
scr_get_row(const uint8_t *scrdata, uint8_t *imgdata)
{
	int byte=(DCLM_DATA_COLS-2)/2-1;
	int bit=0;
	int col;
	uint8_t mask;

	for (col=0; col < DCLM_COLS; col++) {
		mask=1<<bit;
		imgdata[col]=(scrdata[byte] & mask)?0x00:0xff;
		byte-=(++bit)>>3;
		bit &= 7;
	}
}

extern void
dclmScrToiImg(const DCLEDMatrixScreen *scr, DCLMImage *img)
{
	int row;
	const uint8_t *scrdata;
	uint8_t *imgdata;

	assert(scr);
	assert(scr->dclm);
	assert(img);
	assert(img->data);
	assert(img->dims[0] >= (size_t)scr->dclm->cols);
	assert(img->dims[1] >= (size_t)scr->dclm->rows);

	scrdata=&scr->data[0][2];
	imgdata=img->data;
	for (row=0; row < DCLM_ROWS; row++) {
		scr_get_row(scrdata,imgdata);
		scrdata+= (DCLM_DATA_COLS-2)/2 + ((row & 1)<<1);
		imgdata+=img->dims[0];
	}
}

static void
scr_set_row_wrap(uint8_t *scrdata, const uint8_t *imgdata, int to_x, int w, const uint8_t *img_end, size_t width)
{
	int byte=(DCLM_DATA_COLS-2)/2-1 - (to_x>>3);
	int bit=to_x & 0x7;
	int col;
	uint8_t mask;
	const uint8_t *iptr;

	assert(to_x + w <= DCLM_COLS);

	iptr=imgdata;

	for (col=0; col < w; col++) {
		int wrap;
		mask=1<<bit;
		scrdata[byte]=(~mask & scrdata[byte]) | (mask & ( (~*iptr) >> (7-bit)) );
		byte-=(++bit)>>3;
		bit &= 7;
		wrap=((++iptr)==img_end);
		iptr += wrap * (-width); 
	}
}

extern void
dclmScrFromImgBlit(DCLEDMatrixScreen *scr, const DCLMImage *img,
                   size_t from_x, size_t from_y,
                   int to_x, int to_y, int w, int h)
{
	const DCLEDMatrix *dclm;
	uint8_t *scrdata;
	const uint8_t *imgdata;
	int row;
	int wrap;
	size_t fx,fy;

	assert(scr && scr->dclm && img);

	dclm=scr->dclm;

	/* clip it */
	if (to_x < 0) {
		w += to_x;
		to_x = 0;
	}
	if (to_y < 0) {
		h += to_y;
		to_y=0;
	}
	if ( to_x + w > dclm->cols ) {
		w=dclm->cols - to_x;
	}
	if ( to_y + h > dclm->rows ) {
		h=dclm->rows - to_y;
	}

	if (w < 1 || h < 1) {
		/* completey outside */
		return;
	}

	assert(img->dims[0] > 0 && img->dims[1] > 0);

	scrdata=&scr->data[to_y/2][2] + (to_y & 1) * ((DCLM_DATA_COLS-2)/2);

	fx=from_x % img->dims[0];
	fy=from_y % img->dims[1];
	imgdata=DCLM_IMG_PIXEL(img, fx, fy);
	wrap=dclm->cols-fx;
	
	for (row=0; row<h; row++) {
		scr_set_row_wrap(scrdata, imgdata, to_x, w, imgdata + wrap, img->dims[0]);
		scrdata+= (DCLM_DATA_COLS-2)/2 + (((row + to_y)& 1)<<1);
		if (++fy == img->dims[1]) {
			/* wrap around y */
			imgdata=DCLM_IMG_PIXEL(img, fx, fy=0);
		} else {
			imgdata+=img->dims[0];
		}
	}
}

/****************************************************************************
 * MANAGEMENT OF THE DCLEDMatrix struct                                     *
 ****************************************************************************/ 

static void
dclmInit(DCLEDMatrix *dclm)
{
	dclm->error_state=DCLM_OK;

	dclm->flags=DCLM_FLAGS_DEFAULT;
	dclm->idVendor=DCLM_VENDOR_ID;
	dclm->idProduct=DCLM_PRODUCT_ID;

	dclm->dev=NULL;

	dclm->rows=DCLM_ROWS;
	dclm->cols=DCLM_COLS;
	dclm->max_brightness=DCLM_MAX_BRIGHTNESS;

	dclm->scr_off=NULL;
}

static void
dclmCleanup(DCLEDMatrix *dclm)
{
	if (dclm) {
		dclmScrDestroy(dclm->scr_off);
		dclmCloseHIDInternal(dclm);
	}
}

static DCLEDMatrix *
dclmCreate(void)
{
	DCLEDMatrix *dclm;

	dclm=malloc(sizeof(*dclm));
	if (dclm) {
		dclmInit(dclm);
	} else {
		dclmError(NULL, DCLM_OUT_OF_MEMORY,"Create");
	}

	return dclm;
}

static void
dclmDestroy(DCLEDMatrix *dclm)
{
	if (dclm) {
		dclmCleanup(dclm);
		free(dclm);	
	}
}

/****************************************************************************
 * EXTERNAL API                                                             *
 ****************************************************************************/ 

extern DCLEDMatrix *
dclmOpen(const char *options)
{
	DCLEDMatrix *dclm;

	(void)options;

	dclm=dclmCreate();
	if (dclm) {
		dclmOpenHID(dclm);
		dclmScrDestroy(dclm->scr_off);
		dclm->scr_off=dclmScrCreate(dclm);	
	}
	return dclm;
}

extern DCLEDMatrixError
dclmGetError(const DCLEDMatrix *dclm)
{
	if (dclm) {
		return dclm->error_state;
	}
	dclmError(NULL, DCLM_NO_CONTEXT, "GetError");
	return DCLM_NO_CONTEXT;
}

extern int
dclmGetInt(const DCLEDMatrix *dclm, DCLEDMatrixParam param)
{
	if (!dclm)
		return -1;

	switch(param) {
		case DCLM_PARAM_ROWS:
			return dclm->rows;
		case DCLM_PARAM_COLUMNS:
			return dclm->cols;
	}

	return -1;
}

extern DCLEDMatrixError
dclmSendScreen(DCLEDMatrixScreen *scr)
{
	DCLEDMatrix *dclm;

	if (!scr) {
		return dclmError(NULL, DCLM_NO_SCREEN, "SendScreen");
	}

	dclm=scr->dclm;

	if (!dclm) {
		return dclmError(NULL, DCLM_NO_CONTEXT, "SendScreen");
	}

	if (!(dclm->flags) & DCLM_OPEN) {
		return dclmError(NULL, DCLM_NOT_OPEN, "SendScreen");
	}

	return dclmSendScreenHID(dclm, scr);
}

extern DCLEDMatrixError
dclmBlankScreen(DCLEDMatrix *dclm)
{
	if (!dclm) {
		return dclmError(NULL, DCLM_NO_CONTEXT, "BlankScreen");
	}
	return dclmSendScreen(dclm->scr_off);
}

extern DCLEDMatrixError 
dclmClose(DCLEDMatrix *dclm)
{
	DCLEDMatrixError err;
	if (dclm) {
		err=dclmCloseHID(dclm);
		dclmDestroy(dclm);
		return err;
	}
	return dclmError(NULL, DCLM_NO_CONTEXT, "Close");
}
