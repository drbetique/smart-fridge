// =============================================================================
// Smart Commercial Refrigerator — Main Controller
// Board   : Dasduino ESP32-WROVER-E
// IDE     : VS Code + PlatformIO
// =============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ThingerESP32.h>
#include <DHT.h>
#include <HX711.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

DHT          dht(DHT_PIN, DHT_TYPE);
HX711        scale;
#define TFT_BL    3
Adafruit_ST7789 tft = Adafruit_ST7789(14, 2, 23, 18, 15);
ThingerESP32 thing(THINGER_USER, THINGER_DEVICE, THINGER_TOKEN);

float         temperature    = 0.0f;
float         humidity       = 0.0f;
float         weight_kg      = 0.0f;
int          door_open      = 0;
int          door_alert     = 0;
int          temp_alert     = 0;
unsigned long door_open_ts   = 0;
unsigned long last_sensor_ts = 0;
String        ocr_result        = "";
int           display_mode      = 0;
unsigned long scan_display_ts   = 0;
unsigned long alert_display_ts  = 0;
String        expiry_date       = "";
int           days_until_expiry = 999;
float         last_temperature  = -999.0f;
float         last_humidity     = -999.0f;
float         last_weight       = -999.0f;
int           last_door_open    = -1;
int           last_door_alert   = -1;
int           last_temp_alert   = -1;

void readDHT22();
void readLoadCell();
void readDoorSwitch();
void updateDisplay();

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Fridge Main Controller ===");

  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dht.begin();
  Serial.println("[OK] DHT22 initialised");

  scale.begin(HX711_DOUT, HX711_CLK);
  scale.tare();
  scale.set_scale(LOAD_CELL_CALIB);
  Serial.printf("[HX711] Calibration factor: %.1f\n", (float)LOAD_CELL_CALIB);
  Serial.println("[OK] HX711 initialised and tared");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init(240, 240);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Connecting...");
  Serial.println("[OK] TFT initialised");

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1,1,1,1));

  thing.add_wifi(WIFI_SSID, WIFI_PASSWORD);

  // Wait for connection with timeout
  Serial.print("[WIFI] Connecting");
  unsigned long start_time = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start_time < 30000) {  // 30 second timeout
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] Connected!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("[WIFI] Failed to connect. Status: ");
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL (SSID not found)"); break;
      case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED (Wrong password)"); break;
      case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
      case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
      default: Serial.println(WiFi.status()); break;
    }
    Serial.println("[WIFI] Check SSID, password, and network compatibility");
  }

  Serial.println("[THINGER] Connecting to Thinger.io...");
  // Thinger connection happens in thing.handle()

  thing["temperature"] >> [](pson& out){ out = temperature; };
  thing["humidity"]    >> [](pson& out){ out = humidity; };
  thing["weight_kg"]   >> [](pson& out){ out = weight_kg; };
  thing["door_open"]   >> [](pson& out){ out = door_open; };
  thing["door_alert"]  >> [](pson& out){ out = door_alert; };
  thing["temp_alert"]  >> [](pson& out){ out = temp_alert; };

  thing["ocr_result"] << [](pson& in) {
    ocr_result = (const char*) in;
    scan_display_ts = millis();
    Serial.print("[CAM] OCR received: ");
    Serial.println(ocr_result);
  };


  thing["tare_scale"] = []() {
    scale.tare();
    Serial.println("[ACTION] Scale tared remotely");
    return true;
  };

  thing["show_scan"] << [](pson& in) {
    if (ocr_result.isEmpty()) {
      ocr_result        = "BEST BY 042027";
      expiry_date       = "30/4/2027";
      days_until_expiry = 365;
    }
    scan_display_ts = millis();
    Serial.println("[ACTION] Scan screen triggered");
  };

  // ── Full system diagnostic ──────────────────────────────────────────
Serial.println("\n========================================");
Serial.println("       SYSTEM DIAGNOSTIC REPORT");
Serial.println("========================================");

