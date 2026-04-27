// =============================================================================
// Smart Fridge — Load Cell Calibration
// Board : Dasduino ESP32-WROVER-E
// Run this LAST — after DHT22, TFT, door switch, and ESP32-CAM are confirmed
// =============================================================================
#include <Arduino.h>
#include <HX711.h>

// NOTE: GPIO 16/17 reserved on WROVER-E. Use 25/26.
#define HX711_DOUT  25
#define HX711_CLK   26

// Change this to match your known weight in kg
// Examples: 0.5 = 500g, 1.0 = 1kg, 0.2 = 200g
const float KNOWN_WEIGHT_KG = 0.5f;

HX711 scale;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("==============================================");
  Serial.println("  Smart Fridge — Load Cell Calibration");
  Serial.println("==============================================");
  Serial.println();
  Serial.printf("  Known weight set to: %.0fg\n", KNOWN_WEIGHT_KG * 1000);
  Serial.println();

  scale.begin(HX711_DOUT, HX711_CLK);

  if (!scale.is_ready()) {
    Serial.println("[ERROR] HX711 not detected!");
    Serial.println("  Check wiring:");
    Serial.println("  HX711 DT  -> GPIO 25");
    Serial.println("  HX711 SCK -> GPIO 26");
    Serial.println("  HX711 VCC -> 5V");
    Serial.println("  HX711 GND -> GND");
    while (true) delay(1000);
  }

  Serial.println("Step 1: Scale is empty. Taring in 3 seconds...");
  delay(3000);
  scale.tare();
  Serial.println("  Tare done. Scale reads zero.");
  Serial.println();
  Serial.printf("Step 2: Place your %.0fg weight on the scale now.\n",
                KNOWN_WEIGHT_KG * 1000);
  Serial.println("  Waiting 5 seconds...");
  delay(5000);
}

void loop() {
  if (!scale.is_ready()) {
    Serial.println("[WARN] HX711 not ready");
    delay(2000);
    return;
  }

  long raw = scale.read_average(20);
  float calib = (float)raw / KNOWN_WEIGHT_KG;

  Serial.println("----------------------------------------------");
  Serial.printf("  Raw reading       : %ld\n", raw);
  Serial.printf("  Known weight      : %.3f kg\n", KNOWN_WEIGHT_KG);
  Serial.printf("  Calibration factor: %.1f\n", calib);
  Serial.println();
  Serial.println("  Copy this line into main_esp32/include/config.h:");
  Serial.printf("  #define LOAD_CELL_CALIB   %.1ff\n", calib);
  Serial.println("----------------------------------------------");
  Serial.println("  Next reading in 5 seconds...");
  Serial.println();

  delay(5000);
}
