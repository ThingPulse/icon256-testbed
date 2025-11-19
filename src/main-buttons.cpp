#include <Arduino.h>
#include <FastLED.h>
#include "settings.h"

// LED matrix properties
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

// Brightness setting (0-255)
#define BRIGHTNESS 100

// Button pins
#define BUTTON_1_PIN 11
#define BUTTON_2_PIN 12
#define BUTTON_3_PIN 13

// Debounce delay in milliseconds
#define DEBOUNCE_DELAY 50

// Define the array of leds
CRGB leds[NUM_LEDS];

// Variables for button state tracking
int button1State = HIGH;
int button2State = HIGH;
int button3State = HIGH;
int lastButton1State = HIGH;
int lastButton2State = HIGH;
int lastButton3State = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTime3 = 0;

// Function to map matrix coordinates to the linear LED index
uint16_t XY(uint8_t x, uint8_t y) {
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
        return -1;
    }
    // Row-major, progressive layout (every row starts on the left)
    return y * MATRIX_WIDTH + x;
}

// 5x7 font for digits 1, 2, 3 (flipped vertically only)
const uint8_t digit1[7][5] = {
    {0, 1, 1, 1, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 0, 1, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 0, 1, 0, 0}
};

const uint8_t digit2[7][5] = {
    {1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0},
    {0, 1, 0, 0, 0},
    {0, 1, 1, 0, 0},
    {0, 0, 0, 0, 1},
    {1, 0, 0, 0, 1},
    {0, 1, 1, 1, 0}
};

const uint8_t digit3[7][5] = {
    {1, 1, 1, 1, 0},
    {0, 0, 0, 0, 1},
    {0, 0, 0, 0, 1},
    {0, 1, 1, 1, 0},
    {0, 0, 0, 0, 1},
    {0, 0, 0, 0, 1},
    {1, 1, 1, 1, 0}
};

// Function to draw a digit on the LED matrix
void drawDigit(const uint8_t digit[7][5], CRGB color) {
    // Center the 5x7 digit on the 16x16 matrix
    int offsetX = (MATRIX_WIDTH - 5) / 2;
    int offsetY = (MATRIX_HEIGHT - 7) / 2;
    
    FastLED.clear();
    
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 5; x++) {
            if (digit[y][x] == 1) {
                int ledIndex = XY(x + offsetX, y + offsetY);
                if (ledIndex != -1) {
                    leds[ledIndex] = color;
                }
            }
        }
    }
    
    FastLED.show();
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    
    // Initialize button pins with internal pull-down resistors
    // Note: Buttons are connected to 3.3V via pull-up, so they will read HIGH when not pressed
    // and LOW when pressed (button connects pin to ground)
    pinMode(BUTTON_1_PIN, INPUT_PULLDOWN);
    pinMode(BUTTON_2_PIN, INPUT_PULLDOWN);
    pinMode(BUTTON_3_PIN, INPUT_PULLDOWN);
    
    // Initialize FastLED
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    
    // Clear all LEDs at startup
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    
    Serial.println("Button test initialized. Press buttons 1, 2, or 3.");
}

void loop() {
    // Read button states
    int reading1 = digitalRead(BUTTON_1_PIN);
    int reading2 = digitalRead(BUTTON_2_PIN);
    int reading3 = digitalRead(BUTTON_3_PIN);
    
    // Debounce button 1
    if (reading1 != lastButton1State) {
        lastDebounceTime1 = millis();
    }
    if ((millis() - lastDebounceTime1) > DEBOUNCE_DELAY) {
        if (reading1 != button1State) {
            button1State = reading1;
            if (button1State == HIGH) {
                Serial.println("Button 1 pressed");
                drawDigit(digit1, CRGB::Red);
            }
        }
    }
    lastButton1State = reading1;
    
    // Debounce button 2
    if (reading2 != lastButton2State) {
        lastDebounceTime2 = millis();
    }
    if ((millis() - lastDebounceTime2) > DEBOUNCE_DELAY) {
        if (reading2 != button2State) {
            button2State = reading2;
            if (button2State == HIGH) {
                Serial.println("Button 2 pressed");
                drawDigit(digit2, CRGB::Green);
            }
        }
    }
    lastButton2State = reading2;
    
    // Debounce button 3
    if (reading3 != lastButton3State) {
        lastDebounceTime3 = millis();
    }
    if ((millis() - lastDebounceTime3) > DEBOUNCE_DELAY) {
        if (reading3 != button3State) {
            button3State = reading3;
            if (button3State == HIGH) {
                Serial.println("Button 3 pressed");
                drawDigit(digit3, CRGB::Blue);
            }
        }
    }
    lastButton3State = reading3;
    
    delay(10); // Small delay to prevent excessive CPU usage
}