// Wi-Fi
Serial.print("[WIFI]    Status : ");
Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "NOT CONNECTED");
if (WiFi.status() == WL_CONNECTED) {
  Serial.print("[WIFI]    IP     : ");
  Serial.println(WiFi.localIP());
}

// Thinger.io
Serial.print("[THINGER] Status : ");
Serial.println(thing.is_connected() ? "CONNECTED" : "NOT CONNECTED");

// DHT22
float test_t = dht.readTemperature();
float test_h = dht.readHumidity();
Serial.print("[DHT22]   Status : ");
if (!isnan(test_t) && !isnan(test_h)) {
  Serial.println("OK");
  Serial.print("[DHT22]   Temp   : "); Serial.print(test_t); Serial.println(" C");
  Serial.print("[DHT22]   Humid  : "); Serial.print(test_h); Serial.println(" %");
} else {
  Serial.println("FAILED — check wiring and 10k pull-up on GPIO 4");
}

// HX711
Serial.print("[HX711]   Status : ");
Serial.println(scale.is_ready() ? "OK" : "FAILED — check wiring on GPIO 25/26");

// Door switch
Serial.print("[SWITCH]  Status : ");
Serial.print("GPIO 5 reads ");
Serial.println(digitalRead(DOOR_PIN) == HIGH ? "HIGH (door open / switch released)" : "LOW (door closed / switch pressed)");

// TFT
Serial.println("[TFT]     Status : Check screen physically — if blank see notes below");
Serial.println("[TFT]     Note   : If screen is black, User_Setup.h may not be updated");
Serial.println("[TFT]     Note   : If screen shows colour but no text, rotation may be wrong");

Serial.println("========================================\n");
// ── End diagnostic ──────────────────────────────────────────────────

  Serial.println("[OK] Setup complete");
}

