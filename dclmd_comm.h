#ifndef DCLMD_COMM_H
#define DCLMD_COMM_H

#include "dclm_error.h"
#include "dclm_image.h"
#include <semaphore.h>

#define DCLMD_COMM_MAX_TEXT_LENGTH	255

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * DATA TYPES                                                               *
 ****************************************************************************/ 

/* A single command to the daemon */
typedef enum {
	DCLMD_CMD_NONE=0,
	DCLMD_CMD_SHOW_IMAGE,
	DCLMD_CMD_SHOW_TEXT,
	DCLMD_CMD_BLANK,	/* go to blank screen, stop refreshing */
	DCLMD_CMD_STOP,		/* stop refreshing */
	DCLMD_CMD_EXIT		/* exit thread */
} DCLMDCommand;

/* A single unit of work for the daemon.
 * This is a command plus additional data,
 * kept in the shm. 
 * The size of the image must not be changed!
 */
typedef struct {
	size_t dims[2]; /* dimensions of the led matrix, client should only read this */
	DCLMDCommand cmd;
	char text[DCLMD_COMM_MAX_TEXT_LENGTH+1];
	int pos_x;
	int pos_y;
} DCLMDWorkEntry;

typedef struct {
	sem_t *sem_mutex;
	sem_t *sem_command;
	int shm_fd;
	size_t shm_size;
	unsigned flags;
	unsigned clientTimeout;  /* in ms */
	unsigned recreateTimeout; /* in ms */
	DCLMDWorkEntry *work;  /* in shm */ 
	DCLMImage img;         /* pointer in shm */
} DCLMDComminucation;

#define DCLMD_FLAG_DAEMON	0x1	/* is daemon side */	

/****************************************************************************
 * FUNCTIONS FOR THE COMMUNICATION INTERFACE                                *
 ****************************************************************************/ 

/* Create the communication interface.
 * If daemon is true: create the deamon side,
 * otherwise create the client side.
 * RETURN: pointer to newly alloced structure,
 *         NULL on error
 */ 
extern DCLMDComminucation *
dclmdCommunicationCreate(int deamon);

/* Destroy the communication interface */
extern void
dclmdCommunicationDestroy(DCLMDComminucation *comm);

/* Lock the communication interface as client.
 * If this returns successfully, you can write
 * to the work entry and MUST call
 * dclmdClientUnlock() afterwards as soon
 * as possible!
 * RETURN: DCLM_OK if successfull, error code otherwise
 */
extern DCLEDMatrixError
dclmdClientLock(DCLMDComminucation *comm);

/* Unlock the communication interface as client.
 * If this returns successfully, the daemon should
 * carry out the command. On error, the command
 * is most likely ignored.
 */
extern DCLEDMatrixError
dclmdClientUnlock(DCLMDComminucation *comm);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLMD_COMM_H */

