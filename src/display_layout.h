#pragma once
#include <Arduino.h>

// Width of the left (logo/QR) zone in pixels — used by both layout and main
constexpr int QR_ZONE_W = 95;

// Called inside a GxEPD2 firstPage()/nextPage() page loop.
// Left zone draws logo or QR code into x: 0..QR_ZONE_W
void drawLeftContent(bool showQR);

// Right zone draws the styled team/callsign layout into x: QR_ZONE_W..display.width()
void drawRightContent(const String& team, const String& callsign);
