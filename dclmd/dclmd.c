#include "dclm.h"
#include "dclm_font.h"
#include "dclmd_comm.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>


#define DCLMD_DEFAULT_REFRESH_MS 300 /* maximum time between LED matrix refresh */

#define DCLMD_SEM "/dlcmd-daemon"

/****************************************************************************
 * ERRORS and DIAGNOSTICS                                                   *
 ****************************************************************************/

static void
dclmdWarning(const char *template, ...)
{
	va_list args;

	fprintf(stderr,"dclmd: warning: ");
	va_start(args, template);
	vfprintf(stderr, template, args);
	va_end(args);
	fputc('\n',stderr);
}

#ifdef NDEBUG
#define dclmdDebug(...) ((void)0)
#else
static void
dclmdDebug(const char *template, ...)
{
	va_list args;

	fprintf(stderr,"dclmd: debug: ");
	va_start(args, template);
	vfprintf(stderr, template, args);
	va_end(args);
	fputc('\n',stderr);
}
#endif

/****************************************************************************
 * DCLMDContext: just a DCLEDMatrix, a Screen and the comm interface        *
 ****************************************************************************/

typedef struct {
	DCLEDMatrix *dclm;
	DCLEDMatrixScreen *scr;
	DCLMDComminucation *comm;
	unsigned refresh_ms;
	sig_atomic_t run;
	unsigned int refresh;
	struct timespec loop_time;
	struct timespec timeout;
} DCLMDContext;

#define DC_REFRESH		0x1
#define DC_REFRESH_ONCE		0x2
#define DC_REFRESH_UNTIL	0x4

static void
dctxInit(DCLMDContext *dc)
{
	dc->dclm=NULL;
	dc->scr=NULL;
	dc->comm=NULL;
	dc->refresh_ms=DCLMD_DEFAULT_REFRESH_MS;
	dc->run=1;
	dc->refresh=0;
}

static void
dctxCleanup(DCLMDContext *dc)
{
	dclmScrDestroy(dc->scr);
	dc->scr=NULL;

	dclmClose(dc->dclm);
	dc->dclm=NULL;

	dclmdCommunicationDestroy(dc->comm);
	dc->comm=NULL;
}

static DCLEDMatrixError
dctxOpen(DCLMDContext *dc, const char *options)
{
	DCLEDMatrixError err;
	int cols,rows;

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
	cols=dclmGetInt(dc->dclm, DCLM_PARAM_COLUMNS);
	rows=dclmGetInt(dc->dclm, DCLM_PARAM_ROWS);
	if (cols < 1 || rows < 1) {
		dclmdWarning("LED matrix does not have LEDs?");
		return DCLM_INVALID_CONFIG;

	}
	dclmdDebug("opened LED matrix device: %dx%d",cols,rows);

	dclmdDebug("creating SHM interface");
	dc->comm = dclmdCommunicationCreate(1, cols, rows);
	if (!dc->comm) {
		dclmdWarning("failed to create SHM interface");
		return DCLMD_COMMUNICATION_ERROR;
	}
	dclmdDebug("created SHM interface");
	return DCLM_OK;
}

/****************************************************************************
 * MAIN LOOP                                                                *
 ****************************************************************************/

