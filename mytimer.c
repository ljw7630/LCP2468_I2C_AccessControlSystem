#include <stdlib.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lpc24xx.h"
#include "controller.h"
#include "lcd_hw.h"
#include "timers.h"

#define timerTaskSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )

static void vTimerCallBack(xTimerHandle xTimer);

extern const portTickType TICKS_TO_WAIT;
extern const ulong FIVE_SECONDS_PASSED;
extern xQueueHandle xGlobalStateQueueQ;

int flag;
xTimerHandle xGlobalTimer;

void vCreateTimer()
{
	/* 5 senconds = 5000 miliseconds. so number of ticks for will be 5000/portTICK_RATE_MS */
	portTickType period = (portTickType) (5000/portTICK_RATE_MS);

	/* 
	 * name is "Timer", with a period of 5 senconds, do not automatically restart, 
	 * pvTimerID = NULL is because we only use one timer, so we don't have to differentiate it in our callback function
	 */
	xGlobalTimer = xTimerCreate("Timer", period, pdFALSE, NULL, vTimerCallBack);
}

/* the callback method is called when the timer expired */
static void vTimerCallBack(xTimerHandle xTimer)
{
	/* send timeout message to the global queue */
	xQueueSend(xGlobalStateQueueQ, &FIVE_SECONDS_PASSED, TICKS_TO_WAIT);	
}

void startTimer()
{
	xTimerStart(xGlobalTimer, TICKS_TO_WAIT);
	printf("timer started\r\n");
}

void stopTimer()
{
	xTimerStop(xGlobalTimer, TICKS_TO_WAIT);
	printf("timer stopped\r\n");
}
