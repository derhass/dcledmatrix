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

