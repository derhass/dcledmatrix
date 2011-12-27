#include "dclm.h"
#include <stdio.h>
#include <time.h>

double time_msec(const struct timespec *ts)
{
	double val;
	
	val=((double)ts->tv_sec)*1000.0;
	val+=((double)ts->tv_nsec)*1.0e-6;
	return val;
}

int main(int argc, char **argv)
{
	DCLEDMatrix *dclm;

	dclm=dclmOpen("test");
	if (dclm) {
		DCLEDMatrixScreen *scr,*scr2;
		struct timespec a,b,c,d;
		int i;

		scr=dclmScrCreate(dclm);
		scr2=dclmScrCreate(dclm);
		dclmScrSetPixel(scr,0,0,1);
		dclmScrSetPixel(scr,1,1,1);
		dclmScrSetPixel(scr,2,2,1);
		dclmScrSetPixel(scr,3,3,1);
		dclmScrSetPixel(scr,4,4,1);
		dclmScrSetPixel(scr,5,5,1);
		dclmScrSetPixel(scr,6,6,1);
		dclmScrSetPixel(scr,7,7,1);

		dclmScrSetPixel(scr2,1,1,1);
		clock_gettime(CLOCK_MONOTONIC,&a);

		c.tv_sec=0;
		c.tv_nsec=50*1000000;
		d.tv_sec=0;
		d.tv_nsec=5*1000000;
		for (i=0; i<1000;i++) {
			dclmSendScreen(scr2);
			nanosleep(&c,NULL);
			dclmSendScreen(scr);
			nanosleep(&d,NULL);
		}
		clock_gettime(CLOCK_MONOTONIC,&b);
		printf("t: %fms\n", time_msec(&b)-time_msec(&a));

		dclmScrDestroy(scr);
		dclmScrDestroy(scr2);
		dclmClose(dclm);
	}

	return 0;
}

