#include <Arduino.h>
#include <FastLED.h>
#include "settings.h"

#define NUM_LEDS 256
#define BRIGHTNESS 100

#define BUTTON_1_PIN 11
#define BUTTON_2_PIN 12
#define BUTTON_3_PIN 13

#define DEBOUNCE_DELAY 50

CRGB leds[NUM_LEDS];

int button1State = LOW;
int button2State = LOW;
int lastButton1Reading = LOW;
int lastButton2Reading = LOW;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;

uint8_t colorIndex = 0;
int activePixelIndex = 0;

void setAllLeds(const CRGB &color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void handleBtn1Click() {
  // Reset single-pixel mode index so the next BTN2 press starts at LED 0.
  activePixelIndex = 0;

  switch (colorIndex) {
    case 0:
      setAllLeds(CRGB::Red);
      Serial.println("BTN1: all red");
      break;
    case 1:
      setAllLeds(CRGB::Green);
      Serial.println("BTN1: all green");
      break;
    case 2:
      setAllLeds(CRGB::Blue);
      Serial.println("BTN1: all blue");
      break;
    default:
      setAllLeds(CRGB::White);
      Serial.println("BTN1: all white");
      break;
  }

  colorIndex = (colorIndex + 1) % 4;
}

void handleBtn2Click() {
  // BTN2 enforces single-pixel mode: clear strip, light one pixel.
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[activePixelIndex] = CRGB::White;
  FastLED.show();

  Serial.print("BTN2: active pixel ");
  Serial.println(activePixelIndex);

  activePixelIndex = (activePixelIndex + 1) % NUM_LEDS;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(BUTTON_1_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_2_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON_3_PIN, INPUT_PULLDOWN);

  FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // Startup state requested by user: all LEDs white.
  setAllLeds(CRGB::White);
  Serial.println("LED test initialized: boot white, BTN1 color cycle, BTN2 moving pixel.");
}

void loop() {
  const int reading1 = digitalRead(BUTTON_1_PIN);
  const int reading2 = digitalRead(BUTTON_2_PIN);

  if (reading1 != lastButton1Reading) {
    lastDebounceTime1 = millis();
    lastButton1Reading = reading1;
  }
  if ((millis() - lastDebounceTime1) > DEBOUNCE_DELAY) {
    if (reading1 != button1State) {
      button1State = reading1;
      if (button1State == HIGH) {
        handleBtn1Click();
      }
    }
  }

  if (reading2 != lastButton2Reading) {
    lastDebounceTime2 = millis();
    lastButton2Reading = reading2;
  }
  if ((millis() - lastDebounceTime2) > DEBOUNCE_DELAY) {
    if (reading2 != button2State) {
      button2State = reading2;
      if (button2State == HIGH) {
        handleBtn2Click();
      }
    }
  }

  delay(5);
}