void loop() {
  thing.handle();

  // Poll OCR result from camera device every 5 seconds
  static unsigned long last_ocr_poll = 0;
  if (millis() - last_ocr_poll > 5000) {
    last_ocr_poll = millis();
    if (thing.is_connected()) {
      WiFiClientSecure secClient;
      secClient.setInsecure();
      HTTPClient http;
      String url = "https://api.thinger.io/v1/users/";
      url += THINGER_USER;
      url += "/devices/smart_fridge_cam/resources/ocr_result";
      http.begin(secClient, url);
      http.addHeader("Authorization", "Bearer " + String(ENDPOINT_TOKEN));
      int code = http.GET();
      if (code == 200) {
        String payload = http.getString();
        // Extract value from JSON
        int start = payload.indexOf("\"out\":\"");
        if (start != -1) {
          start += 7;
          int end = payload.indexOf("\"", start);
          if (end != -1) {
            String new_result = payload.substring(start, end);
        if (new_result != ocr_result && new_result.length() > 0) {
          ocr_result = new_result;
          scan_display_ts = millis();
          Serial.print("[OCR] Updated: ");
          Serial.println(ocr_result);

          // Also fetch expiry_date
          WiFiClientSecure secClient2;
          secClient2.setInsecure();
          HTTPClient http2;
          String url2 = "https://api.thinger.io/v1/users/";
          url2 += THINGER_USER;
          url2 += "/devices/smart_fridge_cam/resources/expiry_date";
          http2.begin(secClient2, url2);
          http2.addHeader("Authorization", "Bearer " + String(ENDPOINT_TOKEN));
          if (http2.GET() == 200) {
            String p2 = http2.getString();
            int s2 = p2.indexOf("\"out\":\"");
            if (s2 != -1) {
              s2 += 7;
              int e2 = p2.indexOf("\"", s2);
              if (e2 != -1) expiry_date = p2.substring(s2, e2);
            }
          }
          http2.end();

          // Also fetch days_remaining
          WiFiClientSecure secClient3;
          secClient3.setInsecure();
          HTTPClient http3;
          String url3 = "https://api.thinger.io/v1/users/";
          url3 += THINGER_USER;
          url3 += "/devices/smart_fridge_cam/resources/days_remaining";
          http3.begin(secClient3, url3);
          http3.addHeader("Authorization", "Bearer " + String(ENDPOINT_TOKEN));
          if (http3.GET() == 200) {
            String p3 = http3.getString();
            int s3 = p3.indexOf("\"out\":");
            if (s3 != -1) {
              s3 += 6;
              int e3 = p3.indexOf("}", s3);
              if (e3 != -1) days_until_expiry = p3.substring(s3, e3).toInt();
            }
          }
          http3.end();

          Serial.printf("[EXPIRY] Date: %s Days: %d\n",
                        expiry_date.c_str(), days_until_expiry);
        }
          }
        }
      }
      http.end();
    }
  }

  static unsigned long last_stream_ts = 0;
  if (millis() - last_stream_ts > 2000) {
    last_stream_ts = millis();

    if (thing.is_connected()) {
      thing.stream("temperature");
      thing.stream("humidity");
      thing.stream("weight_kg");
      thing.stream("door_open");
      thing.stream("door_alert");
      thing.stream("temp_alert");
    }
  }

  // Debug: Print connection status every 10 seconds
  static unsigned long last_status_ts = 0;
  if (millis() - last_status_ts > 10000) {
    last_status_ts = millis();
    Serial.println("---- STATUS ----");
    Serial.print("[WIFI]    "); Serial.println(WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED");
    Serial.print("[THINGER] "); Serial.println(thing.is_connected() ? "OK" : "DISCONNECTED");
    Serial.print("[TEMP]    "); Serial.print(temperature); Serial.println(" C");
    Serial.print("[HUMID]   "); Serial.print(humidity); Serial.println(" %");
    Serial.print("[WEIGHT]  "); Serial.print(weight_kg); Serial.println(" kg");
    Serial.print("[DOOR]    "); Serial.println(door_open ? "OPEN (1)" : "CLOSED (0)");
    Serial.print("[ALERT]   "); Serial.println(door_alert ? "ACTIVE (1)" : "OK (0)");
    Serial.println("----------------");
  }

  unsigned long now = millis();
  if (now - last_sensor_ts >= SENSOR_INTERVAL) {
    last_sensor_ts = now;
    readDHT22();
    readLoadCell();
  }

  readDoorSwitch();
  updateDisplay();
}

void readDHT22() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) { temperature = t; temp_alert = (temperature > TEMP_MAX_C) ? 1 : 0; }
  else Serial.println("[WARN] DHT22 read failed");
  if (!isnan(h)) humidity = h;
}

void readLoadCell() {
  if (scale.is_ready()) {
    scale.set_scale(LOAD_CELL_CALIB);
    float w = scale.get_units(10);
    weight_kg = (w < 0.0f) ? 0.0f : w;
    Serial.printf("[WEIGHT RAW] %.4f kg\n", w);
  } else {
    Serial.println("[WARN] HX711 not ready");
  }
}

void readDoorSwitch() {
  static int last_state = 0;
  int raw = (digitalRead(DOOR_PIN) == HIGH) ? 1 : 0;
  if (raw != last_state) { delay(DEBOUNCE_MS); raw = (digitalRead(DOOR_PIN) == HIGH) ? 1 : 0; }
  last_state = raw;

  if (raw && !door_open) {
    door_open = 1; door_open_ts = millis(); door_alert = 0;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[DOOR] Opened");
  } else if (!raw && door_open) {
    door_open = 0; door_alert = 0;
    digitalWrite(LED_PIN, LOW);
    Serial.println("[DOOR] Closed");
  }
  if (door_open && !door_alert && (millis() - door_open_ts) > DOOR_ALERT_MS) {
    door_alert = 1;
    Serial.println("[ALERT] Door open too long!");
    // Call Thinger.io endpoint directly from firmware
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "https://api.thinger.io/v1/users/";
      url += THINGER_USER;
      url += "/endpoints/door_alert_email/call";
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", "Bearer " + String(ENDPOINT_TOKEN));
      int code = http.POST("{}");
      Serial.printf("[EMAIL] Door alert sent. Response: %d\n", code);
      http.end();
    }
  }
  }


