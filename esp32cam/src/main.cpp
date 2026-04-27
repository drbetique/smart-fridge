/**
 * @file    main.cpp
 * @author  Victor Betiku (github.com/drbetique)
 * @brief   Smart Commercial Refrigerator — Camera Controller
 * @version 2.0
 * @date    2026-04-27
 *
 * @copyright Copyright (c) 2025 Victor Betiku
 *
 * Board   : AI Thinker ESP32-CAM
 * Camera  : OV2640 via ribbon cable
 * Cloud   : Google Cloud Vision API (OCR + barcode detection)
 *           Thinger.io (publishes result, receives capture trigger)
 *
 * FLASH: Hold IO0, press+release RST, release IO0, then Upload in VS Code.
 *        Press RST after flashing to run.
 */

#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ThingerESP32.h>
#include "mbedtls/base64.h"
#include "config.h"

// AI Thinker ESP32-CAM pin map — do not change
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

ThingerESP32 thing(THINGER_USER, THINGER_DEVICE, THINGER_TOKEN);

String  ocr_result        = "";
String  expiry_date       = "";
int     days_until_expiry = 999;
int     expiry_alert      = 0;
bool    capture_requested = false;
bool    camera_ok         = false;

// ---------------------------------------------------------------------------
// Date parsing
// ---------------------------------------------------------------------------

static bool isSep(char c) { return c == '/' || c == '.' || c == '-'; }

static int extractInt(const String& s, int pos, int maxLen) {
    String num = "";
    for (int i = pos; i < pos + maxLen && i < (int)s.length(); i++) {
        if (isDigit(s[i])) num += s[i]; else break;
    }
    return num.toInt();
}

static bool tryParseDate(const String& s, int pos, int& day, int& month, int& year) {
    int len = s.length();
    if (pos >= len) return false;

    // DD/MM/YYYY
    if (pos + 9 < len &&
        isDigit(s[pos]) && isDigit(s[pos+1]) && isSep(s[pos+2]) &&
        isDigit(s[pos+3]) && isDigit(s[pos+4]) && isSep(s[pos+5]) &&
        isDigit(s[pos+6]) && isDigit(s[pos+7]) && isDigit(s[pos+8]) && isDigit(s[pos+9])) {
        day = extractInt(s, pos, 2);
        month = extractInt(s, pos+3, 2);
        year = extractInt(s, pos+6, 4);
        if (year > 2020 && month >= 1 && month <= 12 && day >= 1 && day <= 31) return true;
    }

    // MM/YYYY
    if (pos + 6 < len &&
        isDigit(s[pos]) && isDigit(s[pos+1]) && isSep(s[pos+2]) &&
        isDigit(s[pos+3]) && isDigit(s[pos+4]) && isDigit(s[pos+5]) && isDigit(s[pos+6])) {
        int m = extractInt(s, pos, 2);
        int y = extractInt(s, pos+3, 4);
        if (y > 2020 && m >= 1 && m <= 12) {
            int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            if (y % 4 == 0) dim[1] = 29;
            day = dim[m-1]; month = m; year = y;
            return true;
        }
    }

    // MMYYYY (no separator, e.g. 042027)
    if (pos + 5 < len &&
        isDigit(s[pos]) && isDigit(s[pos+1]) &&
        isDigit(s[pos+2]) && isDigit(s[pos+3]) && isDigit(s[pos+4]) && isDigit(s[pos+5])) {
        int m = extractInt(s, pos, 2);
        int y = extractInt(s, pos+2, 4);
        if (y > 2020 && m >= 1 && m <= 12) {
            int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            if (y % 4 == 0) dim[1] = 29;
            day = dim[m-1]; month = m; year = y;
            return true;
        }
    }
    return false;
}

// Returns days until expiry (negative = expired, 9999 = no date found)
static int findExpiryDate(const String& text, String& found_date) {
    String keywords[] = {"exp","best","use by","bb","bbd","expiry","expires","best before"};
    String lower = text;
    lower.toLowerCase();

    for (auto& kw : keywords) {
        int idx = lower.indexOf(kw);
        while (idx != -1) {
            for (int offset = 1; offset < 30; offset++) {
                int d, m, y;
                if (tryParseDate(text, idx + offset, d, m, y)) {
                    found_date = String(d) + "/" + String(m) + "/" + String(y);
                    time_t now; time(&now);
                    struct tm et = {0};
                    et.tm_mday = d; et.tm_mon = m - 1; et.tm_year = y - 1900;
                    return (int)(difftime(mktime(&et), now) / 86400.0);
                }
            }
            idx = lower.indexOf(kw, idx + 1);
        }
    }

    // Fallback: scan entire text for any recognisable date
    for (int i = 0; i < (int)text.length() - 5; i++) {
        int d, m, y;
        if (tryParseDate(text, i, d, m, y)) {
            found_date = String(d) + "/" + String(m) + "/" + String(y);
            time_t now; time(&now);
            struct tm et = {0};
            et.tm_mday = d; et.tm_mon = m - 1; et.tm_year = y - 1900;
            return (int)(difftime(mktime(&et), now) / 86400.0);
        }
    }
    found_date = "";
    return 9999;
}

// ---------------------------------------------------------------------------
// Camera initialisation
// ---------------------------------------------------------------------------

static bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count     = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("[ERROR] Camera init failed — check ribbon cable");
        return false;
    }

    sensor_t* s = esp_camera_sensor_get();
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);

    Serial.println("[OK] Camera initialised");
    return true;
}

// ---------------------------------------------------------------------------
// Capture image and run OCR via Google Cloud Vision
// ---------------------------------------------------------------------------

