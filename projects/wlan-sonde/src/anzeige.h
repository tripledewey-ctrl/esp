// Anzeige (optional, siehe config.h) – LED-Matrix oder OLED:
//  LED-Matrix
//  - Sonde/Mobil: Balken der eigenen Signalstärke, Fortschritt beim Messpunkt
//  - Zentrale:    Gebäude-Übersicht  | Gebäude A | Halle | Gebäude B |
//                 eine Zeile je Sonde, Farbe = Signalstärke
//  OLED (72x40 oder 128x64), wechselnde Seiten
//  - Signal (große dBm-Zahl + Balken), Mesh-Knoten, Router/Stabilität, Gerät
//  - Mobil: Fortschritt beim Messpunkt, Anzahl Punkte
//  - Zentrale: Liste der Sonden mit Signalstärke
#pragma once
#include <Arduino.h>

enum class Zustand : uint8_t { Start, Portal, Verbinde, Fehler, Verbunden, Zuruecksetzen };

void anzeigeStart();
void anzeigeZustand(Zustand z, const char* text = nullptr);   // Startphase, Portal, Fehler
void anzeigeLoop();
void anzeigeUmschalten();      // Zentrale: Übersicht <-> eigene Signalstärke
bool anzeigeVorhanden();
int bereichSpalte(const char* bereich);   // 0 = Gebäude A, 1 = Halle, 2 = Gebäude B
