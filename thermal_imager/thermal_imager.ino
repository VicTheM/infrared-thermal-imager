#include "thermal_imager_header.h"
#include <WebServer.h>
#include <WiFi.h>

/******************************** Variable declarations ***********************************/
GridEYE grideye;
SPIClass hspi(HSPI);
TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, TFT_BRIGHTNESS);
Button captureButton = {CAPTURE_PIN, false, 0};
Button serverSwitch = {WEB_SERVER_PIN, false, 0};
unsigned int counter;
String filename;
int fileID;
uint16_t* thermalBitmap;

uint16_t x, y;
boolean flag = false;

float pixels[64];
float interpolatedPixels[576];
// float interpolatedPixels[64];

/******************************* Setup code: Entry point to run once ***********************/
void setup() {
  hspi.begin();
  tft.begin(hspi);
  Serial.begin(115200);
  thermalBitmap = new (std::nothrow) uint16_t[IMG_WIDTH * IMG_HEIGHT];
  if (!thermalBitmap) {
    Serial.println("Failed to allocate array");
  }

  pinMode(captureButton.PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_LED_PIN, OUTPUT);

  Wire.begin();
  delay(500);
  grideye.begin();
  delay(500);

  // check for partition
  esp_partition_iterator_t i;
  i = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (i) {
    fsys = &LittleFS;
  } else {
  }
  esp_partition_iterator_release(i);

  // mount and format as needed
  if (!fsys) {
    Serial.println("Fs bad"); // bad
  } else if ((fsys == (fs::FS *)&LittleFS) && (!LittleFS.begin())) {
    LittleFS.format();
    Serial.println("Restarting esp32");
    ESP.restart();
  }

  Serial.setDebugOutput(true);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);

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
      pixels[counter] = grideye.getPixelTemperature(counter);
      // interpolatedPixels[counter] = grideye.getPixelTemperature(counter);
    }
    interpolate_image(pixels, 8, 8, interpolatedPixels, 24, 24);
    // printThermalGrid(interpolatedPixels, SENSOR_WIDTH, SENSOR_HEIGHT);
    // generateThermalImage();
    // tft.drawBitmap(0, 0, thermalBitmap, IMG_WIDTH, IMG_HEIGHT);
    drawThermalImage();
  }

  // CAPTURE IMAGE
  if (captureButton.pressed && serverSwitch.pressed == false) {
    captureButton.pressed = false;

    fileID = getFileID();
    filename = "/img_" + String(fileID) + ".bmp";
    generate_bmp(thermalBitmap, LittleFS, filename.c_str());
    printThermalGrid(interpolatedPixels, SENSOR_WIDTH, SENSOR_HEIGHT);
  }

  // ENABLE WEB SERVER
  if(!digitalRead(serverSwitch.PIN)){
    unsigned long currentTime = millis();
    if (currentTime - serverSwitch.lastPressTime > SERVER_DELAY && serverSwitch.pressed == false) {
      serverSwitch.pressed = true;
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
      stopServer();
      digitalWrite(WEB_SERVER_LED_PIN, LOW);
      serverSwitch.lastPressTime = currentTime;
    }
  }
  if (serverSwitch.pressed == true) {
    handleClient();
  }
}

/********************************** Function definitions ************************************/

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

/**
 * get_point - Obtain a cell from a 2D array
 * @p: The 2D array
 * @rows: The number of rows in array
 * @cols: The number of columns in array
 * @x: The row_index of cell to obtain
 * @y: The column index of cell to obtain
 *
 * Return: Float - The value of that specific index
 */
float get_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= cols)
    x = cols - 1;
  if (y >= rows)
    y = rows - 1;
  return p[y * cols + x];
}

uint16_t get_point(uint16_t *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= cols)
    x = cols - 1;
  if (y >= rows)
    y = rows - 1;
  return p[y * cols + x];
}
/**
 * printThermalGrid - print the thermal heat values to the terminal
 * @p: The 2D array to be printed
 * @rows: The number of rows to print
 * @cols: The number of columns to print
 *
 * Return: Void
 */
void printThermalGrid(float *p, uint8_t rows, uint8_t cols) {
    Serial.println(F("Thermal Sensor Readings:"));
    float val;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            val = get_point(p, rows, cols, x, y);
            Serial.print(val, 2);
            Serial.print("\t");
        }
        Serial.println();
    }
    Serial.println();
}


