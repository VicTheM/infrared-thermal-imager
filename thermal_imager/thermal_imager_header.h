/**

  Header file, contains global variables, constants, structures,
  function declarations and other global objects

**/

#ifndef _THERMAL_CAMERA_HEADER_
#define _THERMAL_CAMERA_HEADER_
#include "Arduino.h"        
#include <Wire.h>                                 
#include <SparkFun_GridEYE_Arduino_Library.h>

/********************************* Pin definitions *********************************/
#define CAPTURE_PIN 14                                // Capture image button
#define WEB_SERVER_PIN 13                             // Button to enable wireless connection
#define WEB_SERVER_LED_PIN 12                         // Web server led indicator
#define AMG8833_SCL_PIN 22                            // I2C serial clock pin for the thermal sensor
#define AMG8833_SDA_PIN 21                            // I2C serial data pin for the thermal sensor

/******************************** Other constants ***********************************/
#define DEBOUNCE_DELAY 2000                           // How fast multiple images can be captured (ie once every 2000 millisec)
#define SERVER_DELAY 3000                             // Same as DEBOUNCE_DELAY, but for the web server
#define THERMAL_SENSOR_RESOLUTION 64                  // Total number of pixels in sensor used



/********************************** Function declarations *************************/
void isr();
void printThermalGrid(float *p, uint8_t rows, uint8_t cols);
float get_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
void set_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f);
void get_adjacents_1d(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
void get_adjacents_2d(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
float cubicInterpolate(float p[], float x);
float bicubicInterpolate(float p[], float x, float y);
void interpolate_image(float *src, uint8_t src_rows, uint8_t src_cols, float *dest,
                        uint8_t dest_rows, uint8_t dest_cols);


/********************************** Structures ************************************/

/**
 * Button: Used to model a typical 2-way button attached to the esp
 * @PIN: The pin where one leg of button is attached (second leg is gnd or vcc)
 * @pressed: Captures a click or a toggle
 * @lastPressTime: Holds timestamp in order to curtail switch debouncing
 */
struct Button {
  const uint8_t PIN;
  bool pressed;
  unsigned long lastPressTime;
};

#endif // _THERMAL_CAMERA_HEADER_