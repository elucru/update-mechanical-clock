/* Last update: 2025-04-55 13:13:00 */
#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <RTClib.h>
#include <EEPROM.h>

/* Use for debugging purpose, comment when done. */
#define DEBUGGING

/* Pins for the stepper motor */
#define STEPPER_DRIVER_ENABLE_PIN       11  /* Enable/disable the stepper motor driver */
#define STEPPER_DIR_PIN                 12  /* Rotation direction */
#define STEPPER_PULSE_PIN               8   /* Pulse command */
/* Pin to check the power supply of the stepper motor */
#define POWER_DOWN_PIN                  3   /* Power up pin is HIGH, power down pin is LOW */

#define DEBOUNCE_DELAY                  5000    /* Seconds of debounce delay for the power down check */
#define CW_DIR                          LOW     /* Clockwise direction */
#define CCW_DIR                         HIGH    /* Counterclockwise direction */
#define STEPPER_PULSE_TIME              5       /* Time in microsec for stepper pulse */
#define STEPPER_DELAY_TIME              720     /* Delay between pulses to set correct time clock */
#define FINE_TIME_ADJUSTMENT            500     /* Time in microsec for stepper pulse for time precision */
#define STEPPER_FAST_TIME_ADJUSTMENT    1       /* Time in miliseconds for stepper pulse to adjust the time faster */

#define EEPROM_MAX_USE_SIZE         1022u   /* 0x03FEu -> Size of EEPROM 1024 from 0 to 1023, and we write 2 Bytes at once. */

#define UPPER_BYTE(x)                   ((x & 0xFF00) >> 8) /* Get the upper byte of a 16-bit number */
#define LOWER_BYTE(x)                   (x & 0x00FF)        /* Get the lower byte of a 16-bit number */
#define SET_TIME_RECOVER_FLAG(x)        (x | 0x80)          /* Set bit 7 to signal time is ready for recover */
#define CLEAR_TIME_RECOVER_FLAG(x)      (x & 0x7F)          /* Clear bit 7 to signal that time was recovered */
#define CHECK_TIME_RECOVER_FLAG(x)      ((x & 0x80) >> 7)   /* Check bit 7 to signal time is ready for recover */
#define SUCCESS                         0u
#define ERROR                           1u

#endif
/* MAIN_H */
/* End of file */
