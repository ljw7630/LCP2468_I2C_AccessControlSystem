/* 
	Sample task that initialises the EA QVGA LCD display
	with touch screen controller and processes touch screen
	interrupt events.

	Jonathan Dukes (jdukes@scss.tcd.ie)
*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lcd.h"
#include "lcd_hw.h"
#include "lcd_grph.h"
#include "serial.h"
#include <stdio.h>
#include <string.h>
#include "controller.h"

/* Maximum task stack size */
#define lcdSTACK_SIZE			( ( unsigned portBASE_TYPE ) 256 )

/* Interrupt handlers */
extern void vLCD_ISREntry( void );
void vLCD_ISRHandler( void );

/* The LCD task. */
static void vLcdTask( void *pvParameters );

/* my assignment code */
extern xComPortHandle xConsolePortHandle(void);

extern const ulong PASSWORD_APPROVED;
static xQueueHandle xTouchScreenPressedQ;
extern xQueueHandle xGlobalStateQueueQ;
extern const portTickType TICKS_TO_WAIT;

void vStartLcd( unsigned portBASE_TYPE uxPriority )
{
	/* my assignment code */
	// create a message queque
	xTouchScreenPressedQ = xQueueCreate(1,0);		

	/* Spawn the console task . */
	xTaskCreate( vLcdTask, ( signed char * ) "Lcd", lcdSTACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );
	
	printf("LCD task started ...\r\n");
}

/* record the button rectangle using [(x0,y0) -> upper left] and [(x1,y1) -> bottom right] */
struct ButtonRectangle
{
	int x0;
	int x1;
	int y0;
	int y1;
};

/* if the x,y coordinates fall in to one button rectangle, return the index */
int inWhichButton(int x, int y, struct ButtonRectangle rects[], const int button_num)
{
	int i;
	for(i=0;i<button_num;++i)
	{
		if( rects[i].x0 < x 
			&& rects[i].x1 > x
			&& rects[i].y0 < y
			&& rects[i].y1 > y)
		{
			return i;
		}
	}
	return -1;
}

/* print all digits the user typed */
void displayResult(short digit[], int len)
{
	int i;
	for(i = 0; i < len; ++i)
	{
		printf("%hd", digit[i]);
	}
	printf("\r\n");
}

void drawStringOnButtonUsingButtonIndex(struct ButtonRectangle rect,unsigned char *str)
{
	int xPos = (rect.x0+rect.x1)/2 - strlen((char *)str)*2;
	int yPos = (rect.y0+rect.y1)/2;
	lcd_putString( xPos, yPos, str);
}

void changeButtonColorUsingButtonIndex(struct ButtonRectangle rect, unsigned char *str, lcd_color_t color)
{
	lcd_fillRect(
		rect.x0,
		rect.y0,
		rect.x1,
		rect.y1, color);

	drawStringOnButtonUsingButtonIndex(rect, str);
}

int checkPassword(const short digit[], const short password[], int len)
{
	int i;
	for(i=0;i<len;++i)
	{
		if(digit[i] != password[i])
			return 0;
	}
	return 1;
}

