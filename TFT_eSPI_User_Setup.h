// =============================================================================
// TFT_eSPI — User_Setup.h for 1.54" TFT LCD Display V1.0 (ST7789, 240x240)
// Board: Dasduino ESP32-WROVER-E
//
// INSTRUCTIONS:
//   1. Install TFT_eSPI library in Arduino IDE (by Bodmer)
//   2. Navigate to your Arduino libraries folder:
//      Windows: Documents\Arduino\libraries\TFT_eSPI\
//      Mac/Linux: ~/Arduino/libraries/TFT_eSPI/
//   3. Replace the existing User_Setup.h with THIS file
//   4. Save and recompile your sketch
// =============================================================================

// ── Driver ───────────────────────────────────────────────────────────────────
#define ST7789_DRIVER

// ── Resolution ───────────────────────────────────────────────────────────────
#define TFT_WIDTH   240
#define TFT_HEIGHT  240

// ── Pin assignments (must match your wiring) ─────────────────────────────────
#define TFT_CS      14   // Chip select
#define TFT_DC       2   // Data/Command
#define TFT_RST     15   // Reset
#define TFT_MOSI    23   // SPI MOSI
#define TFT_SCLK    18   // SPI Clock
// TFT_MISO not needed for write-only display

// ── SPI frequency ────────────────────────────────────────────────────────────
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY   20000000

// ── Colour order (try RGB first, switch to BGR if colours look wrong) ─────────
#define TFT_RGB_ORDER TFT_RGB  // Change to TFT_BGR if colours are inverted

// ── Font loading ─────────────────────────────────────────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
