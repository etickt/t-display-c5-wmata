#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "pin_config.h"
#include "secrets.h"

// ============================================================================
// Wi-Fi & WMATA API Configuration (Navy Yard-Ballpark Station = F05)
// ============================================================================
// WIFI_SSID, WIFI_PASSWORD, and WMATA_API_KEY are defined in secrets.h
const char* WMATA_URL     = "https://api.wmata.com/StationPrediction.svc/json/GetPrediction/F05";
// Group "1" at F05 (Navy Yard-Ballpark) is Northbound toward Greenbelt / Mt Vernon Sq
const char* TARGET_GROUP  = "1";

const unsigned long FETCH_INTERVAL_MS = 20000; // Refresh predictions every 20s
const unsigned long BLINK_INTERVAL_MS = 600;   // Blink ARR / BRD indicator

// Use hardware SPI for fast screen updates
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

// ============================================================================
// WMATA PIDS LED Dot-Matrix Geometry (80 cols x 40 rows on 320x170 screen)
// ============================================================================
// 80 cols * 4px pitch = 320px wide
// 40 rows * 4px pitch = 160px tall (5px top/bottom margin)
#define MATRIX_COLS     80
#define MATRIX_ROWS     40
#define LED_PITCH       4
#define MATRIX_OFFSET_X 0
#define MATRIX_OFFSET_Y 5

// Column X positions in LED dots (without CAR column):
// LN (cols 1..12), DEST (cols 15..59), MIN (cols 62..78)
#define COL_LN_X        1
#define COL_DEST_X      15
#define COL_MIN_X       62

// Row Y positions in LED dots for Header + 3 Train rows:
static const int16_t ROW_Y[4] = { 2, 12, 22, 32 };

// Authentic WMATA PIDS LED Colors (RGB565)
#define WMATA_BG        0x0000  // Pitch black housing
#define WMATA_BEZEL     0x18C3  // Subtle dark metallic housing line
#define LED_OFF_COLOR   0x1860  // Dim unlit LED dot in matrix
#define LED_RED         0xF8A0  // WMATA header / ARR / BRD red-orange LED
#define LED_RED_HALO    0x9000  // Corner anti-aliasing for red LED
#define LED_AMBER       0xFD40  // Classic WMATA glowing amber-orange LED
#define LED_AMBER_HALO  0x9300  // Corner anti-aliasing for amber LED
#define LED_GREEN       0x07E0  // Bright Green LED for "GR" Green Line
#define LED_GREEN_HALO  0x03E0  // Corner anti-aliasing for green LED

// 1-bit offscreen canvas matching our 80x40 LED dot grid
GFXcanvas1 ledCanvas(MATRIX_COLS, MATRIX_ROWS);

struct TrainPrediction {
    bool valid;
    String line;
    String dest;
    String min;
};

TrainPrediction northTrains[3];
bool hasFetchedOnce = false;
bool blinkState = true;
unsigned long lastFetchTime = 0;
unsigned long lastBlinkTime = 0;

// Draw a single glowing 3x3 LED dot at matrix coordinate (col, row)
void drawLedDot(int16_t col, int16_t row, uint16_t coreColor, uint16_t haloColor) {
    int16_t px = MATRIX_OFFSET_X + col * LED_PITCH + 1;
    int16_t py = MATRIX_OFFSET_Y + row * LED_PITCH + 1;

    // 3x3 dot: softer corners (haloColor) + bright cross/center (coreColor)
    tft.drawPixel(px,     py,     haloColor);
    tft.drawPixel(px + 2, py,     haloColor);
    tft.drawPixel(px,     py + 2, haloColor);
    tft.drawPixel(px + 2, py + 2, haloColor);

    tft.drawFastHLine(px, py + 1, 3, coreColor);
    tft.drawFastVLine(px + 1, py, 3, coreColor);
}

// Render a rectangular region of the 1-bit LED canvas to the physical TFT
void renderCanvasRegion(int16_t startRow, int16_t endRow, int16_t startCol, int16_t endCol,
                        uint16_t coreColor, uint16_t haloColor) {
    for (int16_t r = startRow; r <= endRow; r++) {
        for (int16_t c = startCol; c <= endCol; c++) {
            if (ledCanvas.getPixel(c, r)) {
                drawLedDot(c, r, coreColor, haloColor);
            } else {
                drawLedDot(c, r, LED_OFF_COLOR, LED_OFF_COLOR);
            }
        }
    }
}

