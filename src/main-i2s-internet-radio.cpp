#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "Audio.h"
#include "settings.h"

// --- Internet Radio Stream ---
// You can use any other MP3/AAC stream URL
const char* stream_url = "http://peacefulpiano.stream.publicradio.org/peacefulpiano.mp3";

// --- Audio Objects ---
Audio audio;
Preferences preferences;

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

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1000);

    // --- Enable Amplifier ---
    pinMode(PIN_I2S_SPK_CTRL, OUTPUT);
    digitalWrite(PIN_I2S_SPK_CTRL, HIGH);
    Serial.println("I2S Speaker Control pin enabled.");

    // --- Load and Connect to WiFi ---
    preferences.begin("wifi-creds", true); // Read-only
    if (!connect_to_wifi()) {
        preferences.end();
        // If connection fails or no credentials, start setup prompt
        get_wifi_credentials();
        // The device will restart, so this part of setup won't be reached
    }
    preferences.end();
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // --- Configure Audio ---
    // The Audio library uses the ESP-IDF I2S driver underneath.
    // We just need to tell it which pins to use.
    audio.setPinout(PIN_I2S_SPK_BCLK, PIN_I2S_SPK_LRCLK, PIN_I2S_SPK_DATA);

    // Set a default volume (0-21)
    audio.setVolume(15);

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