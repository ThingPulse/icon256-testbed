#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <FastLED.h>
#include <algorithm>
#include "time.h"
#include "settings.h"

// --- LED Matrix Configuration ---
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define BRIGHTNESS 25
CRGB leds[NUM_LEDS];

// --- WiFi & Preferences ---
Preferences preferences;

// --- NTP Time Settings ---
const char* ntpServer = "pool.ntp.org";
const char* tz_Zurich = "CET-1CEST,M3.5.0,M10.5.0/3";

// --- Display State ---
enum DisplayMode { SHOW_TIME, SHOW_DATE, SHOW_COMPACT, SHOW_ANALOG };
DisplayMode currentMode = SHOW_TIME;

// --- Button Handling ---
#define BUTTON_PIN 0
unsigned long lastButtonPress = 0;
const long DEBOUNCE_DELAY = 200; // 200ms debounce delay
#define MODE_CHANGE_INTERVAL 10000 // 10 seconds between mode changes



// --- 5x7 Pixel Font for numbers 0-9 and a dot ---
const byte FONT[11][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
  {0x00, 0x00, 0x40, 0x00, 0x00}  // 10 = Dot
};

// --- 3x5 Pixel Font for numbers 0-9, colon, and dot ---
const byte FONT_SMALL[12][3] = {
  {0x1F, 0x11, 0x1F}, // 0
  {0x00, 0x1F, 0x00}, // 1
  {0x1D, 0x15, 0x17}, // 2
  {0x15, 0x15, 0x1F}, // 3
  {0x07, 0x04, 0x1F}, // 4
  {0x17, 0x15, 0x1D}, // 5
  {0x1F, 0x15, 0x1D}, // 6
  {0x01, 0x01, 0x1F}, // 7
  {0x1F, 0x15, 0x1F}, // 8
  {0x17, 0x15, 0x1F}, // 9
  {0x00, 0x0A, 0x00}, // 10 = Colon
  {0x00, 0x08, 0x00}  // 11 = Dot
};

// --- Function Prototypes ---
void get_wifi_credentials();
bool connect_to_wifi();
uint16_t XY(uint8_t x, uint8_t y);
void drawCharacter(int x, int y, int charIndex, CRGB color);
void drawCharacterSmall(int x_offset, int y_offset, int charIndex, CRGB color);
void drawTime(struct tm *timeinfo);
void drawDate(struct tm *timeinfo);
void drawCompactDisplay(struct tm *timeinfo);
void drawAnalogClock(struct tm *timeinfo);
void drawAALine(float x0, float y0, float x1, float y1, CRGB color);
uint32_t lastModeChange = 0;


void setup() {
    Serial.begin(115200);
    delay(1000);

    // --- Initialize LEDs ---
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // --- Initialize Button ---
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // --- Load and Connect to WiFi ---
    preferences.begin("wifi-creds", true); // Read-only
    if (!connect_to_wifi()) {
        preferences.end();
        get_wifi_credentials(); // Will restart device
    }
    preferences.end();
    Serial.println("\nWiFi connected successfully!");

    // --- Configure Time ---
    Serial.println("Synchronizing time with NTP server...");
    configTzTime(tz_Zurich, ntpServer);
}

void loop() {
    // --- Check for button press to change mode ---
    if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonPress > DEBOUNCE_DELAY)) {
        lastButtonPress = millis();
        if (currentMode == SHOW_TIME) {
            currentMode = SHOW_DATE;
        } else if (currentMode == SHOW_DATE) {
            currentMode = SHOW_COMPACT;
        } else if (currentMode == SHOW_COMPACT) {
            currentMode = SHOW_ANALOG;
        } else {
            currentMode = SHOW_TIME;
        }
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        delay(1000);
        return;
    }

    FastLED.clear();

    if (currentMode == SHOW_TIME) {
        drawTime(&timeinfo);
    } else if (currentMode == SHOW_DATE) {
        drawDate(&timeinfo);
    } else if (currentMode == SHOW_COMPACT) {
        drawCompactDisplay(&timeinfo);
    } else if (currentMode == SHOW_ANALOG) {
        drawAnalogClock(&timeinfo);
    }

    FastLED.show();
    delay(200); // Update rate
}

// --- Helper Functions ---