// Format destination string to fit cleanly in the 7-char (45-dot) DEST column
String formatDest(const String& raw) {
    if (raw.equalsIgnoreCase("Greenbelt")) return "Grnbelt";
    if (raw.startsWith("Mt Vernon")) return "MtVrnSq";
    if (raw.length() > 7) return raw.substring(0, 7);
    return raw;
}

// Draw the static WMATA housing and top Red header ("LN  DEST     MIN")
void initWmataBoard() {
    tft.fillScreen(WMATA_BG);
    tft.drawFastHLine(0, 2, 320, WMATA_BEZEL);
    tft.drawFastHLine(0, 167, 320, WMATA_BEZEL);

    // Fill entire 80x40 matrix with unlit LED dots
    for (int16_t r = 0; r < MATRIX_ROWS; r++) {
        for (int16_t c = 0; c < MATRIX_COLS; c++) {
            drawLedDot(c, r, LED_OFF_COLOR, LED_OFF_COLOR);
        }
    }

    ledCanvas.setTextWrap(false);
    ledCanvas.setTextSize(1);
    ledCanvas.fillScreen(0);

    // Header Row (Row 0): LN   DEST        MIN
    ledCanvas.setCursor(COL_LN_X,   ROW_Y[0]);
    ledCanvas.print("LN");
    ledCanvas.setCursor(COL_DEST_X, ROW_Y[0]);
    ledCanvas.print("DEST");
    ledCanvas.setCursor(COL_MIN_X,  ROW_Y[0]);
    ledCanvas.print("MIN");

    renderCanvasRegion(ROW_Y[0], ROW_Y[0] + 6, 0, MATRIX_COLS - 1, LED_RED, LED_RED_HALO);
}

// Display status message on the 3 train rows (e.g., while connecting to Wi-Fi)
void drawStatusRows(const char* line1, const char* line2, const char* line3) {
    const char* lines[3] = { line1, line2, line3 };
    for (int i = 0; i < 3; i++) {
        int16_t y = ROW_Y[i + 1];
        ledCanvas.fillRect(0, y, MATRIX_COLS, 7, 0);
        if (lines[i] && strlen(lines[i]) > 0) {
            ledCanvas.setCursor(COL_LN_X, y);
            ledCanvas.print(lines[i]);
        }
        renderCanvasRegion(y, y + 6, 0, MATRIX_COLS - 1, LED_AMBER, LED_AMBER_HALO);
    }
}

// Render the MIN column for a specific train row (supports blinking ARR/BRD)
void renderTrainMin(int trainIdx, bool showBlinkingText) {
    int16_t y = ROW_Y[trainIdx + 1];
    ledCanvas.fillRect(COL_MIN_X, y, MATRIX_COLS - COL_MIN_X, 7, 0);

    if (northTrains[trainIdx].valid) {
        String m = northTrains[trainIdx].min;
        bool isArrOrBrd = (m == "ARR" || m == "BRD");

        if (!isArrOrBrd || showBlinkingText) {
            // Right-align 1- or 2-digit minute numbers in the 3-char MIN field
            char buf[6];
            snprintf(buf, sizeof(buf), "%3s", m.c_str());
            ledCanvas.setCursor(COL_MIN_X, y);
            ledCanvas.print(buf);
        }

        if (isArrOrBrd) {
            renderCanvasRegion(y, y + 6, COL_MIN_X, MATRIX_COLS - 1, LED_RED, LED_RED_HALO);
        } else {
            renderCanvasRegion(y, y + 6, COL_MIN_X, MATRIX_COLS - 1, LED_AMBER, LED_AMBER_HALO);
        }
    } else {
        renderCanvasRegion(y, y + 6, COL_MIN_X, MATRIX_COLS - 1, LED_AMBER, LED_AMBER_HALO);
    }
}

