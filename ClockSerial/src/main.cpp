#include <Arduino.h>

/*  Graniceri School Clock - serial */

#define STEPPER_PULSE_PIN    8   // Arduino pin for driver motor comand
#define STEPPER_DIR_PIN      9   // Arduino pin for driver motor direction
#define STEPPER_PULSE_TIME   5   // Time in microsec for stepper pulse
#define NORMAL_TIME_CLOCK  178   // Delay between pulses to set correct time clock
#define FAST_MOVING_CLOCK    1   // Delay between pulses to clock faster
#define STEPPER_FAST_TIME_ADJUSTMENT    100     /* Time in miliseconds for stepper pulse to adjust the time faster */
#define ONE_MINUTES_STEPS  333   // Number of steps for one minute
#define TWELVE_HOURS       720   // Number of minutes in 12 hour

#define CW_DIR                          LOW     /* Clockwise direction */
#define CCW_DIR                         HIGH    /* Counterclockwise direction */

void moveClockHands(uint8_t directionToMove, uint16_t secondsToStep);

void setup() {
  // start serial connection
  Serial.begin(9600);                   // Set serial for clock comands to set correct time
  Serial.println("Program started ...");
  Serial.println("  write f for cw direction." );
  Serial.println("  write b for ccw direction." );
  Serial.println("  type a number of minutes to set the clock quickly." );
  
  // configure pins:
  pinMode(STEPPER_PULSE_PIN, OUTPUT);    // Set pin for stepper command
  pinMode(STEPPER_DIR_PIN, OUTPUT);      // Set pin for stepper direction
  
  // set pins accordingly to 2HSS57 driver specs:
  digitalWrite(STEPPER_DIR_PIN, HIGH);  // Set direction to clockwise
  delayMicroseconds(10);                // This delay 10 microseconds between set direction and puls command for correct direction set
  digitalWrite(STEPPER_PULSE_PIN, HIGH);// Stepper commands are in common anode set so, high means STOP!
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil(' ');
    uint16_t num1 = Serial.parseInt();
    uint16_t num2 = Serial.parseInt();
    uint16_t num3 = Serial.parseInt();
    uint16_t secondsToStep = ((num1 * 3600) + (num2 * 60) + num3); /* Convert hours and minutes to seconds */

    if (command == "fwd")
    {
        Serial.print("CW direction for: ");
        Serial.print(secondsToStep);
        Serial.println(" seconds");

        moveClockHands(CW_DIR, secondsToStep);
        
        Serial.println("Done!");
        Serial.println("");
    }

    if (command == "bwd")
    {
        Serial.print("CCW direction for: ");
        Serial.print(secondsToStep);
        Serial.println(" seconds");

        moveClockHands(CCW_DIR, secondsToStep);
        
        Serial.println("Done!");
        Serial.println("");
    }
  }

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

/***************************************************************************************************************
 * @brief Move the clock hands fast using the stepper motor.
 * @param directionToMove: Direction to move the clock hands (CW or CCW).
 * @param minuteToStep: Number of minutes to move the clock hands.
 * @return None
 * @note This function moves the clock hands very fast using the stepper motor in the specified
 *       direction for the specified number of minutes to set the clock hands correctly.
 *      The function calculates the number of steps required and sends pulse signals to the stepper motor driver.
 */
void moveClockHands(uint8_t directionToMove, uint16_t secondsToStep)
{/* 80.000 de microsteps (1/16) for one turn */
    digitalWrite(STEPPER_DIR_PIN, directionToMove); /* Set direction */
    uint32_t steps = (uint32_t)(((uint32_t)secondsToStep * 22) + ((uint32_t)(secondsToStep) / (uint32_t)4)); /* Convert minutes to steps */

    for (uint32_t i = 0; i < steps; i++)
    {
        digitalWrite(STEPPER_PULSE_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_TIME);
        digitalWrite(STEPPER_PULSE_PIN, LOW);
        delayMicroseconds(STEPPER_FAST_TIME_ADJUSTMENT);
    }
    digitalWrite(STEPPER_DIR_PIN, CW_DIR); /* Set direction cw */
}
