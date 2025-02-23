#include "thermal_imager.h"
#include <WebServer.h>
#include <WiFi.h>

/************************************* Variable declarations ************************************/
GridEYE grideye;
SPIClass hspi(HSPI);
TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, TFT_BRIGHTNESS);
Button captureButton = {CAPTURE_PIN, false, 0};
Button serverSwitch = {WEB_SERVER_PIN, false, 0};
unsigned int counter;
String filename;
int fileID;
uint16_t* thermalBitmap;
float pixels[AMG8833RES_X * AMG8833RES_Y];
float interpolatedPixels[SENSOR_HEIGHT * SENSOR_WIDTH];
_point c1, c2, c3, cc;
int16_t steps = (int16_t)(360 / ROTATE_ANGLE);
const char *_myapSSID = "Thermal Imager";
const char *_myapPassword = "AMG8833_BME";


/******************************* Setup code: Entry point to run once ***********************/
void setup() {
  hspi.begin();
  tft.begin(hspi);
  Wire.begin();
  delay(500);
  grideye.begin();
  delay(500);
  Serial.begin(115200);

  thermalBitmap = new (std::nothrow) uint16_t[IMG_WIDTH * IMG_HEIGHT];
  if (!thermalBitmap) {
    // Serial.println("Failed to allocate array");
  }

  pinMode(captureButton.PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_PIN, INPUT_PULLUP);
  pinMode(WEB_SERVER_LED_PIN, OUTPUT);

  // Welcome Intro
  tft.clear();
  tft.drawRectangle(0, 0, tft.maxX() - 1, tft.maxY() - 1, COLOR_WHITE);
  tft.setFont(Terminal6x8);
  tft.drawText(9, 20, "INFRARED THERMAL IMAGER", COLOR_RED);
  tft.drawText(50, 190, "Loading ...", COLOR_WHITE);

  // check for partition
  esp_partition_iterator_t i;
  i = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (i) {
    fsys = &LittleFS;
  } else {
  }
  esp_partition_iterator_release(i);

  if (!fsys) {
    // Serial.println("Fs bad"); // bad
  } else if ((fsys == (fs::FS *)&LittleFS) && (!LittleFS.begin())) {
    LittleFS.format();
    // Serial.println("Restarting esp32");
    ESP.restart();
  }

  WiFi.setHostname(HOSTNAME);
  attachInterrupt(captureButton.PIN, isr, FALLING);
  digitalWrite(WEB_SERVER_LED_PIN, LOW);
  
  // Define triangle start coordinates (Welcome Intro cont.)
  c1.x = 38;  c1.y = 60;
  c2.x = 128; c2.y = 110;
  c3.x = 88;  c3.y = 160;
  cc = getCoordCentroid(c1, c2, c3);

  for (int8_t i = 0; i < steps; i++) {
    // Draw triangle
    tft.drawTriangle(c1.x, c1.y, c2.x, c2.y, c3.x, c3.y, COLOR_GREEN);
    // Rotate triangle
    rotateTriangle(c1, c2, c3, cc, ROTATE_ANGLE);
    delay(50);
  }
  delay(3000);
  tft.clear();
}


