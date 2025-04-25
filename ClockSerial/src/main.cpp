#include <Arduino.h>

/*  Ceas Scoala Graniceri - serial
 *
**/

#define BUTTON_PIN          11   // Arduino pin for switch button
#define LED_PIN             12   // Arduino pin for LED
#define STEPPER_PULSE_PIN    8   // Arduino pin for driver motor comand
#define STEPPER_DIR_PIN      9   // Arduino pin for driver motor direction
#define STEPPER_PULSE_TIME   5   // Time in microsec for stepper pulse
#define NORMAL_TIME_CLOCK  178   // Delay between pulses to set correct time clock
#define FAST_MOVING_CLOCK    1   // Delay between pulses to clock faster
#define ONE_MINUTES_STEPS  333   // Number of steps for one minute
#define TWELVE_HOURS       720   // Number of minutes in 12 hour

void setup() {
  // start serial connection
  Serial.begin(9600);                   // Set serial for clock comands to set correct time
  Serial.println("Program started ...");
  Serial.println("  write f for cw direction." );
  Serial.println("  write b for ccw direction." );
  Serial.println("  type a number of minutes to set the clock quickly." );
  
  // configure pins:
  pinMode(STEPPER_PULSE_PIN,OUTPUT);    // Set pin for stepper command
  pinMode(STEPPER_DIR_PIN,OUTPUT);      // Set pin for stepper direction
  
  // set pins accordingly to 2HSS57 driver specs:
  digitalWrite(STEPPER_DIR_PIN, HIGH);  // Set direction to clockwise
  delayMicroseconds(10);                // This delay 10 microseconds between set direction and puls command for correct direction set
  digitalWrite(STEPPER_PULSE_PIN, HIGH);// Stepper commands are in common anode set so, high means STOP!
}

void loop() {
  if(Serial.available()){
    long int minutes_number = 0;
    long int steps;
    int count = 0;
    String command = Serial.readStringUntil('\n');
    command.trim();
    if(minutes_number = command.toInt()){     // Convert Strig to Integer
      if(minutes_number > TWELVE_HOURS){      // No more than 12 hours
        minutes_number = TWELVE_HOURS;
      }
      
      minutes_number *= ONE_MINUTES_STEPS;    // Total number of steps

      for(steps = 0; steps < minutes_number; steps++){// helping to set the clock time fast
        digitalWrite(STEPPER_PULSE_PIN, LOW);    // Start stepper pulse
        delayMicroseconds(STEPPER_PULSE_TIME);   // The pulse is on
        digitalWrite(STEPPER_PULSE_PIN, HIGH);   // Stop stepper pulse (pulse is off)
        delay(FAST_MOVING_CLOCK);                // Delay for fast moving
        if(!(steps % ONE_MINUTES_STEPS)){ // it is use for feedback: 
          count++;                        //    it count and print minutes 
          Serial.print(count);            //    through serial monitor.
          Serial.print(", ");
        }
      }

      digitalWrite(STEPPER_DIR_PIN, HIGH);    // Direction of clock is clockwise
      Serial.println("Ready.\nCW direction from now!");
    
    }else{
      if(command == "f"){
        digitalWrite(STEPPER_DIR_PIN, HIGH);    // Direction of clock is clockwise
        Serial.println("CW direction.");
      }
      if(command == "b"){
        digitalWrite(STEPPER_DIR_PIN, LOW);    // Direction of clock is counterclockwise
        Serial.println("CCW direction.");
      }
    }

  }
  digitalWrite(STEPPER_PULSE_PIN, LOW);    // Start stepper pulse
  delayMicroseconds(STEPPER_PULSE_TIME);   // The pulse is on
  digitalWrite(STEPPER_PULSE_PIN, HIGH);   // Stop stepper pulse (pulse is off)
  delay(NORMAL_TIME_CLOCK);                // Clock precision
}
