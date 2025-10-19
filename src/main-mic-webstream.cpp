#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "driver/i2s.h"
#include "settings.h"

// --- WiFi & Preferences ---
Preferences preferences;
WiFiServer server(80);

// --- I2S Microphone Configuration ---
#define I2S_MIC_PORT I2S_NUM_1
#define I2S_MIC_SAMPLE_RATE 16000
#define I2S_MIC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define I2S_MIC_CHANNEL_FORMAT I2S_CHANNEL_FMT_ONLY_LEFT

// When reading 32-bit samples, the audio data is left-aligned.
// We need to shift it to the right to get the actual 24-bit value.
#define MIC_SAMPLE_SHIFT 8

// We will stream as 16-bit audio
#define BITS_PER_SAMPLE_STREAM 16

// Digital gain to apply to the microphone signal
#define MIC_GAIN 16

// Buffer for reading and processing samples
#define I2S_BUFFER_SIZE 2048
int32_t i2s_read_buffer[I2S_BUFFER_SIZE];
int16_t stream_buffer[I2S_BUFFER_SIZE];

// --- Function Prototypes ---
void get_wifi_credentials();
bool connect_to_wifi();
void setup_i2s_mic();
void stream_audio(WiFiClient client);
void write_wav_header(WiFiClient client);


void setup() {
    Serial.begin(115200);
    delay(1000);

    // --- Load and Connect to WiFi ---
    preferences.begin("wifi-creds", true); // Read-only
    if (!connect_to_wifi()) {
        preferences.end();
        get_wifi_credentials(); // Will restart device
    }
    preferences.end();
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: http://");
    Serial.println(WiFi.localIP());
    Serial.println("Open the IP address in your browser to start streaming.");

    // --- Initialize Microphone ---
    setup_i2s_mic();

    // --- Start Web Server ---
    server.begin();
    Serial.println("Web server started.");
}

void loop() {
    WiFiClient client = server.available();
    if (client) {
        Serial.println("Client connected.");
        stream_audio(client);
        Serial.println("Client disconnected.");
    }
}

// --- Core Functions ---

void stream_audio(WiFiClient client) {
    // Read the HTTP request
    String req = client.readStringUntil('\r');
    client.flush();

    // Check if it's a valid GET request
    if (req.indexOf("GET /") == -1) {
        client.stop();
        return;
    }

    // Send HTTP headers
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: audio/wav");
    client.println("Connection: close");
    client.println();

    // Send WAV header
    write_wav_header(client);

    // Stream audio data
    while (client.connected()) {
        size_t bytes_read = 0;
        i2s_read(I2S_MIC_PORT, &i2s_read_buffer, I2S_BUFFER_SIZE * sizeof(int32_t), &bytes_read, portMAX_DELAY);

        if (bytes_read > 0) {
            int samples_read = bytes_read / sizeof(int32_t);
            // Convert 32-bit samples to 16-bit samples for the stream
            for (int i = 0; i < samples_read; i++) {
                // 1. Correctly read the 24-bit sample from the 32-bit I2S buffer.
                int32_t sample24 = i2s_read_buffer[i] >> MIC_SAMPLE_SHIFT;

                // 2. Apply digital gain.
                int32_t amplified_sample = sample24 * MIC_GAIN;

                // 3. Clip the sample to the 16-bit range to prevent overflow.
                amplified_sample = max(-32768, min(32767, amplified_sample));

                stream_buffer[i] = (int16_t)amplified_sample;
            }
            client.write((const uint8_t*)stream_buffer, samples_read * sizeof(int16_t));
        }
    }
    client.stop();
}

void write_wav_header(WiFiClient client) {
    uint32_t sampleRate = I2S_MIC_SAMPLE_RATE;
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = BITS_PER_SAMPLE_STREAM;

    // A large, but not infinite, size for the WAV file fields.
    // This helps trick browsers like Safari into playing the live stream.
    uint32_t fake_file_size = 2000000000; // ~2GB, a few hours of audio

    // WAV header structure
    char header[44];
    strcpy(header, "RIFF");
    *(uint32_t*)(header + 4) = fake_file_size; // ChunkSize
    strcpy(header + 8, "WAVE");
    strcpy(header + 12, "fmt ");
    *(uint32_t*)(header + 16) = 16; // Subchunk1Size (16 for PCM)
    *(uint16_t*)(header + 20) = 1;  // AudioFormat (1 for PCM)
    *(uint16_t*)(header + 22) = numChannels;
    *(uint32_t*)(header + 24) = sampleRate;
    *(uint32_t*)(header + 28) = sampleRate * numChannels * (bitsPerSample / 8); // ByteRate
    *(uint16_t*)(header + 32) = numChannels * (bitsPerSample / 8); // BlockAlign
    *(uint16_t*)(header + 34) = bitsPerSample;
    strcpy(header + 36, "data");
    *(uint32_t*)(header + 40) = fake_file_size - 36; // Subchunk2Size

    client.write(header, 44);
}

void setup_i2s_mic() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_MIC_BITS_PER_SAMPLE,
        .channel_format = I2S_MIC_CHANNEL_FORMAT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_MIC_BCLK,
        .ws_io_num = PIN_I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_MIC_DATA
    };
    i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_MIC_PORT, &pin_config);
}

// --- WiFi Credential Management (copied from other scripts) ---
void get_wifi_credentials() {
    String ssid, password;
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
    Serial.println("********");
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
    int retries = 20;
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
        delay(500);
        Serial.print(".");
        retries--;
    }
    return WiFi.status() == WL_CONNECTED;
}