#include <Adafruit_NeoMatrix.h>

const uint16_t TOTAL_WIDTH = 16;
const uint16_t TOTAL_HEIGHT = 16;

const int PIN_MATRIX = 13; // D7

// mqttBri << BRIGHTNESS_SCALE -> full scale seems to be available
#define BRIGHTNESS_SCALE 0

// MATRIX DECLARATION:
// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

// Example for NeoPixel Shield.  In this application we'd like to use it
// as a 5x8 tall matrix, with the USB port positioned at the top of the
// Arduino.  When held that way, the first pixel is at the top right, and
// lines are arranged in columns, progressive order.  The shield uses
// 800 KHz (v2) pixels that expect GRB color data.
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(TOTAL_WIDTH, TOTAL_HEIGHT, PIN_MATRIX,
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

void matrix_setup(uint8_t brightness)
{
    matrix.begin();
    matrix.setBrightness(brightness);
}

void matrix_brightness(uint8_t brightness)
{
    matrix.setBrightness(brightness);
}

void matrix_update()
{
    matrix.show();
}

void matrix_fill(uint8_t red, uint8_t green, uint8_t blue)
{
    matrix.fillScreen(matrix.Color(red, green, blue));
}

void matrix_pixel(uint16_t x, uint16_t y, uint8_t red, uint8_t green, uint8_t blue)
{
    matrix.drawPixel(x, y, matrix.Color(red, green, blue));
}
