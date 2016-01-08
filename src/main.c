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

/* The LED output is on P1.6. It is active low. */
#define LED_ACTIVATE()		(P1OUT &= ~BIT6)
#define LED_DEACTIVATE()	(P1OUT |= BIT6)

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

	/* Start with the output disabled */
	LED_DEACTIVATE();

	/* Disable any pending P1.3 interrupts and enable the interrupt. */
	P1IFG &= ~BIT3;
	P1IE |= BIT3;

	/* Enable the watchdog interval interrupt */
	IE1 |= WDTIE;

	__enable_interrupt();

	/* Go into LPM4 because we start in the idle state until the
	 * button is pressed. */
	LPM4;

	/* Once we're past the first button press, utilize LPM0 while we're
	 * waiting for the countdown to complete. At that time, the final timer
	 * interrupt will hold us in LPM4 mode on exit until the next button
	 * press. */
	while(1) {
		LPM0;
	}
}

/* This is variable safe, provided that interrupts are not nestable, as is the
 * case here. */
unsigned int shutdown_counter = 0;

#pragma vector = PORT1_VECTOR
__interrupt void port1_isr(void)
{
	/* Start the countdown timer and activate the output. */
	shutdown_counter = LED_TIMEOUT_INTERVALS;
	LED_ACTIVATE();
	/* Restart the watchdog as it was stopped on the last expiration. */
	WDT_INTERVAL_MODE();
	/* Clear the interrupt flags */
	P1IFG = 0x0;
	/* Remain awake after the interrupt exits. The main loop will keep us in
	 * a lower power state until the timer expires. */
	__low_power_mode_off_on_exit();
}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_isr(void)
{
	/* Decrement the shutdown counter and check for time expiration. */
	if(--shutdown_counter == 0) {
		/* Time is up, deactivate the output and go into a deep sleep. */
		LED_DEACTIVATE();
		/* Stop the watchdog to save energy and it's no longer required. */
		WDT_DISABLE();
		/* Exit the interrupt to LPM4. */
		LPM4_EXIT;
	}
#ifdef DEBUG_MODE_ON
	/* Toggle P1.0 on every interrupt for debug. */
	P1OUT ^= BIT0;
#endif
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