bool generate_bmp(uint16_t *src, fs::FS &file, const char *filename) {
    uint8_t bmpHeader[54] = {
        0x42, 0x4D, 0, 0, 0, 0,  // File size (to be calculated)
        0x00, 0x00, 0x00, 0x00,  // Reserved
        0x36, 0x04, 0x00, 0x00,  // Offset to pixel data (54 + 1024 bytes for palette)

        // DIB Header
        0x28, 0x00, 0x00, 0x00,  // Header size
        IMG_WIDTH & 0xFF, (IMG_WIDTH >> 8) & 0xFF, 0x00, 0x00,  // Width
        IMG_HEIGHT & 0xFF, (IMG_HEIGHT >> 8) & 0xFF, 0x00, 0x00,  // Height
        0x01, 0x00,              // Planes (1)
        0x08, 0x00,              // Bits per pixel (8-bit)
        0x00, 0x00, 0x00, 0x00,  // Compression (None)
        0, 0, 0, 0,              // Image size (not needed for uncompressed BMP)
        0x13, 0x0B, 0x00, 0x00,  // Horizontal resolution
        0x13, 0x0B, 0x00, 0x00,  // Vertical resolution
        0x00, 0x01, 0x00, 0x00,  // Colors in palette (256)
        0x00, 0x00, 0x00, 0x00   // Important colors (0)
    };

    int rowSize = (IMG_WIDTH + 3) & ~3;  // Ensure row size is multiple of 4
    int fileSize = 54 + 1024 + (rowSize * IMG_HEIGHT);

    // Update file size in header
    bmpHeader[2] = (uint8_t)(fileSize & 0xFF);
    bmpHeader[3] = (uint8_t)((fileSize >> 8) & 0xFF);
    bmpHeader[4] = (uint8_t)((fileSize >> 16) & 0xFF);
    bmpHeader[5] = (uint8_t)((fileSize >> 24) & 0xFF);

    // Open file for writing
    File fs = file.open(filename, FILE_WRITE);
    if (!fs) {
        Serial.println("Failed to create file");
        return false;
    }

    // Write BMP header
    fs.write(bmpHeader, 54);

    // --- Custom Color Palette ---
    for (int i = 0; i < 256; i++) {
        uint8_t r, g, b;

        if (i < 85) {  // Blue to Cyan transition
            b = 255;
            g = i * 3;
            r = 0;
        } else if (i < 170) {  // Cyan to Yellow transition
            b = 255 - (i - 85) * 3;
            g = 255;
            r = (i - 85) * 3;
        } else {  // Yellow to Red transition
            b = 0;
            g = 255 - (i - 170) * 3;
            r = 255;
        }

        fs.write(b);  // Blue
        fs.write(g);  // Green
        fs.write(r);  // Red
        fs.write(0);  // Reserved (must be 0)
    }

    // --- Write Pixel Data (Bottom-Up Order) ---
    uint8_t padding[3] = {0, 0, 0};  
    for (int y = IMG_HEIGHT - 1; y >= 0; y--) {  
        for (int x = 0; x < IMG_WIDTH; x++) {
            uint16_t temp = get_point(src, IMG_HEIGHT, IMG_WIDTH, x, y);
            temp = (temp > MAXTEMP) ? MAXTEMP : (temp < MINTEMP) ? MINTEMP : temp;
            uint8_t pixelIndex = map(temp, MINTEMP, MAXTEMP, 0, 255);  // MIN = blue, MAX = red

            fs.write(pixelIndex);
        }
        fs.write(padding, rowSize - IMG_WIDTH);
    }

    fs.close();
    Serial.println("8-bit BMP file created with color palette!");
    return true;
}

int getFileID() {
  Preferences mem;
  bool mem_active;
  int fileID = 0;

  mem_active = mem.begin(PREFERENCE_NAMESPACE, false);

  // Initialize for first call
  if (mem_active && !mem.isKey(PREFERENCE_KEY)) {
    mem.putInt(PREFERENCE_KEY, 2);
    mem.end();
    return(1);
  }

  if (mem_active) {
    fileID = mem.getInt(PREFERENCE_KEY);
    if (fileID < 300) {
      mem.putInt(PREFERENCE_KEY, fileID + 1);
      mem.end();
    }
    else {
      mem.putInt(PREFERENCE_KEY, 1);
    }
    return(fileID);
    }
  else {
    fileID = 301; // failed to read counter from memory
    return(fileID);
  }

  return fileID;
}



