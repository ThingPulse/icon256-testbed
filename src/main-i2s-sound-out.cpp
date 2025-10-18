#include <Arduino.h>
#include "driver/i2s.h"
#include "settings.h"
#include <math.h>

// I2S settings
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16

// Tone generation settings
#define TONE_FREQUENCY 440.0f // A4 note
#define AMPLITUDE 5000       // Amplitude of the sine wave (max for 16-bit is 32767)
#define DURATION_MS 500      // Duration to play the tone
#define SILENCE_MS 500       // Duration of silence between tones

void setup_i2s() {
    // --- Enable Amplifier ---
    // The MAX98357A has a shutdown/enable pin. Let's drive it HIGH to enable it.
    pinMode(PIN_I2S_SPK_CTRL, OUTPUT);
    digitalWrite(PIN_I2S_SPK_CTRL, HIGH);
    Serial.println("I2S Speaker Control pin enabled.");

    // --- I2S Configuration ---
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // or I2S_CHANNEL_FMT_ALL_RIGHT
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false, // Use internal APLL for better clock accuracy
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    // --- I2S Pin Configuration ---
    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_SPK_BCLK,
        .ws_io_num = PIN_I2S_SPK_LRCLK,
        .data_out_num = PIN_I2S_SPK_DATA,
        .data_in_num = I2S_PIN_NO_CHANGE // Not used for speaker output
    };

    // --- Install and start I2S driver ---
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %d\n", err);
        return;
    }

    Serial.println("I2S driver installed and configured successfully.");
}

void play_tone() {
    int num_samples = (int)(SAMPLE_RATE * (DURATION_MS / 1000.0));
    int16_t* samples = (int16_t*)malloc(num_samples * 2 * sizeof(int16_t)); // 2 channels

    if (!samples) {
        Serial.println("Failed to allocate memory for samples!");
        return;
    }

    Serial.printf("Generating %d samples for a %.0f Hz tone...\n", num_samples, TONE_FREQUENCY);

    for (int i = 0; i < num_samples; ++i) {
        int16_t sample_val = (int16_t)(AMPLITUDE * sin(2 * PI * TONE_FREQUENCY * i / SAMPLE_RATE));
        samples[i * 2] = sample_val;     // Left channel
        samples[i * 2 + 1] = sample_val; // Right channel (mono sound)
    }

    size_t bytes_written;
    esp_err_t err = i2s_write(I2S_PORT, samples, num_samples * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);

    if (err != ESP_OK) {
        Serial.printf("I2S write failed: %d\n", err);
    } else if (bytes_written < num_samples * 2 * sizeof(int16_t)) {
        Serial.printf("Warning: Only wrote %d out of %d bytes\n", bytes_written, num_samples * 2 * sizeof(int16_t));
    }

    free(samples);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  setup_i2s();
}

void loop() {
  play_tone();
  delay(SILENCE_MS);
}