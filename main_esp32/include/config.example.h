#pragma once

// =============================================================================
// config.h — Smart Fridge Main Controller
// Copy this file to config.h and fill in your credentials before building.
// config.h is git-ignored and will never be committed.
// =============================================================================

// ── Wi-Fi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID         "YOUR_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"

// ── Thinger.io ────────────────────────────────────────────────────────────────
#define THINGER_USER      "YOUR_THINGER_USERNAME"
#define THINGER_DEVICE    "smart_fridge_main"
#define THINGER_TOKEN     "YOUR_THINGER_DEVICE_TOKEN"
#define ENDPOINT_TOKEN    "YOUR_THINGER_ENDPOINT_TOKEN"

// ── Pin definitions ───────────────────────────────────────────────────────────
// IMPORTANT: GPIO 16 and 17 are reserved by PSRAM on WROVER-E. Do not use.
// IMPORTANT: GPIO 12 must be LOW at boot. Do not pull HIGH.
#define DHT_PIN           4       // DHT22 DATA
#define DHT_TYPE          DHT22
#define HX711_DOUT        25      // HX711 DT  — NOT GPIO 16
#define HX711_CLK         26      // HX711 SCK — NOT GPIO 17
#define DOOR_PIN          5       // Tactile switch: one leg here, other to GND
#define LED_PIN           32      // Optional: onboard LED for status indication

// ── Thresholds and timing ─────────────────────────────────────────────────────
#define LOAD_CELL_CALIB   472680.0f  // Replace with your value from calibration.ino
#define DOOR_ALERT_MS     30000      // Alert if door stays open > 30 seconds
#define TEMP_MAX_C        35.0f      // Temperature alert threshold in Celsius
#define SENSOR_INTERVAL   2000       // Read sensors every 2000 ms
#define DEBOUNCE_MS       10         // Switch debounce delay in ms
