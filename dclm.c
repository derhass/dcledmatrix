#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <libusb-1.0/libusb.h>

#include "dclm.h"

/****************************************************************************
 * CONFIGURATION DEFAULTS AND CONSTANTS                                     *
 ****************************************************************************/ 

#define DCLM_VENDOR_ID 0x1d34
#define DCLM_PRODUCT_ID 0x0013
#define DCLM_RETRY_DETACH 5

#define DCLM_REPORT_SEND 0x09
#define DCLM_RT_OUTPUT 0x02

#define DCLM_ROWS 7
#define DCLM_COLS 21
#define DCLM_MAX_BRIGHTNESS 2

#define DCLM_DATA_ROWS 4
#define DCLM_DATA_COLS 8

/****************************************************************************
 * INTERNAL DATA TYPES                                                      *
 ****************************************************************************/ 

struct DCLEDMatrix_s {
	DCLEDMatrixError error_state;
	uint16_t idVendor;
	uint16_t idProduct;
	int retry_detach;
	unsigned int flags;
	libusb_context *ctx;
	libusb_device_handle *dev;
	int ifnum;
	int rows;
	int cols;
	int max_brightness;
	struct DCLEDMatrixScreen_s *scr_off;
};

struct DCLEDMatrixScreen_s {
	uint8_t data[DCLM_DATA_ROWS][DCLM_DATA_COLS];
	DCLEDMatrix *dclm;
};

/* flags */
#define DCLM_OPEN	0x1 /* device is opened */
#define DCLM_FLAGS_DEFAULT 0

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
 * LIBUSB STUFF                                                             *
 ****************************************************************************/ 

static DCLEDMatrixError
dclmInitUSB(DCLEDMatrix *dclm)
{
	/* TODO MH: are we allowed to call this twice or more times?
	 * if not, we could use some reference counting her... */
	libusb_init(&dclm->ctx);

	return DCLM_OK;
}

static DCLEDMatrixError
dclmDeinitUSB(DCLEDMatrix *dclm)
{
	libusb_exit(dclm->ctx);
	dclm->ctx=NULL;;
	return DCLM_OK;
}

static int
isDCLMDevice(DCLEDMatrix *dclm, struct libusb_device_descriptor *dev)
{
	if (dev) {
		if ( (dev->idVendor == dclm->idVendor) &&
		     (dev->idProduct == dclm->idProduct)) {
   			return 1;
		}
	}

	/* not the device we are looking for */
	return 0;
}

static DCLEDMatrixError
dclmUseUSBDeviceConfig(DCLEDMatrix *dclm, libusb_device *dev, struct libusb_device_descriptor *desc, int config_id, int if_id)
{
	const struct libusb_interface *usb_if;
	struct libusb_config_descriptor *conf;
	int code = 0;
	int config;
	int interface;

	if (desc->bNumConfigurations <= config_id) {
		return DCLM_INVALID_CONFIG;
	}
	code = libusb_get_config_descriptor(dev, config_id, &conf);
	if (code < 0) {
		return DCLM_FAILED_CONFIG;
	}
	config=conf->bConfigurationValue;


	if (conf->bNumInterfaces <= if_id) {
		libusb_free_config_descriptor(conf);
		return DCLM_INVALID_INTERFACE;
	}
	usb_if=&conf->interface[if_id];
	if (usb_if->num_altsetting < 1) {
		libusb_free_config_descriptor(conf);
		return DCLM_INVALID_INTERFACE;
	}

	interface=usb_if->altsetting[0].bInterfaceNumber;


	libusb_free_config_descriptor(conf);

	code = libusb_set_configuration(dclm->dev, config);
	if (code < 0) {
		return DCLM_FAILED_CONFIG;
	}
	code = libusb_claim_interface(dclm->dev, interface);
	if (code < 0) {
		libusb_set_configuration(dclm->dev, -1);
		return DCLM_FAILED_CLAIM;
	}

	dclm->ifnum=interface;
	return DCLM_OK;
}

static DCLEDMatrixError
dclmUseUSBDevice(DCLEDMatrix *dclm, struct libusb_device *dev, struct libusb_device_descriptor *desc)
{
	DCLEDMatrixError err;

	if (libusb_open(dev,&dclm->dev)<0) {
		return DCLM_USB_OPEN_FAILED;
	}
	
	libusb_set_auto_detach_kernel_driver(dclm->dev, 1);

	/* use first interface of first device */
	err=dclmUseUSBDeviceConfig(dclm, dev, desc, 0, 0);

	if (err) {
		libusb_close(dclm->dev);
	}
	return err;
}

static DCLEDMatrixError
dclmOpenUSBDevice(DCLEDMatrix *dclm)
{
	struct libusb_device_descriptor desc;
	libusb_device **devList;
	ssize_t cnt,i;
	DCLEDMatrixError last_err=DCLM_OK;

	cnt = libusb_get_device_list(dclm->ctx, &devList);
	if (cnt < 0) {
		return dclmError(dclm, DCLM_USB_ERROR, "get device list");
	}

	for (i=0; i<cnt; i++) {
		if (libusb_get_device_descriptor(devList[i], &desc) >= 0) {
			if (!isDCLMDevice(dclm,&desc)) {
				continue;
			}
			if ( (last_err=dclmUseUSBDevice(dclm,devList[i],&desc)) == DCLM_OK) {
				break;
			}
		}
	}

	libusb_free_device_list(devList, 1);
	return last_err;
}

