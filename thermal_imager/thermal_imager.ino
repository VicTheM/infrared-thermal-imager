#include "thermal_imager_header.h"
#include <WebServer.h>
#include <WiFi.h>

/******************************** Variable declarations ***********************************/
GridEYE grideye;
Button captureButton = {CAPTURE_PIN, false, 0};
Button serverSwitch = {WEB_SERVER_PIN, false, 0};
unsigned int counter;
float pixels[64];
float singlePixelValue;
float interpolatedPixels[576];

/******************************* Setup code: Entry point to run once ***********************/
void setup() {
  Serial.begin(115200);

  pinMode(captureButton.PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_LED_PIN, OUTPUT);

  Wire.begin();
  delay(500);
  grideye.begin();
  delay(500);

   // ----- check partitions for finding the filesystem type -----
  esp_partition_iterator_t i;

  i = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (i) {
    Serial.print("SPIFFS partition found.");
    fsys = &LittleFS;

  } else {
    Serial.print("no partition found.");
  }
  esp_partition_iterator_release(i);

  // mount and format as needed
  Serial.print("Mounting the filesystem...\n");
  if (!fsys) {
    Serial.print("need to change the board configuration to include a partition for files.\n");
    delay(30000);
  } else if ((fsys == (fs::FS *)&LittleFS) && (!LittleFS.begin())) {
    Serial.print("could not mount the filesystem...\n");
    delay(2000);
    Serial.print("formatting LittleFS...\n");
    LittleFS.format();
    delay(2000);
    Serial.print("restarting...\n");
    delay(2000);
    ESP.restart();
  }

  Serial.setDebugOutput(true);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);

  // enable ETAG header in webserver results (used by serveStatic handler
  attachInterrupt(captureButton.PIN, isr, FALLING);
  digitalWrite(WEB_SERVER_LED_PIN, LOW);
}


/******************************* Main program loop *****************************************/
void loop() {

  // VIDEO STREAM IF WEBSERVER IS OFF
  /**
   * A routine is followed here. we confirm the webserver is off them we obtain current temp
   * values from the sensor, interpolate it and write it to screen (1 frame)
   * The next two block of code simply checks if the current screen should be saved to memory
   * or if web server should be enabled. while the webserver is off, the loop continues
   * at a refresh rate of about 20Hz. Capturing an image does not affect the video, it only
   * decreases refresh rate while it is being saved to memory
   */
  if (serverSwitch.pressed == false) {
    for (counter=0; counter < THERMAL_SENSOR_RESOLUTION; counter++) {
      singlePixelValue = grideye.getPixelTemperature(counter);
      pixels[counter] = singlePixelValue;
    }
    interpolate_image(pixels, 8, 8, interpolatedPixels, 24, 24);
  }

  // CAPTURE IMAGE
  if (captureButton.pressed && serverSwitch.pressed == false) {
    captureButton.pressed = false;
    generate_bmp(interpolatedPixels, LittleFS, "/thermal.bmp");
    printThermalGrid(interpolatedPixels, 24, 24);
  }

  // ENABLE WEB SERVER
  if(!digitalRead(serverSwitch.PIN)){
    unsigned long currentTime = millis();
    if (currentTime - serverSwitch.lastPressTime > SERVER_DELAY && serverSwitch.pressed == false) {
      serverSwitch.pressed = true;
      Serial.println("Turning web server on");
      startServer();
      digitalWrite(WEB_SERVER_LED_PIN, HIGH);
      serverSwitch.lastPressTime = currentTime;
    }
  }
  else {
    unsigned long currentTime = millis();
    if (serverSwitch.pressed == true && currentTime - serverSwitch.lastPressTime > SERVER_DELAY) {
      captureButton.pressed = false; // they might have pressed it while the server was on
      serverSwitch.pressed = false;
      Serial.println("Turning web server off");
      stopServer();
      digitalWrite(WEB_SERVER_LED_PIN, LOW);
      serverSwitch.lastPressTime = currentTime;
    }
  }
  if (serverSwitch.pressed == true) {
    handleClient();
  }
}

/**
 * IRAM_ATTR - Interrupt service routine
 * Description: This function monitors the capture button
 * and activates a variable responsible for capturing an image when it is pressed
 *
 * Return: Void
 */
void IRAM_ATTR isr() {
  unsigned long currentTime = millis();
  if (currentTime - captureButton.lastPressTime > DEBOUNCE_DELAY) {
    captureButton.pressed = true;
    captureButton.lastPressTime = currentTime;
  }
}