/*************************************** Main program loop ********************************************/
void loop() {
  /**
   * HOW THIS LOOP WORKS
   * 1. Read the thermal sensor and store the values in an array
   * 2. Interpolate the values to a higher resolution and display on screen 
   * It keeps doing this until either the capture button get pressed or the server switch is toggled
   * If capture button is pressed
   * 3. Yet interpolate the interpolated values to a higher resolution and save as a bmp file
   * If server switch is toggled
   * 4. Start a web server and disable capture button
   * 5. Display the server details on the TFT screen
   * 6. Respomd to requests from web server
   */
  if (serverSwitch.pressed == false) {
    for (counter=0; counter < THERMAL_SENSOR_RESOLUTION; counter++) {
      pixels[counter] = grideye.getPixelTemperature(counter);
    }
    interpolate_image(pixels, AMG8833RES_X, AMG8833RES_Y, interpolatedPixels, SENSOR_WIDTH, SENSOR_HEIGHT);
    drawThermalImage(); // low resolution image
  }

  // CAPTURE IMAGE
  if (captureButton.pressed && serverSwitch.pressed == false) {
    captureButton.pressed = false;
    fileID = getFileID();
    filename = "/img_" + String(fileID) + ".bmp";
    interpolate1DArray(interpolatedPixels, thermalBitmap); // high resolution
    if (createTemperatureBMP(thermalBitmap, filename.c_str())) {
      ;
    } else {
      ;
    }
  }

  // ENABLE WEB SERVER
  if(!digitalRead(serverSwitch.PIN)){
    unsigned long currentTime = millis();
    if (currentTime - serverSwitch.lastPressTime > SERVER_DELAY && serverSwitch.pressed == false) {
      serverSwitch.pressed = true;
      if (!WiFi.softAP(_myapSSID, _myapPassword)){
        ;
      }
      IPAddress IP = WiFi.softAPIP();
      String apURL = "http://" + IP.toString() + "/";
      startServer();
      serverSwitch.lastPressTime = currentTime;
      digitalWrite(WEB_SERVER_LED_PIN, HIGH);
      tft.clear();
      tft.drawRectangle(0, 0, tft.maxX() - 1, tft.maxY() - 1, COLOR_WHITE);
      tft.setFont(Terminal6x8);
      tft.drawText(30, 60, "WEB SERVER IS ON", COLOR_RED);
      tft.drawText(9, 100, "1. Connect to hotspot", COLOR_WHITE);
      tft.drawText(9, 120, "2. Enter url in browser", COLOR_WHITE);
      tft.drawText(9, 160, (String("Wifi Name: ") + _myapSSID).c_str(), COLOR_WHITE);
      tft.drawText(9, 180, (String("Password: ") + _myapPassword).c_str(), COLOR_WHITE);
      tft.drawText(9, 200, (String("URL: http://") + IP.toString()).c_str(), COLOR_WHITE);
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
      tft.clear();
      WiFi.softAPdisconnect(true);
    }
  }
  if (serverSwitch.pressed == true) {
    handleClient();
  }
}


/**************************************** Function definitions *****************************************/
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
    float val;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            val = get_point(p, rows, cols, x, y);
            Serial.print(val, 2);
            // Serial.print("\t");
        }
        // Serial.println();
    }
    // Serial.println();
}


/**
 * printThermalGrid - print the thermal heat values to the terminal
 * @p: The 1D array to be printed on a 2D plane
 * @rows: The number of rows to print
 * @cols: The number of columns to print
 *
 * Return: Void
 */
void printThermalGrid(uint16_t *p, uint8_t rows, uint8_t cols) {
    // Serial.println(F("Thermal Sensor Readings:"));
    // float val;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            // val = get_point(p, rows, cols, x, y);
            // Serial.print(val, 2);
            // Serial.print("\t");
        }
        // Serial.println();
    }
    // Serial.println();
}


/**
 * getFileID - Get the next file ID for saving thermal images
 *
 * Return: Integer - The next file ID
 */
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
    if (fileID < 1000) {
      mem.putInt(PREFERENCE_KEY, fileID + 1);
      mem.end();
    }
    else {
      mem.putInt(PREFERENCE_KEY, 1);
    }
    return(fileID);
    }
  else {
    fileID = 1001; // failed to read counter from memory
    return(fileID);
  }

  return fileID;
}


/**
 * getColorFromTemperature - Get the color of a pixel based on its temperature
 * @temp: The temperature of the pixel
 *
 * Return: 16-bit RGB565 color
 */
