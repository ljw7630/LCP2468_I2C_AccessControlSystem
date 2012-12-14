#include <stdlib.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lpc24xx.h"
#include "controller.h"

#define timerTaskSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )

static void vTimerTask(void *pvParameters);
void timer_handler(void);

extern const ulong FIVE_SECONDS_PASSED;
extern xQueueHandle xGlobalStateQueueQ;

void vStartTimer( unsigned portBASE_TYPE uxPriority )
{
	xTaskCreate( vTimerTask, ( signed char * ) "Timer", timerTaskSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );	
}

void startTimer()
{
	T0TCR = 0x1;	/* Start timer0 */
}

void stopTimer()
{
	T0TCR = 0x2;	/* Stop and reset TIMER0 */
}

static portTASK_FUNCTION( vTimerTask, pvParameters )
{
	/* configure TIMER0 to count for 5 second */
	T0IR = 0xFF;		/* Clear any previous interrupts */
	T0TCR = 0x2;		/* Stop and reset TIMER0 */
	T0CTCR = 0x0;		/* Timer mode */
	T0MR0 = 12000000 * 5;	/* Match every 5 seconds */
	T0MCR = 0x3;		/* Interrupt, reset and re-start on match */
	T0PR = 0x0;			/* Prescale = 1 */
	
	VICIntSelect &= ~(1 << 4);	/* Configure vector 4 (TIMER0) for IRQ */
    VICVectPriority4 = 8;	/* Set vector priority to 8 (mid-level priority) */

	/* Set handler vector. This uses a "function pointer" which is just
     * the address of the function. */
    VICVectAddr4 = (ulong)timer_handler;
	
}

void timer_handler(void)
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Clear interrupt source (on the TIMER peripheral) */
	T0IR = 0xFF;
	
	/* send message to the queue */
	xQueueSendFromISR(xGlobalStateQueueQ, &FIVE_SECONDS_PASSED, &xHigherPriorityTaskWoken);

	/* Acknowledge VIC interrupt (on the VIC) */
    VICVectAddr = 0;

	stopTimer();

	portEXIT_SWITCHING_ISR( xHigherPriorityTaskWoken );		
}
