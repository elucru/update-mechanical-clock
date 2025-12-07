# update-mechanical-clock
## Replace the old mechanism with a stepper motor
The project is to breathe life into the old clock from the Primary School, by replacing the old mechanism (which has missing parts and is non-functional) with a microcontroller (Arduino) controlled mechanism.
- Clock face, clock minute and hour indicators, and the mechanism of hours remained the same.
- The stepper motor, Nema 23 with encoder, is connected directly to the minute indicator via a 50:1 gearbox, because the minute indicator alone is almost a meter and several pounds, plus exposure to strong wind, ice deposits ...
- Arduino Uno drives a Hybrid Step Servo 2HSS57.

The project is being implemented in several phases:
- In the first phase, I use a very simple program to rotate the minutes using the gearbox, but programmed to rotate the minute hand 360 degrees in an hour. Setting the clock is done serially from a Laptop with another program.
- In the second phase, we use an RTC to keep time up to date wheh power go down, an resistive devider to signal when power go down. Arduino is connected to an UPS and when the main power goes down, save the time in EEPROM. When power is restored, read the time from EEPROM compare with the time from RTC and set the clock accordingly.
- In the third phase, we want to use a feedback (Hall sensor and a magnet set at 12 o'clock) and to set different songs to sing like an announcements.

## Technical data
Next Time ... until then if you have some questions:

elucru@gmail.com

