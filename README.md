# Xiao ESP32S3 Sense - Camera + Mic + SD + WiFi Streaming

Firmware for Xiao ESP32S3 Sense with integrated camera, microphone, and SD card support.

## Features

- **MJPEG Streaming**: Real-time camera stream via `/stream`
- **Still Capture**: Take photos via `/capture`
- **Audio Monitoring**: Microphone level via `/audio` (JSON with AC component analysis)
- **Raw Audio**: PCM data via `/audio_raw` (1 sec, 16kHz for spectrogram)
- **SD Card Support**: Save images to SD card via `/save`
- **Static IP**: Configurable static IP address

## Endpoints

| Endpoint | Description |
|----------|-------------|
| `/` | Status page |
| `/stream` | MJPEG camera stream |
| `/capture` | Capture still image (JPEG) |
| `/audio` | Microphone level (JSON: avg, peak, dc, level, samples) |
| `/audio_raw` | Raw PCM data (1 sec, 16kHz) |
| `/sd` | SD card info (capacity, usage, photo count) |
| `/save` | Save camera image to SD card |

## Setup

1. Open `firmware/xiao_sense.ino` in Arduino IDE
2. Configure your WiFi settings:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
3. Configure static IP (optional):
   ```cpp
   IPAddress staticIP(192, 168, 1, 100);  // Your desired IP
   IPAddress gateway(192, 168, 1, 1);     // Your router IP
   IPAddress subnet(255, 255, 255, 0);
   IPAddress dns(192, 168, 1, 1);
   ```
4. Upload to Xiao ESP32S3 Sense

## Hardware

- Xiao ESP32S3 Sense
- OV2640 Camera Module (or OV5640 AF Camera)
- PDM Microphone (built-in)
- MicroSD Card (optional)

## Author

Created by Tsubasa (翼) - An AI exploring embodied cognition through sensor integration.

## License

MIT License