uint16_t getColorFromTemperature(float temp) {
    int tem = constrain(temp, MINTEMP, MAXTEMP);
    int t = (int)((tem - MINTEMP) * (255.0 / (MAXTEMP - MINTEMP)));

    // Define gradient colors: Blue (cold) → Cyan → Green → Yellow → Red (hot)
    uint8_t r, g, b;
    if (t < 64) { r = 0; g = t * 4; b = 255; }                      // Blue to Cyan
    else if (t < 128) { r = 0; g = 255; b = 255 - (t - 64) * 4; }   // Cyan to Green
    else if (t < 192) { r = (t - 128) * 4; g = 255; b = 0; }        // Green to Yellow
    else { r = 255; g = 255 - (t - 192) * 4; b = 0; }               // Yellow to Red

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);        // Convert to 16-bit RGB565
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


/**
 * drawThermalImage - Draws the thermal image (low resolution) on the TFT screen.
 * It scales the picture to fit the screen size by using rectangles to represent pixels.
 * The size of a rectangle is the scale factor and it is low resolution because of this.
 *
 * Return: Void
 */
void drawThermalImage() {
    int blockWidth = SCREEN_WIDTH / SENSOR_WIDTH;
    int blockHeight = SCREEN_HEIGHT / SENSOR_HEIGHT;

    for (int y = 0; y < SENSOR_HEIGHT; y++) {
        for (int x = 0; x < SENSOR_WIDTH; x++) {
            float temp = interpolatedPixels[y * SENSOR_WIDTH + x];
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


/**
 * getTemperatureColor - Get the RGB color for a given temperature.
 * @temp: The temperature value.
 * @minTemp: The minimum temperature in the color map.
 * @maxTemp: The maximum temperature in the color map.
 * @r: Pointer to the red component of the color.
 * @g: Pointer to the green component of the color.
 * @b: Pointer to the blue component of the color.
 *
 * Return: Void
 */
void getTemperatureColor(uint16_t temp, float minTemp, float maxTemp, uint8_t& r, uint8_t& g, uint8_t& b) {
    int tem;
    tem = constrain(temp, minTemp, maxTemp);
    int t = (int)((tem - minTemp) * (255.0 / (maxTemp - minTemp)));

    // Define gradient colors: Blue (cold) → Cyan → Green → Yellow → Red (hot)
    if (t < 64) { r = 0; g = t * 4; b = 255; } // Blue to Cyan
    else if (t < 128) { r = 0; g = 255; b = 255 - (t - 64) * 4; } // Cyan to Green
    else if (t < 192) { r = (t - 128) * 4; g = 255; b = 0; }      // Green to Yellow
    else { r = 255; g = 255 - (t - 192) * 4; b = 0; }             // Yellow to Red
}


/**
 * createTemperatureBMP - Creates a BMP file from the temperature array.
 * @tempArray: The temperature array.
 * @filename: The filename of the BMP file.
 *
 * Return: True if the file was created successfully, false otherwise.
 */
bool createTemperatureBMP(uint16_t* tempArray, const char* filename) {
    // Calculate BMP sizes
    uint32_t rowSize = ((24 * IMG_WIDTH + 31) / 32) * 4;  // Padding to 4-byte boundary
    uint32_t imageSize = rowSize * IMG_HEIGHT;
    uint32_t fileSize = 54 + imageSize;                   // Header + pixel data

    uint8_t bmpHeader[54] = {
        0x42, 0x4D, 0, 0, 0, 0,  // File size (to be calculated)
        0x00, 0x00, 0x00, 0x00,  // Reserved
        0x36, 0x00, 0x00, 0x00,  // Offset to pixel data (54 bytes, no color palette)

        // DIB Header
        0x28, 0x00, 0x00, 0x00,  // Header size (40 bytes)
        0x00, 0x00, 0x00, 0x00,  // TO BE COMPUTED
        0x00, 0x00, 0x00, 0x00,  // TO BE COMPUTED
        0x01, 0x00,              // Planes (1)
        0x18, 0x00,              // Bits per pixel (24-bit)
        0x00, 0x00, 0x00, 0x00,  // Compression (None)
        0, 0, 0, 0,              // Image size (can be 0 for uncompressed)
        0x13, 0x0B, 0x00, 0x00,  // Horizontal resolution (2835 pixels/meter)
        0x13, 0x0B, 0x00, 0x00,  // Vertical resolution (2835 pixels/meter)
        0x00, 0x00, 0x00, 0x00,  // Colors in palette (0 for 24-bit)
        0x00, 0x00, 0x00, 0x00   // Important colors (0)
    };
    // Convert integers to little-endian byte order
    bmpHeader[18] = (uint8_t)(IMG_WIDTH & 0xFF);
    bmpHeader[19] = (uint8_t)((IMG_WIDTH >> 8) & 0xFF);
    bmpHeader[20] = (uint8_t)((IMG_WIDTH >> 16) & 0xFF);
    bmpHeader[21] = (uint8_t)((IMG_WIDTH >> 24) & 0xFF);

    bmpHeader[22] = (uint8_t)(IMG_HEIGHT & 0xFF);
    bmpHeader[23] = (uint8_t)((IMG_HEIGHT >> 8) & 0xFF);
    bmpHeader[24] = (uint8_t)((IMG_HEIGHT >> 16) & 0xFF);
    bmpHeader[25] = (uint8_t)((IMG_HEIGHT >> 24) & 0xFF);

    // Open file in LittleFS
    monitorMemoryOverflow();
    File file = fsys->open(filename, "w");
    if (!file) {
        // Serial.println("Failed to create file");
        return false;
    }

    // Update file size in header
    bmpHeader[2] = (uint8_t)(fileSize & 0xFF);
    bmpHeader[3] = (uint8_t)((fileSize >> 8) & 0xFF);
    bmpHeader[4] = (uint8_t)((fileSize >> 16) & 0xFF);
    bmpHeader[5] = (uint8_t)((fileSize >> 24) & 0xFF);

    // Write BMP header
    file.write(bmpHeader, 54);

    // Create pixel buffer for one row
    uint8_t* rowBuffer = (uint8_t*)malloc(rowSize);
    if (!rowBuffer) {
        file.close();
        return false;
    }

    // Generate image data (bottom-up as per BMP spec)
    for (int y = IMG_HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            uint16_t temp = tempArray[y * IMG_WIDTH + x];

            // Get color for this temperature
            uint8_t r, g, b;
            getTemperatureColor(temp, MINTEMP, MAXTEMP, r, g, b);

            // Write BGR (BMP format uses BGR order)
            rowBuffer[x * 3] = b;
            rowBuffer[x * 3 + 1] = g;
            rowBuffer[x * 3 + 2] = r;
        }

        // Pad row to 4-byte boundary
        for (int i = IMG_WIDTH * 3; i < rowSize; i++) {
            rowBuffer[i] = 0;
        }

        // Write row to file
        file.write(rowBuffer, rowSize);
    }

    free(rowBuffer);
    file.close();
    return true;
}


/**
 * mapIntoRGB565Color - Maps the temperature values into RGB565 colors.
 * @arr: Array of temperature values.
 * @len: Length of the array.
 *
 * Return: Void
 */
void mapIntoRGB565Color(uint16_t *arr, int len) {
    uint16_t t;
    uint8_t r, g, b;
    for (int x = 0; x < len; x++) {
      t = (int)((arr[x] - MINTEMP) * (255.0 / (MAXTEMP - MINTEMP)));

      // Define gradient colors: Blue (cold) → Cyan → Green → Yellow → Red (hot)
      if (t < 64) {
        r = 0; g = t * 4; b = 255;
        }                             // Blue to Cyan
      else if (t < 128) {
        r = 0; g = 255; b = 255 - (t - 64) * 4;
        }                             // Cyan to Green
      else if (t < 192) {
        r = (t - 128) * 4; g = 255; b = 0;
        }                             // Green to Yellow
      else {
        r = 255; g = 255 - (t - 192) * 4; b = 0;
        }                              // Yellow to Red
      arr[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); // Convert to 16-bit RGB565
    }
}


/**
 * interpolate1DArray - Interpolates a 1D array to a higher resolution.
 * @sensorData: 1D array of sensor data.
 * @imgData: 1D array to store interpolated data.
 *
 * Return: Void
 */
void interpolate1DArray(float* sensorData, uint16_t* imgData) {
    float xRatio = (float)(SENSOR_WIDTH - 1) / (IMG_WIDTH - 1);
    float yRatio = (float)(SENSOR_HEIGHT - 1) / (IMG_HEIGHT - 1);

    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            float gx = x * xRatio;
            float gy = y * yRatio;

            int x0 = (int)gx;
            int y0 = (int)gy;
            int x1 = min(x0 + 1, SENSOR_WIDTH - 1);
            int y1 = min(y0 + 1, SENSOR_HEIGHT - 1);

            float dx = gx - x0;
            float dy = gy - y0;

            float topLeft = sensorData[y0 * SENSOR_WIDTH + x0];
            float topRight = sensorData[y0 * SENSOR_WIDTH + x1];
            float bottomLeft = sensorData[y1 * SENSOR_WIDTH + x0];
            float bottomRight = sensorData[y1 * SENSOR_WIDTH + x1];

            float topInterp = topLeft + dx * (topRight - topLeft);
            float bottomInterp = bottomLeft + dx * (bottomRight - bottomLeft);
            float finalValue = topInterp + dy * (bottomInterp - topInterp);

            imgData[y * IMG_WIDTH + x] = finalValue;
        }
    }
}


