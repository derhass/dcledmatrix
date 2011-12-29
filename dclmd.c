#include "dclm.h"

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define DCLMD_DEFAULT_REFRESH_MS 250 /* maximum time between LED matrix refresh */

/****************************************************************************
 * ERRORS and DIAGNOSTICS                                                   *
 ****************************************************************************/ 

static void
dclmdWarning(const char *template, ...)
{
	va_list args;

	fprintf(stderr,"warning: ");
	va_start(args, template);
	vfprintf(stderr, template, args);
	va_end(args);
	fputc('\n',stderr);
}

#ifdef NDEBUG
#define dclmdDebug(a,...) ((void)0)
#else
static void
dclmdDebug(const char *template, ...)
{
	va_list args;

	fprintf(stderr,"debug: ");
	va_start(args, template);
	vfprintf(stderr, template, args);
	va_end(args);
	fputc('\n',stderr);
}
#endif

/****************************************************************************
 * SEMAPHORE HELPERS                                                        *
 ****************************************************************************/ 

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

	dclmdWarning("semaphore wait failed");
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

	dclmdWarning("semaphore wait failed");
	return -1;
}

/****************************************************************************
 * DCLMContext: just a DCLEDMatrix and a Screen                             *
 ****************************************************************************/ 

typedef struct {
	DCLEDMatrix *dclm;
	DCLEDMatrixScreen *scr;
	int cols;
	int rows;
} DCLMContext;

void
dctxInit(DCLMContext *dc)
{
	dc->dclm=NULL;
	dc->scr=NULL;
	dc->cols=0;
	dc->rows=0;
}

void
dctxCleanup(DCLMContext *dc)
{
	dclmScrDestroy(dc->scr);
	dc->scr=NULL;

	dclmClose(dc->dclm);
	dc->dclm=NULL;
}

DCLEDMatrixError
dctxOpen(DCLMContext *dc, const char *options)
{
	DCLEDMatrixError err;

	dclmdDebug("opening LED matrix device, options='%s'",options);
	if (dc->dclm) {
		dclmdWarning("tried to re-open LED matrix device");
		return DCLM_ALREADY_OPEN;
	}

	dc->dclm=dclmOpen(options);
	if (!dc->dclm) {
		dclmdWarning("out of memory opening LED matrix device");
		return DCLM_OUT_OF_MEMORY;
	}

	if ( (err=dclmGetError(dc->dclm)) ) {
		dclmdWarning("error opening LED matrix device: %d",err);
		return err;
	}

	dc->scr=dclmScrCreate(dc->dclm);
	if (!dc->scr) {
		dclmdWarning("out of memory creating LED matrix screen");
		return DCLM_OUT_OF_MEMORY;
	}

	dc->cols=dclmGetInt(dc->dclm, DCLM_PARAM_COLUMNS);
	dc->rows=dclmGetInt(dc->dclm, DCLM_PARAM_ROWS);

	if (dc->cols < 1 || dc->rows < 1) {
		dclmdWarning("LED matrix does not have LEDs?");
		return DCLM_INVALID_CONFIG;

	}
	dclmdDebug("opened LED matrix device");
	return DCLM_OK;
}

/****************************************************************************
 * DCLMTContext: the context of a LED matrix and a thread controlling it    *
 ****************************************************************************/ 

typedef enum {
	DCLMT_CMD_SHOW_IMAGE=0,
	DCLMT_CMD_BLANK,	/* go to blank screen, stop refreshing */
	DCLMT_CMD_STOP,		/* stop refreshing */
	DCLMT_CMD_EXIT		/* exit thread */
} DCLMTCmd;

typedef struct {
	const DCLMImage *img;
	size_t pos_x;
	size_t pos_y;
	DCLMTCmd cmd;
} DCLMTWorkEntry;

typedef struct {
	DCLMContext dc;	/* the LED matrix we are using */
	pthread_t led_thread; /* the thread ID */ 
	unsigned int flags;
	unsigned int refresh_ms; /* milliseconds between refresh */

	/* this block is used for communication */
	volatile DCLMTWorkEntry communication;
	sem_t sem_free;
	sem_t sem_avail;



} DCLMTContext;

#define DCLMT_THREAD_STARTED	0x1 /* thread was sucessfully started */
#define DCLMT_SEM_FREE_INIT	0x2 /* thread was sucessfully started */
#define DCLMT_SEM_AVAIL_INIT	0x4 /* thread was sucessfully started */

