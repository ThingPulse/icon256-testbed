
#include <Arduino.h>
#include <FastLED.h>
#include "settings.h"

// LED matrix properties
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

// Brightness setting (0-255)
#define BRIGHTNESS 180

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  // Initialize FastLED
  // WS2812B LEDs typically use the GRB color order.
  FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  //FastLED.setMaxRefreshRate(10);
  //FastLED.setDither(DISABLE_DITHER);
  //FastLED.setCorrection(UncorrectedColor);

  // Clear all LEDs at startup
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void loop() {
  // --- Test 1: Cycle through solid colors ---
  Serial.println("Testing with solid colors...");
  // Red
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  delay(1000);

  // Green
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(5000);

  // Blue
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
  delay(1000);

  // --- Test 2: Moving dot to check every pixel ---
  Serial.println("Testing with a moving dot...");
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::White;
    FastLED.show();
    delay(50);
    leds[i] = CRGB::Black;
  }
  FastLED.show(); // Ensure the last pixel is turned off
  delay(1000);
}
