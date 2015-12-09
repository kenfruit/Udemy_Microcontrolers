#include <msp430.h>
#include <driverlib.h>
#include <string.h>
#include <stdio.h>

#include "myGpio.h"
#include "myClocks.h"
#include "myLcd.h"


#define STOP_WATCHDOG   0x5A80 	// Stop the watchdog timer
#define ENABLE_PINS     0xFFFE 	// Required to use inputs and outputs


#define ACLK 0x0100 			// Timer ACLK source
#define UP   0x0010 		    // Timer Up mode



// PROGRAMMING - Length of the workout can be set in this state
// WAITING - Waiting for a button press to start the workout
// WARMUP - Preset warm-up period
// WORKING - 90 second up tempo period
// RECOVERING - 30 second recovery period
// COOLING - cool down period
// DONE - Workout DONE message
enum { PROGRAMMING, WAITING, WARMUP, WORKING, RECOVERING, COOLING, DONE};

// Parameters for LED setting function
enum { LED_OFF, LED_ON, LED_TOGGLE};

// Holder of the current state machine state, start in PROGRAMMING mode
int  currentState = PROGRAMMING;

// Lengths of the various program segments
// Demo/Testing
#define  DEFAULT_SESSION_TIME  (2 * 60)
#define  DEFAULT_WARMUP_TIME  60
#define  DEFAULT_WORK_TIME  12
#define  DEFAULT_REST_TIME  7
#define  DEFAULT_COOL_TIME  15

//
//#define  DEFAULT_SESSION_TIME  (2 * 60)
//#define  DEFAULT_WARMUP_TIME  (10 * 60)
//#define  DEFAULT_WORK_TIME  90
//#define  DEFAULT_REST_TIME  30
//#define  DEFAULT_COOL_TIME  (5 * 60)



// We want a clock interupt every 1/2 second so the LEDs blink cycle is 1 second
// This will alternate between true and false and gives us a way to track whole seconds
// vs half seconds
bool tickTock = true;

int totalTime = DEFAULT_SESSION_TIME;     // In seconds ticks
int segmentTime = DEFAULT_WARMUP_TIME;    // Warmup time

bool isPaused = false;

// Number of Work/Recover segments.
int remainingSegments = 2;

// Flags set in interupt handlers
bool button1Interupt = false;
bool button2Interupt = false;

bool timerInterupt = false;


void ScrollWords(char words[250]); // Works for messages of up to 250 characters
void OnButton1Press();
void OnButton2Press();
void OnTimerTick();
void ShowTime(int displayTicks);
void ShowText(char text[7]);

void RedLED(unsigned int state);
void GreenLED(unsigned int state);
void Colon(unsigned int state);


main()
{


 	WDTCTL = STOP_WATCHDOG;     // Stop WDT

	PM5CTL0 = ENABLE_PINS;		// Enable inputs and outputs

	P1DIR = BIT0; 				// P1.0 will be output for red LED
	P9DIR = BIT7; 				// P9.7 will be output for green LED

	P1OUT = BIT1 | BIT2; 		// Pull-up resistors for buttons
	P1REN = BIT1 | BIT2;

	P1IE = BIT1 | BIT2; 		// Enable interrupt for P1.1 and P1.2
	P1IES = BIT1 | BIT2; 		// For transitions from HI-->LO

	P1IFG = 0x00; 				// Ensure no interrupts are pending

	TA0CCR0 = 20000;            // Sets value of Timer_0
	TA0CTL = ACLK + UP;         // Set ACLK, UP MODE
	TA0CCTL0 = CCIE;            // Enable interrupt for Timer_0


	initGPIO(); // Initialize General Purpose Inputs and Outputs
	initClocks(); // Initialize clocks
	myLCD_init(); // Initialize Liquid Crystal Display

	//ScrollWords("WORKOUT 1"); // Scroll this message across the LCD
    ShowTime(totalTime);

 	_BIS_SR(GIE);                  // Activate all interrupts


 	// Loop and process interupts
	while(1)
	{
		if(button1Interupt)
		{
			button1Interupt = false;
			OnButton1Press();
		}

		if(button2Interupt)
		{
			button2Interupt = false;
			OnButton2Press();
		}
		if(timerInterupt)
		{
			timerInterupt = false;
			OnTimerTick();
		}

	}
}



