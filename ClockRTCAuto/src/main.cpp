/*  Last update: 2025-04-14
  The code adjusts the time of a clock using a stepper motor and an RTC DS3231 module (connected: SCL -> A5, SDA -> A4).
  It reads the current time from the RTC, saves it to EEPROM during power interruptions,
  and adjusts the clock hands when power is restored.
  Activate DEBUGGING to see the serial output.
  The code also includes a serial interface for manual control of the clock hands and EEPROM operations.
    The serial commands are:
        fwd     - move clock cw with num1*60 + num2 minutes
        bwd     - move clock ccw with num1*60 + num2 minutes
        ret     - read and display hour and minute saved to EEPROM index
        reb     - read and display bytes from num1 to num2 from EEPROM
        rtc     - read and display time and date form RTC
        wes     - write variable writeToEEPROMStatus with num1
        wrt     - write time to RTC from host computer
        wet     - write to EEPROM num1, num2 at num3
        web     - write to EEPROM bytes from num1 to num2 with num3 value
        reset   - reset all EEPROM Bytes to 0xFF (255) value

    commands, num1, num2 are mandatory and separated by space, num3 can be optional

    type command num1 num2 num3

    command: fwd/bwd/rtc/reb/ret/wrt/wet/reset

    num1: hour    (0 -  12) or indexToStart (0-1023)
    num2: minutes (0 -  59) or indexToStop (0-1023)
    num3: value   (0 - 255) or indexToWrite (0-1023)
    example: fwd 12 0 2   hit enter to send (means forward 12 hours and 0 minutes, num3 is ignored in this case)
*/

#include <main.h>

/* Define global variables */
#ifdef DEBUGGING
const char daysOfTheWeek[7][10] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    // "Duminica", "Luni", "Marti", "Miercuri", "Joi", "Vineri", "Sambata"};
#endif

void debugSerial(void);
void handlePowerDown(void);
void saveTime(void);
uint8_t writeToEEPROM(uint16_t index, uint8_t currentHour, uint8_t currentMinute);
void resetEEPROM(void);
void resetCounter(void);
void handlePowerUp(void);
uint8_t setClock(void);
void moveClockHands(uint8_t directionToMove, uint16_t minuteToStep);

static bool saveTimeToEEPROM = false;   /* true means hour and minutes was save to EEPROM */
static boolean powerDown = false;       /* true means power down detected - Clock has no power */
static uint8_t writeToEEPROMStatus = 0; /* Status of the write operation */

RTC_DS3231 rtc;

/***************************************************************************************************************
 * @brief Setup function that initializes the RTC, EEPROM, and pin modes.
 * @param None
 * @return None
 * @note This function is called once at the beginning of the program to set up the necessary components.
 */
void setup()
{
#ifdef DEBUGGING
    Serial.begin(9600); /* Initialize serial communication for debugging */
    while (!Serial)
        /* Wait for serial connection to be established */;
#endif

    if (!rtc.begin())
    {
#ifdef DEBUGGING
        Serial.println("Couldn't find RTC!"); /* Print error message if RTC is not found */
        Serial.flush();
#endif
        while (1)
            delay(10); /* Stop the program if RTC is not found */
    }

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW); /* Turn off the built-in LED */

    pinMode(POWER_DOWN_PIN, INPUT);
    pinMode(STEPPER_DRIVER_ENABLE_PIN, OUTPUT);
    digitalWrite(STEPPER_DRIVER_ENABLE_PIN, LOW); /* Activate stepper motor driver */

    pinMode(STEPPER_PULSE_PIN, OUTPUT);
    pinMode(STEPPER_DIR_PIN, OUTPUT);
    digitalWrite(STEPPER_DIR_PIN, LOW); /* Set default direction (CW) */
   
    /* Reset EEPROM if counter exceeds limit */
    if (EEPROM[0] > 0x03u)
    {
        resetCounter();
    }

    /* Check if time is allready saved in EEPROM in case of uC reset. */
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
    uint8_t recoveryFlag = CHECK_TIME_RECOVER_FLAG(EEPROM[index]);
    
    if (0 != recoveryFlag)
    { /* Check if the MSB bit is set, indicating that time was not recovery yet. */
        saveTimeToEEPROM = true;
        powerDown = true; /* Set powerDown to true to avoid immediate power down handling */
        Serial.println("recoveryFlag Conditional");
    }
    // rtc.adjust(DateTime(2025, 4, 13, 18, 57, 10));
}

/***************************************************************************************************************
 * @brief Main loop function that checks for power down and handles clock adjustments.
 * @param None
 * @return None
 * @note This function continuously checks the power state and handles power down and power up events.
 *       It also includes a debounce mechanism to avoid false triggers during power fluctuations.
 */
