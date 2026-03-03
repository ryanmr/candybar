// =============================================================
// I2C Scanner — Run this FIRST to verify your pin mapping
//
// This will scan the I2C bus and print all detected device
// addresses. Use it to confirm:
//   - I2C SDA/SCL pins are correct
//   - Expected devices are responding at the right addresses
//
// Expected devices on ESP32-S3-Touch-LCD-3.49:
//   0x18  ES8311    (audio codec)
//   0x34  AXP2101   (power management)
//   0x3B  AXS15231B (touch controller)
//   0x51  PCF85063  (RTC)
//   0x6B  QMI8658   (IMU)
// =============================================================

#include <Wire.h>

// Touch controller I2C bus
#define TOUCH_SDA  17
#define TOUCH_SCL  18

// Peripheral I2C bus (IMU, RTC, audio codec)
#define PERIPH_SDA  47
#define PERIPH_SCL  48

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n================================");
  Serial.println("  I2C Scanner (dual bus)");
  Serial.printf("  Touch bus:  SDA=GPIO%d  SCL=GPIO%d\n", TOUCH_SDA, TOUCH_SCL);
  Serial.printf("  Periph bus: SDA=GPIO%d  SCL=GPIO%d\n", PERIPH_SDA, PERIPH_SCL);
  Serial.println("================================\n");

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire1.begin(PERIPH_SDA, PERIPH_SCL);
}

void scanBus(TwoWire &wire, const char* name) {
  int found = 0;

  Serial.printf("--- %s ---\n\n", name);
  Serial.println("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

  for (int row = 0; row < 8; row++) {
    Serial.printf("%02X: ", row * 16);
    for (int col = 0; col < 16; col++) {
      uint8_t addr = row * 16 + col;

      if (addr < 0x03 || addr > 0x77) {
        Serial.print("   ");
        continue;
      }

      wire.beginTransmission(addr);
      uint8_t err = wire.endTransmission();

      if (err == 0) {
        Serial.printf("%02X ", addr);
        found++;
      } else {
        Serial.print("-- ");
      }
    }
    Serial.println();
  }

  Serial.printf("\nFound %d device(s)\n\n", found);
}

void loop() {
  Serial.println("Scanning both I2C buses...\n");

  scanBus(Wire, "Touch Bus (GPIO 17/18)");
  scanBus(Wire1, "Peripheral Bus (GPIO 47/48)");

  Serial.println("--- Next scan in 10 seconds ---\n");
  delay(10000);
}
