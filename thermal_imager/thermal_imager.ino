#include "thermal_imager_header.h"

/******************************** Variable declarations ***********************************/
Button captureButton = {CAPTURE_PIN, false, 0};
Button serverSwitch = {WEB_SERVER_PIN, false, 0};
unsigned int n = 0;


/******************************* Setup code: Entry point to run once ***********************/
void setup() {
  Serial.begin(115200);

  pinMode(captureButton.PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_LED_PIN, OUTPUT);

  attachInterrupt(captureButton.PIN, isr, FALLING);
  digitalWrite(WEB_SERVER_LED_PIN, LOW);
}


/******************************* Main program loop *****************************************/
void loop() {

  // CAPTURE IMAGE
  if (captureButton.pressed) {
    n++;
    Serial.printf("Button 1 has been pressed %u times\n", n);
    captureButton.pressed = false;
  }

  // ENABLE WEB SERVER
  if(!digitalRead(serverSwitch.PIN)){
    unsigned long currentTime = millis();
    if (currentTime - serverSwitch.lastPressTime > SERVER_DELAY && serverSwitch.pressed == false) {
      serverSwitch.pressed = true;
      Serial.println("Turning web server on");
      digitalWrite(WEB_SERVER_LED_PIN, HIGH);
      serverSwitch.lastPressTime = currentTime;
    }
  }
  else {
    unsigned long currentTime = millis();
    if (serverSwitch.pressed == true && currentTime - serverSwitch.lastPressTime > SERVER_DELAY) {
      serverSwitch.pressed = false;
      Serial.println("Turning web server off");
      digitalWrite(WEB_SERVER_LED_PIN, LOW);
      serverSwitch.lastPressTime = currentTime;
    }
  }
}


/********************************** Function definitions ************************************/
/**
 * IRAM_ATTR - Interrupt service routine
 * Description: This function monitors the capture button
 * and activates a variable responsible for capturing an image when it is pressed
 */
void IRAM_ATTR isr() {
  unsigned long currentTime = millis();
  if (currentTime - captureButton.lastPressTime > DEBOUNCE_DELAY) {
    captureButton.pressed = true;
    captureButton.lastPressTime = currentTime;
  }
}