static int
handle_command(DCLMDContext *dc)
{
	DCLMDWorkEntry *work = dc->comm->work;
	if (work->cmd_flags == 0) {
		return 0;
	}

	if (work->cmd_flags & DCLMD_CMD_BRIGHTNESS) {
		dclmScrSetBrightness(dc->scr, work->brightness);
		dc->refresh=DC_REFRESH_ONCE;
	}
	if (work->cmd_flags & DCLMD_CMD_CLEAR_SCREEN) {
		dclmBlankScreen(dc->dclm);
		dc->refresh=DC_REFRESH_ONCE;
	}
	if (work->cmd_flags & DCLMD_CMD_SHOW_IMAGE) {
		dclmScrFromImgBlit(dc->scr, &dc->comm->img, work->img_pos_x, work->img_pos_y,0,0,dc->comm->img.dims[0], dc->comm->img.dims[1]);
		dc->refresh=DC_REFRESH | DC_REFRESH_ONCE;
	}
	if (work->cmd_flags & DCLMD_CMD_SHOW_TEXT) {
		if (work->text[0]) {
			work->text[DCLMD_COMM_MAX_TEXT_LENGTH]=0;
			dclmTextToScr(dc->scr,work->text_pos_x, work->text, 0, dclmFontBase);
			dc->refresh=DC_REFRESH | DC_REFRESH_ONCE;
		}
	}
	if (work->cmd_flags & DCLMD_CMD_STOP_REFRESH) {
		dc->refresh &= ~(DC_REFRESH | DC_REFRESH_UNTIL);
	}
	if (work->cmd_flags & DCLMD_CMD_START_REFRESH) {
		dc->refresh |= DC_REFRESH;
	}
	if (work->cmd_flags & DCLMD_CMD_TIMEOUT) {
		if (work->timeout_ms) {
			dc->refresh |= DC_REFRESH_UNTIL;
			dclmdCalcWaitTimeMS(&dc->timeout, NULL, work->timeout_ms);
		} else {
			dc->refresh &= DC_REFRESH_UNTIL;
		}
	}
	if (work->cmd_flags & DCLMD_CMD_EXIT) {
		dc->run=0;
	}

	work->cmd_flags = 0;
	return 0;
}

#if 0
static double
dtime(const struct timespec *a, const struct timespec *b)
{
	double ta = ((double)a->tv_sec) + a->tv_nsec/1000000000.0;
	double tb = ((double)b->tv_sec) + b->tv_nsec/1000000000.0;
	return (tb-ta)*1000.0;
}
#endif

static int
main_loop(DCLMDContext* dc)
{
	struct timespec next_wakeup;
	struct timespec *wakeup;

	int status = 0;
	int res;

	dclmdDebug("entering main loop");

	while(dc->run > 0) {
		if (clock_gettime(CLOCK_REALTIME,&dc->loop_time)) {
			dclmdWarning("failed to get time, giving up");
			status = 1;
			break;
		}

		/* wait for new command or the refresh timeout */
		if (dc->refresh) {
			dclmdCalcWaitTimeMS(&next_wakeup, &dc->loop_time, dc->refresh_ms);
			wakeup = &next_wakeup;
			if (dc->refresh & DC_REFRESH_UNTIL) {
				if (dclmdCompareTime(&next_wakeup, &dc->timeout) > 0) {
					if (dclmdCompareTime(&dc->loop_time, &dc->timeout) > 0) {
						dc->refresh &= ~(DC_REFRESH | DC_REFRESH_UNTIL);
						dclmdDebug("timeout reached");
					} else {
						dclmdDebug("waiting until timeout");
						wakeup = &dc->timeout;
					}
				}
			}
		} else {
			wakeup = NULL;
		}

#if 0
		if (wakeup) {
			dclmdDebug("wakeup: until: %f, next: %f, timeout:%f",
				dtime(&dc->loop_time, wakeup),
				dtime(&dc->loop_time, &next_wakeup),
				dtime(&dc->loop_time, &dc->timeout));
				
		}
#endif
		res = dclmdDaemonGetCommand(dc->comm, wakeup, &dc->loop_time);
		dclmdDebug("wakeup from loop: %d",res);
		if (dc->run < 1) {
			break;
		}
		if (res < 0) {
			dclmdWarning("failed to get new command, giving up");
			status = 2;
			break;
		} else if (res > 0) {
			status = handle_command(dc);
			if (dclmdDaemonUnlock(dc->comm)) {
				dclmdWarning("failed to unlock the client side again, giving up");
				status = 3;
			}
			if (status) {
				dclmdWarning("failed to complete command cycle");
				break;
			}
		}
		if (dc->refresh) {
			dclmSendScreen(dc->scr);
			dc->refresh &= ~DC_REFRESH_ONCE;
		}
	}

	if (dc->run < 0) {
		dclmdWarning("got signal to terminate");
	}
	dclmdDebug("left main loop: status %d",status);
	return status;
}

/****************************************************************************
 * main                                                                     *
 ****************************************************************************/ 

static DCLMDContext dclmdCtx;

static void sigterm_handler(int s)
{
	(void)s;
	dclmdCtx.run = -1;
	sem_post(dclmdCtx.comm->sem_command);
}

