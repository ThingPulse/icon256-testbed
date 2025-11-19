#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "Audio.h"
#include "settings.h"
#include "time.h"

// --- Audio & Preferences Objects ---
Audio audio;
Preferences preferences;

// --- NTP Time Settings ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;      // Replace with your GMT offset in seconds
const int   daylightOffset_sec = 3600; // Replace with your daylight saving offset
const char * timezone = "CET-1CEST,M3.5.0,M10.5.0/3";


// --- Timing for periodic announcement ---
unsigned long previousMillis = 0;
const long interval = 10000; // 1 minute in milliseconds


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

    // --- Configure Time ---
    Serial.println("Synchronizing time with NTP server...");
    configTzTime(timezone, ntpServer);
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
        Serial.println("Time synchronized.");
    } else {
        Serial.println("Time synchronization failed.");
    }

    // --- Configure Audio ---
    audio.setPinout(PIN_I2S_SPK_BCLK, PIN_I2S_SPK_LRCLK, PIN_I2S_SPK_DATA);
    audio.setVolume(21); // Set a comfortable volume
}

void loop() {
    unsigned long currentMillis = millis();

    // --- Announce the time every minute ---
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            Serial.println("Failed to obtain time");
            audio.connecttospeech("I could not get the time.", "en-US");
        } else {
            char time_str_hour[3];
            char time_str_minute[3];
            strftime(time_str_hour, sizeof(time_str_hour), "%H", &timeinfo);
            strftime(time_str_minute, sizeof(time_str_minute), "%M", &timeinfo);

            String time_announcement = "The current time is ";
            time_announcement += time_str_hour;
            time_announcement += " ";
            time_announcement += time_str_minute;

            Serial.printf("Announcing: %s\n", time_announcement.c_str());
            audio.connecttospeech(time_announcement.c_str(), "en-US");
        }
    }

    // The audio library must be called continuously to process the stream
    audio.loop();
}

// Optional: You can add audio events to get more info from the library
void audio_info(const char *info){ Serial.print("info        "); Serial.println(info); }
void audio_eof_mp3(const char *info){ Serial.print("eof_mp3     "); Serial.println(info); }
