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

#include "dclmd_comm.h"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/****************************************************************************
 * INTERNAL: SHARED OBJECT NAMES                                            *
 ****************************************************************************/

#define DCLMD_COMM_SHARED_PREFIX "/dclmd"
#define DCLMD_COMM_SHARED_NAME_LEN 32

static void
dclmdCommNameSem(char *str, size_t len, unsigned int idx)
{
	snprintf(str, len, "%s-sem%u", DCLMD_COMM_SHARED_PREFIX, idx);
}

static void
dclmdCommNameShm(char *str, size_t len)
{
	snprintf(str, len, "%s-shm", DCLMD_COMM_SHARED_PREFIX);
}

/****************************************************************************
 * TIMING HELPERS                                                           *
 ****************************************************************************/ 

/* calculate the time in ms milliseconds from now
 * if now is NULL, get the current time internally */
extern void
dclmdCalcWaitTimeMS(struct timespec *ts, const struct timespec *now, unsigned int ms)
{
	struct timespec intnow;
	long part_ms=(long)(ms%1000);
	if (!now) {
		clock_gettime(CLOCK_REALTIME, &intnow);
		now=&intnow;
	}
	ts->tv_sec=now->tv_sec + ms/1000;
	ts->tv_nsec=now->tv_nsec + part_ms*1000000L;
	if (ts->tv_nsec >= 1000000000) {
		ts->tv_sec++;
		ts->tv_nsec-=1000000000;
	}
}

/* compare two timespecs
 * RETURN -1: a before b
 *         0: a == b
 *         1: a after b
 */
extern int
dclmdCompareTime(const struct timespec *a, const struct timespec *b)
{
	if (a->tv_sec < b->tv_sec) {
		return -1;
	}
	if (a->tv_sec > b->tv_sec) {
		return 1;
	}
	if (a->tv_nsec < b->tv_nsec) {
		return -1;
	}
	if (a->tv_nsec > b->tv_nsec) {
		return 1;
	}
	return 0;
}

/****************************************************************************
 * SEMAPHORE HELPERS                                                        *
 ****************************************************************************/

static sem_t *
dclmdSemOpen(unsigned int idx)
{
	char name[DCLMD_COMM_SHARED_NAME_LEN];
	sem_t *sem;

	dclmdCommNameSem(name, sizeof(name), idx);
	sem = sem_open(name, O_RDWR);
	return sem;
}

static sem_t *
dclmdSemCreate(unsigned int idx, unsigned int initial)
{
	char name[DCLMD_COMM_SHARED_NAME_LEN];
	sem_t *sem;

	dclmdCommNameSem(name, sizeof(name), idx);
	sem = sem_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, initial);
	return sem;
}

static void
dclmdSemUnlink(unsigned int idx)
{
	char name[DCLMD_COMM_SHARED_NAME_LEN];

	dclmdCommNameSem(name, sizeof(name), idx);
	sem_unlink(name);
}

static int
dclmdSemWait(sem_t *sem)
{
	do {
		if (sem_wait(sem)) {
			if (errno != EINTR) {
				/* failed */
				break;
			}
			/* in EINTR case, just repeat */
		} else {
			/* success */
			return 0;
		}
	} while (1);

	return -1;
}

extern int
dclmdSemTryWait(sem_t *sem)
{
	do {
		if (sem_trywait(sem)) {
			if (errno == EAGAIN) {
				return 1;
			} else if (errno != EINTR) {
				/* failed */
				break;
			}
			/* in EINTR case, just repeat */
		} else {
			/* success */
			return 0;
		}
	} while (1);

	return -1;
}

static int
dclmdSemTimedWait(sem_t *sem, const struct timespec *ts)
{
	do {
		if (sem_timedwait(sem,ts)) {
			if (errno == ETIMEDOUT) {
				return 1;
			} else if (errno != EINTR) {
				/* failed */
				break;
			} 
			/* in EINTR case, just repeat */
		} else {
			/* success */
			return 0;
		}
	} while (1);

	return -1;
}

extern int
dclmdSemTimedWaitMS(sem_t *sem, unsigned int ms)
{
	struct timespec ts;
	dclmdCalcWaitTimeMS(&ts, NULL, ms);

	return dclmdSemTimedWait(sem, &ts);
}