static int 
main_run (const char *options, sem_t *sem)
{
	DCLEDMatrixError err;
	int status = 1000;

	if (sem) {
		while (!dclmdSemTryWait(sem));
	}

	err =  dctxOpen(&dclmdCtx, options);
	if (sem) {
		dclmdDebug("informing parent about daemon progress");
		sem_post(sem);
	}

	if (err == DCLM_OK) {
		signal(SIGTERM, sigterm_handler); /* handle termination in a way to gracefully exit */
		signal(SIGINT,  sigterm_handler); /* handle termination in a way to gracefully exit */
		status = main_loop(&dclmdCtx);
		signal(SIGTERM, SIG_IGN); /* ignore it, we want to terminate safely */
		signal(SIGINT,  SIG_IGN); /* ignore it, we want to terminate safely */
	}

	if (sem) {
		sem_unlink(DCLMD_SEM);
		sem_close(sem);
	}
	dctxCleanup(&dclmdCtx);
	return status;
}

static int 
daemon_run(const char *options)
{
	pid_t pid;
	sem_t *sem;
	int res;

	dclmdDebug("checking if daemon is already running");
	sem = sem_open(DCLMD_SEM, O_RDWR);
	if (sem) {
		res = dclmdSemTimedWaitMS(sem, 3000);
		if (res) {
			dclmdWarning("daemon semaphore %s seems stuck, recreating", DCLMD_SEM);
			sem_unlink(DCLMD_SEM);
			sem_close(sem);
		} else {
			sem_post(sem);
			sem_close(sem);
			dclmdDebug("daemon already running");
			return 0;
		}
	}

	sem = sem_open(DCLMD_SEM, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, 0);
	if (!sem) {
		dclmdWarning("existing daemon seems stuck, you have to kill it manually!");
		return 1001;
	}
	dclmdDebug("becoming daemon");

	/* count it to zero to be sure */
	while (!dclmdSemTryWait(sem));

	pid = fork();
	if (pid < 0) {
		dclmdWarning("failed to daemonize (1st fork!)");
		return 999;
	} else if (pid > 0) {
		/* wait for daemon to start*/
		dclmdDebug("waiting for daemon to initialize");
		res = dclmdSemTimedWaitMS(sem, 3000);
		if (res) {
			dclmdWarning("daemon did not react in time", DCLMD_SEM);
		} else {
			sem_post(sem);
		}
		sem_close(sem);
		return 0;
	}

	/* we are the child: create new progress group */
	if (setsid() < 0) {
		dclmdWarning("setsid() failed but going on...");
	}
	pid=fork();
	if (pid < 0) {
		dclmdWarning("failed to daemonize (2nd fork!)");
		return 998;
	} else if (pid>0) {
		sem_close(sem);
		return 0;
	}

	/* now we are a new child of init, detached from any tty */
	dclmdDebug("running as daemon");
	return main_run(options,sem);
}

int main(int argc, char **argv)
{
	const char *options=NULL;
	int no_daemon = 0;
	int kill_daemon = 0;
	int i;
	int status = 0;
	
	dctxInit(&dclmdCtx);
	signal(SIGTERM, SIG_IGN); /* temporarily ignore it unti the main loop starts */
	signal(SIGINT,  SIG_IGN); /* temporarily ignore it unti the main loop starts */

	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i],"-n") || !strcmp(argv[i], "--no-daemon") ) {
			no_daemon = 1;
			continue;
		}
		if (!strcmp(argv[i],"-k") || !strcmp(argv[i], "--kill-daemon") ) {
			kill_daemon = 1;
			continue;
		}
	}

	if (kill_daemon) {
		DCLMDComminucation *comm;
		dclmdDebug("attempting to kill daemon");
		comm = dclmdCommunicationClientCreate();
		if (comm) {
			if (dclmdClientShowText(comm, "KILL", 0, 0, DCLMD_CMD_CLEAR_SCREEN | DCLMD_CMD_SHOW_TEXT | DCLMD_CMD_EXIT, 0) != DCLM_OK) {
				dclmdWarning("daemon could not be reuested to kill");
				status = 2;
			}
		} else {
			dclmdWarning("daemon does not seem to be running");
			status = 1;
		}
		dclmdCommunicationDestroy(comm);
		return status;
	}

	if (no_daemon) {
		status = main_run(options, NULL);
	} else {
		status = daemon_run(options);
	}

	return status;
}

