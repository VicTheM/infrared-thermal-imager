#ifndef _THERMAL_CAMERA_HEADER_
#define _THERMAL_CAMERA_HEADER_

#include "server.h"                                    
#include <SparkFun_GridEYE_Arduino_Library.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Wire.h> 
#include <Preferences.h>
#include "SPI.h"
#include "TFT_22_ILI9225.h"
#include "math.h"

/*************************************** Pin definitions *************************************************/
#define AMG8833_SCL_PIN 22               // I2C serial clock pin for the thermal sensor
#define AMG8833_SDA_PIN 21               // I2C serial data pin for the thermal sensor

#define CAPTURE_PIN 19                   // Capture image button
#define WEB_SERVER_PIN 18                // Button to enable wireless connection
#define WEB_SERVER_LED_PIN 12            // Web server led indicator

#define TFT_RST 26                       // IO 26
#define TFT_RS  25                       // IO 25
#define TFT_CLK 14                       // HSPI-SCK
#define TFT_SDI 13                       // HSPI-MOSI
#define TFT_CS  15                       // HSPI-SS0
#define TFT_LED 0                        // Backlight

/********************************************** Other constants ************************************************/
#define DEBOUNCE_DELAY 2000                // How fast multiple images can be captured (ie once every 2000 millisec)
#define SERVER_DELAY 3000                  // Same as DEBOUNCE_DELAY, but for the web server
#define THERMAL_SENSOR_RESOLUTION 64       // Total number of pixels in sensor used
#define PREFERENCE_NAMESPACE "file_id"     // Namespace for preferences storage
#define PREFERENCE_KEY "idx"               // Key for preferences dictionary-like storage

#define TFT_BRIGHTNESS 200                 // TFT backlight brightness
#define NUM_COLOR 26                       // Number of colors in the color map (unused in version 1.x.x)
#define MAXTEMP 35                         // Assumed Maximum temperature in the color map
#define MINTEMP 19                         // Assumed Minimum temperature in the color map
#define IMG_WIDTH 176                      // Width of the thermal image in pixels
#define IMG_HEIGHT 220                     // Height of the thermal image in pixels
#define SCREEN_WIDTH  176                  // Width of the TFT screen
#define SCREEN_HEIGHT 220                   // Height of the TFT screen
#define SENSOR_WIDTH  24                    // Width of the thermal sensor (after first interpolation)
#define SENSOR_HEIGHT 24                    // Height of the thermal sensor (after first interpolation)
#define AMG8833RES_X 8                      // Width of the thermal sensor (before first interpolation)
#define AMG8833RES_Y 8                      // Height of the thermal sensor (before first interpolation)
#define ROTATE_ANGLE 10                     // How far the welcom triangle rotates
#define MAX_NUMBER_OF_FILES 10              // The total cachable file in internal memory (based on size)


/******************************************** Structures *************************************************/
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

/**
 * _point: A simple dot in space
 * @x: x-coordinate
 * @y: y-coordinate
 */
struct _point {
    int16_t x;
    int16_t y;
};


/****************************************** Function declarations *******************************************/
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
bool generate_bmp(float *src, fs::FS &file, const char *filename);
void deleteFile(fs::FS &fs, const char *path);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
float getInterpolatedTemperature(float x, float y);
uint16_t getColorFromTemperature(float temp);
void generateThermalImage();
void printThermalGrid(uint16_t *p, uint8_t rows, uint8_t cols);
void drawpixels(float *p, uint8_t rows, uint8_t cols, uint8_t scaleX, uint8_t scaleY);
bool createTemperatureBMP(uint16_t* tempArray, const char* filename);
void getTemperatureColor(uint16_t temp, float minTemp, float maxTemp, uint8_t& r, uint8_t& g, uint8_t& b);
void mapIntoRGB565Color(uint16_t *arr, int len);
void interpolate1DArray(float* sensorData, uint16_t* imgData);
_point rotatePoint( _point c, float angle, _point p );
void rotateTriangle( _point &a, _point &b, _point &c, _point r, int16_t deg );
_point getCoordCentroid( _point a, _point b, _point c );
unsigned int getNumberOfFiles();
void deleteFiles();
void monitorMemoryOverflow();

#endif // _THERMAL_CAMERA_HEADER_