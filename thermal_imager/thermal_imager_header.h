/**

  Header file, contains global variables, constants, structures,
  function declarations and other global objects

**/

#ifndef _THERMAL_CAMERA_HEADER_
#define _THERMAL_CAMERA_HEADER_
#include "Arduino.h"

#define CAPTURE_PIN 14                                // Capture image button
#define WEB_SERVER_PIN 13                             // Button to enable wireless connection
#define WEB_SERVER_LED_PIN 12                         // Web server led indicator
#define DEBOUNCE_DELAY 1000                           // How fast multiple images can be captured (ie once every 1000 millisec)
#define SERVER_DELAY 3000                             // Same as DEBOUNCE_DELAY, but for the web server



/********************************** Function declarations *************************/
void IRAM_ATTR isr();                                 // Interrupt to capture button press


/********************************** Structures ************************************/
struct Button {
  const uint8_t PIN;
  bool pressed;
  unsigned long lastPressTime;
};

#endif // _THERMAL_CAMERA_HEADER_