static void
get_command(DCLMTContext *tc,  DCLMTWorkEntry *current)
{
	*current=tc->communication;
	/* clear it */
	tc->communication.img=NULL;
	tc->communication.cmd=DCLMT_CMD_STOP;

	/* unblock anyone who might want to send us a new command */
	if (sem_post(&tc->sem_free)) {
		dclmdWarning("LMthread: failed to signal semahore");
	}

	/* parse command */
	if (current->cmd == DCLMT_CMD_SHOW_IMAGE) {
		if (current->img) {
			dclmScrFromImgBlit(tc->dc.scr,current->img,current->pos_x, current->pos_y,0,0,tc->dc.cols,tc->dc.rows);
		} else {
			current->cmd = DCLMT_CMD_BLANK;
		}
	} else {
		current->img=NULL;
	}
}

static void
we_init(volatile DCLMTWorkEntry *we)
{
	we->img=NULL;
	we->pos_x=0;
	we->pos_y=0;
	we->cmd=DCLMT_CMD_STOP;
}

static void
calc_wait_time_ms(struct timespec *ts, const struct timespec *now, unsigned int ms)
{
	long part_ms=(long)(ms%1000);
	ts->tv_sec=now->tv_sec + ms/1000;
	ts->tv_nsec=now->tv_nsec + part_ms*1000000L;
	if (ts->tv_nsec >= 1000000000) {
		ts->tv_sec++;
		ts->tv_nsec-=1000000000;
	}
}

static void *
lm_thread(void *tc_v)
{
	DCLMTWorkEntry current;
	DCLMTContext *tc=(DCLMTContext *)tc_v;
	struct timespec loop_time,wait_ts;
	int run=1;
	int wait;

	if (!tc) {
		dclmdWarning("LMthread: dod not get a valid context!");
		return NULL;
	}

	we_init(&current);

	dclmdDebug("LMthread: entering event loop");
	while(run) {
		if (clock_gettime(CLOCK_REALTIME,&loop_time)) {
			dclmdWarning("LMthread: failed to get time, giving up");
			break;
		}

		switch(current.cmd) {
			case DCLMT_CMD_SHOW_IMAGE:
				dclmSendScreen(tc->dc.scr);
				/* wait until new command, or until timeout to refresh */
				calc_wait_time_ms(&wait_ts, &loop_time, tc->refresh_ms);
				wait=dclmdSemTimedWait(&tc->sem_avail, &wait_ts);
			       	if (wait < 0) {
					dclmdWarning("LMthread: failed to wait for semaphore, giving up");
					return NULL;
				}
				if (!wait) {	
					get_command(tc, &current);
				}
				/* if wait is > 0, timeout without new command */
				break;
			case DCLMT_CMD_BLANK:
				dclmBlankScreen(tc->dc.dclm);
				current.cmd=DCLMT_CMD_STOP;
			case DCLMT_CMD_STOP:
				/* wait ontly we get new data */
				if (dclmdSemWait(&tc->sem_avail)) {
					dclmdWarning("LMthread: failed to wait for semaphore, giving up");
					return NULL;
				}	
				get_command(tc, &current);
				break;
			case DCLMT_CMD_EXIT:
				run=0;
				break;	
			default:
				dclmdWarning("LMthread: unknown command %d, ingored",current.cmd);
				current.cmd=DCLMT_CMD_STOP;
		}
	}

	dclmdDebug("LMthread: left event loop");
	return NULL;
}

static void
dtctxInit(DCLMTContext *dtc)
{
	dctxInit(&dtc->dc);
	we_init(&dtc->communication);
	dtc->flags=0;
	dtc->refresh_ms=DCLMD_DEFAULT_REFRESH_MS;
}

static DCLEDMatrixError
dtctxOpen(DCLMTContext *dtc, const char *options)
{
	return dctxOpen(&dtc->dc, options);
}

static int
dtctxStartThread(DCLMTContext *dtc)
{
	dclmdDebug("starting  LED matrix thread");

	if (dtc->flags & DCLMT_THREAD_STARTED) {
		dclmdWarning("attempt to re-start LED matrix thread");
		return -1;
	}

	if (pthread_create(&dtc->led_thread, NULL, lm_thread, dtc)) {
		dclmdWarning("failed to create LED matrix thread");
		return -2;
	}

	dtc->flags |= DCLMT_THREAD_STARTED;
	return 0;
}

static int
dtctxStart(DCLMTContext *dtc, const char *options)
{
	if (dtctxOpen(dtc, options)) {
		return -1;
	}

	/* create the semaphores */
	if (sem_init(&dtc->sem_free, 0, 1)) {
		dclmdWarning("failed to initialze semaphore");
		return -3;
	}
	dtc->flags |= DCLMT_SEM_FREE_INIT;
	if (sem_init(&dtc->sem_avail, 0, 0)) {
		dclmdWarning("failed to initialze semaphore");
		return -3;
	}
	dtc->flags |= DCLMT_SEM_AVAIL_INIT;

	if (dtctxStartThread(dtc)) {
		return -2;
	}

	return 0;
}