static DCLEDMatrixError
dclmOpenUSB(DCLEDMatrix *dclm)
{
	DCLEDMatrixError err;

	if (!dclm) {
		return dclmError(NULL, DCLM_NO_CONTEXT, "OpenUSB");
	}
	if (dclm->flags & DCLM_OPEN) {
		return 	dclmError(dclm, DCLM_ALREADY_OPEN, "OpenUSB");
	}

	if ( (err=dclmInitUSB(dclm)) ) {
		return dclmError(dclm, err, "initialize libusb");
	}
	
	if ( (err=dclmOpenUSBDevice(dclm)) ) {
		return err;	
	}

	/* sucessfully openend the device */
	dclm->flags |= DCLM_OPEN;
	return DCLM_OK;
}

static DCLEDMatrixError
dclmCloseUSBInternal(DCLEDMatrix *dclm)
{
	DCLEDMatrixError err=DCLM_OK;
	if (dclm->dev) {
		if (dclm->ifnum >= 0) {
			if (libusb_release_interface(dclm->dev, dclm->ifnum)) {
				err=DCLM_FAILED_RELEASE;
			}
		}
		libusb_set_configuration(dclm->dev, -1);
		libusb_close(dclm->dev);
		dclm->dev=NULL;
	}

	dclmDeinitUSB(dclm);

	dclm->flags &= ~DCLM_OPEN;
	return err;
}

static DCLEDMatrixError
dclmCloseUSB(DCLEDMatrix *dclm)
{
	if (!dclm) {
		return dclmError(NULL, DCLM_NO_CONTEXT, "CloseUSB");
	}
	if (!(dclm->flags & DCLM_OPEN)) {
		return 	dclmError(dclm, DCLM_NOT_OPEN, "CloseUSB");
	}

	return dclmCloseUSBInternal(dclm);
}

static DCLEDMatrixError
dclmSendReport(DCLEDMatrix *dclm, const uint8_t *buffer, int size)
{
	int len;
	int timeout;

	assert(dclm);
	assert(dclm->flags & DCLM_OPEN);
	assert(dclm->dev);
	assert(dclm->ifnum >= 0);
	assert(size <= 4096);

	/* TODO MH: adapt timeout to len */
	timeout=8*size; /* in wort case, we should 6.25 ms per byte, so
			  seems like a save default and is fast enough to calculate */
	/* TODO MH: we simply ignore the report ID for now */
	len=libusb_control_transfer(
		dclm->dev, 
		LIBUSB_RECIPIENT_INTERFACE | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_ENDPOINT_OUT,
	        DCLM_REPORT_SEND, 
		0 | (DCLM_RT_OUTPUT  << 8),
		dclm->ifnum, 
		(unsigned char*)buffer, 
		size,
		timeout);

	if (len != size) {
		dclmError(dclm,DCLM_FAILED_CTRL_MSG,"failed to send USB HID report packet");
	}
	return DCLM_OK;
}

static DCLEDMatrixError
dclmSendScreenUSB(DCLEDMatrix *dclm, const DCLEDMatrixScreen *scr)
{
	int fail=0;
	int i;

	assert(dclm);
	assert(scr);
	assert(dclm == scr->dclm);

	for (i=0; i<DCLM_DATA_ROWS; i++) {
		fail+=dclmSendReport(dclm,&scr->data[i][0],DCLM_DATA_COLS);
	}

	return fail?DCLM_FAILED_CTRL_MSG:DCLM_OK;
}

/****************************************************************************
 * DCLEDMatrixScreen                                                        *
 ****************************************************************************/ 

extern void
dclmScrSetBrightness(DCLEDMatrixScreen *scr, int brightness)
{
	int i;
	uint8_t b;

	/* convert range 0 ... max_brightness to 
	 * max_brightness ... 0. like the HW expects it */
	if (brightness > scr->dclm->max_brightness) {
		b=0; /* maximum brightness */
	} else if (brightness < 0) {
		b=scr->dclm->max_brightness; /* minimum brightness */
	} else {
		b=(uint8_t)(scr->dclm->max_brightness - brightness);
	}


	for (i=0; i<DCLM_DATA_ROWS; i++) {
		scr->data[i][0]=b;
	}
}

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
dclmScrSetPixel(DCLEDMatrixScreen *scr, unsigned int x, unsigned int y, int value)
{
	uint8_t *data;
	unsigned int bit;

	if (y >= scr->dclm->rows) {
		return;
	}

	data=&scr->data[0][2] + DCLM_DATA_COLS * (y >> 1) + 3 * (y & 1);
	if (x >= scr->dclm->cols) {
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
	DCLEDMatrix *dclm;
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
	dclm->retry_detach=DCLM_RETRY_DETACH;

	dclm->ctx=NULL;
	dclm->dev=NULL;
	dclm->ifnum=-1;

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
		dclmCloseUSBInternal(dclm);
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
		dclmOpenUSB(dclm);
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

	return dclmSendScreenUSB(dclm, scr);
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
		err=dclmCloseUSB(dclm);
		dclmDestroy(dclm);
		return err;
	}
	return dclmError(NULL, DCLM_NO_CONTEXT, "Close");
}
