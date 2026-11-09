# LILYGO T-Display C5 — WMATA Metro Arrival Board (PIDS)

A real-time Washington Metro (WMATA) Passenger Information Display System (PIDS) LED dot-matrix sign built for the **LILYGO ESP32-C5 T-Display** (`ESP32-C5` RISC-V MCU with 16MB QIO Flash, 8MB QIO PSRAM, and a 1.9-inch `ST7789` 170×320 IPS LCD).

Displays real-time Northbound train predictions at **Navy Yard–Ballpark (`F05`)** rendered on a simulated 80×40 glowing LED matrix (`LN`, `DEST`, `MIN`) with blinking `ARR`/`BRD` indicators.

## Configuration (`secrets.h`)

Wi-Fi and WMATA API credentials are kept out of version control using `secrets.h`:

1. Copy the template file:
   - **PlatformIO:** copy `include/secrets.example.h` to `include/secrets.h`
   - **Arduino IDE:** copy `sketch/secrets.example.h` to `sketch/secrets.h`
2. Fill in your credentials in `secrets.h`:
   ```cpp
   #pragma once

   #define WIFI_SSID     "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   #define WMATA_API_KEY "YOUR_WMATA_API_KEY"
   ```

## Hardware Pinout Summary

| Function | ESP32-C5 GPIO | Description |
| :--- | :--- | :--- |
| `TFT_CS_PIN` | `GPIO 26` | LCD SPI Chip Select |
| `TFT_SCLK_PIN` | `GPIO 7` | LCD SPI Clock |
| `TFT_MOSI_PIN` | `GPIO 9` | LCD SPI MOSI (Data) |
| `TFT_DC_PIN` | `GPIO 8` | LCD Data/Command |
| `TFT_RST_PIN` | `GPIO 23` | LCD Hardware Reset |
| `TFT_BL_PIN` | `GPIO 25` | LCD Backlight PWM (Active HIGH) |
| `I2C_SDA_PIN` | `GPIO 2` | CST816S Touch & AXP2602 PMU SDA |
| `I2C_SCL_PIN` | `GPIO 3` | CST816S Touch & AXP2602 PMU SCL |

## Building & Flashing in VS Code (PlatformIO)

1. Open the **`t-display-c5-hello`** folder in **VS Code** with the **PlatformIO IDE** extension installed.
2. Connect your **LILYGO T-Display C5** via USB-C.
3. Click **PlatformIO: Build** (`✓` icon in the bottom status bar) to compile and download dependencies automatically.
4. Click **PlatformIO: Upload** (`→` icon in the bottom status bar) to flash the board.

> **Tip (Entering Download Mode):** If the upload fails to connect automatically, hold down the **BOOT** button, press and release **RST**, then release **BOOT** and click Upload again.
