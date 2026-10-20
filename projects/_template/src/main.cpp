// Projektvorlage: Minimaler Sketch, der auf ESP32 und ESP8266 baut.
#include <Arduino.h>

void setup() {
  Serial.begin(MONITOR_SPEED);
  delay(200);
  Serial.println();
  Serial.println(F("Projekt gestartet"));
}

void loop() {
  delay(1000);
}
