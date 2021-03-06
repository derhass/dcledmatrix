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

#ifndef DCLM_INTERNAL_H
#define DCLM_INTERNAL_H

#include "dclm.h"
#include <hidapi/hidapi.h>

#ifdef __cplusplus
extern "C" {
#endif

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
#define DCLM_MAX_BRIGHTNESS 255

#define DCLM_DATA_ROWS 4
#define DCLM_DATA_COLS 8

/****************************************************************************
 * INTERNAL DATA TYPES                                                      *
 ****************************************************************************/ 

struct DCLEDMatrix_s {
	DCLEDMatrixError error_state;
	uint16_t idVendor;
	uint16_t idProduct;
	unsigned int flags;
	hid_device *dev;
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

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLM_INTERNAL_H */

