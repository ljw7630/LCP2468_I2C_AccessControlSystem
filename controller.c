#include <stdlib.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lpc24xx.h"
#include "controller.h"
#include "sensors.h"
#include "mytimer.h"
#include "lcd_hw.h"

#define controllerSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )

const int NUMBER_OF_STATES = 5;
const int NUMBER_OF_TRANSITIONS = 8;

const ulong OUTDOOR_LOCK_INDOOR_LOCK = 0L;
const ulong OUTDOOR_UNLOCK_INDOOR_LOCK = 1L;
const ulong OUTDOOR_OPEN_INDOOR_LOCK = 2L;
const ulong OUTDOOR_LOCK_INDOOR_UNLOCK = 3L;
const ulong OUTDOOR_LOCK_INDOOR_OPEN = 4L;

const ulong PASSWORD_APPROVED = 0L;
const ulong OUTDOOR_BTN_PRESSED = 1L;
const ulong FIVE_SECONDS_PASSED = 2L;
const ulong OUTDOOR_OPEN = 3L;
const ulong OUTDOOR_CLOSE = 4L;
const ulong INDOOR_BTN_PRESSED = 5L;
const ulong INDOOR_OPEN = 6L;
const ulong INDOOR_CLOSE = 7L;

const portTickType TICKS_TO_WAIT = 10;

extern xQueueHandle xGlobalStateQueueQ;

static ulong globalState = OUTDOOR_LOCK_INDOOR_LOCK;

ulong (*stateMachine[5][8])(ulong, ulong);

static void vControllerTask(void *pvParameters);

static ulong outdoorBtnPressed(ulong current_state, ulong state_transition)
{
	printf("outer door unlock, inner door lock\r\n");
	printf("index 0 light on\r\n");
	putLights( setLightOn(0) );
	startTimer();
   	
	printf("\r\n");
	return OUTDOOR_UNLOCK_INDOOR_LOCK;
}

static ulong outdoorFiveSecondsPassed(ulong current_state, ulong state_transition)
{
	printf("outer door lock, inner door lock\r\n");
	printf("index 0 light off\r\n");
	putLights( 0 );

	printf("\r\n");
	return OUTDOOR_LOCK_INDOOR_LOCK;
}

static ulong outdoorPasswordApproved(ulong current_state, ulong state_transition)
{
	printf("outer door unlock, inner door lock\r\n");
	printf("index 0 light on\r\n");
	putLights( setLightOn(0) );
	startTimer();

	printf("\r\n");
	return OUTDOOR_UNLOCK_INDOOR_LOCK;
}

static ulong outdoorOpen(ulong current_state, ulong state_transition)
{
	printf("outer door open, inner door lock\r\n");
	stopTimer();
	printf("index 0 light off\r\n");
	putLights( 0 );
	
	printf("\r\n");
	return OUTDOOR_OPEN_INDOOR_LOCK;
}

static ulong outdoorClose(ulong current_state, ulong state_transition)
{
	printf("outer door close(lock), inner door lock\r\n");
	printf("\r\n");
	return OUTDOOR_LOCK_INDOOR_LOCK;
}

static ulong indoorBtnPressed(ulong current_state, ulong state_transition)
{
	printf("outer door lock, inner door unlock\r\n");
	printf("index 2 light on\r\n");
	putLights( setLightOn(2) );
	startTimer();
	
	printf("\r\n");
	return OUTDOOR_LOCK_INDOOR_UNLOCK;
}

static ulong indoorFiveSecondsPassed(ulong current_state, ulong state_transition)
{
	printf("outer door lock, inner door lock\r\n");
	printf("index 2 light off\r\n");
	putLights( 0 );

	printf("\r\n");
	return OUTDOOR_LOCK_INDOOR_LOCK;
}

static ulong indoorOpen(ulong current_state, ulong state_transition)
{
	printf("outer door lock, inner door open\r\n");
	stopTimer();
	printf("index 2 light off\r\n");
	putLights( 0 );

	printf("\r\n");
	return OUTDOOR_LOCK_INDOOR_OPEN;
}

static ulong indoorClose(ulong current_state, ulong state_transition)
{
	printf("outer door lock, inner door close(lock)\r\n");

	printf("\r\n");
	return OUTDOOR_LOCK_INDOOR_LOCK;
}