/**
 * getInterpolatedTemperature - Interpolates the temperature at a given point.
 * @x: X-coordinate of the point.
 * @y: Y-coordinate of the point.
 *
 * Return: Interpolated temperature.
 */
_point getCoordCentroid( _point a, _point b, _point c ) {
  _point o;

  o.x = (int16_t)((a.x + b.x + c.x) / 3);
  o.y = (int16_t)((a.y + b.y + c.y) / 3);

  return o;
}


/**
  * rotateTriangle - Rotates a triangle around a point by a given angle.
  * @a: First point of the triangle.
  * @b: Second point of the triangle.
  * @c: Third point of the triangle.
  * @r: Point to rotate around.
  * @deg: Angle of rotation in degrees.
  *
  * Return: Void
  */
void rotateTriangle( _point &a, _point &b, _point &c, _point r, int16_t deg ) {

  // Convert degrees to radians
  float angle = (float)deg * 1000 / 57296;

  // Rotate each individual point
  a = rotatePoint( r, angle, a);
  b = rotatePoint( r, angle, b);
  c = rotatePoint( r, angle, c);
}


/**
 * rotatePoint - Rotates a point around a center point by a given angle.
 * @c: Center point to rotate around.
 * @angle: Angle of rotation in radians.
 * @p: Point to rotate.
 *
 * Return: Rotated point.
 */
