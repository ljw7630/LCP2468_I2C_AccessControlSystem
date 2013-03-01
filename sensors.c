/* 
	Sample task that initialises the EA QVGA LCD display
	with touch screen controller and processes touch screen
	interrupt events.

	Jonathan Dukes (jdukes@scss.tcd.ie)
*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lpc24xx.h"
#include <stdio.h>
#include <string.h>
#include "sensors.h"
#include "controller.h"

#define I2C_AA      0x00000004
#define I2C_SI      0x00000008
#define I2C_STO     0x00000010
#define I2C_STA     0x00000020
#define I2C_I2EN    0x00000040

extern const ulong OUTDOOR_LOCK_INDOOR_LOCK;
extern const ulong OUTDOOR_UNLOCK_INDOOR_LOCK;
extern const ulong OUTDOOR_OPEN_INDOOR_LOCK;
extern const ulong OUTDOOR_LOCK_INDOOR_UNLOCK;
extern const ulong OUTDOOR_LOCK_INDOOR_OPEN;

extern const ulong PASSWORD_APPROVED;
extern const ulong OUTDOOR_BTN_PRESSED;
extern const ulong FIVE_SECONDS_PASSED;
extern const ulong OUTDOOR_OPEN;
extern const ulong OUTDOOR_CLOSE;
extern const ulong INDOOR_BTN_PRESSED ;
extern const ulong INDOOR_OPEN;
extern const ulong INDOOR_CLOSE;

/* Maximum task stack size */
#define sensorsSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )

/* The LCD task. */
static void vSensorsTask( void *pvParameters );

extern xQueueHandle xGlobalStateQueueQ;

void vStartSensors( unsigned portBASE_TYPE uxPriority )
{
	/* Enable and configure I2C0 */
	PCONP    |=  (1 << 7);                /* Enable power for I2C0              */

	/* Initialize pins for SDA (P0.27) and SCL (P0.28) functions                */
	/* We can check this though PINSEL1 table */
	PINSEL1  &= ~0x03C00000;
	PINSEL1  |=  0x01400000;

	/* Clear I2C state machine                                                  */
	I20CONCLR =  I2C_AA | I2C_SI | I2C_STA | I2C_I2EN;
	
	/* Setup I2C clock speed                                                    */
	I20SCLL   =  0x80;
	I20SCLH   =  0x80;
	
	I20CONSET =  I2C_I2EN;

	/* Spawn the console task . */
	xTaskCreate( vSensorsTask, ( signed char * ) "Sensors", sensorsSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );

	printf("Sensor task started ...\r\n");
}


/* Get I2C button status */
unsigned char getButtons()
{
	unsigned char ledData;

	/* Initialise */
	I20CONCLR =  I2C_AA | I2C_SI | I2C_STA | I2C_STO;
	
	/* Request send START */
	I20CONSET =  I2C_STA;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */		
	I20DAT    =  0xC0;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Send control word to read PCA9532 INPUT0 register */
	I20DAT = 0x00;
	I20CONCLR =  I2C_SI;

	/* Wait for DATA with control word to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send repeated START */
	I20CONSET =  I2C_STA;
	I20CONCLR =  I2C_SI;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */		
	I20DAT    =  0xC1;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	I20CONCLR = I2C_SI;

	/* Wait for DATA to be received */
	while (!(I20CONSET & I2C_SI));

	ledData = I20DAT;

	/* Request send NAQ and STOP */
	I20CONSET =  I2C_STO;
	I20CONCLR =  I2C_SI | I2C_AA;

	/* Wait for STOP to be sent */
	while (I20CONSET & I2C_STO);

	return ledData ^ 0xf;
}

unsigned char setLightOff(int index)
{
	unsigned char light = (0x3 << (index *2));
	return ~light;
}

unsigned char setLightOn(int index)
{
	unsigned char light = 0;
	light |= (1 << (index*2));
	return light;
}

