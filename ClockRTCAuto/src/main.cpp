/*  Last update: 2025-06-21
  This code is mainly to power up a stepper motor connect to a mechanical clock throuth a 1:25 gear. 
  It use and an RTC DS3231 module (connected: SCL -> A5, SDA -> A4) to reads the current time,
  and saves it to EEPROM during power interruptions. It adjusts the clock hands when power is restored.

  The code also includes a serial interface for debugging, manual control of the clock hands and EEPROM
  operations, if DEBUGGING is activated.
    The serial commands are:
        fwd     - move clock cw with num1*60 + num2 minutes
        bwd     - move clock ccw with num1*60 + num2 minutes
        ret     - read and display hour and minute saved to EEPROM index
        reb     - read and display bytes from num1 to num2 from EEPROM
        rtc     - read and display time and date form RTC
        wet     - write to EEPROM num1, num2 at num3
        web     - write to EEPROM bytes from num1 to num2 with num3 value
        reset   - reset all EEPROM Bytes to 0xFF (255) value

    type: command [num1] [num2] [num3]
Where:
    command is one of folowing: fwd, bwd, rtc, reb, ret, wet, web, reset
    num1: hour (0 -  12) or indexToStart (0-1023)
    num2: minutes (0 - 59) or indexToStop (0-1023)
    num3: seconds (0 - 59) or indexToWrite (0-1023)
    
example: fwd 12 0 2   hit enter to send (means forward 12 hours and 0 minutes, num3 is ignored in this case)
*/

#include <main.h>

/* Define global variables */
#ifdef DEBUGGING
const char daysOfTheWeek[7][10] = {
    // "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    "Duminica", "Luni", "Marti", "Miercuri", "Joi", "Vineri", "Sambata"};
#endif
static bool saveTimeToEEPROM = false;           /* true means hour and minutes was save to EEPROM */
static boolean powerDown = false;               /* true means power down detected - Clock has no power */
static uint8_t writeToEEPROMStatus = SUCCESS;   /* Status of the write operation ERROR (1) means that the EEPROM is failing, broken or burned. */

RTC_DS3231 rtc;

/* Functions prototipes */
void handlePowerDown(void);
void handlePowerUp(void);
void saveTime(void);
void resetEEPROM(void);
void resetCounters(void);
void moveClockHands(uint8_t directionToMove, uint16_t secondsToStep);
#ifdef DEBUGGING
void debugSerial(void);
void printFormatedDateAndTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year);
#endif
uint8_t setClock(void);
uint8_t writeToEEPROM(uint16_t index, uint8_t currentHour, uint8_t currentMinute, uint8_t currentSecond, uint8_t currentDay, uint8_t currentMonth, uint8_t currentYear);


/***************************************************************************************************************
 * @brief Setup function initializes the RTC, EEPROM, and uC pin modes.
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

#ifdef DEBUGGING
    Serial.println("Serial communication ready!");
#endif

    /* Check if the RTC work properly, otherwhise it will stop the program to run. */
    if (!rtc.begin())
    {
#ifdef DEBUGGING
        Serial.println("Couldn't find RTC!");
        Serial.flush();
#endif
        while (1)
        {
            delay(10); /* Stop the program if RTC is not found */
            if (rtc.begin())
            {/* Start the program if RTC is found */
                Serial.println("RTC OK now!");
                break;
            }
        }
    }

#ifdef DEBUGGING
    Serial.println("RTC ready!");
#endif

    /* Turn off the built-in LED to save some energy or ... :) */
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    /* Stepper and power down setup */
    pinMode(POWER_DOWN_PIN, INPUT);
    pinMode(STEPPER_DRIVER_ENABLE_PIN, OUTPUT);
    digitalWrite(STEPPER_DRIVER_ENABLE_PIN, LOW); /* Activate stepper motor driver */

    pinMode(STEPPER_PULSE_PIN, OUTPUT);
    pinMode(STEPPER_DIR_PIN, OUTPUT);
    digitalWrite(STEPPER_DIR_PIN, LOW); /* Set default direction (CW) */

    /* EEPROM is used for saving date and time in case of power down. */
    /* Read index from EEPROM for robustness and to verify if was an reset or power down. */
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1]; 

    /* Check if index exceeds EEPROM size */
    if (index > EEPROM_MAX_USE_SIZE)
    {/* Reset EEPROM if index exceeds limit (1024) or is a new uC. */
        resetCounters();
        EEPROM[1u] = 4u;/* This will be incremented with 3 at first power down, so time will be saved in EEPROM[7]! */
    }
    else
    {/* Check if time is allready saved in EEPROM. 
        In case of uC reset don't save time in EEPROM again, just normal clock action. */
        uint8_t recoveryFlag = CHECK_TIME_RECOVER_FLAG(EEPROM[index]);
        
        if (false != recoveryFlag)
        { /* Check if the MSB bit is set, indicating that time was not recovery yet. */
            saveTimeToEEPROM = true;
            powerDown = true; /* Set powerDown to true to avoid immediate power down handling */
    #ifdef DEBUGGING
            Serial.println("recoveryFlag on, time is ready to recover!");
    #endif
        }
    }

