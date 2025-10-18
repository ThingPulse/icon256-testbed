#include <Arduino.h>
#include <WiFi.h>
#include "Audio.h"
#include "settings.h"

// --- WiFi Settings ---
// Replace with your actual WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// --- Internet Radio Stream ---
// You can use any other MP3/AAC stream URL
const char* stream_url = "http://peacefulpiano.stream.publicradio.org/peacefulpiano.mp3";

// --- Audio Objects ---
Audio audio;

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1000);

    // --- Enable Amplifier ---
    pinMode(PIN_I2S_SPK_CTRL, OUTPUT);
    digitalWrite(PIN_I2S_SPK_CTRL, HIGH);
    Serial.println("I2S Speaker Control pin enabled.");

    // --- Connect to WiFi ---
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // --- Configure Audio ---
    // The Audio library uses the ESP-IDF I2S driver underneath.
    // We just need to tell it which pins to use.
    audio.setPinout(PIN_I2S_SPK_BCLK, PIN_I2S_SPK_LRCLK, PIN_I2S_SPK_DATA);

    // Set a default volume (0-21)
    audio.setVolume(21);

    // --- Connect to the audio stream ---
    Serial.printf("Connecting to audio stream: %s\n", stream_url);
    if (!audio.connecttohost(stream_url)) {
        Serial.println("Failed to connect to host!");
        return;
    }

    Serial.println("Connected to host. Audio should start playing...");
}

void loop() {
    // The audio library handles the streaming and decoding in the background.
    // We just need to call its loop function.
    audio.loop();
}

// Optional: You can add audio events to get more info from the library
void audio_info(const char *info){ Serial.print("info        "); Serial.println(info); }
void audio_eof_mp3(const char *info){ Serial.print("eof_mp3     "); Serial.println(info); }