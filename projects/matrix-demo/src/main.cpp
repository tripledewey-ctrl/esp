// matrix-demo: Prüft die LED-Reihenfolge einer 8x8-Matrix und zeigt Effekte.
// Die BOOT-Taste (GPIO0) schaltet zwischen den Modi um:
//   0 = Pixel-Test: LED 0..63 leuchtet nacheinander, Index im seriellen Monitor
//   1 = Regenbogen
//   2 = Lauftext-Balken (Spalten-Scan, zeigt die x/y-Zuordnung)
//
// Achtung: 64 LEDs auf kleiner Fläche werden schnell heiß – Helligkeit niedrig lassen.
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#ifndef MATRIX_PIN
#define MATRIX_PIN 14
#endif
#ifndef MATRIX_WIDTH
#define MATRIX_WIDTH 8
#endif
#ifndef MATRIX_HEIGHT
#define MATRIX_HEIGHT 8
#endif
#ifndef MATRIX_COLOR_ORDER
#define MATRIX_COLOR_ORDER NEO_GRB
#endif

static constexpr uint16_t NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;
static constexpr uint8_t BRIGHTNESS = 20;  // 0–255, für Dauerbetrieb <= 40 empfohlen
static constexpr uint8_t BUTTON_PIN = 0;
static constexpr uint8_t NUM_MODES = 3;

static Adafruit_NeoPixel strip(NUM_LEDS, MATRIX_PIN, MATRIX_COLOR_ORDER + NEO_KHZ800);
static uint8_t mode = 0;

// Zeilenweise Verdrahtung (Zeile für Zeile von links nach rechts).
// Zeigt der Spalten-Scan schräge oder zickzackförmige Linien, ist die Matrix
// anders verdrahtet – dann hier die Zuordnung anpassen (z. B. Serpentine).
static uint16_t xy(uint8_t x, uint8_t y) { return y * MATRIX_WIDTH + x; }

static void pixelTest() {
  static uint16_t i = 0;
  strip.clear();
  strip.setPixelColor(i, strip.Color(255, 255, 255));
  strip.show();
  Serial.printf("LED %2u  (x=%u, y=%u bei zeilenweiser Verdrahtung)\n", i, i % MATRIX_WIDTH,
                i / MATRIX_WIDTH);
  i = (i + 1) % NUM_LEDS;
  delay(300);
}

static void rainbow() {
  static uint16_t hue = 0;
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue + i * 65536UL / NUM_LEDS)));
  }
  strip.show();
  hue += 256;
  delay(20);
}

static void columnScan() {
  static uint8_t col = 0;
  strip.clear();
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    strip.setPixelColor(xy(col, y), strip.Color(0, 120, 255));
  }
  strip.show();
  col = (col + 1) % MATRIX_WIDTH;
  delay(150);
}

void setup() {
  Serial.begin(MONITOR_SPEED);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();
  delay(500);

  // Farbtest: stimmt die Reihenfolge nicht, MATRIX_COLOR_ORDER anpassen (NEO_RGB/NEO_GRB)
  const struct { uint32_t color; const char* name; } colors[] = {
      {strip.Color(255, 0, 0), "Rot"}, {strip.Color(0, 255, 0), "Grün"}, {strip.Color(0, 0, 255), "Blau"}};
  for (const auto& c : colors) {
    Serial.printf("Farbtest: %s\n", c.name);
    strip.fill(c.color);
    strip.show();
    delay(700);
  }
  strip.clear();
  strip.show();

  Serial.printf("matrix-demo: %ux%u LEDs an GPIO%d, BOOT-Taste wechselt den Modus\n",
                MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_PIN);
}

void loop() {
  static bool lastButton = HIGH;
  const bool button = digitalRead(BUTTON_PIN);
  if (lastButton == HIGH && button == LOW) {
    mode = (mode + 1) % NUM_MODES;
    Serial.printf("Modus %u\n", mode);
  }
  lastButton = button;

  switch (mode) {
    case 0: pixelTest(); break;
    case 1: rainbow(); break;
    default: columnScan(); break;
  }
}
