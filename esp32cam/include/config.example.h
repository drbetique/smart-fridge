#pragma once

// =============================================================================
// config.h — ESP32-CAM Controller
// Copy this file to config.h and fill in your credentials before building.
// config.h is git-ignored and will never be committed.
// =============================================================================

#define WIFI_SSID         "YOUR_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"

#define THINGER_USER      "YOUR_THINGER_USERNAME"
#define THINGER_DEVICE    "smart_fridge_cam"
#define THINGER_TOKEN     "YOUR_THINGER_DEVICE_TOKEN"

#define GOOGLE_API_KEY    "YOUR_GOOGLE_CLOUD_API_KEY"

// Flash LED — built into AI Thinker ESP32-CAM board
#define FLASH_GPIO_NUM    4
#define FLASH_DELAY_MS    300   // ms to wait after flash before capture
