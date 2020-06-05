#include "dclm.h"
#include "dclm_font.h"
#include "dclmd_comm.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define DCLMD_DEFAULT_REFRESH_MS 300 /* maximum time between LED matrix refresh */

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
	int refresh;
} DCLMDContext;

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
	switch(work->cmd) {
		case DCLMD_CMD_SHOW_IMAGE:
			dclmScrFromImgBlit(dc->scr, &dc->comm->img, work->pos_x, work->pos_y,0,0,dc->comm->img.dims[0], dc->comm->img.dims[1]);
			dc->refresh=1;
			break;
		case DCLMD_CMD_SHOW_TEXT:
			if (work->text[0]) {
				work->text[DCLMD_COMM_MAX_TEXT_LENGTH]=0;
				dclmTextToScr(dc->scr,work->pos_x, work->text, 0, dclmFontBase);
				dc->refresh=1;
			} else {
				dclmBlankScreen(dc->dclm);
				dc->refresh=0;
			}
			break;
		case DCLMD_CMD_BLANK:
			dclmBlankScreen(dc->dclm);
			dc->refresh=0;
			break;
		case DCLMD_CMD_STOP:
			dc->refresh=0;
			break;
		case DCLMD_CMD_EXIT:
			dc->run=0;
			break;	
		default:
			dclmdWarning("unknown command %d, ingored",work->cmd);
	}
	return 0;
}

static int
main_loop(DCLMDContext* dc)
{
	struct timespec loop_time;
	int status = 0;
	int res;

	dclmdDebug("entering main loop");

	while(dc->run > 0) {
		if (clock_gettime(CLOCK_REALTIME,&loop_time)) {
			dclmdWarning("failed to get time, giving up");
			status = 1;
			break;
		}

		/* wait for new command or the refresh timeout */
		res = dclmdDaemonGetCommand(dc->comm, (dc->refresh)?dc->refresh_ms:DCLMD_TIMEOUT_INFINITE, &loop_time);
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

int main(int argc, char **argv)
{
	const char *options=NULL;
	int status = 1000;
	
	signal(SIGTERM, SIG_IGN); /* temporarily ignore it unti the main loop starts */
	signal(SIGINT,  SIG_IGN); /* temporarily ignore it unti the main loop starts */
	dctxInit(&dclmdCtx);

	/* TODO MH: parse arguments */

	if (dctxOpen(&dclmdCtx, options) == DCLM_OK) {
		signal(SIGTERM, sigterm_handler); /* handle termination in a way to gracefully exit */
		signal(SIGINT,  sigterm_handler); /* handle termination in a way to gracefully exit */
		status = main_loop(&dclmdCtx);
		signal(SIGTERM, SIG_IGN); /* ignore it, we want to terminate safely */
		signal(SIGINT,  SIG_IGN); /* ignore it, we want to terminate safely */
	}
	dctxCleanup(&dclmdCtx);
	return status;
}