_point rotatePoint( _point c, float angle, _point p ) {
  _point r;
  
  // 1. translate point back to origin
  // 2. rotate point
  // 3. translate point back

  r.x = cos(angle) * (p.x - c.x) - sin(angle) * (p.y - c.y) + c.x;
  r.y = sin(angle) * (p.x - c.x) + cos(angle) * (p.y - c.y) + c.y;

  return r;
}


/**
 * getNumberOfFiles - Get the number of files in the root directory.
 *
 * Return: Number of files.
 */
unsigned int getNumberOfFiles() {
  File dir = fsys->open("/", "r");
  unsigned int n = 0;

  while (File entry = dir.openNextFile()) {
    n++;
  }

  return n;
}


/**
 * deleteFiles - Deletes all files in the root directory.
 *
 * Return: Void
 */
void deleteFiles() {
    Serial.println("Deleting all files...");
    File root = fsys->open("/", "r");
    
    while (File file = root.openNextFile("r")) {
        String filename = file.name();
        filename = "/" + filename;
        file.close();
        fsys->remove(filename);
    }
}


/**
 * monitorMemoryOverflow - Monitors the number of files in the root directory and deletes them if it exceeds the limit.
 *
 * Return: Void
 */
void monitorMemoryOverflow() {
  unsigned int numberOfFiles = getNumberOfFiles();
  if (numberOfFiles >= MAX_NUMBER_OF_FILES) {
    deleteFiles();
  }
}