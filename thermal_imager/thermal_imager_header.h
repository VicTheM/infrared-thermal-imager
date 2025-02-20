#ifndef _THERMAL_CAMERA_HEADER_
#define _THERMAL_CAMERA_HEADER_

#include "server.h"                                    
#include <SparkFun_GridEYE_Arduino_Library.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Wire.h> 
#include <Preferences.h>
#include "SPI.h"
#include "lib.h"

/********************************* Pin definitions *********************************/
#define AMG8833_SCL_PIN 22                            // I2C serial clock pin for the thermal sensor
#define AMG8833_SDA_PIN 21                            // I2C serial data pin for the thermal sensor

#define CAPTURE_PIN 19                                // Capture image button
#define WEB_SERVER_PIN 18                             // Button to enable wireless connection
#define WEB_SERVER_LED_PIN 12                         // Web server led indicator

#define TFT_RST 26  // IO 26
#define TFT_RS  25  // IO 25
#define TFT_CLK 14  // HSPI-SCK
#define TFT_SDI 13  // HSPI-MOSI
#define TFT_CS  15  // HSPI-SS0
#define TFT_LED 0 
#define TFT_BRIGHTNESS 200

/******************************** Other constants ***********************************/
#define DEBOUNCE_DELAY 2000                           // How fast multiple images can be captured (ie once every 2000 millisec)
#define SERVER_DELAY 3000                             // Same as DEBOUNCE_DELAY, but for the web server
#define THERMAL_SENSOR_RESOLUTION 64                  // Total number of pixels in sensor used
#define PREFERENCE_NAMESPACE "file_id"
#define PREFERENCE_KEY "idx"

#define NUM_COLOR 26
#define MAXTEMP 36
#define MINTEMP 19
#define IMG_WIDTH 176
#define IMG_HEIGHT 220
#define SCREEN_WIDTH  176
#define SCREEN_HEIGHT 220
#define SENSOR_WIDTH  24
#define SENSOR_HEIGHT 24



/********************************** Function declarations *************************/
void isr();
void printThermalGrid(float *p, uint8_t rows, uint8_t cols);
float get_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
uint16_t get_point(uint16_t *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
void set_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f);
void get_adjacents_1d(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
void get_adjacents_2d(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
float cubicInterpolate(float p[], float x);
float bicubicInterpolate(float p[], float x, float y);
void interpolate_image(float *src, uint8_t src_rows, uint8_t src_cols, float *dest,
                        uint8_t dest_rows, uint8_t dest_cols);
int getFileID();
bool generate_bmp(uint16_t *src, fs::FS &file, const char *filename);
void deleteFile(fs::FS &fs, const char *path);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
float getInterpolatedTemperature(float x, float y);
uint16_t getColorFromTemperature(float temp);
void generateThermalImage();

void drawpixels(float *p, uint8_t rows, uint8_t cols, uint8_t scaleX, uint8_t scaleY);


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