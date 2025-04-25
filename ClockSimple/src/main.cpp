#include <Arduino.h>

/*  Graniceri School Clock - using 2HSS57 stepper driver and a stepper motor.
    The clock is set to run at 1 second intervals, with a pulse width of 5 microseconds.
    The clock uses the Arduino delay function to control the timing of the pulses.
    The clock is set to run in a clockwise direction.
    The clock is set to run at a speed of 179 microseconds between pulses.
    The clock is set to run at a speed of 984 microseconds for precision.
 *
**/
#define STEPPER_PULSE_PIN    8   // Arduino pin for driver motor comand
#define STEPPER_DIR_PIN      9   // Arduino pin for driver motor direction
#define STEPPER_PULSE_TIME   5   // Time in microsec for stepper pulse
#define NORMAL_TIME_CLOCK  179   // Delay between pulses to set correct time clock
#define CLOCK_PRECISION     984   // Clock precision in microsec


void setup() {
  // configure pins:
  pinMode(STEPPER_PULSE_PIN,OUTPUT);    // Set pin for stepper command
  pinMode(STEPPER_DIR_PIN,OUTPUT);      // Set pin for stepper direction
  
  // set pins accordingly to 2HSS57 driver specs:
  digitalWrite(STEPPER_DIR_PIN, HIGH);  // Set direction to clockwise
  delayMicroseconds(10);                // This delay 10 microseconds between set direction and puls command for correct direction set
  digitalWrite(STEPPER_PULSE_PIN, HIGH);// Stepper commands are in common anode set so, high means STOP!
}

void loop() {
  digitalWrite(STEPPER_PULSE_PIN, LOW);    // Start stepper pulse
  delayMicroseconds(STEPPER_PULSE_TIME);   // The pulse is on
  digitalWrite(STEPPER_PULSE_PIN, HIGH);   // Stop stepper pulse (pulse is off)
  delay(NORMAL_TIME_CLOCK);                // Clock function parameter
  delayMicroseconds(CLOCK_PRECISION);      // Clock precision 
}
