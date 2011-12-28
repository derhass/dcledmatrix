#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <usb.h>

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
	usb_dev_handle *dev;
	int ifnum;
	int rows;
	int cols;
	int max_brightness;
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
dclmInitUSB(void)
{
	/* TODO MH: are we allowed to call this twice or more times?
	 * if not, we could use some reference counting her... */
	usb_init();

	return DCLM_OK;
}

static DCLEDMatrixError
dclmDeinitUSB(void)
{
	(void)0;
	return DCLM_OK;
}

static int
isDCLMDevice(DCLEDMatrix *dclm, struct usb_device *dev)
{
	if (dev) {
		if ( (dev->descriptor.idVendor == dclm->idVendor) &&
		     (dev->descriptor.idProduct == dclm->idProduct)) {
   			return 1;
		}
	}

	/* not the device we are looking for */
	return 0;
}

static DCLEDMatrixError
dclmDetachUSB(DCLEDMatrix *dclm, int ifnum)
{
	int try;

	/* TODO MH: this is linux specific */
	for (try=0; try < dclm->retry_detach; try++) {
		if (usb_detach_kernel_driver_np(dclm->dev,ifnum)) {
			/* TODO MH: error code, sleep? */
			printf("DETACH FAILED!\n");
		} else {
			printf("DETACH OK!\n");
			return DCLM_OK;
		}
	}

	return DCLM_FAILED_DETACH;
}

static DCLEDMatrixError
dclmUseUSBDeviceConfig(DCLEDMatrix *dclm, struct usb_device *dev, int config_id, int if_id)
{
	struct usb_config_descriptor *conf;
	struct usb_interface_descriptor *usb_if;
	DCLEDMatrixError err;
	int detach=0;

	if (dev->descriptor.bNumConfigurations <= config_id) {
		return DCLM_INVALID_CONFIG;
	}

	conf=&dev->config[config_id];

	if (conf->bNumInterfaces <= if_id) {
		return DCLM_INVALID_INTERFACE;
	}

	usb_if=conf->interface[if_id].altsetting;

	do {
		if (detach>0) {
			if ( (err=dclmDetachUSB(dclm, usb_if->bInterfaceNumber)) ) {
				return err;
			}
		}
		detach++;

		/* try to set the configuration */
		if (usb_set_configuration(dclm->dev,conf->bConfigurationValue)) {
			if (detach) {
				return DCLM_FAILED_CONFIG;
			}
			/* retry after detaching the if */
			continue;
		}

		/* try to claim the interface */
		if (usb_claim_interface(dclm->dev,usb_if->bInterfaceNumber)) {
			if (detach) {
				return DCLM_FAILED_CLAIM;
			}
			/* retry after detaching the if */
			continue;
		} else {
			break;
		}
	} while (detach < 2);

	dclm->ifnum=(int)usb_if->bInterfaceNumber;
	return DCLM_OK;
}

static DCLEDMatrixError
dclmUseUSBDevice(DCLEDMatrix *dclm, struct usb_device *dev)
{
	DCLEDMatrixError err;

	dclm->dev=usb_open(dev);

	if (!dclm->dev) {
		return DCLM_USB_OPEN_FAILED;
	}

	/* use first interface of first device */
	err=dclmUseUSBDeviceConfig(dclm, dev, 0, 0);

	/* TODO MH: close usb device in error case? */
	return err;
}

static DCLEDMatrixError
dclmOpenUSBDevice(DCLEDMatrix *dclm)
{
	struct usb_bus *busses, *bus;
	struct usb_device *dev;
	DCLEDMatrixError last_err=DCLM_OK;
	int found=0;

	usb_find_busses();
	usb_find_devices();

	busses=usb_get_busses();
	if (!busses) {
		return dclmError(dclm, DCLM_USB_ERROR, "get busses");
	}

	for (bus=busses; bus; bus=bus->next) {
		for (dev=bus->devices; dev; dev=dev->next) {
			/* printf("0x%x 0x%x 0x%x %s\n",dev->descriptor.idVendor, dev->descriptor.idProduct, dev->descriptor.bDeviceClass, dev->filename); */
			if (isDCLMDevice(dclm, dev)) {
				found++;

				if ( (last_err=dclmUseUSBDevice(dclm,dev)) == DCLM_OK) {
					return DCLM_OK;
				}
				/* otherwise: search for another device
				 * which we might be able to use
				 */
			}
		}
	}
	
	if (found) {
		return dclmError(dclm,last_err,"failed to open USB device");
	}
	return dclmError(dclm,DCLM_NO_DEVICE,"no DCLEDMatrix device found");
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

	if ( (err=dclmInitUSB()) ) {
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
			if (usb_release_interface(dclm->dev, dclm->ifnum)) {
				err=DCLM_FAILED_RELEASE;
			}
		}
		usb_close(dclm->dev);
		dclm->dev=NULL;
	}

	dclmDeinitUSB();

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
	len=usb_control_msg(dclm->dev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
	                    DCLM_REPORT_SEND, 0 | (DCLM_RT_OUTPUT  << 8),
			    dclm->ifnum, (char*)buffer, size, timeout);

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

#if 0
extern void
dclmScrFromImg(DCLEDMatrixScreen *scr, const DCLMImage *img,
	       size_t from_x, size_t from_y, int w, int h,
	       int to_x, int to_y)
{
	DCLEDMatrix *dclm;
	uint8_t *scrdata;
	const uint8_t *imgdata;
	int row;

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

	scrdata=&scr->data[to_y/2][2] + (to_y & 1) * 3;
	imgdata=DCLM_IMG_PIXEL(img, from_x % img->dims[0], from_y % img->dims[1]);
	
	for (row=0; row<h; row++) {
		int col;
		for (col=to_x; col <to_x+w; col++) {
			static const
		}
	}





}
#endif

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

	dclm->dev=NULL;
	dclm->ifnum=-1;

	dclm->rows=DCLM_ROWS;
	dclm->cols=DCLM_COLS;
	dclm->max_brightness=DCLM_MAX_BRIGHTNESS;
}

static void
dclmCleanup(DCLEDMatrix *dclm)
{
	if (dclm) {
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