float getInterpolatedTemperature(float x, float y) {
    int x1 = (int)x;
    int y1 = (int)y;
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    // Ensure indices are within bounds
    if (x2 >= SENSOR_WIDTH)  x2 = SENSOR_WIDTH - 1;
    if (y2 >= SENSOR_HEIGHT) y2 = SENSOR_HEIGHT - 1;

    // Convert 2D (row, col) to 1D index
    float Q11 = interpolatedPixels[y1 * SENSOR_WIDTH + x1];  
    float Q12 = interpolatedPixels[y2 * SENSOR_WIDTH + x1];  
    float Q21 = interpolatedPixels[y1 * SENSOR_WIDTH + x2];  
    float Q22 = interpolatedPixels[y2 * SENSOR_WIDTH + x2];  

    float R1 = ((x2 - x) * Q11) + ((x - x1) * Q21);
    float R2 = ((x2 - x) * Q12) + ((x - x1) * Q22);
    return ((y2 - y) * R1) + ((y - y1) * R2);
}

// Function to map temperature to a 16-bit RGB color
uint16_t getColorFromTemperature(float temp) {
    // Normalize temperature range (18°C to 70°C) to (0 to 255)
    int tem;
    tem = constrain(temp, MINTEMP, MAXTEMP);
    int t = (int)((tem - MINTEMP) * (255.0 / (MAXTEMP - MINTEMP)));

    // Define gradient colors: Blue (cold) → Cyan → Green → Yellow → Red (hot)
    uint8_t r, g, b;
    if (t < 64) { r = 0; g = t * 4; b = 255; } // Blue to Cyan
    else if (t < 128) { r = 0; g = 255; b = 255 - (t - 64) * 4; } // Cyan to Green
    else if (t < 192) { r = (t - 128) * 4; g = 255; b = 0; } // Green to Yellow
    else { r = 255; g = 255 - (t - 192) * 4; b = 0; } // Yellow to Red

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); // Convert to 16-bit RGB565
}

void generateThermalImage() {
    float scaleX = (float)SENSOR_WIDTH / IMG_WIDTH;
    float scaleY = (float)SENSOR_HEIGHT / IMG_HEIGHT;

    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            float sensorX = x * scaleX;
            float sensorY = y * scaleY;

            float temp = getInterpolatedTemperature(sensorX, sensorY);
            thermalBitmap[y * IMG_WIDTH + x] = getColorFromTemperature(temp);
        }
    }
}

/**
 * set_point - sets a cell in a 2D array
 * @p: The 2D array
 * @rows: The number of rows in array
 * @cols: The number of columns in array
 * @x: The row_index of cell to set
 * @y: The column index of cell to set
 * @f: The value to set cell to
 *
 * Return: Void
 */
void set_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f) {
  if ((x < 0) || (x >= cols))
    return;
  if ((y < 0) || (y >= rows))
    return;
  p[y * cols + x] = f;
}

/**
 * interpolate_image - magnifies (inflates) an image from a low resolution to a higher resolution
 *                     using bicubic interpolation
 * @src: pointer to raw image array
 * @src_rows: image array row size
 * @src_cols: image array column size
 * @dest: pointer to destination to sore interpolated image
 * @dest_rows: size of rows for destination array
 * @dest_cols: size of columns for destination array
 *
 * Return: Void
 */
void interpolate_image(float *src, uint8_t src_rows, uint8_t src_cols,
                       float *dest, uint8_t dest_rows, uint8_t dest_cols) {
  float mu_x = (src_cols - 1.0) / (dest_cols - 1.0);
  float mu_y = (src_rows - 1.0) / (dest_rows - 1.0);

  float adj_2d[16]; // matrix for storing adjacents

  for (uint8_t y_idx = 0; y_idx < dest_rows; y_idx++) {
    for (uint8_t x_idx = 0; x_idx < dest_cols; x_idx++) {
      float x = x_idx * mu_x;
      float y = y_idx * mu_y;
      get_adjacents_2d(src, adj_2d, src_rows, src_cols, x, y);
      float frac_x = x - (int)x; // we only need the ~delta~ between the points
      float frac_y = y - (int)y; // we only need the ~delta~ between the points
      float out = bicubicInterpolate(adj_2d, frac_x, frac_y);
      set_point(dest, dest_rows, dest_cols, x_idx, y_idx, out);
    }
  }
}

