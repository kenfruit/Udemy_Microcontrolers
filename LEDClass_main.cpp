

#include <msp430.h>

#define RED_LED 0x0001 // P1.0 is the red LED
#define GREEN_LED 0x0080 // P9.7 is the green LED
#define STOP_WATCHDOG 0x5A80 // Stop the watchdog timer
#define ACLK 0x0100 // Timer_A ACLK source
#define UP 0x0010 // Timer_A Up mode
#define ENABLE_PINS 0xFFFE // Required to use inputs and outputs


// This class allows you to easily create an LED object on a specific
// port and pin.  This can be generalized to any digital output.
//
// Instead of writing this:
//
//		P1DIR = BIT0;
// 		P1OUT = P1OUT ^ BIT0; 
//
// You write:
//
// 		LED RedLED((unsigned char *)&P1OUT, RED_LED);
//		RedLED.Toggle();
// 
class LED
{

private:
	unsigned char *m_pOUT;		// Holds the address of the Output port byte
	unsigned char *m_pDIR;      // Holds the address of the Direction Byte
	unsigned char m_Pin;        // Holds the address of the specific pin f

public:
	
	// Constructor
	LED(unsigned char *port, unsigned char pin)
	{
		m_pOUT = port;
		m_pDIR = m_pOUT + 2;				// The direction byte is 2 higher than the Output byte 
		m_Pin  = pin;

		*m_pDIR = pin;					    // Since this is an LED, set Direction to OUT

		return;
	}
	
	void On(){ *m_pOUT = m_Pin; return;}
	void Off(){*m_pOUT = 0x0000; return;}
	void Toggle(){*m_pOUT =  *m_pOUT ^ m_Pin; return;}
};

// Interupt flags
bool toggleRed = false;
bool toggleGreen = false;

main()
{
	// Create a Red LED object and a Green LED object
	LED RedLED((unsigned char *)&P1OUT, RED_LED);
	LED GreenLED((unsigned char *)&P9OUT, GREEN_LED);

	WDTCTL = STOP_WATCHDOG;       // Stop the watchdog timer

	PM5CTL0 = ENABLE_PINS;        // Required to use inputs and outputs

	TA0CCR0 = 20000;              // Sets value of Timer_0
	TA0CTL = ACLK + UP;           // Set ACLK, UP MODE
	TA0CCTL0 = CCIE;              // Enable interrupt for Timer_0

	TA1CCR0 = 3000;               // Sets value of Timer_0
	TA1CTL = ACLK + UP;           // Set ACLK, UP MODE
	TA1CCTL0 = CCIE;              // Enable interrupt for Timer_0

	_BIS_SR(GIE);                 // Activate interrupts previously enabled

	while(1)
	{
		// if the Red LED timer fired, toggle the Red LED
		if(toggleRed)
		{
			toggleRed = false;
			RedLED.Toggle();
		}
		// if the Green LED timer fired, toggle the Green LED
		if(toggleGreen)
		{
			toggleGreen = false;
			GreenLED.Toggle();

		}
		; // Wait here for interrupt
	}
}
//************************************************************************
// Timer0 Interrupt Service Routine
//************************************************************************
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_ISR (void)
{
	// Set the flag and return
	toggleRed = true;
}
//************************************************************************
// Timer1 Interrupt Service Routine
//************************************************************************
#pragma vector=TIMER1_A0_VECTOR // Note the difference for Timer1
__interrupt void Timer1_ISR (void) // Remember, the name can be anything
{
	// Set the flag and return
	toggleGreen = true;
}
