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
	portTickType period = (portTickType) (5000/portTICK_RATE_MS);
	xGlobalTimer = xTimerCreate("Timer", period, pdFALSE, NULL, vTimerCallBack);
}

static void vTimerCallBack(xTimerHandle xTimer)
{
	xQueueSend(xGlobalStateQueueQ, &FIVE_SECONDS_PASSED, TICKS_TO_WAIT);	
	// sendToGlobalQueue(FIVE_SECONDS_PASSED);
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

/*
void vStartTimer( unsigned portBASE_TYPE uxPriority )
{
	xTimerQueue = xQueueCreate(1,0);
	xTaskCreate( vTimerTask, ( signed char * ) "Timer", timerTaskSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );	
}

void startTimer()
{
	flag = 1;
	xQueueSend(xTimerQueue, 0, TICKS_TO_WAIT);
	printf("start timer\r\n");
}

void stopTimer()
{
	flag = 0;
	xQueueReceive(xTimerQueue, NULL, portMAX_DELAY);
	printf("stop timer\r\n");
}

static portTASK_FUNCTION( vTimerTask, pvParameters )
{
	while(1)
	{
		if( pdTRUE  == xQueueReceive(xTimerQueue, NULL, portMAX_DELAY) )
		{
			mdelay(5000);
			xQueueSend(xGlobalStateQueueQ, &FIVE_SECONDS_PASSED, TICKS_TO_WAIT);
		}
	}	
}
*/