static int
dtctxNewData(DCLMTContext *dtc, const DCLMImage *img, size_t x, size_t y, DCLMTCmd cmd)
{
	if ( (dtc->flags & (DCLMT_THREAD_STARTED | DCLMT_SEM_FREE_INIT | DCLMT_SEM_AVAIL_INIT)) !=
             (DCLMT_THREAD_STARTED | DCLMT_SEM_FREE_INIT | DCLMT_SEM_AVAIL_INIT)) {
		return -1;
	}

	if (dclmdSemWait(&dtc->sem_free)) {
		return -2;
	}

	dtc->communication.img=img;
	dtc->communication.pos_x=x;
	dtc->communication.pos_y=y;
	dtc->communication.cmd=cmd;

	if (sem_post(&dtc->sem_avail)) {
		dclmdWarning("failed to signal semahore");
		return -3;
	}

	return 0;
}

static int
dtctxStopThread(DCLMTContext *dtc)
{
	void *ret=NULL;

	if (!(dtc->flags & DCLMT_THREAD_STARTED)) {
		return 0;
	}

	dclmdDebug("stopping LED matrix thread");

	/* Signal the thread that it should exit */
	if (dtctxNewData(dtc, NULL, 0, 0, DCLMT_CMD_EXIT)) {
		 dclmdWarning("can't signal LED matrix thread to stop, ignoring it");
		 return -1;
	}

	/* join the thread */
	dclmdDebug("waiting for LED matrix thread to finish");
	if (pthread_join(dtc->led_thread, &ret)) {
		 dclmdWarning("failed to join LED matrix thread");
		 return -2;
	}

	dtc->flags &= ~DCLMT_THREAD_STARTED;
	dclmdDebug("stopped LED matrix thread");
	return 0;
}

static void
dtctxCleanup(DCLMTContext *dtc)
{
	dtctxStopThread(dtc);

	if (dtc->flags & DCLMT_SEM_FREE_INIT) {
		if (sem_destroy(&dtc->sem_free)) {
			dclmdWarning("failed to destroy semaphore");
		}
		dtc->flags &= ~DCLMT_SEM_FREE_INIT;
	}
	if (dtc->flags & DCLMT_SEM_AVAIL_INIT) {
		if (sem_destroy(&dtc->sem_avail)) {
			dclmdWarning("failed to destroy semaphore");
		}
		dtc->flags &= ~DCLMT_SEM_AVAIL_INIT;
	}

	dctxCleanup(&dtc->dc);
}

/****************************************************************************
 * DCLMDContext: the context of the whole Daemon                            *
 ****************************************************************************/ 

typedef struct {
	DCLMTContext tc; /* the thread controlling the LED matrix */
	/* TODO MH: bunch of clients */
} DCLMDContext;

static void
ddctxInit(DCLMDContext *ddc)
{
	dtctxInit(&ddc->tc);
}

static void
ddctxCleanup(DCLMDContext *ddc)
{
	dtctxCleanup(&ddc->tc);
}

static int
ddctxStart(DCLMDContext *ddc, const char *options)
{
	if (dtctxStart(&ddc->tc, options)) {
		return -1;
	}

	return 0;
}

/****************************************************************************
 * main                                                                     *
 ****************************************************************************/ 
int main(int argc, char **argv)
{
	const char *options=NULL;
	int status;
	DCLMDContext ddc; /* the only istance in the whole program! */

	ddctxInit(&ddc);

	/* TODO MH: parse arguments */

	status=ddctxStart(&ddc, options);
	if (!status) {
		/* TODO MH: handle the clients */
		DCLMImage *img;
		struct timespec ts;


		img=dclmImageCreateFit(ddc.tc.dc.dclm);
		dclmImageClear(img);
		dclmImageSetPixel(img,0,0,0xff);
		dclmImageSetPixel(img,1,1,0xff);
		dclmImageSetPixel(img,2,2,0xff);
		dclmImageSetPixel(img,3,3,0xff);
		dclmImageSetPixel(img,4,4,0xff);
		dclmImageSetPixel(img,5,5,0xff);
		dclmImageSetPixel(img,6,6,0xff);
		dclmImageSetPixel(img,20,6,0xff);
		dclmImageSetPixel(img,20,0,0xff);
		dtctxNewData(&ddc.tc, img, 0,0, DCLMT_CMD_SHOW_IMAGE);
		ts.tv_sec=5;
		ts.tv_nsec=0;
		nanosleep(&ts,NULL);
		dtctxNewData(&ddc.tc, NULL, 0,0, DCLMT_CMD_SHOW_IMAGE);
		dclmImageDestroy(img);
	}
	ddctxCleanup(&ddc);
	return status; 
}