// Button One has two functions.
//
// In the PROGRAMMING state, pressing the button increases the workout time by
// 4 minutes (2 cycles) up to 90 minutes before rolling over.
//
// In WARMUP, WORKING, and RECOVERING states, pressing the button pauses/restarts
// the countdown timer.
void OnButton1Press()
{
    switch (currentState)
    {
        case PROGRAMMING:
            totalTime += 4 * 60;
            remainingSegments += 2;

            if (90 * 60 < totalTime)
            {
                totalTime = DEFAULT_SESSION_TIME;
                remainingSegments = 2;
            }
            ShowTime(totalTime);
            break;

        case WAITING:
            break;

        case WARMUP:
        case WORKING:
        case RECOVERING:
        case COOLING:
            isPaused = !isPaused;
            break;

        case DONE:
            break;
    }

}

// <summary>
// Pressing Button 2 move causes a transition from
// PROGRAMMING state to WAITING state and from
// WAITING state to WARMUP state.
// </summary>
// <param name="sender"></param>
// <param name="e"></param>
void OnButton2Press()
{

    switch (currentState)
    {
        case PROGRAMMING:
            currentState = WAITING;
            ShowText(" START");
            break;
        case WAITING:
            currentState = WARMUP;
            break;

        case WARMUP:
            break;

        case WORKING:
             break;

        case RECOVERING:
            break;

        case COOLING:
            break;

        case DONE:
            break;
    }
}

// <summary>
// Toggle the LEDs
// </summary>
// <param name="redIsOn">If true, set the red led ON</param>
// <param name="greenIsOn">If true, set the green led ON</param>
void SetLEDs(unsigned int redState, unsigned int greenState)
{
    RedLED(redState);
    GreenLED(greenState);
}


// <summary>
// Display the time
// </summary>
// <param name="displayTicks"></param>
void ShowTime(int displayTicks)
{
    int minutes = displayTicks / 60;
    int seconds = (displayTicks % 60);

    char buffer[7]
				;
    if(0 < minutes)
    {
    	sprintf(buffer, "  %2d%02d",minutes,seconds);
    }
    else
    {
    	sprintf(buffer, "    %02d",seconds);
    }

    ShowText(buffer);
    Colon(LCD_UPDATE);

    return;
}

// <summary>
// Display Text in LCD display
// </summary>
// <param name="textToShow"></param>
void ShowText(char text[7])
{
    Colon(LCD_CLEAR);

    unsigned int i = 0;
	for(i = 0; i < 6; i++)
	{
		myLCD_showChar( text[i], i+1 );
	}
	return;
}

