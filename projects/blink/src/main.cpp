// Blink: lässt die Onboard-LED blinken und gibt einen Zähler seriell aus.
// Der LED-Pin kommt aus boards/boards.ini (LED_BUILTIN_PIN).
#include <Arduino.h>

#ifndef LED_BUILTIN_PIN
#define LED_BUILTIN_PIN 2
#endif

static uint32_t counter = 0;

void setup() {
  Serial.begin(MONITOR_SPEED);
  pinMode(LED_BUILTIN_PIN, OUTPUT);
  delay(200);
  Serial.println();
  Serial.printf("Blink gestartet, LED an GPIO %d\n", LED_BUILTIN_PIN);
}

void loop() {
  digitalWrite(LED_BUILTIN_PIN, !digitalRead(LED_BUILTIN_PIN));
  Serial.printf("Tick %lu\n", (unsigned long)counter++);
  delay(500);
}
