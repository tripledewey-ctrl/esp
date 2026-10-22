// LED-Matrix (optional, siehe config.h):
//  - Sonde/Mobil: Balken der eigenen Signalstärke, Fortschritt beim Messpunkt
//  - Zentrale:    Gebäude-Übersicht  | Gebäude A | Halle | Gebäude B |
//                 eine Zeile je Sonde, Farbe = Signalstärke
#pragma once
#include <Arduino.h>

void anzeigeStart();
void anzeigeFarbe(uint8_t r, uint8_t g, uint8_t b);
void anzeigeLoop();
void anzeigeUmschalten();      // Zentrale: Übersicht <-> eigene Signalstärke
bool anzeigeVorhanden();
int bereichSpalte(const char* bereich);   // 0 = Gebäude A, 1 = Halle, 2 = Gebäude B
