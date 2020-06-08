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

#ifndef DCLM_ERROR_H
#define DCLM_ERROR_H

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
	DCLM_NOT_OPEN,
	DCLM_ALREADY_OPEN,
	DCLM_NO_DEVICE,
	DCLM_HID_OPEN_FAILED,
	DCLM_FAILED_HIDAPI,
	DCLM_FAILED_REPORT,
	DCLM_INVALID_CONFIG,

	/* DLCMD error codes */
	DCLMD_NOT_CONNECTED=0x1000,
	DCLMD_COMMUNICATION_ERROR,
	DCLMD_COMMUNICATION_TIMEOUT,
} DCLEDMatrixError;

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLM_ERROR_H */

