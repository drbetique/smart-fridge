# Smart Commercial Refrigerator — Complete Project Package
Häme University of Applied Sciences | Spring 2026

**Author:** Victor Betiku — [@drbetique](https://github.com/drbetique)

## Contents

### Firmware (open each folder in VS Code separately)
- main_esp32/     Dasduino ESP32-WROVER-E — sensors, display, cloud
- esp32cam/       AI Thinker ESP32-CAM — camera, OCR, Google Vision
- calibration/    Load cell calibration sketch — run last

### Configuration
- TFT_eSPI_User_Setup.h   Copy this into .platformio/lib/TFT_eSPI/User_Setup.h

### Documents (in docs/ folder)
- 0_Build_Guide.docx                  Complete hardware wiring guide
- 1_Thinger_IO_Setup.docx             Step-by-step Thinger.io dashboard setup
- 2_Google_Vision_Setup.docx          Step-by-step Google Cloud Vision API setup
- 3_Testing_Guide.docx                Component-by-component test checklist

## Order of operations
1. Fill in config.h files (Wi-Fi, Thinger.io tokens, Google API key)
2. Copy TFT_eSPI_User_Setup.h to PlatformIO library folder
3. Flash and test main_esp32 (DHT22, TFT, door switch)
4. Set up Thinger.io — see docs/1_Thinger_IO_Setup.docx
5. Set up Google Vision API — see docs/2_Google_Vision_Setup.docx
6. Flash and test esp32cam
7. Run through docs/3_Testing_Guide.docx
8. Flash and run calibration/ last