static void captureAndOCR() {
    Serial.println("[CAM] Capturing...");

    // Warm-up frame — let auto-exposure settle before the real shot
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    delay(FLASH_DELAY_MS);
    camera_fb_t* warmup = esp_camera_fb_get();
    if (warmup) esp_camera_fb_return(warmup);
    delay(200);

    camera_fb_t* fb = esp_camera_fb_get();
    digitalWrite(FLASH_GPIO_NUM, LOW);

    if (!fb) { ocr_result = "Capture failed"; return; }
    Serial.printf("[CAM] Image: %u bytes\n", fb->len);

    size_t buf_size = ((fb->len + 2) / 3) * 4 + 1;
    size_t enc_len  = 0;
    unsigned char* encoded = psramFound()
        ? (unsigned char*) ps_malloc(buf_size)
        : (unsigned char*) malloc(buf_size);

    if (!encoded) {
        esp_camera_fb_return(fb);
        ocr_result = "Memory error";
        return;
    }

    mbedtls_base64_encode(encoded, buf_size, &enc_len, fb->buf, fb->len);
    encoded[enc_len] = '\0';
    esp_camera_fb_return(fb);

    Serial.printf("[CAM] Base64: %u bytes\n", enc_len);

    String url = "https://vision.googleapis.com/v1/images:annotate?key=";
    url += GOOGLE_API_KEY;

    String body;
    body.reserve(20000);
    body  = "{\"requests\":[{\"image\":{\"content\":\"";
    body += (char*) encoded;
    body += "\"},\"features\":[";
    body += "{\"type\":\"TEXT_DETECTION\",\"maxResults\":10},";
    body += "{\"type\":\"DOCUMENT_TEXT_DETECTION\",\"maxResults\":1}";
    body += "]}]}";
    free(encoded);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (!http.begin(client, url)) {
        Serial.println("[HTTP] Begin failed");
        ocr_result = "Connection error";
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);
    int code = http.POST(body);
    Serial.printf("[HTTP] Code: %d\n", code);

    if (code != 200) {
        http.end();
        ocr_result = "HTTP " + String(code);
        return;
    }

    // Stream response to avoid large heap allocation
    WiFiClient* stream = http.getStreamPtr();
    String buffer    = "";
    String extracted = "";
    bool   found     = false;
    int    bytesRead = 0;

    while (stream->available() || http.connected()) {
        while (stream->available()) {
            char c = stream->read();
            buffer += c;
            bytesRead++;
            if (buffer.length() > 500) buffer = buffer.substring(200);

            if (!found) {
                int idx = buffer.indexOf("\"fullTextAnnotation\"");
                if (idx != -1) {
                    int ti = buffer.indexOf("\"text\":\"", idx);
                    if (ti != -1) {
                        int pos = ti + 8;
                        String text = "";
                        while (pos < (int)buffer.length()) {
                            char ch = buffer[pos];
                            if (ch == '"' && (pos == 0 || buffer[pos-1] != '\\')) break;
                            text += ch;
                            pos++;
                        }
                        if (pos < (int)buffer.length()) {
                            extracted = text;
                            found = true;
                        }
                    }
                }
            }
            if (bytesRead >= 32000) break;
        }
        if (bytesRead >= 32000 || found) break;
        delay(1);
    }

    // Fallback: first textAnnotation description
    if (!found || extracted.isEmpty()) {
        int di = buffer.indexOf("\"description\":\"");
        if (di != -1) {
            di += 15;
            int ei = di;
            while (ei < (int)buffer.length()) {
                if (buffer[ei] == '"' && buffer[ei-1] != '\\') break;
                ei++;
            }
            extracted = buffer.substring(di, ei);
        }
    }

    http.end();

    extracted.replace("\\n", " ");
    extracted.replace("\\r", "");
    extracted.replace("\\t", " ");

    Serial.printf("[OCR] %d bytes read: %s\n", bytesRead, extracted.substring(0, 80).c_str());

    if (extracted.isEmpty()) {
        ocr_result   = "No text found";
        expiry_alert = 0;
        return;
    }

    ocr_result = extracted.substring(0, 100);
    days_until_expiry = findExpiryDate(extracted, expiry_date);
    Serial.printf("[DATE] %s  Days: %d\n", expiry_date.c_str(), days_until_expiry);

    if (days_until_expiry == 9999) {
        expiry_alert = 0;
    } else if (days_until_expiry < 0) {
        expiry_alert = 2;
        Serial.println("[ALERT] EXPIRED");
    } else if (days_until_expiry <= 7) {
        expiry_alert = 1;
        Serial.printf("[ALERT] Expires in %d days\n", days_until_expiry);
    } else {
        expiry_alert = 0;
        Serial.printf("[OK] Good for %d days\n", days_until_expiry);
    }
}

static void runOCRTask(void* param) {
    captureAndOCR();
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Smart Fridge Camera Controller ===");

    pinMode(FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_GPIO_NUM, LOW);

    camera_ok = initCamera();

    thing.add_wifi(WIFI_SSID, WIFI_PASSWORD);

    thing["ocr_result"]     >> outputValue(ocr_result);
    thing["expiry_date"]    >> outputValue(expiry_date);
    thing["days_remaining"] >> outputValue(days_until_expiry);
    thing["expiry_alert"]   >> outputValue(expiry_alert);

    thing["capture"] << [](pson& in) {
        if ((bool)in) {
            capture_requested = true;
            Serial.println("[TRIGGER] Capture requested from dashboard");
        }
    };

    Serial.println("[OK] Camera controller ready");
}

void loop() {
    thing.handle();

    if (capture_requested && camera_ok) {
        capture_requested = false;
        xTaskCreatePinnedToCore(runOCRTask, "OCR", 16384, NULL, 1, NULL, 0);
    }

    delay(100);
}
