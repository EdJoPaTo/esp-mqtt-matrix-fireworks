#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Configure for your panel(s) as appropriate!
const uint16_t PANEL_WIDTH = 64;
const uint16_t PANEL_HEIGHT = 64;
const uint16_t PANELS_NUMBER = 1;
const int8_t PIN_E = 18;

const uint16_t TOTAL_WIDTH = PANEL_WIDTH * PANELS_NUMBER;
const uint16_t TOTAL_HEIGHT = PANEL_HEIGHT;

// mqttBri << BRIGHTNESS_SCALE -> 0-3, 4-7, 8-11, 12-15, ... seem to be equal anyway
#define BRIGHTNESS_SCALE 2

// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;

void matrix_setup(uint8_t brightness)
{
	HUB75_I2S_CFG mxconfig;
	mxconfig.mx_height = PANEL_HEIGHT;
	mxconfig.chain_length = PANELS_NUMBER;
	mxconfig.gpio.e = PIN_E;
	dma_display = new MatrixPanel_I2S_DMA(mxconfig);
	dma_display->setBrightness8(brightness);

	// Allocate memory and start DMA display
	if (not dma_display->begin())
	{
		Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
	}
}

void matrix_brightness(uint8_t brightness)
{
	dma_display->setBrightness8(brightness);
}

void matrix_update() {}

void matrix_fill(uint8_t red, uint8_t green, uint8_t blue)
{
	dma_display->fillScreenRGB888(red, green, blue);
}

void matrix_pixel(uint16_t x, uint16_t y, uint8_t red, uint8_t green, uint8_t blue)
{
	dma_display->drawPixelRGB888(x, y, red, green, blue);
}