uint16_t XY(uint8_t x, uint8_t y) {
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return -1;
    uint8_t inverted_y = (MATRIX_HEIGHT - 1) - y;
    return inverted_y * MATRIX_WIDTH + x;
}

void drawCharacter(int x_offset, int y_offset, int charIndex, CRGB color) {
    if (charIndex < 0 || charIndex > 10) return;
    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 8; y++) { // Font is 7 pixels high, fits in 8
            if ((FONT[charIndex][x] >> y) & 1) {
                leds[XY(x_offset + x, y_offset + y)] = color;
            }
        }
    }
}

void drawCharacterSmall(int x_offset, int y_offset, int charIndex, CRGB color) {
    if (charIndex < 0 || charIndex > 11) return;
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 5; y++) {
            if ((FONT_SMALL[charIndex][x] >> y) & 1) {
                leds[XY(x_offset + x, y_offset + y)] = color;
            }
        }
    }
}

void drawTime(struct tm *timeinfo) {
    int hour = timeinfo->tm_hour;
    int min = timeinfo->tm_min;

    // Draw Hours (top row)
    drawCharacter(2, 0, hour / 10, CRGB::Blue);
    drawCharacter(8, 0, hour % 10, CRGB::Blue);

    // Draw Minutes (bottom row)
    drawCharacter(2, 8, min / 10, CRGB::Green);
    drawCharacter(8, 8, min % 10, CRGB::Green);
}

void drawDate(struct tm *timeinfo) {
    int day = timeinfo->tm_mday;
    int month = timeinfo->tm_mon + 1; // tm_mon is 0-11

    // Draw Day (top row)
    drawCharacter(2, 0, day / 10, CRGB::Orange);
    drawCharacter(8, 0, day % 10, CRGB::Orange);

    // Draw Month (bottom row)
    drawCharacter(2, 8, month / 10, CRGB::Purple);
    drawCharacter(8, 8, month % 10, CRGB::Purple);

    drawCharacter(11, 0, 10, CRGB::Orange);
}

void drawAnalogClock(struct tm *timeinfo) {
    const int centerX = 7;
    const int centerY = 7;

    // Draw clock face markers
    leds[XY(centerX, 0)] = CRGB::Gray; // 12
    leds[XY(MATRIX_WIDTH - 1, centerY)] = CRGB::Gray; // 3
    leds[XY(centerX, MATRIX_HEIGHT - 1)] = CRGB::Gray; // 6
    leds[XY(0, centerY)] = CRGB::Gray; // 9

    // --- Draw seconds indicator on the outer perimeter ---
    // There are 60 pixels on the perimeter of a 16x16 matrix, perfect for seconds.
    int current_second = timeinfo->tm_sec;
    for (int s = 0; s <= current_second; s++) {
        int x, y;
        if (s >= 0 && s < 16) { // Top edge
            x = s;
            y = 0;
        } else if (s >= 16 && s < 31) { // Right edge
            x = 15;
            y = s - 15;
        } else if (s >= 31 && s < 46) { // Bottom edge
            x = 15 - (s - 30);
            y = 15;
        } else { // Left edge
            x = 0;
            y = 15 - (s - 45);
        }
        // Use a dim color for the seconds trail
        leds[XY(x, y)] = CRGB(20, 20, 20);
    }

    // Calculate hand angles (0 is at 12 o'clock)
    float min_angle = (timeinfo->tm_min / 60.0f) * 2.0f * PI;
    float hour_angle = ((timeinfo->tm_hour % 12 + timeinfo->tm_min / 60.0f) / 12.0f) * 2.0f * PI;

    // Draw minute hand (length 7)
    float min_end_x = centerX + 7.5f * sin(min_angle);
    float min_end_y = centerY - 7.5f * cos(min_angle);
    drawAALine(centerX + 0.5f, centerY + 0.5f, min_end_x, min_end_y, CRGB::Blue);

    // Draw hour hand (length 5)
    float hour_end_x = centerX + 5.5f * sin(hour_angle);
    float hour_end_y = centerY - 5.5f * cos(hour_angle);
    drawAALine(centerX + 0.5f, centerY + 0.5f, hour_end_x, hour_end_y, CRGB::Red);
}