// Render all 3 Northbound train rows onto the LED dot-matrix
void renderTrains() {
    for (int i = 0; i < 3; i++) {
        int16_t y = ROW_Y[i + 1];
        ledCanvas.fillRect(0, y, MATRIX_COLS, 7, 0);

        if (northTrains[i].valid) {
            String ln = northTrains[i].line.substring(0, 2);

            // Line ("GR", etc.)
            ledCanvas.setCursor(COL_LN_X, y);
            ledCanvas.print(ln);

            // Destination
            ledCanvas.setCursor(COL_DEST_X, y);
            ledCanvas.print(formatDest(northTrains[i].dest));

            // Render "GR" in bright Green LEDs (or Amber for other lines)
            if (ln == "GR") {
                renderCanvasRegion(y, y + 6, 0, COL_DEST_X - 1, LED_GREEN, LED_GREEN_HALO);
            } else {
                renderCanvasRegion(y, y + 6, 0, COL_DEST_X - 1, LED_AMBER, LED_AMBER_HALO);
            }

            // Render DEST in classic WMATA Amber LEDs
            renderCanvasRegion(y, y + 6, COL_DEST_X, COL_MIN_X - 1, LED_AMBER, LED_AMBER_HALO);
        } else {
            if (i == 0) {
                ledCanvas.setCursor(COL_LN_X, y);
                ledCanvas.print("NO TRAINS");
            }
            renderCanvasRegion(y, y + 6, 0, COL_MIN_X - 1, LED_AMBER, LED_AMBER_HALO);
        }

        renderTrainMin(i, blinkState);
    }
}

bool fetchWmataPredictions() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Skip CA cert bundle verification for fast HTTPS requests

    HTTPClient https;
    if (!https.begin(client, WMATA_URL)) {
        Serial.println("HTTPS begin failed");
        return false;
    }

    https.addHeader("api_key", WMATA_API_KEY);
    https.setTimeout(8000);

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("WMATA HTTP error: %d\n", httpCode);
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    for (int i = 0; i < 3; i++) {
        northTrains[i].valid = false;
    }

    int count = 0;
    JsonArray trains = doc["Trains"].as<JsonArray>();
    for (JsonObject t : trains) {
        const char* group = t["Group"] | "";
        // Track Group "1" at F05 (Navy Yard-Ballpark) is Northbound
        if (strcmp(group, TARGET_GROUP) == 0) {
            northTrains[count].valid = true;
            northTrains[count].line  = t["Line"] | "GR";
            northTrains[count].dest  = t["Destination"] | "Grnbelt";
            northTrains[count].min   = t["Min"]  | "";
            count++;
            if (count >= 3) break;
        }
    }

    Serial.printf("Fetched %d Northbound trains at Navy Yard (F05)\n", count);
    return true;
}

void connectWiFi() {
    String wifiLabel = String("WIFI: ") + WIFI_SSID;
    drawStatusRows(wifiLabel.c_str(), "CONNECTING...", "NAVY YARD (N)");
    Serial.printf("Connecting to Wi-Fi SSID: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Wi-Fi connected! IP: ");
        Serial.println(WiFi.localIP());
        drawStatusRows("WIFI OK!", "LOADING WMATA", "NAVY YARD (N)");
    } else {
        Serial.println("Wi-Fi connection failed.");
        String errLabel = String("CHECK: ") + WIFI_SSID;
        drawStatusRows("WIFI ERROR", errLabel.c_str(), "RETRYING...");
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Starting LILYGO T-Display C5 — WMATA Navy Yard Northbound PIDS");

    // Enable display backlight
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Initialize Hardware SPI with T-Display C5 LCD pins
    SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);

    // Initialize 1.9-inch ST7789 display (170x320 resolution) in landscape mode (320x170)
    tft.init(LCD_WIDTH, LCD_HEIGHT);
    tft.setRotation(1);

    initWmataBoard();
    connectWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        if (fetchWmataPredictions()) {
            hasFetchedOnce = true;
            renderTrains();
        } else {
            drawStatusRows("WMATA API ERR", "RETRYING...", "NAVY YARD (N)");
        }
    }
    lastFetchTime = millis();
    lastBlinkTime = millis();
}

void loop() {
    unsigned long now = millis();

    // Blink ARR / BRD indicators every 600ms
    if (hasFetchedOnce && (now - lastBlinkTime >= BLINK_INTERVAL_MS)) {
        lastBlinkTime = now;
        blinkState = !blinkState;
        for (int i = 0; i < 3; i++) {
            if (northTrains[i].valid && (northTrains[i].min == "ARR" || northTrains[i].min == "BRD")) {
                renderTrainMin(i, blinkState);
            }
        }
    }

    // Refresh WMATA predictions every 20 seconds
    if (now - lastFetchTime >= FETCH_INTERVAL_MS) {
        lastFetchTime = now;

        if (WiFi.status() != WL_CONNECTED) {
            connectWiFi();
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (fetchWmataPredictions()) {
                hasFetchedOnce = true;
                renderTrains();
            }
        }
    }

    delay(50);
}
