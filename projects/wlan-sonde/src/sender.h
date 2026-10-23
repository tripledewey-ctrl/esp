// Sender (Rolle sonde): Messungen an die Zentrale schicken. Was offline
// gemessen wurde, wird nachgesendet, solange es noch im Puffer liegt.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

void senderLoop();
void senderStatus(JsonObject o);
uint32_t senderOffen();        // noch nicht an die Zentrale gesendete Messungen