/****************************************************************************
 * INTERNAL: THE COMMUNICATION INTERFACE                                    *
 ****************************************************************************/

/* initialize a DCLMDComminucation structure */
static void
dclmdCommInit(DCLMDComminucation *comm)
{
	comm->sem_mutex = NULL;
	comm->sem_command = NULL;
	comm->shm_fd = -1;
	comm->shm_size = 0;
	comm->work = NULL;
	comm->flags = 0;
	comm->img.dims[0] = 0;
	comm->img.dims[1] = 0;
	comm->img.size = 0;
	comm->img.data = NULL;

	comm->clientTimeout = 200;
	comm->recreateTimeout = 1000;
}

/* get the communication image pointer from the shm
 * RETURN: 0: OK
 *        -1: inconsistent with shm size
 */
static int
dclmdCommGetImg(DCLMDComminucation *comm)
{
	size_t size = comm->work->dims[0] * comm->work->dims[1];
	if (size + sizeof(*comm->work) > comm->shm_size) {
		return -1;
	}

	comm->img.dims[0] = comm->work->dims[0];
	comm->img.dims[1] = comm->work->dims[1];
	comm->img.size = size;
	comm->img.data = ((uint8_t*)comm->work) + sizeof(*comm->work);
	return 0;
}

/* Initialize the daemon end of the communication interface
 * RETURN 0: OK
 *       -1: error
 */
static int
dclmdCommInitDaemon(DCLMDComminucation *comm, int dims_x, int dims_y)
{
	int res;

	comm->work->hdr_size = sizeof(*comm->work);
	comm->work->hdr_version = DCLMD_COMM_VERSION;
	comm->work->dims[0] = dims_x;
	comm->work->dims[1] = dims_y;

	comm->work->cmd_flags = 0;
	comm->work->img_pos_x = 0;
	comm->work->img_pos_y = 0;
	comm->work->text_pos_x = 0;
	comm->work->text[0] = 0;

	if (dclmdCommGetImg(comm)) {
		return -1;
	}

	/* lock semaphore: make sure we count to 0 */
	do {
		res = dclmdSemTryWait(comm->sem_command);
	} while (res == 0);
	if (res < 0) {
		return -1;
	}

	/* unlock the semaphore */
	if (sem_post(comm->sem_mutex)) {
		return -1;
	}

	return 0;
}

/* Initialize the client end of the communication interface
 * RETURN 0: OK
 *       -1: error
 */
static int
dclmdCommInitClient(DCLMDComminucation *comm)
{
	int res = dclmdSemTimedWaitMS(comm->sem_mutex, comm->clientTimeout);
	if (res) {
		return -1;
	}

	if (comm->work->hdr_size != sizeof(*comm->work)) {
		/* incompatible version */
		return -1;
	}

	if (comm->work->hdr_version != DCLMD_COMM_VERSION) {
		/* incompatible version */
		return -1;
	}

	res = dclmdCommGetImg(comm);


	/* unlock the semaphore */
	if (sem_post(comm->sem_mutex)) {
		return -1;
	}

	return res;
}

/* open shared communication interface
 * RETURN: 0: OK
 *        -1: semaphore error
 *        -2: shm error
 */
