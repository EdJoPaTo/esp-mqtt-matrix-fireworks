#include <NeoPixelBus.h>

const uint16_t TOTAL_WIDTH = 16;
const uint16_t TOTAL_HEIGHT = 16;

const uint16_t TOTAL_PIXELS = TOTAL_WIDTH * TOTAL_HEIGHT;

// Efficient connection via DMA on pin RDX0 GPIO3 RX
// See <https://github.com/Makuna/NeoPixelBus/wiki/FAQ-%231>
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(TOTAL_PIXELS);

// bitbanging (Fallback)
// const int PIN_MATRIX = 13; // D7
// NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(TOTAL_PIXELS, PIN_MATRIX);

NeoTopology<ColumnMajorAlternating180Layout> topo(TOTAL_WIDTH, TOTAL_HEIGHT);