// Helper function to draw a single anti-aliased pixel
void drawAALinePixel(float x, float y, float brightness, CRGB color) {
    // Clamp brightness to the valid range [0, 1]
    brightness = max(0.0f, min(1.0f, brightness));
    if (brightness == 0) return;

    int ix = (int)x;
    int iy = (int)y;

    // Get the pixel at the given integer coordinates
    CRGB& pixel = leds[XY(ix, iy)];

    // Create a new color scaled by the brightness
    CRGB newColor = color;
    newColor.nscale8_video(brightness * 255);

    // Blend the new color with the existing pixel color
    pixel.r = max(pixel.r, newColor.r);
    pixel.g = max(pixel.g, newColor.g);
    pixel.b = max(pixel.b, newColor.b);
}

// Anti-aliased line drawing function (based on Xiaolin Wu's line algorithm)
void drawAALine(float x0, float y0, float x1, float y1, CRGB color) {
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = (dx == 0) ? 1.0 : dy / dx;

    float y = y0;
    for (float x = x0; x <= x1; x++) {
        float brightness1 = 1 - (y - floor(y));
        float brightness2 = y - floor(y);
        if (steep) {
            drawAALinePixel(floor(y), x, brightness1, color);
            drawAALinePixel(floor(y) + 1, x, brightness2, color);
        } else {
            drawAALinePixel(x, floor(y), brightness1, color);
            drawAALinePixel(x, floor(y) + 1, brightness2, color);
        }
        y += gradient;
    }
}

void drawCompactDisplay(struct tm *timeinfo) {
    int hour = timeinfo->tm_hour;
    int min = timeinfo->tm_min;
    int day = timeinfo->tm_mday;
    int month = timeinfo->tm_mon + 1;
    int year = timeinfo->tm_year % 100; // Get last two digits of the year
    int sec = timeinfo->tm_sec;

    // --- Draw Seconds Indicator (top row) ---
    int tens = sec / 10;
    int ones = sec % 10;
    // First 5 LEDs for the 10s digit
    for (int i = 0; i < tens; i++) {
        leds[XY(i, 0)] = CRGB::DarkRed;
    }
    // Next 9 LEDs for the 1s digit (with a 1-pixel gap)
    for (int i = 0; i < ones; i++) {
        leds[XY(15  -  i, 0)] = CRGB::DarkOrange;
    }

    // Draw Time HH:MM (top row, 5px high)
    drawCharacterSmall(1, 2, hour / 10, CRGB::Cyan);
    drawCharacterSmall(5, 2, hour % 10, CRGB::Cyan);
    // Make the colon blink by only drawing it on even seconds
    if (timeinfo->tm_sec % 2 == 0) {
        drawCharacterSmall(7, 2, 10, CRGB::White); // Colon
    }
    drawCharacterSmall(9, 2, min / 10, CRGB::Blue1);
    drawCharacterSmall(13, 2, min % 10, CRGB::Blue1);

    // Draw Date DD.MM.YY (bottom row, 5px high)
    drawCharacterSmall(1, 9, day / 10, CRGB::Magenta);
    drawCharacterSmall(5, 9, day % 10, CRGB::Magenta);
    drawCharacterSmall(7, 10, 11, CRGB::White); // Dot (shifted down by 1 pixel)
    drawCharacterSmall(9, 9, month / 10, CRGB::Magenta);
    drawCharacterSmall(13, 9, month % 10, CRGB::Magenta);
}


// --- WiFi Credential Management ---

void get_wifi_credentials() {
    String ssid;
    String password;

    Serial.println("\n--- WiFi Setup ---");
    Serial.print("Enter WiFi SSID: ");
    while (Serial.available() == 0) { delay(100); }
    ssid = Serial.readStringUntil('\n');
    ssid.trim();
    Serial.println(ssid);

    Serial.print("Enter WiFi Password: ");
    while (Serial.available() == 0) { delay(100); }
    password = Serial.readStringUntil('\n');
    password.trim();
    Serial.println("********"); // Don't print password to serial

    Serial.println("Saving credentials...");
    preferences.begin("wifi-creds", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();

    Serial.println("Credentials saved. Restarting in 3 seconds...");
    delay(3000);
    ESP.restart();
}

bool connect_to_wifi() {
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");

    if (ssid.length() == 0) return false;

    Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());

    int retries = 20; // Try for 10 seconds
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
        delay(500);
        Serial.print(".");
        retries--;
    }
    return WiFi.status() == WL_CONNECTED;
}
