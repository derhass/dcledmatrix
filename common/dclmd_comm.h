#ifndef DCLMD_COMM_H
#define DCLMD_COMM_H

#include "dclm_error.h"
#include "dclm_image.h"
#include <semaphore.h>

#define DCLMD_COMM_MAX_TEXT_LENGTH	255
#define DCLMD_COMM_VERSION		1

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * DATA TYPES                                                               *
 ****************************************************************************/ 

/* A single unit of work for the daemon.
 * This is a command plus additional data,
 * kept in the shm. 
 * The size of the image must not be changed!
 */
typedef struct {
	size_t hdr_size;
	unsigned int hdr_version;
	size_t dims[2]; /* dimensions of the led matrix, client should only read this */
	unsigned int cmd_flags;
	char text[DCLMD_COMM_MAX_TEXT_LENGTH+1];
	int img_pos_x;
	int img_pos_y;
	int text_pos_x;
	int brightness;
	unsigned int timeout_ms;
} DCLMDWorkEntry;

/* commands to the deamon */
#define DCLMD_CMD_CLEAR_SCREEN	0x1
#define DCLMD_CMD_SHOW_IMAGE	0x2
#define DCLMD_CMD_SHOW_TEXT	0x4
#define DCLMD_CMD_START_REFRESH	0x8
#define DCLMD_CMD_STOP_REFRESH	0x10
#define DCLMD_CMD_BRIGHTNESS	0x20		/* DOESN'T WORK! */
#define DCLMD_CMD_TIMEOUT	0x40
#define DCLMD_CMD_EXIT		0x80000000

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
 * TIMING HELPERS                                                           *
 ****************************************************************************/ 

/* calculate the time in ms milliseconds from now
 * if now is NULL, get the current time internally */
extern void
dclmdCalcWaitTimeMS(struct timespec *ts, const struct timespec *now, unsigned int ms);

/* compare two timespecs
 * RETURN -1: a before b
 *         0: a == b
 *         1: a after b
 */
extern int
dclmdCompareTime(const struct timespec *a, const struct timespec *b);

/****************************************************************************
 * SEMAPHORE HELPERS                                                        *
 ****************************************************************************/

extern int
dclmdSemTryWait(sem_t *sem);

extern int
dclmdSemTimedWaitMS(sem_t *sem, unsigned int ms);

/****************************************************************************
 * FUNCTIONS FOR THE COMMUNICATION INTERFACE (client and daemon)            *
 ****************************************************************************/ 

/* Create the communication interface.
 * If daemon is true: create the deamon side,
 * in daemon mode, the size of the LED matrix must be specified!
 * otherwise create the client side.
 * RETURN: pointer to newly alloced structure,
 *         NULL on error
 */ 
extern DCLMDComminucation *
dclmdCommunicationCreate(int deamon, int dims_x, int dims_y);

/* Destroy the communication interface */
extern void
dclmdCommunicationDestroy(DCLMDComminucation *comm);

/****************************************************************************
 * EXTERNAL API: THE COMMUNICATION INTERFACE (client side)                  *
 ****************************************************************************/

/* Create the communication interface as client.
 * RETURN: pointer to newly alloced structure,
 *         NULL on error
 */ 
extern DCLMDComminucation *
dclmdCommunicationClientCreate(void);

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

/* Full cycle: Show text
 * if str is NULL: blank
 * If len is 0: use strlen
 * timeout_ms: 0 means infinite
 */
extern DCLEDMatrixError
dclmdClientShowText(DCLMDComminucation *comm, const char *str, size_t len, int pos_x,  unsigned int additional_flags, unsigned int timeout_ms);

/* Full cycle: Blank the screen
 */
extern DCLEDMatrixError
dclmdClientBlank(DCLMDComminucation *comm, unsigned int additional_flags);

/****************************************************************************
 * EXTERNAL API: THE COMMUNICATION INTERFACE (daemon side)                  *
 ****************************************************************************/

/* Wait for a command from the client,
 * if reached, lock the mutex
 * wait_until: if not NULL: timeout, otherwise: infinite
 * RETURN 1: got client command, mutex locked
 *        0: got no command, mutex not locked
 *       -1: error
 */
extern int
dclmdDaemonGetCommand(DCLMDComminucation *comm, const struct timespec *wait_until, const struct timespec *now);

#define DCLMD_TIMEOUT_INFINITE ((unsigned int)-1)

/* Unlock the mutex from the daemon side
 * Must only be called after dclmdDaemonGetCommand() returned 1
 * RETURN 0: OK
 *       -1: error
 */
extern int
dclmdDaemonUnlock(DCLMDComminucation *comm);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLMD_COMM_H */