/* Set I2C LEDs */
void putLights(unsigned char lights)
{
	//printf("in put lights: %d\r\n", lights);
    /* Initialise */
	I20CONCLR =  I2C_AA | I2C_SI | I2C_STA | I2C_STO;

	/* Request send START */
	I20CONSET =  I2C_STA;

	/* Wait for START to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send PCA9532 ADDRESS and R/W bit and clear SI */		
	I20DAT    =  0xC0;
	I20CONCLR =  I2C_SI | I2C_STA;

	/* Wait for ADDRESS and R/W to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Send control word to write PCA9532 LS2(LED8-LED11) register */
	I20DAT = 0x08;
	I20CONCLR =  I2C_SI;

	/* Wait for DATA with control word to be sent */
	while (!(I20CONSET & I2C_SI));

	I20DAT = lights;
	I20CONCLR =  I2C_SI;
	/* Wait for DATA to be sent */
	while (!(I20CONSET & I2C_SI));

	/* Request send NAQ and STOP */
	I20CONSET =  I2C_STO;
	I20CONCLR =  I2C_SI | I2C_AA;

	/* Wait for STOP to be sent */
	while (I20CONSET & I2C_STO);	
}


static portTASK_FUNCTION( vSensorsTask, pvParameters )
{
	portTickType xLastWakeTime;
	unsigned char buttonState;
	unsigned char lastButtonState;
	unsigned char changeState;
	unsigned char mask[4] = {1 << 0, 1 << 1, 1 << 2, 1<< 3};

	(void) pvParameters;
	printf("Starting sensor poll ...\r\n");

    /* initialise lastState with all buttons off */
	lastButtonState = 0;

	/* initial xLastWakeTime for accurate polling interval */
    xLastWakeTime = xTaskGetTickCount();

    while(1)
    {
    	buttonState = getButtons();
    	changeState = buttonState ^ lastButtonState;

    	if(buttonState != lastButtonState)
    	{
    		if( changeState & mask[0] )
    		{				
	    		if( buttonState & mask[0] )
	    		{
	    			printf("button A press\r\n");
	    			xQueueSend(xGlobalStateQueueQ, &OUTDOOR_BTN_PRESSED, 10);
	    			// sendToGlobalQueue(OUTDOOR_BTN_PRESSED);
	    		}
	    	}

    		// outer door pressed means open
    		if( changeState & mask[1])
    		{
				// printf("button 1 changed\r\n");
				if( buttonState & mask[1] )
				{
					printf("button B hold\r\n");
					xQueueSend(xGlobalStateQueueQ, &OUTDOOR_OPEN, 10);
					// sendToGlobalQueue(OUTDOOR_OPEN);
				}
				else // outer door release means close
				{
					printf("button B release\r\n");
					xQueueSend(xGlobalStateQueueQ, &OUTDOOR_CLOSE, 10);
					// sendToGlobalQueue(OUTDOOR_CLOSE);
				}
    		}

    		if( changeState & mask[2] )
    		{				    			
				if( buttonState & mask[2] ) 
    			{
    				printf("button C press\r\n");
    				xQueueSend(xGlobalStateQueueQ, &INDOOR_BTN_PRESSED, 10);
    				// sendToGlobalQueue(INDOOR_BTN_PRESSED);
    			}
    		}

    		if( changeState & mask[3] )
    		{
				// printf("button 3 changed\r\n");
	    		if( buttonState & mask[3] )
	    		{
	    			printf("button D hold\r\n");
	    			xQueueSend(xGlobalStateQueueQ, &INDOOR_OPEN, 10);
	    			// sendToGlobalQueue(INDOOR_OPEN);
	    		}
	    		else
	    		{
	    			printf("button D realse\r\n");
	    			xQueueSend(xGlobalStateQueueQ, &INDOOR_CLOSE, 10);
	    			// sendToGlobalQueue(INDOOR_CLOSE);
	    		}
	    	}
			/* remember new state */
			lastButtonState = buttonState;    		
    	}

		/* delay before next poll */
    	vTaskDelayUntil( &xLastWakeTime, 20);
    }
}