static ulong waitingState(ulong current_state, ulong state_transition)
{
	// push the state_transition back to the tail of the queque
	xQueueSendToBack(xGlobalStateQueueQ, &state_transition, 10);

	// delay 10 milliseconds
   	vTaskDelay(10/portTICK_RATE_MS);

	// printf("\r\n");
   	// does not change the current state
	return current_state;
}

static ulong emptyState(ulong current_state, ulong state_transition)
{
	// do nothing
	printf("The action was rejected by the state matchine\r\n");

	printf("\r\n");
	return current_state;
}


inline void sendToGlobalQueue(ulong state)
{
	xQueueSend(xGlobalStateQueueQ, &state, TICKS_TO_WAIT);
	printf("message: %d send to queue\r\n", state);
}



void initializeStateMachine()
{
	int i, j;
	for(i=0;i<NUMBER_OF_STATES;++i)
	{
		for(j=0;j<NUMBER_OF_TRANSITIONS;++j)
		{
			stateMachine[i][j] = emptyState;
		}
	}

	// normal state transitions
	stateMachine[OUTDOOR_LOCK_INDOOR_LOCK][PASSWORD_APPROVED] = outdoorPasswordApproved;
	stateMachine[OUTDOOR_LOCK_INDOOR_LOCK][OUTDOOR_BTN_PRESSED] = outdoorBtnPressed;
	stateMachine[OUTDOOR_LOCK_INDOOR_LOCK][INDOOR_BTN_PRESSED] = indoorBtnPressed;

	stateMachine[OUTDOOR_UNLOCK_INDOOR_LOCK][FIVE_SECONDS_PASSED] = outdoorFiveSecondsPassed;
	stateMachine[OUTDOOR_UNLOCK_INDOOR_LOCK][OUTDOOR_OPEN] = outdoorOpen;

	stateMachine[OUTDOOR_OPEN_INDOOR_LOCK][OUTDOOR_CLOSE] = outdoorClose;

	stateMachine[OUTDOOR_LOCK_INDOOR_UNLOCK][FIVE_SECONDS_PASSED] = indoorFiveSecondsPassed;
	stateMachine[OUTDOOR_LOCK_INDOOR_UNLOCK][INDOOR_OPEN] = indoorOpen;

	stateMachine[OUTDOOR_LOCK_INDOOR_OPEN][INDOOR_CLOSE] = indoorClose;
	
	// waiting transitions
	/* For example: if the inner door unclock and outer door button press,
	 * My implementation will wait until the inner door lock and then unlock the outer door
	 */
	stateMachine[OUTDOOR_LOCK_INDOOR_UNLOCK][OUTDOOR_BTN_PRESSED] = waitingState;
	stateMachine[OUTDOOR_LOCK_INDOOR_OPEN][OUTDOOR_BTN_PRESSED] = waitingState;

	stateMachine[OUTDOOR_LOCK_INDOOR_UNLOCK][PASSWORD_APPROVED] = waitingState;
	stateMachine[OUTDOOR_LOCK_INDOOR_OPEN][PASSWORD_APPROVED] = waitingState;

	stateMachine[OUTDOOR_UNLOCK_INDOOR_LOCK][INDOOR_BTN_PRESSED] = waitingState;
	stateMachine[OUTDOOR_OPEN_INDOOR_LOCK][INDOOR_BTN_PRESSED] = waitingState;
}


void vStartController( unsigned portBASE_TYPE uxPriority )
{
	initializeStateMachine();
	xTaskCreate( vControllerTask, ( signed char * )"Controller", controllerSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL);

	printf("Controller task started ...\r\n");
}

static portTASK_FUNCTION(vControllerTask, pvParameters)
{
	ulong stateTransition;
	printf("initial state: outer door lock, inner door lock\r\n");
	while(1)
	{
		/* if receive sth */
		if( xQueueReceive( xGlobalStateQueueQ, &stateTransition, portMAX_DELAY) == pdTRUE )
		{
			/* call the state transition function, update the globalState(current_state) */
			globalState = stateMachine[globalState][stateTransition](globalState, stateTransition);
		}		
	}
}
