#pragma once

// ============================================================================
// LILYGO T-Display C5 (ESP32-C5) Hardware Pin Definitions
// ============================================================================

// --- 1.9" ST7789 IPS LCD Display (170 x 320, 4-wire SPI) ---
#define LCD_WIDTH       170
#define LCD_HEIGHT      320

#define TFT_CS_PIN      26
#define TFT_SCLK_PIN    7
#define TFT_MOSI_PIN    9
#define TFT_DC_PIN      8
#define TFT_RST_PIN     23
#define TFT_BL_PIN      25  // Backlight PWM (Active HIGH)