/**
 * cubicInterpolate - Performs cubic interpolation in one dimension.
 * @p: Array of 4 adjacent points (2 on the left and 2 on the right).
 * @x: Fractional distance between the points (0 to 1).
 *
 * Return: Interpolated value.
 */
float cubicInterpolate(float p[], float x) {
  float r = p[1] + (0.5 * x *
                    (p[2] - p[0] +
                     x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] +
                          x * (3.0 * (p[1] - p[2]) + p[3] - p[0]))));
  return r;
}

/**
 * bicubicInterpolate - Performs bicubic interpolation using cubic interpolation in two dimensions.
 * @p: A 16-element (4x4) array of neighboring points around the target pixel.
 * @x: Fractional distance in the x-direction.
 * @y: Fractional distance in the y-direction.
 *
 * Return: Interpolated pixel value.
 */
float bicubicInterpolate(float p[], float x, float y) {
  float arr[4] = {0, 0, 0, 0};
  arr[0] = cubicInterpolate(p + 0, x);
  arr[1] = cubicInterpolate(p + 4, x);
  arr[2] = cubicInterpolate(p + 8, x);
  arr[3] = cubicInterpolate(p + 12, x);
  return cubicInterpolate(arr, y);
}

/**
 * get_adjacents_1d - Retrieves four adjacent points from a 2D image for 1D cubic interpolation.
 * @src: Pointer to the source image array.
 * @dest: Pre-allocated array to store the four adjacent points.
 * @rows: Number of rows in the source image.
 * @cols: Number of columns in the source image.
 * @x: X-coordinate of the target pixel.
 * @y: Y-coordinate of the target pixel.
 *
 * Return: Void
 */
void get_adjacents_1d(float *src, float *dest, uint8_t rows, uint8_t cols,
                      int8_t x, int8_t y) {
  dest[0] = get_point(src, rows, cols, x - 1, y);
  dest[1] = get_point(src, rows, cols, x, y);
  dest[2] = get_point(src, rows, cols, x + 1, y);
  dest[3] = get_point(src, rows, cols, x + 2, y);
}

/**
 * get_adjacents_2d - Retrieves a 4x4 grid of neighboring pixels around a target pixel for bicubic interpolation.
 * @src: Pointer to the source image array.
 * @dest: Pre-allocated 16-element array to store the 4x4 neighboring pixels.
 * @rows: Number of rows in the source image.
 * @cols: Number of columns in the source image.
 * @x: X-coordinate of the target pixel.
 * @y: Y-coordinate of the target pixel.
 *
 * Return: Void
 */
void get_adjacents_2d(float *src, float *dest, uint8_t rows, uint8_t cols,
                      int8_t x, int8_t y) {
  for (int8_t delta_y = -1; delta_y < 3; delta_y++) {
    float *row = dest + 4 * (delta_y + 1);
    for (int8_t delta_x = -1; delta_x < 3; delta_x++) {
      row[delta_x + 1] = get_point(src, rows, cols, x + delta_x, y + delta_y);
    }
  }
}

void drawThermalImage() {
    int blockWidth = SCREEN_WIDTH / SENSOR_WIDTH;
    int blockHeight = SCREEN_HEIGHT / SENSOR_HEIGHT;

    for (int y = 0; y < SENSOR_HEIGHT; y++) {
        for (int x = 0; x < SENSOR_WIDTH; x++) {
            float temp = interpolatedPixels[y * SENSOR_WIDTH + x];  // Get temperature
            uint16_t color = getColorFromTemperature(temp);

            // Compute rectangle corners
            uint16_t x1 = x * blockWidth;
            uint16_t y1 = y * blockHeight;
            uint16_t x2 = x1 + blockWidth - 1;
            uint16_t y2 = y1 + blockHeight - 1;

            tft.fillRectangle(x1, y1, x2, y2, color);
        }
    }
}
