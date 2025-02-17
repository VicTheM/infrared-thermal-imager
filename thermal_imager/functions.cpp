#include "thermal_imager_header.h"

/********************************** Function definitions ************************************/

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


bool generate_bmp(float *src, fs::FS &file, const char *filename) {
    // BMP Header with little-endian width & height
    uint8_t bmpHeader[54] = {
        0x42, 0x4D, 0, 0, 0, 0,  // File size (to be calculated)
        0x00, 0x00, 0x00, 0x00,  // Reserved
        0x36, 0x00, 0x00, 0x00,  // Offset to pixel data (54 bytes)

        // DIB Header (40 bytes)
        0x28, 0x00, 0x00, 0x00,  // Header size
        IMG_WIDTH & 0xFF, (IMG_WIDTH >> 8) & 0xFF, 0x00, 0x00,  // Width (little-endian)
        IMG_HEIGHT & 0xFF, (IMG_HEIGHT >> 8) & 0xFF, 0x00, 0x00,  // Height (little-endian)
        0x01, 0x00,              // Planes (1)
        0x18, 0x00,              // Bits per pixel (24-bit)
        0x00, 0x00, 0x00, 0x00,  // Compression (None)
        0, 0, 0, 0,              // Image size (not needed for uncompressed BMP)
        0x13, 0x0B, 0x00, 0x00,  // Horizontal resolution (2835 pixels/m)
        0x13, 0x0B, 0x00, 0x00,  // Vertical resolution (2835 pixels/m)
        0x00, 0x00, 0x00, 0x00,  // Colors in palette (0 = all)
        0x00, 0x00, 0x00, 0x00   // Important colors (0 = all)
    };

    // Calculate file size: header + pixel data
    int rowSize = (IMG_WIDTH * 3 + 3) & ~3;  // Each row must be padded to 4 bytes
    int fileSize = 54 + (rowSize * IMG_HEIGHT);
    
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

    // Write pixel data (bottom-up order)
    uint8_t padding[3] = {0, 0, 0};  // BMP row padding
    for (int y = IMG_HEIGHT - 1; y >= 0; y--) {  
        for (int x = 0; x < IMG_WIDTH; x++) {
            float temp = get_point(src, IMG_HEIGHT, IMG_WIDTH, x, y);  // Get temperature data
            uint8_t grayscale = map(temp, 28, 40, 0, 255);  // Normalize to 0-255
            
            fs.write(grayscale);  // Blue
            fs.write(grayscale);  // Green
            fs.write(grayscale);  // Red
        }
        // Write padding if necessary
        fs.write(padding, rowSize - (IMG_WIDTH * 3));
    }

    fs.close();
    Serial.println("BMP file created successfully");
    return true;
}