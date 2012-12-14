/* Standard includes. */
#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "console.h"
#include "sensors.h"
#include "lcd.h"
#include "lcd_hw.h"
#include "lcd_grph.h"
#include "controller.h"
#include "timer.h"

/*
 * Configure the processor for use with the Keil demo board.  This is very
 * minimal as most of the setup is managed by the settings in the project
 * file.
 */
static void prvSetupHardware( void );

xQueueHandle xGlobalStateQueueQ;

int main (void)
{
	/* Setup the hardware for use with the Keil demo board. */
	prvSetupHardware();
	
	xGlobalStateQueueQ = xQueueCreate( 16, sizeof(ulong));
		
    /* Start the console task */
	vStartConsole(1, 19200);

	/* Start the lcd task, passing it a pointer to the handle for the
	 * button event queue */

	vStartController(3);

	vStartTimer(3);

	vStartSensors(2);

	vStartLcd(2);

	/* Start the FreeRTOS Scheduler ... after this we're pre-emptive multitasking ...

	NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
	The processor MUST be in supervisor mode when vTaskStartScheduler is 
	called.  The demo applications included in the FreeRTOS.org download switch
	to supervisor mode prior to main being called.  If you are not using one of
	these demo application projects then ensure Supervisor mode is used here. */
	vTaskStartScheduler();

	/* Should never reach here!  If you do then there was not enough heap
	available for the idle task to be created. */
	while(1);
}

static void prvSetupHardware( void )
{
	/* Perform the hardware setup required.  This is minimal as most of the
	setup is managed by the settings in the project file. */
	
    /* Enable UART0. */
    PCONP   |= (1 << 3);                 /* Enable UART0 power                */
    PINSEL0 |= 0x00000050;               /* Enable TxD0 and RxD0              */
}