static int
dclmdCommOpen(DCLMDComminucation *comm, int as_daemon, int dims_x, int dims_y)
{
	char name[DCLMD_COMM_SHARED_NAME_LEN];
	int res;
	dclmdCommNameShm(name, sizeof(name));

	if (as_daemon) {
		comm->flags |= DCLMD_FLAG_DAEMON;

		if (dims_x < 1 || dims_y < 1) {
			return -1;
		}

		comm->shm_size = sizeof(*comm->work) + dims_x * dims_y;
		comm->sem_mutex = dclmdSemCreate(0, 1);
		comm->sem_command = dclmdSemCreate(1, 0);
		if (!comm->sem_mutex || !comm->sem_command) {
			return -1;
		}

		/* lock semaphore,
		 * it the timeout expires, we consider the client to be dead
		 * and ignore its lock! */
		res = dclmdSemTimedWaitMS(comm->sem_mutex, comm->recreateTimeout);
		if (res < 0) {
			return -1;
		}

		/* lock semaphore: make sure we count to 0 */
		do {
			res = dclmdSemTryWait(comm->sem_mutex);
		} while (res == 0);
		if (res < 0) {
			return -1;
		}

		comm->shm_fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (comm->shm_fd >= 0) {
			if (ftruncate(comm->shm_fd, (off_t)comm->shm_size) < 0) {
				comm->shm_size = 0;
			}
		}
	} else {
		comm->shm_fd = shm_open(name, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (comm->shm_fd >= 0) {
			struct stat s;
			if (fstat(comm->shm_fd,&s) >= 0) {
				comm->shm_size = (size_t)s.st_size;
			}
		}
		comm->sem_mutex = dclmdSemOpen(0);
		comm->sem_command = dclmdSemOpen(1);

		if (!comm->sem_mutex || !comm->sem_command) {
			return -1;
		}
	}

	if (comm->shm_fd < 0 || !comm->shm_size) {
		return -2;
	}

	comm->work = mmap(NULL, (off_t)comm->shm_size, PROT_READ|PROT_WRITE, MAP_SHARED, comm->shm_fd,0);
	if (comm->work == MAP_FAILED) {
		comm->work = NULL;
	}
	if (!comm->work) {
		return -2;
	}

	if (as_daemon) {
		return dclmdCommInitDaemon(comm, dims_x, dims_y);
	} else {
		return dclmdCommInitClient(comm);
	}
}

/****************************************************************************
 * EXTERNAL API: THE COMMUNICATION INTERFACE (client and daemon)            *
 ****************************************************************************/

/* Create the communication interface.
 * If as_daemon is true: create the daemon side,
 * otherwise create the client side.
 * RETURN: pointer to newly alloced structure,
 *         NULL on error
 */ 
extern DCLMDComminucation *
dclmdCommunicationCreate(int as_daemon, int dims_x, int dims_y)
{
	DCLMDComminucation *comm = malloc(sizeof(*comm));
	if (!comm) {
		return NULL;
	}

	dclmdCommInit(comm);

	if (dclmdCommOpen(comm, as_daemon, dims_x, dims_y)) {
		dclmdCommunicationDestroy(comm);
		comm=NULL;
	}

	return comm;
}

/* Destroy the communication interface */
extern void
dclmdCommunicationDestroy(DCLMDComminucation *comm)
{
	if (!comm) {
		return;
	}

	if (comm->flags & DCLMD_FLAG_DAEMON) {
		if (comm->shm_fd) {
			char name[DCLMD_COMM_SHARED_NAME_LEN];
			dclmdCommNameShm(name, sizeof(name));
			shm_unlink(name);
		}
		if (comm->sem_mutex) {
			dclmdSemUnlink(0);
		}
		if (comm->sem_command) {
			dclmdSemUnlink(1);
		}
	}

	if (comm->sem_mutex) {
		sem_close(comm->sem_mutex);
		comm->sem_mutex = NULL;
	}
	if (comm->sem_command) {
		sem_close(comm->sem_command);
		comm->sem_command = NULL;
	}
	if (comm->work) {
		munmap(comm->work, comm->shm_size);
		comm->work = NULL;
	}
	if (comm->shm_fd >= 0) {
		close(comm->shm_fd);
		comm->shm_fd = -1;
	}

	free(comm);
}

/****************************************************************************
 * EXTERNAL API: THE COMMUNICATION INTERFACE (client side)                  *
 ****************************************************************************/

/* Create the communication interface as client.
 * RETURN: pointer to newly alloced structure,
 *         NULL on error
 */ 
extern DCLMDComminucation *
dclmdCommunicationClientCreate(void)
{
	return dclmdCommunicationCreate(0,0,0);
}

/* Lock the communication interface as client.
 * If this returns successfully, you can write
 * to the work entry and MUST call
 * dclmdClientUnlock() afterwards as soon
 * as possible!
 * RETURN: DCLM_OK if successfull, error code otherwise
 */
extern DCLEDMatrixError
dclmdClientLock(DCLMDComminucation *comm)
{
	int res;

	if (!comm) {
		return DCLMD_NOT_CONNECTED;
	}

	res = dclmdSemTimedWaitMS(comm->sem_mutex, comm->clientTimeout);
	if (res < 0) {
		return DCLMD_COMMUNICATION_ERROR;
	} else if (res > 0) {
		return DCLMD_COMMUNICATION_TIMEOUT;
	}

	return 0;
}

/* Unlock the communication interface as client.
 * If this returns successfully, the daemon should
 * carry out the command. On error, the command
 * is most likely ignored.
 */
extern DCLEDMatrixError
dclmdClientUnlock(DCLMDComminucation *comm)
{
	int res;

	if (!comm) {
		return DCLMD_NOT_CONNECTED;
	}

	res = sem_post(comm->sem_mutex);
	if (res) {
		return DCLMD_COMMUNICATION_ERROR;
	}
	res = sem_post(comm->sem_command);
	if (res) {
		return DCLMD_COMMUNICATION_ERROR;
	}

	return 0;
}

/* Full cycle: Show text
 * if str is NULL: blank
 * If len is 0: use strlen
 * timeout_ms: 0 means infinite
 */
extern DCLEDMatrixError
dclmdClientShowText(DCLMDComminucation *comm, const char *str, size_t len, int pos_x,  unsigned int additional_flags, unsigned int timeout_ms)
{
	DCLEDMatrixError err;
	if ( (err = dclmdClientLock(comm) ) == DCLM_OK ) {
		DCLMDWorkEntry *work = comm->work;
		/* note: it is OK if work->text is not 0-terminated, dclmd takes care */
		if (len) {
			if (len >= sizeof(work->text)) {
				len = sizeof(work->text) - 1;
			}
			memcpy(work->text, str, len);
			work->text[len]=0;
		} else {
			strncpy(work->text, str, sizeof(work->text));
		}
		work->timeout_ms = timeout_ms;
		work->cmd_flags |= DCLMD_CMD_SHOW_TEXT | DCLMD_CMD_TIMEOUT | additional_flags;
		work->text_pos_x = pos_x;
		err = dclmdClientUnlock(comm);
	}
	return err;
}

/* Full cycle: Blank the screen
 */
extern DCLEDMatrixError
dclmdClientBlank(DCLMDComminucation *comm, unsigned int additional_flags)
{
	DCLEDMatrixError err;
	if ( (err = dclmdClientLock(comm) ) == DCLM_OK ) {
		DCLMDWorkEntry *work = comm->work;
		/* note: it is OK if work->text is not 0-terminated, dclmd takes care */
		work->cmd_flags = DCLMD_CMD_CLEAR_SCREEN | additional_flags;
		err = dclmdClientUnlock(comm);
	}
	return err;
}

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
dclmdDaemonGetCommand(DCLMDComminucation *comm, const struct timespec *wait_until, const struct timespec *now)
{
	struct timespec until;
	int res;

	if (wait_until) {
		res = dclmdSemTimedWait(comm->sem_command, wait_until);
	} else {
		res = dclmdSemWait(comm->sem_command);
	}

	if (res < 0) {
		return -1;
	}
	if (res > 0) {
		return 0;
	}

	/* count down semaphore to be sure */
	while(!dclmdSemTryWait(comm->sem_command));

	/* lock the mutex */
	dclmdCalcWaitTimeMS(&until, now, comm->clientTimeout);
	res = dclmdSemTimedWait(comm->sem_mutex, &until);
	if (res < 0) {
		return -1;
	}
	if (res > 0) {
		/* assume client is dead, unlock it again for the next client */
		if (sem_post(comm->sem_mutex)) {
			return -1;
		}
		return 0;
	}

	return 1;
}

/* Unlock the mutex from the daemon side
 * Must only be called after dclmdDaemonGetCommand() returned 1
 * RETURN 0: OK
 *       -1: error
 */
extern int
dclmdDaemonUnlock(DCLMDComminucation *comm)
{
	/* count down semaphore to be sure */
	while(!dclmdSemTryWait(comm->sem_mutex));

	return sem_post(comm->sem_mutex);
}