// <summary>
// Process a timer interupt
//
// Check for state transitions
// Decrement to countdown timers
// Toggle the LEDs
// Update the LCD display
// </summary>
// <param name="sender"></param>
// <param name="e"></param>
void OnTimerTick()
{
    // Do nothing if in one of these states
    if (currentState == PROGRAMMING ||
        currentState == WAITING ||
        currentState == DONE)
    {
        return;
    }


    // Check for segment state transition
    if (segmentTime == 0)
    {
        switch (currentState)
        {
            case WARMUP:
                // WARMUP always transitions to WORKING
                currentState = WORKING;
                segmentTime = DEFAULT_WORK_TIME;
                break;

            case WORKING:
                // If this is a
                if (0 == remainingSegments)
                {
                    currentState = COOLING;
                    segmentTime = DEFAULT_COOL_TIME;
                }
                else
                {
                    currentState = RECOVERING;
                    segmentTime = DEFAULT_REST_TIME;
                }
            	remainingSegments--;
                break;

            case RECOVERING:
                if (0 == remainingSegments)
                {
                    currentState = COOLING;
                    ShowText(" COOL ");
                    segmentTime = DEFAULT_COOL_TIME;
                }
                else
                {
                    currentState = WORKING;
                    segmentTime = DEFAULT_WORK_TIME;
                }
                break;

            case COOLING:
                currentState = DONE;
                break;

        }
    }



    // Set the LEDs
    // Last 5 seconds of a segment -- Flash Both
    // WARMUP -- Alternate flashing red and green.-- Flash Both
    // WORKING -- Flash Green
    // RECOVERING -- Flash Red
    // COOLING -- Alternate flashing red and green.-- Flash Both
    // DONE -- Both on Steady
    if (segmentTime < 6)                               // Last 5 seconds of a segment  -- Flash Both
    {
    	tickTock ? RedLED(LED_ON) : RedLED(LED_OFF);
    	tickTock ? GreenLED(LED_ON) : GreenLED(LED_OFF);
    }
    else if (currentState == WARMUP )                  // WARMUP -- Alternate flashing red and green
    {
    	tickTock ? RedLED(LED_ON) : RedLED(LED_OFF);
    	tickTock ? GreenLED(LED_OFF) : GreenLED(LED_ON);
    }
    else if(currentState == WORKING)                   // WORKING -- Flash Green
    {
    	RedLED(LED_OFF);
    	GreenLED(LED_TOGGLE);
    }
    else if (currentState == RECOVERING)               // RECOVERING -- Flash Red
    {
    	RedLED(LED_TOGGLE);
    	GreenLED(LED_OFF);

    }
    else if (currentState == COOLING)                  // COOLING -- Alternate flashing red and green
    {
    	tickTock ? RedLED(LED_ON) : RedLED(LED_OFF);
    	tickTock ? GreenLED(LED_OFF) : GreenLED(LED_ON);
    }
    else if (currentState == DONE)                  // DONE -- Both on Steady
    {
    	RedLED(LED_ON);
    	GreenLED(LED_ON);
    }





    // The timer fires every 500 ms so we
    // count down a second every other tick
    tickTock = !tickTock;

    // if we are paused, don't count down
    if(tickTock && !isPaused)
    {
        totalTime--;
        segmentTime--;
    }

    // In the cooling state, show the remaining
    // time, but also flash "COOL"
    if (currentState == COOLING &&
        totalTime % 20 == 0)
    {
        ShowText(" COOL ");
    }
    else if (currentState == DONE)
    {
        ShowText(" DONE ");
    }
    else
    {
        ShowTime(segmentTime);

        // Blink the colon on the half-second
        if(!tickTock)
        {
         Colon(LCD_CLEAR);
        }

    }

}


// Set the RedLED
// Call using:
// enum { LED_OFF, LED_ON, LED_TOGGLE};
void RedLED(unsigned int state)
{
	switch (state)
	{
		case LED_OFF:
			P1OUT &= ~BIT0;	//Off
			break;
		case LED_ON:
			P1OUT |= BIT0;  //On
			break;
		case LED_TOGGLE:
			P1OUT ^= BIT0;  //Toggle
			break;
		default:
			break;
	}
}

// Set the GreenLED
// Call using:
// enum { LED_OFF, LED_ON, LED_TOGGLE};
void GreenLED(unsigned int state)
{
	switch (state)
	{
		case LED_OFF:
			P9OUT &= ~BIT7;	//Off
			break;
		case LED_ON:
			P9OUT |= BIT7;  //On
			break;
		case LED_TOGGLE:
			P9OUT ^= BIT7;  //Toggle
			break;
		default:
			break;
	}
}

/// Set the right Colon
void Colon(unsigned int state)
{
	myLCD_showSymbol(state, LCD_A4COL, 0);
}




//***********************************************************************
//* Port 1 Interrupt Service Routine
//***********************************************************************
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
	unsigned long delay; 			// Wait for bouncing to end
	for(delay=0 ;
			delay<12345 ;
			delay=delay+1);

	// Set a flag and return.
	// The rest of the work takes place in main()
	switch(P1IV) 					// What is stored in P1IV?
	{
		case 4: 					// Come here if P1.1 interrupt
		{
			button1Interupt = true;	// There was a P1.1 push
			break;
		}
		case 6: 					// Come here if P1.2 interrupt
		{
			button2Interupt = true;	// There was a P1.2 push
			break;
		}
	}
}// end
//************************************************************************
// Timer0 Interrupt Service Routine
//************************************************************************
#pragma vector=TIMER0_A0_VECTOR      // The ISR must be put into a special
                                     // place in the microcontroller program
                                     // memory. That's what this line does.
                                     // While you do not need this comment,
                                     // the code line itself must always
                                     // appear exactly like this in your
                                     // program.
//************************************************************************
__interrupt void Timer0_ISR (void)   // This officially names this ISR as
                                     // "Timer0_ISR"
//************************************************************************
{
	// Set a flag and return.
	// The rest of the work takes place in main()
	timerInterupt = true;
}


//************************************************************************