/* To set the RTC time and date, choose one option from: 
Manual adjust date and time: */
    // rtc.adjust(DateTime(2025, 4, 13, num1, num2, 0));
/* or automatic adjust date and time */
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
/* or automatic adjust date and time and compensate 8 seconds lost in flashing */
    // DateTime compileTime(F(__DATE__), F(__TIME__));
    // DateTime adjustedTime = compileTime + TimeSpan(0, 0, 0, 8); // Add 8 seconds
    // rtc.adjust(adjustedTime);

#ifdef DEBUGGING
    Serial.println("Program started!");
#endif
}

/***************************************************************************************************************
 * @brief Main loop function that checks for power down and handles clock and clock adjustments.
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
    {/* The POWER_DOWN_PIN state has been changed, monitoring the duration of the change begins. */
        lastDebounceTime = millis();
        if (lastDebounceTime >= MAX_MILLIS_IN_DELAY)
        {
            lastDebounceTime = 0u; /* Reset if overflow is less then 5 seconds. */
        }  
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
    {/* Only the change above the DEBOUNCE_DELAY threshold is taken into account, to avoid spikes and short power downs. */
        powerDown = (currentPowerCheckState == LOW);
    }
    /* Update the POWER_DOWN_PIN state. */
    lastPowerCheckState = currentPowerCheckState;

    if (SUCCESS == writeToEEPROMStatus)
    {/* If the last save in EEPROM is corrupted or unusable the clock stops. */
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
 * @note This function checks if the time is allready saved in EEPROM otherwhise it saves it.
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
 * @brief Save the current date and time in EEPROM.
 * @param None
 * @return None
 * @note This function saves the current hour and minutes and seconds to EEPROM at the specified index.
 *  It also saves the current day, month, and year to fixed location in EEPROM if are different from what is
 *   allready saved.
 * Also it sets a flag to indicate that time has been saved to EEPROM and not recover yet. When the power is up,
 *  the flag is reset.
 */
void saveTime(void)
{
    DateTime currentTime = rtc.now();
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
    uint16_t countResetIdx = (EEPROM[2] << 8) | EEPROM[3];
    uint8_t currentHour = currentTime.hour();
    uint8_t currentMinute = currentTime.minute();
    uint8_t currentSecond = currentTime.second();
    uint8_t currentDay = currentTime.day();
    uint8_t currentMonth = currentTime.month();
    uint8_t currentYear = currentTime.year();

    index += 3u; /* Increment index to save the current time */

    if (index > EEPROM_MAX_USE_SIZE)
    {/* Check if index exceeds EEPROM size */
        resetCounters(); /* Reset EEPROM if index exceeds limit (1024 - 2 Bytes) */
        index = 7u; /* Reset index to EEPROM[4] where start the savings. */
        countResetIdx++; /* Increment the counter for reset index */
        EEPROM[1u] = index; /* Write new index to EEPROM */
        EEPROM[2u] = UPPER_BYTE(countResetIdx); /* Write upper byte of index */
        EEPROM[3u] = LOWER_BYTE(countResetIdx); /* Write lower byte of index */
    }

    currentYear -= 2000u; /* Convert year to 2-digit format */
/* Use the EEPROM date if it is the same to avoid overwriting the EEPROM */
    if (currentDay == EEPROM[4u])
    {
        currentDay = 0u; /* Use the day from EEPROM if it matches */
    }

    if (currentMonth == EEPROM[5u])
    {
        currentMonth = 0u; /* Use the month from EEPROM if it matches */
    }

    if (currentYear == EEPROM[6u])
    {
        currentYear = 0u; /* Use the year from EEPROM if it matches */
    }
    /* Set the MSB bit 8 to signal time is ready for recover */
    currentHour = SET_TIME_RECOVER_FLAG(currentHour); 
    /* Write the data in EEPROM and check the status of writing. */
    writeToEEPROMStatus = writeToEEPROM(index, currentHour, currentMinute, currentSecond, currentDay, currentMonth, currentYear); /* Write current time to EEPROM */
    /* Set flag to indicate time has been saved */
    saveTimeToEEPROM = true;
}

/***************************************************************************************************************
 * @brief Write time to EEPROM with error handling and verification.
 * @param index: EEPROM index where the time is saveing.
 * @param currentHour: hour when power down was detected.
 * @param currentMinute: minute when power down was detected.
 * @param currentSecond: second when power down was detected.
 * @param currentDay: day when power down was detected.
 * @param currentMonth: month when power down was detected.
 * @param currentYear: year when power down was detected.
 * @return status of the write operation:
 *                  0: EEPROM write successful,
 *                  1: failed to write in EEPROM.
 *
 * @note This function writes the current hour, minutes and seconds to the EEPROM at the specified index.
 * It also writes the upper and lower bytes of the index to the first two EEPROM locations, and the current day,
 * month, and year to fixed locations in the EEPROM if they are not zero (different from the previous one).
 * The function checks for successful write operations by reading it back to verify that the value
 *  was written correctly and prints error messages if any write fails if DEBUGGING is enabled.
 *
 * The function uses the EEPROM library to write data to the EEPROM.
 */
uint8_t writeToEEPROM(uint16_t index, uint8_t currentHour, uint8_t currentMinute, uint8_t currentSecond, uint8_t currentDay, uint8_t currentMonth, uint8_t currentYear)
{
    /* Write upper and lower bytes of the index */
    EEPROM.write(0u, UPPER_BYTE(index));
    if (EEPROM.read(0u) != UPPER_BYTE(index))
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write upper byte of index to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

    EEPROM.write(1u, LOWER_BYTE(index));
    if (EEPROM.read(1u) != LOWER_BYTE(index))
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
    EEPROM.write(index + 1u, currentMinute);
    if (EEPROM.read(index + 1u) != currentMinute)
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write current minute to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

    /* Write current seconds next to minutes */
    EEPROM.write(index + 2u, currentSecond);
    if (EEPROM.read(index + 2u) != currentSecond)
    {
#ifdef DEBUGGING
        Serial.println("Error: Failed to write current seconds to EEPROM, status ERROR. ");
#endif
        return ERROR;
    }

    if (currentDay != 0u)
    {
        Serial.println("Write current day to EEPROM, status OK. ");
        /* Write current day */
        EEPROM.write(4u, currentDay);
        if (EEPROM.read(4u) != currentDay)
        {
    #ifdef DEBUGGING
            Serial.println("Error: Failed to write current day to EEPROM, status ERROR. ");
    #endif
            return ERROR;
        }
    }

    if (currentMonth != 0u)
    {
        /* Write current month */
        Serial.println("Write current month to EEPROM, status Ok. ");
        EEPROM.write(5u, currentMonth);
        if (EEPROM.read(5u) != currentMonth)
        {
    #ifdef DEBUGGING
            Serial.println("Error: Failed to write current month to EEPROM, status ERROR. ");
    #endif
            return ERROR;
        }
    }

    if (currentYear != 0u)
    {
        /* Write current year */
        Serial.println("Write current year to EEPROM, status Ok. ");
        EEPROM.write(6u, currentYear);
        if (EEPROM.read(6u) != currentYear)
        {
    #ifdef DEBUGGING
            Serial.println("Error: Failed to write current year to EEPROM, status ERROR. ");
    #endif
            return ERROR;
        }
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
    for (uint8_t i = 0; i < 1024; i++)
    {
        /* code */
        EEPROM.write(i, 255); /* Reset all bytes in EEPROM */
    }
}

/***************************************************************************************************************
 * @brief Reset the EEPROM counter to 0.
 * @param None
 * @return None
 * @note This function resets the upper and lower bytes of the index and the data location in EEPROM to 0.
 *       It is used to clear the EEPROM data when the index exceeds the maximum size.
 */
void resetCounters(void)
{
    for (uint8_t i = 0; i < 7u; i++)
    {
        EEPROM.write(i, 0);/* Reset index, counter and date.*/
    }
}

/***************************************************************************************************************
 * @brief Handle power-up events and adjust the clock hands accordingly.
 * @param None
 * @return None
 * @note This function checks if the power is up and adjusts the clock hands using the stepper motor.
 *       If the power is down, it saves the current time to EEPROM.
 */
void handlePowerUp(void)
{
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
        { /* Check if the power is up and signal if the clock was set correctly. */
            uint8_t setClockFlag = SUCCESS;
#ifdef DEBUGGING
            Serial.println("Read EEPROM");
#endif
            setClockFlag = setClock();

            if (SUCCESS != setClockFlag)
            {/* Set error status */
                writeToEEPROMStatus = ERROR; 
            }
        }
    }
}

/***************************************************************************************************************
 * @brief Adjust the clock hands using the saved time from EEPROM.
 * @param None
 * @return None
 * @note This function retrieves the current time from the RTC and compares it with the saved time
 *        in EEPROM. It calculates the difference in hours, minutes and seconds, and moves the clock hands
 *        accordingly using the stepper motor. The function also resets the stepper motor direction
 *        to default (CW) and resets the flag indicating that time recovery is complete.
 */
uint8_t setClock(void)
{ /* Adjust clock using EEPROM data */
    DateTime currentTime = rtc.now();
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
    uint8_t currentHour = currentTime.hour();
    uint8_t currentMinute = currentTime.minute();
    uint8_t currentSecond = currentTime.second();
    uint8_t savedHour = EEPROM[index];
    uint8_t savedMinute = EEPROM[index + 1];
    uint8_t savedSecond = EEPROM[index + 2]; /* Read saved seconds from EEPROM */
    uint8_t directionToMove = CW_DIR;
    uint16_t minuteToStep = 0;
    uint16_t secondsToStep = 0;

    /* Clear the MSB bit of the saved hour to indicate recovery is complete */
    savedHour = CLEAR_TIME_RECOVER_FLAG(savedHour);
    /* Robustness for a new EEPROM (all 0xFFu) */
    if (savedHour > 24)
    { /* Robustness for an invalid hour. */
        return ERROR;
    }
    /* Save hour after clear the recovery flag to mark that recovery was done. */
    EEPROM[index] = savedHour;

    /* Convert to 12-hour format */
    savedHour = (savedHour == 0) ? 12 : (savedHour > 12 ? savedHour - 12 : savedHour);
    currentHour = (currentHour == 0) ? 12 : (currentHour > 12 ? currentHour - 12 : currentHour);

    /* Calculate hour difference */
    uint8_t hourDifference = (currentHour >= savedHour) ? currentHour - savedHour : savedHour - currentHour;

    if (hourDifference >= 6)
    {
        hourDifference = 12 - hourDifference;
        directionToMove = (currentHour > savedHour) ? CCW_DIR : CW_DIR;
    }
    else
    {
        directionToMove = (currentHour >= savedHour) ? CW_DIR : CCW_DIR;

    }

    /* Convert the difference from hours to minutes. */
    minuteToStep = hourDifference * 60;
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
            if (0u == minuteToStep)
            {
                directionToMove = CCW_DIR;
            }
            
            minuteToStep = (minuteToStep >= minuteDifference) ? minuteToStep - minuteDifference : minuteDifference - minuteToStep;
        }
        
    }
    else
    {
        if (currentMinute >= savedMinute)
        {
            if (0u == minuteToStep)
            {
                directionToMove = CW_DIR;
            }

            minuteToStep = (minuteToStep >= minuteDifference) ? minuteToStep - minuteDifference : minuteDifference - minuteToStep;
        }
        else
        {
            minuteToStep = minuteToStep + minuteDifference;
        }
    }

    /* Calculate seconds difference */
    uint8_t secondsDifference = (currentSecond >= savedSecond) ? currentSecond - savedSecond : savedSecond - currentSecond;
    /* Convert minutes to seconds */
    secondsToStep = minuteToStep * 60; /* Convert minutes to seconds */

    /* If the saved second is greater than the current second, adjust the minute to step */
    if (directionToMove == CW_DIR)
    {
        if (savedSecond > currentSecond)
        {
            if (0u == secondsToStep)
            {
                directionToMove = CCW_DIR; /* Change direction to CCW if no seconds to step */
            }

            secondsToStep = (secondsToStep >= secondsDifference) ? secondsToStep - secondsDifference : secondsDifference - secondsToStep; /* Subtract seconds if moving clockwise */
        }
        else
        {
            secondsToStep = secondsToStep + secondsDifference; /* Add seconds if moving clockwise */
        }
    }
    else
    {
        if (savedSecond > currentSecond)
        {
            secondsToStep = secondsToStep + secondsDifference; /* Add seconds if moving clockwise */
        }
        else
        {
            if (0u == secondsToStep)
            {
                directionToMove = CW_DIR; /* Change direction to CW if no seconds to step */
            }

            secondsToStep = (secondsToStep >= secondsDifference) ? secondsToStep - secondsDifference : secondsDifference - secondsToStep; /* Subtract seconds if moving counterclockwise */
        }
    }

    if (directionToMove == CW_DIR)
    {/* Add the delay from debaunce to ensure the clock precision. */
        secondsToStep += 5u;
    }
    else
    {
        secondsToStep -= 5u;
    }

