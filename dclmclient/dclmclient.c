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
#include <stdarg.h>
#include <string.h>

/****************************************************************************
 * ERRORS and DIAGNOSTICS                                                   *
 ****************************************************************************/

static void
dclmcWarning(const char *template, ...)
{
	va_list args;

	fprintf(stderr,"warning: ");
	va_start(args, template);
	vfprintf(stderr, template, args);
	va_end(args);
	fputc('\n',stderr);
}

#ifdef NDEBUG
#define dclmcDebug(...) ((void)0)
#else
static void
dclmcDebug(const char *template, ...)
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
 * main                                                                     *
 ****************************************************************************/ 

int main(int argc, char **argv)
{
	DCLMDComminucation *comm;
	DCLEDMatrixError err;
	int status = 0;

	/* TODO MH: parse arguments */

	dclmcDebug("connecting to daemon SHM");
	comm = dclmdCommunicationClientCreate();
	if (comm) {
		dclmcDebug("locking SHM");
		err = dclmdClientLock(comm);
		if (err == DCLM_OK) {
			dclmcDebug("preparing command");
			snprintf(comm->work->text,sizeof(comm->work->text),"Test.");
			comm->work->text_pos_x=0;
			comm->work->timeout_ms=3000;
			comm->work->cmd_flags |= DCLMD_CMD_SHOW_TEXT | DCLMD_CMD_TIMEOUT;
			dclmcDebug("sending command");
			err = dclmdClientUnlock(comm);
			if (err != DCLM_OK) {
				dclmcWarning("failed to finish SHM communication: %d!",(int)err);
				status= 3;
			} else {
				dclmcDebug("sent command!");
			}
		} else {
			dclmcWarning("failed to lock SHM: %d!",(int)err);
			status = 2;
		}
		dclmcDebug("closing shm connection!");
		dclmdCommunicationDestroy(comm);
	} else {
		dclmcWarning("failed to connect to daemon via SHM!");
		status = 1;
	}

	dclmcDebug("finished with code %d",status);
	return status;
}