void loop()
{
    static uint32_t lastDebounceTime = 0;
    static uint8_t lastPowerCheckState = HIGH;

    uint8_t currentPowerCheckState = digitalRead(POWER_DOWN_PIN);

    /* Handle debounce logic */
    if (currentPowerCheckState != lastPowerCheckState)
    {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
    {
        powerDown = (currentPowerCheckState == LOW);
    }

    lastPowerCheckState = currentPowerCheckState;

    if (0u == writeToEEPROMStatus)
    {
        if (true == powerDown)
        {
            handlePowerDown();
        }
        else
        {
            handlePowerUp();
        }
    }
#ifdef DEBUGGING
    debugSerial();
#endif
}

/***************************************************************************************************************
 * @brief Handle power down events and save the current time to EEPROM.
 * @param None
 * @return None
 * @note This function checks if the power is down and saves the current time to EEPROM.
 */
void handlePowerDown(void)
{
    if (true != saveTimeToEEPROM)
    {
#ifdef DEBUGGING
        Serial.println("Write EEPROM");
#endif
        saveTime(); /* Save RTC data to EEPROM */
    }
}

/***************************************************************************************************************
 * @brief Save the current time to EEPROM when power down is detected.
 * @param None
 * @return None
 * @note This function saves the current hour and minute to EEPROM at the specified index.
 *       It also sets a flag to indicate that time has been saved to EEPROM and never use it. When the power is up,
 *        the flag is reset.
 */
void saveTime(void)
{
    DateTime currentTime = rtc.now();
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
    uint8_t currentHour = currentTime.hour();
    uint8_t currentMinute = currentTime.minute();

    if (index > EEPROM_MAX_USE_SIZE)
    {
        resetCounter(); /* Reset EEPROM if index exceeds limit (1024 - 2 Bytes) */
        index = 2; /* Reset index to EEPROM[2] where start the savings. */
    }
    else
    {
        index += 2;
    }

    /* Convert to 12-hour format */
    if (currentHour > 12)
    {
        currentHour -= 12;
    }
    else if (currentHour == 0)
    {
        currentHour = 12;
    }

    currentHour = SET_TIME_RECOVER_FLAG(currentHour); /* Set the MSB bit 8 to signal time is ready for recover */

    writeToEEPROMStatus = writeToEEPROM(index, currentHour, currentMinute); /* Write current time to EEPROM */

    saveTimeToEEPROM = true; /* Set flag to indicate time has been saved */
}

/***************************************************************************************************************
 * @brief Write time to EEPROM with error handling and verification.
 * @param index: 2 Bytes EEPROM index to write to.
 * @param currentHour: hour when power down was detected.
 * @param currentMinute: minute when power down was detected.
 * @return status of the write operation:
 *                  0: EEPROM write successful,
 *                  1: failed to write in EEPROM.
 *
 * @note This function writes the current hour and minute to the EEPROM at the specified index.
 * It also writes the upper and lower bytes of the index to the first two EEPROM locations.
 * The function checks for successful write operations by reading it back to verify that the value
 *  was written correctly and prints error messages if any write fails if DEBUGGING is enabled.
 *
 * The function uses the EEPROM library to write data to the EEPROM.
 */
uint8_t writeToEEPROM(uint16_t index, uint8_t currentHour, uint8_t currentMinute)
{
    /* Write upper and lower bytes of the index */
    EEPROM.write(0, UPPER_BYTE(index));
    if (EEPROM.read(0) != UPPER_BYTE(index))
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write upper byte of index to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

    EEPROM.write(1, LOWER_BYTE(index));
    if (EEPROM.read(1) != LOWER_BYTE(index))
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write lower byte of index to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

    /* Write current hour */
    EEPROM.write(index, currentHour);
    if (EEPROM.read(index) != currentHour)
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write current hour to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

    /* Write current minute next to hour */
    EEPROM.write(index + 1, currentMinute);
    if (EEPROM.read(index + 1) != currentMinute)
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write current minute to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

#ifdef DEBUGGING
    Serial.println("EEPROM write successful.");
#endif
    return SUCCESS; /* Return success status */
}

/***************************************************************************************************************
 * @brief Reset all bytes in EEPROM to 0xFF (255).
 * @param None
 * @return None
 * @note This function resets all bytes in the EEPROM to 0xFF (255) to clear the stored data.
 *       It is used to reset the EEPROM when needed.
 */
void resetEEPROM(void)
{
    for (size_t i = 0; i < 1024; i++)
    {
        /* code */
        EEPROM.write(i, 255); /* Reset all bytes in EEPROM */
    }
}

/***************************************************************************************************************
 * @brief Reset the EEPROM counter to 0.
 * @param None
 * @return None
 * @note This function resets the upper and lower bytes of the index in EEPROM to 0.
 *       It is used to clear the EEPROM data when the index exceeds the maximum size.
 */
void resetCounter(void)
{
    EEPROM.write(0, 0); /* Reset upper byte of index */
    EEPROM.write(1, 0); /* Reset lower byte of index */
}

/***************************************************************************************************************
 * @brief Handle power-up events and adjust the clock hands accordingly.
 * @param None
 * @return None
 * @note This function checks if the power is up and adjusts the clock hands using the stepper motor.
 *       If the power is down, it saves the current time to EEPROM. The function uses a stepper motor
 *       to move the clock hands in the specified direction (CW or CCW) for a specified duration.
 */
void handlePowerUp(void)
{
    uint8_t setClockFlag = SUCCESS; /* Flag to indicate if the clock was set correctly. */
    if (false == saveTimeToEEPROM)
    { /* Normal clock function */
        digitalWrite(STEPPER_PULSE_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_TIME);
        digitalWrite(STEPPER_PULSE_PIN, LOW);
        delay(STEPPER_DELAY_TIME);
        delayMicroseconds(FINE_TIME_ADJUSTMENT);
    }
    else
    { /* Adjust clock using EEPROM data */
        if (!powerDown)
        { /* Check if the power is up */
#ifdef DEBUGGING
        Serial.println("Read EEPROM");
#endif
        setClockFlag = setClock();

        if (SUCCESS != setClockFlag)
        {
            writeToEEPROMStatus = ERROR; /* Set error status */
        }
        
        }
    }
}

/***************************************************************************************************************
 * @brief Adjust the clock hands using the saved time from EEPROM.
 * @param None
 * @return None
 * @note This function retrieves the current time from the RTC and compares it with the saved time
 *        in EEPROM. It calculates the difference in hours and minutes and moves the clock hands
 *        accordingly using the stepper motor. The function also resets the stepper motor direction
 *        to default (CW) and resets the flag indicating that time recovery is complete.
 */
uint8_t setClock(void)
{ /* Adjust clock using EEPROM data */
    DateTime currentTime = rtc.now();
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
    uint8_t currentHour = currentTime.hour();
    uint8_t currentMinute = currentTime.minute();
    uint8_t savedHour = EEPROM[index];
    uint8_t savedMinute = EEPROM[index + 1];
    uint8_t directionToMove = CW_DIR;
    uint16_t minuteToStep = 0;

    /* Clear the MSB bit of the saved hour to indicate recovery is complete */
    savedHour = CLEAR_TIME_RECOVER_FLAG(savedHour);
    /* Robustness for a new EEPROM (all 0xFFu) */
    if (savedHour > 12)
    { /* Robustness for an invalid hour. */
        return ERROR;
    }

    EEPROM[index] = savedHour;

    /* Convert to 12-hour format */
    currentHour = (currentHour == 0) ? 12 : (currentHour > 12 ? currentHour - 12 : currentHour);

    /* Calculate hour difference */
    uint8_t hourDifference = (currentHour > savedHour) ? currentHour - savedHour : savedHour - currentHour;

    if (hourDifference >= 6)
    {
        hourDifference = 12 - hourDifference;
        directionToMove = (currentHour > savedHour) ? CCW_DIR : CW_DIR;
    }
    else
    {
        directionToMove = (currentHour > savedHour) ? CW_DIR : CCW_DIR;
    }

    /* Convert the difference from hours to minutes. */
    minuteToStep = hourDifference * 60;
    Serial.print("Hour difference: ");
    Serial.println(hourDifference); 

    /* Calculate minute difference */
    uint8_t minuteDifference = (currentMinute >= savedMinute) ? currentMinute - savedMinute : savedMinute - currentMinute;

    if (directionToMove == CW_DIR)
    {
        if (currentMinute >= savedMinute)
        {
            minuteToStep = minuteToStep + minuteDifference;
        }
        else
        {
            minuteToStep = (minuteToStep >= minuteDifference) ? minuteToStep - minuteDifference : minuteDifference - minuteToStep;
        }
        
    }
    else
    {
        if (currentMinute >= savedMinute)
        {
            minuteToStep = (minuteToStep >= minuteDifference) ? minuteToStep - minuteDifference : minuteDifference - minuteToStep;
        }
        else
        {
            minuteToStep = minuteToStep + minuteDifference;
        }
    }


    /* Move the hands of the clock with the respective minutes. */
    moveClockHands(directionToMove, minuteToStep);
    /* Reset the stepper motor direction to default (CW) */
    digitalWrite(STEPPER_DIR_PIN, CW_DIR);
    /* Reset the flag indicating time recovery is complete */
    saveTimeToEEPROM = false;
#ifdef DEBUGGING
    Serial.print("Clock adjustment complete! ");
#endif
    return SUCCESS;
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
void moveClockHands(uint8_t directionToMove, uint16_t minuteToStep)
{
    digitalWrite(STEPPER_DIR_PIN, directionToMove); /* Set direction */
    uint16_t steps = ((minuteToStep * 83) + (minuteToStep / (uint16_t)3)); /* Convert minutes to steps */

    for (uint16_t i = 0; i < steps; i++)
    {
        digitalWrite(STEPPER_PULSE_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_TIME);
        digitalWrite(STEPPER_PULSE_PIN, LOW);
        delay(STEPPER_FAST_TIME_ADJUSTMENT);
    }

    digitalWrite(STEPPER_DIR_PIN, CW_DIR); /* Set direction cw */
}

/***************************************************************************************************************
 * @brief Handle serial commands for debugging and manual control.
 * @param None
 * @return None
 * @note This function reads commands from the serial interface and performs various operations
 *       such as moving the clock hands, reading/writing to EEPROM, and adjusting the RTC time.
 */
void debugSerial(void)
{
    if (Serial.available())
    {
        String command = Serial.readStringUntil(' ');
        uint16_t num1 = Serial.parseInt();
        uint16_t num2 = Serial.parseInt();
        uint16_t num3 = Serial.parseInt();
        uint16_t minuteToStep = (num1 * 60) + num2;

        if (command == "fwd")
        {
            Serial.print("CW direction for: ");
            Serial.print(minuteToStep);
            Serial.println(" minutes");

            moveClockHands(CW_DIR, minuteToStep);
            
            Serial.println("Done!");
            Serial.println("");
        }

        if (command == "bwd")
        {
            Serial.print("CCW direction for: ");
            Serial.print(minuteToStep);
            Serial.println(" minutes");

            moveClockHands(CCW_DIR, minuteToStep);
            
            Serial.println("Done!");
            Serial.println("");
        }
        
        if (command == "ret")
        {
            uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
            
            Serial.println("read EEPROM: ");
            Serial.print("index: ");
            Serial.println(index);
            Serial.print("Hour: ");
            Serial.println(EEPROM[index]);
            Serial.print("Minute: ");
            Serial.println(EEPROM[index + 1]);
            Serial.println("Done!");
            Serial.println("");
        }

        if (command == "reb")
        {
            Serial.println("read bytes from EEPROM: ");

            for (; num1 < num2; num1++)
            {
                Serial.print(EEPROM[num1]);
                Serial.print(", ");
            }
            
            Serial.println("Done!");
            Serial.println("");
        }

        if (command == "rtc")
        { /* Read RTC */
            DateTime currentTime = rtc.now();

            Serial.print("Current time: ");
            Serial.print(currentTime.hour(), DEC);
            Serial.print(":");
            Serial.print(currentTime.minute(), DEC);
            Serial.print(":");
            Serial.println(currentTime.second(), DEC);
            Serial.print("Current date: ");
            Serial.print(currentTime.day(), DEC);
            Serial.print('/');
            Serial.print(currentTime.month(), DEC);
            Serial.print('/');
            Serial.println(currentTime.year(), DEC);
            Serial.println(daysOfTheWeek[currentTime.dayOfTheWeek()]);
            Serial.println("Done!");
            Serial.println("");
        }

        if (command == "wes")
        {
            Serial.print("Write variable writeToEEPROMStatus with: ");
            Serial.println(num1);

            writeToEEPROMStatus = num1;

            Serial.println("Done!");
            Serial.println("");
        }

        if (command == "wrt")
        { /* Write RTC */
            // rtc.adjust(DateTime(2025, 4, 13, num1, num2, 0));
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            Serial.println("RTC time was set.");
        }

        if (command == "wet")
        {
            uint8_t retval = SUCCESS;
            Serial.println("write time to EEPROM ");

            num1 = SET_TIME_RECOVER_FLAG(num1); /* Set the MSB bit 8 to signal time is ready for recover */

            retval = writeToEEPROM(num3, num1, num2);

            Serial.print("Done with status ");
            if (retval == SUCCESS)
            {
                Serial.println("successful");
            }
            else
            {
                Serial.println("error");
            }
            
            Serial.println("");
        }

        if (command == "web")
        {
            Serial.println("Write bytes to EEPROM");

            for (; num1 < num2; num1++)
            {
                EEPROM[num1] = num3; /* Write num3 value to EEPROM */
            }

            Serial.println("Done!");
            Serial.println("");
        }

        if (command == "reset")
        {
            Serial.println("Reset all bytes to EEPROM to 0xFF");

            resetEEPROM();
            
            Serial.println("Done!");
            Serial.println("");
        }
    }
}
