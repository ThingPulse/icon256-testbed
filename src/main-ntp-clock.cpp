#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <FastLED.h>
#include "time.h"
#include "settings.h"

// --- LED Matrix Configuration ---
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define BRIGHTNESS 50
CRGB leds[NUM_LEDS];

// --- WiFi & Preferences ---
Preferences preferences;

// --- NTP Time Settings ---
const char* ntpServer = "pool.ntp.org";
const char* tz_Zurich = "CET-1CEST,M3.5.0,M10.5.0/3";

// --- Display State ---
enum DisplayMode { SHOW_TIME, SHOW_DATE };
DisplayMode currentMode = SHOW_TIME;
unsigned long lastModeChange = 0;
const long MODE_CHANGE_INTERVAL = 10000; // Switch between time and date every 10 seconds

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

// --- Function Prototypes ---
void get_wifi_credentials();
bool connect_to_wifi();
uint16_t XY(uint8_t x, uint8_t y);
void drawCharacter(int x, int y, int charIndex, CRGB color);
void drawTime(struct tm *timeinfo);
void drawDate(struct tm *timeinfo);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // --- Initialize LEDs ---
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

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
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        delay(1000);
        return;
    }

    FastLED.clear();

    // --- Switch between Time and Date display ---
    if (millis() - lastModeChange > MODE_CHANGE_INTERVAL) {
        lastModeChange = millis();
        currentMode = (currentMode == SHOW_TIME) ? SHOW_DATE : SHOW_TIME;
    }

    if (currentMode == SHOW_TIME) {
        drawTime(&timeinfo);
    } else {
        drawDate(&timeinfo);
    }

    FastLED.show();
    delay(200); // Update rate
}

// --- Helper Functions ---

uint16_t XY(uint8_t x, uint8_t y) {
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return -1;

    // Invert both X and Y coordinates to correct for a layout where
    // the first pixel is at the bottom-left.
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

    // Draw Dot in the middle of the bottom row
    drawCharacter(5, 8, 10, CRGB::Red);

    // Draw Month (bottom row)
    drawCharacter(2, 8, month / 10, CRGB::Purple);
    drawCharacter(8, 8, month % 10, CRGB::Purple);
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
