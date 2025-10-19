# icon256-testbed

This is a PlatformIO project to test the functionality of a custom 16x16 RGB LED matrix device. The device features:
*   A 16x16 matrix of WS2812B RGB LEDs (256 total).
*   A MAX98357A I2S audio amplifier and loudspeaker.
*   An ICS-43434 I2S MEMS microphone.
*   An ESP32-S3 microcontroller.

## Test Environments

This project contains several test environments, each in a separate `src/main-<env-name>.cpp` file. You can build and upload a specific test using the PlatformIO CLI.

For example, to run the `rgb-led` test:
```sh
pio run -e rgb-led --target upload
```

---

### `rgb-led`
*   **File**: `src/main-rgb-led.cpp`
*   **Description**: A basic test for the 16x16 RGB LED matrix. It cycles the entire matrix through solid red, green, and blue, and then runs a single white pixel across all 256 LEDs to verify functionality and addressing.

### `i2s-sound-out`
*   **File**: `src/main-i2s-sound-out.cpp`
*   **Description**: Tests the MAX98357A I2S amplifier and speaker by generating and playing a continuous 440Hz sine wave (A4 note).

### `i2s-mic-test`
*   **File**: `src/main-i2s-mic-test.cpp`
*   **Description**: Verifies the ICS-43434 I2S microphone. It reads audio samples and prints the calculated RMS (Root Mean Square) volume level to the serial monitor.

### `mic-visualizer`
*   **File**: `src/main-mic-visualizer.cpp`
*   **Description**: A 16-band audio spectrum visualizer. It uses the I2S microphone to capture audio, performs an FFT to analyze frequencies, and displays the result as animated bars on the 16x16 LED matrix.

### `mic-webstream`
*   **File**: `src/main-mic-webstream.cpp`
*   **Description**: Streams live audio from the I2S microphone over a local web server. After connecting to WiFi, you can open the device's IP address in a web browser to listen to the microphone in real-time. Includes a serial prompt to configure WiFi credentials.

### `i2s-internet-radio`
*   **File**: `src/main-i2s-internet-radio.cpp`
*   **Description**: Connects to WiFi and streams an internet radio station, playing the audio through the I2S speaker. It features a command-line prompt via the serial monitor to enter and save WiFi credentials to persistent storage.

### `tts-time`
*   **File**: `src/main-tts-time.cpp`
*   **Description**: A talking clock that uses Google's Text-to-Speech service. It connects to WiFi, synchronizes time via NTP, and announces the current time through the speaker at the top of every minute. It also uses the command-line prompt for WiFi setup.

### `ntp-clock`
*   **File**: `src/main-ntp-clock.cpp`
*   **Description**: A graphical clock for the 16x16 LED matrix. It gets the time via NTP and features multiple display modes (large digital, compact digital, and analog) that can be cycled by pressing the BOOT button (GPIO 0).

## WiFi Credential Setup

Several of the network-enabled examples (`mic-webstream`, `i2s-internet-radio`, `tts-time`, `ntp-clock`) use a common WiFi credential manager.

1.  The first time you run one of these examples, open the Serial Monitor.
2.  You will be prompted to enter your WiFi SSID and password.
3.  The credentials will be saved to the device's non-volatile storage.
4.  The device will restart and automatically connect on subsequent boots.

If the device cannot connect to the saved network, it will automatically re-enter the setup prompt.