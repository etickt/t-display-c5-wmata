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

// --- CST816S Capacitive Touch Controller (I2C) ---
#define TOUCH_SDA_PIN   2
#define TOUCH_SCL_PIN   3
#define TOUCH_INT_PIN   27
#define TOUCH_RST_PIN   24

// --- AXP2602 Power Management Unit (Shared I2C Bus) ---
#define I2C_SDA_PIN     2
#define I2C_SCL_PIN     3
#define AXP_INT_PIN     10

// --- Onboard Buttons ---
#define USER_BTN_PIN    0   // User button / wake-up button
#define BOOT_BTN_PIN    28  // Boot button