#ifdef DEBUGGING
    Serial.print("Moving clock hands in direction: ");
    Serial.println(directionToMove);
    /* Print the time to be set */
    printFormatedDateAndTime(hourDifference, minuteDifference, secondsDifference, 0u, 0u, 0u); 
#endif
    /* Move the hands of the clock with the respective secondes. */
    moveClockHands(directionToMove, secondsToStep);
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
 * @param secondsToStep: Number of minutes to move the clock hands.
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

#ifdef DEBUGGING
/***************************************************************************************************************
 * @brief Handle serial commands for debugging and manual clock adjustments.
 * @param None
 * @return None
 * @note This function reads commands from the serial interface and performs various operations
 *       such as moving the clock hands, reading/writing to EEPROM, and adjusting the RTC time.
 */
void debugSerial(void)
{
    if (Serial.available())
    {
        /* Read the command to execute and three bytes with data to use in the command. */
        String command = Serial.readStringUntil(' ');
        uint16_t num1 = Serial.parseInt();
        uint16_t num2 = Serial.parseInt();
        uint16_t num3 = Serial.parseInt();
        uint16_t secondsToStep = ((num1 * 3600) + (num2 * 60) + num3); /* Convert hours and minutes to seconds */

        if (command == "fwd")
        {/* This command is used to move the clock forward with number of seconds calculate above. */
            Serial.print("CW direction for: ");
            
            printFormatedDateAndTime(num1, num2, num3, 0u, 0u, 0u); /* Print the time to be set */

            moveClockHands(CW_DIR, secondsToStep);
        }

        if (command == "bwd")
        {/* This command is used to move the clock backward with number of seconds calculate above. */
            Serial.print("CCW direction for: ");

            printFormatedDateAndTime(num1, num2, num3, 0u, 0u, 0u); /* Print the time to be set */

            moveClockHands(CCW_DIR, secondsToStep);
            
            Serial.println("Done!");
            Serial.println("");
        }
        
        if (command == "ret")
        {
            uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
            
            Serial.print("read EEPROM index: ");
            Serial.println(index);

            printFormatedDateAndTime(CLEAR_TIME_RECOVER_FLAG(EEPROM[index]), EEPROM[index + 1], EEPROM[index + 2],
                                     EEPROM[4], EEPROM[5], EEPROM[6] + 2000); /* Read saved time from EEPROM */
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

            printFormatedDateAndTime(currentTime.hour(), currentTime.minute(), currentTime.second(),
                                     currentTime.day(), currentTime.month(), currentTime.year());
        }

        if (command == "wet")
        {
            uint8_t retval = SUCCESS;
            Serial.println("write time to EEPROM ");

            num1 = SET_TIME_RECOVER_FLAG(num1); /* Set the MSB bit 8 to signal time is ready for recover */

            retval = writeToEEPROM(num3, num1, num2, 30u, 0, 0, 0); /* Write current time to EEPROM */

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
            resetCounters();/* For robustness if a reset not occured. */
            
            Serial.println("Done!");
            Serial.println("");
        }
    }
}


/***************************************************************************************************************
 * @brief Print time and date in a formated mode.
 * @param hour: one bytes, hour to print.
 * @param minute: one bytes, minute to print.
 * @param second: one bytes, second to print.
 * @param day: one bytes, day to print.
 * @param month: one bytes, month to print.
 * @param year: two bytes, year to print.
 * @return None
 * @note This function prints the current time and date from the RTC to the serial interface.
 */
void printFormatedDateAndTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year)
{
    Serial.print(hour, DEC);
    Serial.print(":");
    Serial.print(minute, DEC);
    Serial.print(":");
    Serial.println(second, DEC);

    if (0u != day)
    {
        Serial.print("Current date: ");
        Serial.print(day, DEC);
        Serial.print('/');
        Serial.print(month, DEC);
        Serial.print('/');
        Serial.println(year, DEC);
    }

    Serial.println("Done!");
    Serial.println("");
}
#endif // DEBUGGING
