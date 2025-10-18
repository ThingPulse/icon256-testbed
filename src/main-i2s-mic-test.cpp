#include <Arduino.h>
#include "driver/i2s.h"
#include "settings.h"
#include <math.h>

// --- I2S Configuration ---
// Use I2S_NUM_1 to avoid conflicts if you were to use speaker and mic at the same time
#define I2S_MIC_PORT I2S_NUM_1 

// The ICS-43434 outputs 24-bit data, but we read it as 32-bit samples from the I2S peripheral.
// The data is in the most significant 24 bits.
#define I2S_MIC_SAMPLE_RATE 16000
#define I2S_MIC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define I2S_MIC_CHANNEL_FORMAT I2S_CHANNEL_FMT_ONLY_LEFT

// --- Audio Processing ---
// When reading 32-bit samples, the audio data is left-aligned.
// We need to shift it to the right to get the actual 24-bit value.
#define MIC_SAMPLE_SHIFT 8

// Buffer for reading samples from I2S
#define I2S_BUFFER_SIZE 1024
int32_t i2s_read_buffer[I2S_BUFFER_SIZE];


void setup_i2s_mic() {
    Serial.println("Configuring I2S for microphone...");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_MIC_BITS_PER_SAMPLE,
        .channel_format = I2S_MIC_CHANNEL_FORMAT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_MIC_BCLK,
        .ws_io_num = PIN_I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_MIC_DATA
    };

    esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %d\n", err);
        return;
    }

    Serial.println("I2S microphone driver installed.");
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1000);
    setup_i2s_mic();
}

void loop() {
    size_t bytes_read = 0;
    esp_err_t result = i2s_read(I2S_MIC_PORT, &i2s_read_buffer, I2S_BUFFER_SIZE * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    if (result == ESP_OK && bytes_read > 0) {
        int samples_read = bytes_read / sizeof(int32_t);
        
        // Calculate RMS (Root Mean Square) to get an idea of the volume
        double sum_sq = 0;
        for (int i = 0; i < samples_read; i++) {
            // Shift the 32-bit sample to get the 24-bit value, then treat as signed
            int32_t sample = i2s_read_buffer[i] >> MIC_SAMPLE_SHIFT;
            sum_sq += (sample * sample);
        }
        
        double rms = sqrt(sum_sq / samples_read);
        Serial.printf("RMS: %.0f\n", rms);
    } else {
        Serial.printf("I2S read failed: %d\n", result);
    }
    delay(10); // Small delay to avoid spamming the serial port
}