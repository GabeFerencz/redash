# redash - Reviving Rainbow Dash

My daughter had a light up Rainbow Dash night light. On a button press, the 
LED came on for a period of time, then turned off. For the first month or so 
it worked intermittently. I was typically able to give Rainbow Dash a bit of 
percussive maintenance (AKA whack it on the side) to get it working again. 
Once I was no longer able to get it working, it was time for a tear down. The 
drive FET and other components still work. It appears that the epoxied chip 
is no longer functioning, likely due to the warping of the thin PCB.

I could spend $14 or so to replace it on Amazon, but what kind of engineer 
would that make me? I first considered using a 555 timer to activate the LED 
for a period of time. I decided that I may want additional functionality that 
would make the 555 impractical. I have several MSP430 Launchpad evaluation 
boards sitting around (because hey, they were only $4.30), so I instead 
decided to go that route. They were cheap, handy, and have some nice low 
power modes. In the interest of moving the project along, I'm starting with 
the minimum viable project: press the button and the LED will remain lit for 
15 minutes.

# Hardware
* Rainbow Dash mold with battery case and mounted LED [reuse]
* MSP430G2231 (Using the just the IC part from the Launchpad)

## Microcontroller Power
* VBAT - direct to micro from battery case (Add decoupling/bulk capacitor?)
* GND - direct to micro from battery case

## Microcontroller Inputs
* SWITCH - momentary pushbutton between P1.3 and ground

## Microcontroller Outputs
* LED - pulled high through resistor to battery, P1.6 active low through micro

# Software
* Written/tested against CodeComposerStudio MSP430 v4.1.2
* No special requirements for the project, so only the source is included (of 
which there isn't much)
