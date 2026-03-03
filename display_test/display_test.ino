// Canvas full-refresh test — QSPI display requires full-frame writes.
// Draw to an in-memory Canvas, then flush the entire frame at once.
// Portrait mode (172x640) — native panel orientation.

#include <Arduino.h>

#define ESP32QSPI_SPI_HOST SPI3_HOST
#include <Arduino_GFX_Library.h>

#define LCD_CS   9
#define LCD_CLK  10
#define LCD_D0   11
#define LCD_D1   12
#define LCD_D2   13
#define LCD_D3   14
#define LCD_RST  21
#define LCD_BL   8

static const uint8_t minimal_init[] = {
  BEGIN_WRITE, WRITE_COMMAND_8, 0x11, END_WRITE,
  DELAY, 200,
  BEGIN_WRITE, WRITE_C8_D8, 0x3A, 0x55, END_WRITE,
  BEGIN_WRITE, WRITE_COMMAND_8, 0x29, END_WRITE,
  DELAY, 50,
};

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);

// Display in native portrait orientation
Arduino_AXS15231B *display = new Arduino_AXS15231B(
  bus, LCD_RST, 0, false, 172, 640,
  0, 0, 0, 0, minimal_init, sizeof(minimal_init)
);

// Canvas: draw in portrait, flush full frame
Arduino_Canvas *gfx = new Arduino_Canvas(172, 640, display);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== Canvas Full-Refresh Test ===");

  if (!gfx->begin()) {
    Serial.println("gfx->begin() FAILED");
    while(1) delay(100);
  }
  Serial.println("Canvas begin OK");

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LOW);

  // Draw color bars to the Canvas (in-memory)
  int h = 80;
  uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F, 0xFFFF, 0x7BEF};
  const char* names[] = {"RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA", "WHITE", "GRAY"};

  for (int i = 0; i < 8; i++) {
    gfx->fillRect(0, i * h, 172, h, colors[i]);
    Serial.printf("Drew bar %d: %s (in canvas)\n", i+1, names[i]);
  }

  // Draw text on the canvas
  gfx->setTextColor(0x0000, 0xF800);  // black on red
  gfx->setTextSize(3);
  gfx->setCursor(10, 20);
  gfx->print("Hello!");

  gfx->setTextColor(0xFFFF, 0x001F);  // white on blue
  gfx->setTextSize(2);
  gfx->setCursor(10, 180);
  gfx->print("Canvas Works?");

  Serial.println("Flushing entire canvas to display...");
  gfx->flush();
  Serial.println("Done! Check display (portrait mode).");
}

void loop() { delay(1000); }
