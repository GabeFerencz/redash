/* The MIT License (MIT)

Copyright (c) 2016 Gabe Ferencz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */
#include <msp430.h>
#include <stdbool.h>

//#define DEBUG_MODE_ON

#define WDT_DISABLE()			(WDTCTL = WDTPW + WDTHOLD)
/* Use the watchdog interval mode, sourced by SMCLK/32768 */
#define WDT_INTERVAL_MODE()		(WDTCTL = WDTPW + WDTTMSEL + WDTCNTCL)
/* Measured watchdog interval at 28.7ms with no SMCLK divider running
 * at the default frequency.
 * With the maximum SMCLK divider (/8), the interval is measured as 230 ms.
 * 15 minutes / 28.7 milliseconds = 31358.885
 * 15 minutes / 230 milliseconds = 3913.04348 */
#define LED_TIMEOUT_INTERVALS	(3913)
/* Redefine a small interval for debug mode */
#ifdef DEBUG_MODE_ON
	#undef LED_TIMEOUT_INTERVALS
	#define LED_TIMEOUT_INTERVALS	(4)
#endif

/* The LED output is on P1.6. It is active low. Note that this is opposite
 * of the LED configuration on the Launchpad. */
#define LED_ACTIVATE()		(P1OUT &= ~BIT6)
#define LED_DEACTIVATE()	(P1OUT |= BIT6)

/* Set to true by the GPIO interrupt on a button press. Set to false by
 * the main thread after it is handled. */
bool button_pressed = false;
/* Set to true by the WDT interrupt when an interval has completed. Set
 * to false by the main thread after it is handled. */
bool interval_complete = false;

enum states {
	/* In the sleep state:
	 * 	LED: OFF
	 * 	BUTTON: Move to the start timer state.
	 * sleep: LPM4 to conserve as much energy as possible. */
	STATE_SLEEP,
	/* In the active states:
	 * 	LED: ON in activate, unchanged in active
	 * 	BUTTON: Cancel timer and move to sleep state.
	 * 	sleep: LPM0 to keep the WDT interval clock alive. */
	STATE_ACTIVATE,
	STATE_ACTIVE
};
enum states state = STATE_SLEEP;

static inline void state_machine(void)
{
	/* Holds the count of intervals until time expiration. */
	static unsigned int shutdown_counter = 0;

	/* Start the timer if the button was pressed from the sleep state,
	 * otherwise go back to sleep. */
	switch(state) {
		case STATE_SLEEP:
			/* Ensure that the LED is off. */
			LED_DEACTIVATE();
			/* Stop the watchdog to save energy. */
			WDT_DISABLE();
			/* Go into LPM4, disabling all clocks and waiting for button. */
			LPM4;

			/* If woken from LPM4, it was likely from a button press, but
			 * check to be sure. */
			if (button_pressed) {
				button_pressed = false;
				state = STATE_ACTIVATE;
			}
			break;

		/* The activate state does active mode initiatialization, then
		 * falls through to the active state. */
		case STATE_ACTIVATE:
			/* Initialize the countdown timer and activate the output. */
			shutdown_counter = LED_TIMEOUT_INTERVALS;
			LED_ACTIVATE();
			/* Restart the watchdog timer in interval mode. */
			WDT_INTERVAL_MODE();
			/* Now that the active state is configured, wait for the timer
			 * to expire or for a button press. */
			state = STATE_ACTIVE;
		case STATE_ACTIVE:
			/* If an interval has completed, process it now. */
			if (interval_complete) {
				interval_complete = false;
				/* Decrement the shutdown counter and check for time
				 * expiration. */
				if(--shutdown_counter == 0) {
					/* The timer has expired, move to the sleep state. */
					state = STATE_SLEEP;
				}
			}
			/* Go into LPM0 because we need the WDT interval clock to run. */
			LPM0;
			/* If the button is pressed in the active state, move to
			 * the sleep state. */
			if (button_pressed) {
				button_pressed = false;
				state = STATE_SLEEP;
			}
			break;

		default:
			#ifdef DEBUG_MODE_ON
				/* Trap here if we're in debug mode. */
				while(1);
			#endif
			/* This should not be possible, but fail to the sleep state. */
			state = STATE_SLEEP;
			break;
	}
}

void main(void) {
	/* Disable the watchdog as it will be used as an interval timer after
	 * a button press. */
	WDT_DISABLE();

	/* Use the maximum SMCLK divider (/8), as we don't need a fast clock. */
	BCSCTL2 = DIVS_3;

	/* Initialize unused pins of ports 1 and 2 to the recommended low
	 * power state of outputs driven low.
	 * P1.3 is the switch input (P1DIR.BIT3 = 0)
	 * P1.6 is the LED output (active low - initially off, P1OUT.BIT6 = 1) */
	P1DIR = ~(BIT3);
	P1OUT = BIT6;
	P2DIR = 0xFF;
	P2OUT = 0x00;

	/* Disable any pending P1.3 interrupts and enable the interrupt. */
	P1IFG &= ~BIT3;
	P1IE |= BIT3;

	/* Enable the watchdog interval interrupt */
	IE1 |= WDTIE;

	__enable_interrupt();

	while(1) {
		state_machine();
	}
}

#pragma vector = PORT1_VECTOR
__interrupt void port1_isr(void)
{
	/* Let the main thread know we've just completed had a button press. */
	button_pressed = true;
	/* Clear the interrupt flags */
	P1IFG = 0x0;
	/* Allow the state machine to control the sleep mode. */
	__low_power_mode_off_on_exit();
}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_isr(void)
{
	#ifdef DEBUG_MODE_ON
		/* Toggle P1.0 on every interrupt for debug. */
		P1OUT ^= BIT0;
	#endif
	/* Let the main thread know we've just completed an interval. */
	interval_complete = true;
	/* Allow the state machine to control the sleep mode. */
	__low_power_mode_off_on_exit();
}

/* An interrupt from any unused interrupt vectors does nothing */
#pragma vector = PORT2_VECTOR, /*PORT1_VECTOR,*/ USI_VECTOR, \
	ADC10_VECTOR, TIMERA1_VECTOR, TIMERA0_VECTOR, \
	/*WDT_VECTOR,*/ NMI_VECTOR
__interrupt void trapisr(void) {
	#ifdef DEBUG_MODE_ON
		/* Trap here if we're in debug mode. */
		while(1);
	#endif
}
