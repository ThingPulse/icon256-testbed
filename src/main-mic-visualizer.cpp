#include <Arduino.h>
#include <FastLED.h>
#include "driver/i2s.h"
#include "arduinoFFT.h"
#include "settings.h"

// --- LED Matrix Configuration ---
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define BRIGHTNESS 60
CRGB leds[NUM_LEDS];

// --- I2S Microphone Configuration ---
#define I2S_MIC_PORT I2S_NUM_1
#define I2S_MIC_SAMPLE_RATE 16000
#define I2S_MIC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define I2S_MIC_CHANNEL_FORMAT I2S_CHANNEL_FMT_ONLY_LEFT
#define MIC_SAMPLE_SHIFT 8 // The ICS-43434 is 24-bit, but we read 32-bit samples. Shift to get the real value.

// --- FFT Configuration ---
#define SAMPLES 1024 // Must be a power of 2



double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, I2S_MIC_SAMPLE_RATE);

// --- Visualization Configuration ---
#define NOISE_THRESHOLD 5000 // Threshold for ignoring background noise

// Function to map matrix coordinates to the linear LED index
// (This depends on how your matrix is wired)
uint16_t XY(uint8_t x, uint8_t y) {
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
        return -1;
    }

    // Row-major, progressive layout (every row starts on the left).
    // This is the most common layout for LED matrices.
    return y * MATRIX_WIDTH + x;
}

// Floating-point map function to preserve precision
double fmap(double x, double in_min, double in_max, double out_min, double out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

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
        .dma_buf_len = SAMPLES,
        .use_apll = false,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_MIC_BCLK,
        .ws_io_num = PIN_I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_MIC_DATA
    };

    i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_MIC_PORT, &pin_config);
    Serial.println("I2S microphone driver installed.");
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // --- Initialize LEDs ---
    FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // --- Initialize Microphone ---
    setup_i2s_mic();
}

void loop() {
    // --- 1. Read Audio Samples ---
    size_t bytes_read = 0;
    int32_t i2s_read_buffer[SAMPLES];
    esp_err_t result = i2s_read(I2S_MIC_PORT, &i2s_read_buffer, SAMPLES * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    if (result != ESP_OK || bytes_read == 0) {
        Serial.println("I2S read failed!");
        return;
    }

    // --- 1a. Remove DC Offset ---
    // Calculate the average (DC offset) of the samples
    long total = 0;
    for (int i = 0; i < SAMPLES; i++) {
        total += (i2s_read_buffer[i] >> MIC_SAMPLE_SHIFT);
    }
    long average = total / SAMPLES;

    // Subtract the DC offset from each sample and fill the FFT input arrays
    int samples_read = bytes_read / sizeof(int32_t);
    for (int i = 0; i < samples_read; i++) {
        // Subtract the average (DC offset) from the sample
        vReal[i] = (double)((i2s_read_buffer[i] >> MIC_SAMPLE_SHIFT) - average);
        vImag[i] = 0;
    }

    // --- 2. Perform FFT ---
    FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, SAMPLES);

    // --- 3. Process FFT Results into 16 Bands ---
    double band_magnitudes[MATRIX_WIDTH] = {0};
    
    // The FFT gives us SAMPLES/2 bins, from 0 Hz to SAMPLE_RATE/2 Hz.
    // We will group these bins into 16 bands for our visualizer.
    // We use a logarithmic-like scale for the bands.
    int band_limits[] = {2, 3, 4, 5, 7, 9, 12, 15, 20, 26, 34, 44, 57, 74, 95, 123, SAMPLES/2};

    for (int i = 2; i < (SAMPLES / 2); i++) { // Start at 2 to ignore DC component
        if (vReal[i] > NOISE_THRESHOLD) {
            for (int band = 0; band < MATRIX_WIDTH; band++) {
                if (i >= band_limits[band] && i < band_limits[band+1]) {
                    band_magnitudes[band] += vReal[i];
                }
            }
        }
    }

    // Average the magnitudes within each band
    for (int band = 0; band < MATRIX_WIDTH; band++) {
        int count = band_limits[band+1] - band_limits[band];
        if (count > 0) {
            band_magnitudes[band] /= count;
        }
    }

    // --- 4. Draw the Equalizer on the LED Matrix ---
    FastLED.clear();

    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        // Scale the magnitude to the matrix height.
        // Using log10 helps to make quiet sounds more visible.
        // The map function provides better control over the sensitivity.
        // You can adjust the input range (e.g., 3.0 to 6.0) to tune the visualizer.
        int height = 0;
        if (band_magnitudes[x] > 0) {
            // Map the logarithmic magnitude to the matrix height
            height = fmap(log10(band_magnitudes[x]), 3.0, 6.0, 0, MATRIX_HEIGHT);
        }

        // Clamp the height to the matrix dimensions
        height = constrain(height, 0, MATRIX_HEIGHT);

        // Draw the bar for this band
        for (uint8_t y = 0; y < height; y++) {
            // Create a color gradient from green to red
            CRGB color = CHSV(map(y, 0, MATRIX_HEIGHT - 1, 85, 0), 255, 255);
            leds[XY(x, y)] = color;
        }
    }

    FastLED.show();
}