static portTASK_FUNCTION( vLcdTask, pvParameters )
{
	/* my variables */

	const int digit_len = 4;  // the maximun digits a user can press, for one round
	short digit[digit_len]; // the array with store digits. use short to reduce memory usage 
	int flag;	// a boolean flag, see code below
	int selected_button_index, activated_button_index;	// a temporary variable, see code below
	int digit_current_index = 0;	// the index of current position of digit[digit_len]. 
	const int block_num_x = 3;	// how many buttons in every row
	const int block_num_y = 4;	// how many buttons in every column
	const int line_border = 12;	// the gap between two buttons. to differentiate one button from another
	const int button_num = 12;	// the total number of buttons
	int block_width = (DISPLAY_WIDTH-(block_num_x+1) * line_border)/block_num_x; 	// the width of a button
	int block_height = (DISPLAY_HEIGHT-(block_num_y+1)*line_border)/block_num_y;	// the height of a button
	int row,col, x0,x1,y0,y1, index;	// see code below
	unsigned int x_pos, y_pos, pressure;	// get the x and y coordinate and pressure each time polling the touch screen
	const short password[4] = { 4, 3, 2, 1};

	// an array with 12 elements to store the (uppder left x, upper left y, bottom right x, bottom right y)
	struct ButtonRectangle buttonRects[button_num];	
	
	// the strings that will be displayed on the buttons
	unsigned char displayStrings[button_num][8] = {
		"1", "2", "3"
		,"4", "5", "6"
		,"7", "8", "9"
		,"OK", "0", "CANCEL"};
	
	// the corresponding integer value of the display strings
	short displayNumbers[button_num] = {
		1, 2, 3
		, 4, 5, 6
		, 7, 8, 9
		, -1, 0, -1
	};
	
	// the index of "OK" and "CANCEL button"
	const int ok_index = 9, cancel_index = 11;

	/* Just to stop compiler warnings. */
	( void ) pvParameters;


	/* Initialise LCD display */
	/* NOTE: We needed to delay calling lcd_init() until here because it uses
	 * xTaskDelay to implement a delay and, as a result, can only be called from
	 * a task */
	lcd_init();

	 printf("\r\nthe maximum number of digits you can typed in is: %d\r\n", digit_len);
	 printf("if more than the number, the buffer will be cleared\r\n");

	// fill the screen with color BLACK
	lcd_fillScreen(BLACK);

   	// draw button
	y_pos = line_border;
	index = 0;

	/* 	for each row and for each column, get the x0, y0, x1, y1 by: 
	 *	the width of the button, the height of the button, and the gap between buttons   */
	for(row=0; row < block_num_y;++row)
	{
		x_pos = 0;
		for(col=0;col<block_num_x;++col)
		{
			/* the horizontal distance between a button and an edge or  a button to a button is
			 * define by the "line_border" */
			x_pos += line_border;	
			x0 = x_pos;
			x1 = x0 + block_width;
			y0 = y_pos;
			y1 = y0 + block_height;
			
			// store these values, we need to use it to tell with button the user pressed
			buttonRects[index].x0 = x0;
			buttonRects[index].x1 = x1;
			buttonRects[index].y0 = y0;
			buttonRects[index].y1 = y1;
			
			x_pos = x1;
			
			// draw a OLIVE button
			// display the string on the button
			changeButtonColorUsingButtonIndex(buttonRects[index], displayStrings[index], OLIVE);

			++index;
		}
	   	
		/* the vertical distance between a button and an edge or  a button to a button is
			 * define by the "line_border" */
		y_pos += block_height + line_border;
	}

	/* Infinite loop blocks waiting for a touch screen interrupt event from
	 * the queue. */
	for( ;; )
	{
		/* Clear TS interrupts (EINT3) */
		EXTINT = 8;

		/* Enable TS interrupt vector (VIC) (vector 17) */
		VICIntEnable |= 1 << 17;	/* Enable interrupts on vector 17 */

		/* Block on a quete waiting for an event from the TS interrupt handler */		
		xQueueReceive(xTouchScreenPressedQ, NULL, portMAX_DELAY);
		
		/* Disable TS interrupt vector (VIC) (vector 17) */
		VICIntEnClr = 1 << 17;
						
		/* +++ This point in the code can be interpreted as a screen button push event +++ */
		/* Start polling the touchscreen pressure and position ( getTouch(...) ) */
		/* Keep polling until pressure == 0 */
		
		pressure = 1;
		getTouch(&x_pos, &y_pos, &pressure);
		flag = 0;
		selected_button_index = -1;
		activated_button_index = -1;
		/* while the pressure is not 0 */
		while(pressure)
		{ 	
			/* see which button the user pressed
			 * get the index of the button in buttonRectangles array */
			selected_button_index = inWhichButton(x_pos, y_pos, buttonRects, button_num);
			//printf("selected button index: %d\r\n",selected_button_index);
			/* only store the value when the first time the user touch the screen
			 * i.e. if the user touch the screen and move his finger elsewhere,
			 * I will not record the following event */
			/* re-enable this flag only when the user release his finger */
			if(!flag)
			{
				if(0 <= selected_button_index) /* if the index is valid */
				{
					// the index of the activated button
					activated_button_index = selected_button_index;

					// change the background color of the button to give the user a feedback
					changeButtonColorUsingButtonIndex(buttonRects[activated_button_index], displayStrings[activated_button_index], LIGHT_GRAY);
					
					// if the current index is pointing to length of the buffer, the buffer is full
					if(digit_current_index == digit_len)
					{
						// if the user presses the OK button
						if(ok_index == selected_button_index)
						{
							printf("OK button pressed\r\n");
							// check password
							if(checkPassword(digit, password, digit_len))
							{
								// password is valid, send message to queue
								printf("Password correct\r\n");
								printf("\r\n");
								xQueueSend(xGlobalStateQueueQ, &PASSWORD_APPROVED, TICKS_TO_WAIT);
								// sendToGlobalQueue(PASSWORD_APPROVED);
							}
							else
							{
								// password is wrong, display error message
								printf("The password you typed is wrong, type in again\r\n");
							}
						}
						else
						{
							// discard input
							printf("Input was disgarded\r\n");
						}

						digit_current_index = 0;
						flag = 1;
						continue;
					}
						

					/* if the index is equal to the index of "OK" button */
					if(ok_index == selected_button_index) 
					{
						/*
						// feedback
						printf("Button OK pressed, you typed:\r\n");

						// print out all use input digits
						displayResult(digit, digit_current_index);

						// reset the index of the digit array to 0
						digit_current_index = 0;
						*/
						printf("OK button pressed\r\n");
						printf("You type less than four digits, input discarded\r\n");
						digit_current_index = 0;
					}
					else if(cancel_index == selected_button_index)	// if the index is equal to the index of "CANCEL" button
					{
						// feedback
						printf("Button CANCEL pressed, all digits type in disgarded\r\n");

						// reset the index of the digit array to 0
						digit_current_index = 0;					
					}
					else
					{
						// get the real value from displayNumbers array
						digit[digit_current_index] = displayNumbers[selected_button_index];

						// feedback
						printf("Button %d pressed, index: %d\r\n",displayNumbers[selected_button_index], digit_current_index);
						
						// increase the index of the digit array
						++digit_current_index;
								
					}
				}
				flag = 1; // set the flag, so will not record any new digit before the user release his finger
			}
			else
			{
				/* If the user press a button, we change the color to "LIGHT_GRAY",
				 * but if the user didn't release his finger and move to other places,
				 * I restore the color of the "activated button" to "OLIVE" */
				if(-1 != selected_button_index 
					&& -1 != activated_button_index
					&& activated_button_index != selected_button_index)
				{
					changeButtonColorUsingButtonIndex(buttonRects[activated_button_index], displayStrings[activated_button_index], OLIVE);
					activated_button_index = -1;
				}
			}

			// keep polling
			getTouch(&x_pos, &y_pos, &pressure);

			// delay 100 milliseconds
			// don't want to poll that quick
			mdelay(100);
		} 
		
		// delay 100 milliseconds
		// mdelay(100);
		/* +++ This point in the code can be interpreted as a screen button release event +++ */
	   	
		// restore the button backgroun color to OLIVE
		if(-1 != activated_button_index)
		{
			changeButtonColorUsingButtonIndex(buttonRects[activated_button_index], displayStrings[activated_button_index], OLIVE);			
		}
	}
}


void vLCD_ISRHandler( void )
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Process the touchscreen interrupt */
	/* We would want to indicate to the task above that an event has occurred */
	xQueueSendFromISR(xTouchScreenPressedQ, 0, &xHigherPriorityTaskWoken);

	EXTINT = 8;					/* Reset EINT3 */
	VICVectAddr = 0;			/* Clear VIC interrupt */

	/* Exit the ISR.  If a task was woken by either a character being received
	or transmitted then a context switch will occur. */
	portEXIT_SWITCHING_ISR( xHigherPriorityTaskWoken );
}