void drawNormalScreen() {
  uint16_t TEAL    = tft.color565(0, 128, 128);
  uint16_t LGREY   = tft.color565(240, 240, 240);
  uint16_t DGREY   = tft.color565(80, 80, 80);
  uint16_t ORANGE  = tft.color565(255, 140, 0);

  // Header bar
  tft.fillRect(0, 0, 240, 36, TEAL);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(28, 10);
  tft.println("SMART FRIDGE");

  // Content area
  tft.fillRect(0, 36, 240, 204, LGREY);

  // Temperature row
  tft.fillRect(0, 36, 240, 50, LGREY);
  tft.fillCircle(20, 61, 10, temp_alert ? ST77XX_RED : TEAL);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(38, 52);
  tft.print("Temp: ");
  tft.setTextColor(temp_alert ? ST77XX_RED : ST77XX_BLACK);
  tft.print(temperature, 1);
  tft.println(" C");

  // Divider
  tft.drawFastHLine(10, 86, 220, DGREY);

  // Humidity row
  tft.fillCircle(20, 101, 10, TEAL);
  tft.setTextColor(ST77XX_BLACK);
  tft.setCursor(38, 92);
  tft.print("Humid:");
  tft.print(humidity, 1);
  tft.println("%");

  // Divider
  tft.drawFastHLine(10, 121, 220, DGREY);

  // Weight row
  tft.fillCircle(20, 141, 10, TEAL);
  tft.setCursor(38, 132);
  tft.print("Weight:");
  tft.print(weight_kg, 2);
  tft.println("kg");

  // Divider
  tft.drawFastHLine(10, 161, 220, DGREY);

  // Door row
  uint16_t door_color = door_open ? ST77XX_RED : tft.color565(0, 180, 0);
  tft.fillCircle(20, 181, 10, door_color);
  tft.setTextColor(ST77XX_BLACK);
  tft.setCursor(38, 172);
  tft.print("Door: ");
  tft.setTextColor(door_open ? ST77XX_RED : tft.color565(0, 140, 0));
  tft.println(door_open ? "OPEN" : "CLOSED");

  // Footer
  tft.fillRect(0, 210, 240, 30, TEAL);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 220);
  tft.print("WiFi:OK  Thinger:OK");
}

void drawAlertScreen() {
  uint16_t DARK_RED = tft.color565(180, 0, 0);

  tft.fillScreen(ST77XX_RED);
  tft.fillRect(0, 0, 240, 40, DARK_RED);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(60, 12);
  tft.println("!! ALERT !!");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);

  if (door_alert) {
    tft.setCursor(20, 80);
    tft.println("DOOR");
    tft.setCursor(20, 115);
    tft.println("OPEN");
    tft.setTextSize(1);
    tft.setCursor(20, 155);
    tft.println("Door open over 30 seconds");
    tft.setCursor(20, 170);
    tft.println("Please close immediately");
  }

  if (temp_alert) {
    tft.setCursor(20, 80);
    tft.println("TEMP");
    tft.setCursor(20, 115);
    tft.println("HIGH");
    tft.setTextSize(1);
    tft.setCursor(20, 155);
    tft.print("Reading: ");
    tft.print(temperature, 1);
    tft.println(" C");
    tft.setCursor(20, 170);
    tft.println("Check refrigerator now");
  }

  // Pulse border
  tft.drawRect(2, 2, 236, 236, ST77XX_WHITE);
  tft.drawRect(5, 5, 230, 230, ST77XX_WHITE);
}

void drawScanScreen() {
  uint16_t TEAL   = tft.color565(0, 128, 128);
  uint16_t LGREY  = tft.color565(240, 240, 240);
  uint16_t GOLD   = tft.color565(255, 200, 0);

  // Header
  tft.fillRect(0, 0, 240, 36, TEAL);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 10);
  tft.println("SCAN RESULT");

  // Content
  tft.fillRect(0, 36, 240, 204, LGREY);

  // OCR text
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(8, 44);
  tft.println("Detected text:");
  tft.setTextColor(tft.color565(0, 0, 180));
  tft.setCursor(8, 56);
  tft.println(ocr_result.substring(0, 35));
  tft.println(ocr_result.substring(35, 70));

  // Divider
  tft.drawFastHLine(10, 86, 220, tft.color565(180, 180, 180));

  // Expiry info
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(8, 92);
  tft.print("Expiry date: ");
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(0, 100, 0));
  tft.setCursor(8, 104);
  if (expiry_date.length() > 0) {
    tft.println(expiry_date);
  } else {
    tft.println("Not found");
  }

  // Days remaining
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);
  tft.setCursor(8, 128);
  tft.print("Days remaining: ");
  tft.setTextSize(2);
  if (days_until_expiry < 0) {
    tft.setTextColor(ST77XX_RED);
    tft.println("EXPIRED");
  } else if (days_until_expiry <= 7) {
    tft.setTextColor(ST77XX_RED);
    tft.println(days_until_expiry);
  } else if (days_until_expiry == 999) {
    tft.setTextColor(tft.color565(100, 100, 100));
    tft.println("Unknown");
  } else {
    tft.setTextColor(tft.color565(0, 140, 0));
    tft.println(days_until_expiry);
  }

  // Discount badge if expiring soon or expired
  if (days_until_expiry <= 7 && days_until_expiry != 999) {
    tft.fillRoundRect(20, 158, 200, 44, 8, GOLD);
    tft.drawRoundRect(20, 158, 200, 44, 8, ST77XX_WHITE);
    tft.setTextColor(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(35, 163);
    tft.println("20% DISCOUNT");
    tft.setTextSize(1);
    tft.setCursor(50, 185);
    tft.println("Product near expiry");
  } else if (days_until_expiry > 7) {
    tft.fillRoundRect(20, 158, 200, 44, 8, tft.color565(200, 255, 200));
    tft.setTextColor(tft.color565(0, 120, 0));
    tft.setTextSize(2);
    tft.setCursor(55, 168);
    tft.println("PRODUCT OK");
  }

  // Footer
  tft.fillRect(0, 210, 240, 30, TEAL);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(50, 220);
  tft.println("Tap to dismiss");
}

void updateDisplay() {
  static int last_mode = -1;

  // Decide which screen to show
  if (door_alert || temp_alert) {
    display_mode = 1;
  } else if (ocr_result.length() > 0 &&
             (millis() - scan_display_ts < 15000)) {
    display_mode = 2;
  } else {
    display_mode = 0;
  }

  if (display_mode == 0) {
    // Only redraw normal screen if a value changed
    bool changed = (last_mode != 0)
      || (temperature   != last_temperature)
      || (humidity      != last_humidity)
      || (weight_kg     != last_weight)
      || (door_open     != last_door_open)
      || (door_alert    != last_door_alert)
      || (temp_alert    != last_temp_alert);

    if (changed) {
      drawNormalScreen();
      last_temperature = temperature;
      last_humidity    = humidity;
      last_weight      = weight_kg;
      last_door_open   = door_open;
      last_door_alert  = door_alert;
      last_temp_alert  = temp_alert;
    }

  } else if (display_mode == 1 && last_mode != 1) {
    drawAlertScreen();

  } else if (display_mode == 2 && last_mode != 2) {
    drawScanScreen();
  }

  last_mode = display_mode;
}
