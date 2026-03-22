// Auto-generated layout adapted for GxEPD2 partial refresh.
// Regenerate the shaped regions from your design tool, then update
// drawRightContent() below. Keep drawLeftContent() unless the left
// zone geometry changes.

#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_BN.h>
#include <Fonts/Org_01.h>
#include <qrcode.h>

#include "display_layout.h"
#include "logo.h"

// Shared objects defined in main.cpp
extern GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display;
extern const char* AP_SSID;

static const int QR_VERSION = 3;
static const int QR_SCALE   = 3;

// ---- Icon bitmaps (from layout tool) ----------------------------------------

static const unsigned char PROGMEM image_battery_67_bits[] = {
  0x03,0xe0,0x04,0x10,0x04,0x10,0x1c,0x1c,0x20,0x02,0x20,0x02,0x20,0x02,
  0x20,0x02,0x20,0x02,0x20,0x02,0x20,0x02,0x2f,0xfa,0x2f,0xfa,0x20,0x02,
  0x2f,0xfa,0x2f,0xfa,0x20,0x02,0x2f,0xfa,0x2f,0xfa,0x20,0x02,0x2f,0xfa,
  0x2f,0xfa,0x20,0x02,0x1f,0xfc
};

static const unsigned char PROGMEM image_celsius_bits[] = {
  0xff,0xff,0xff,0x80,0x00,0x01,0x80,0x02,0x01,0x80,0x05,0x01,0x86,0x32,
  0x61,0x89,0x48,0x91,0x81,0x10,0x81,0x82,0x08,0x91,0x84,0x48,0x61,0x8f,
  0x30,0x01,0x80,0x00,0x01,0xc0,0x00,0x03,0x60,0x00,0x06,0x30,0x00,0x0c,
  0x18,0x00,0x18,0x0c,0x00,0x30,0x06,0x00,0x60,0x03,0x00,0xc0,0x01,0x81,
  0x80,0x00,0xc3,0x00,0x00,0x66,0x00,0x00,0x3c,0x00,0x00,0x18,0x00
};

static const unsigned char PROGMEM image_music_radio_broadcast_bits[] = {
  0x07,0xc0,0x18,0x30,0x27,0xc8,0x48,0x24,0x93,0x92,0xa4,0x4a,0xa9,0x2a,
  0xa3,0x8a,0x06,0xc0,0x03,0x80,0x01,0x00,0x03,0x80,0x02,0x80,0x06,0xc0,
  0x04,0x40,0x00,0x00
};

// ---- Left zone --------------------------------------------------------------

void drawLeftContent(bool showQR)
{
    if (showQR) {
        char wifiStr[32];
        snprintf(wifiStr, sizeof(wifiStr), "WIFI:T:nopass;S:%s;;", AP_SSID);

        QRCode qr;
        uint8_t qrBuf[110];
        qrcode_initText(&qr, qrBuf, QR_VERSION, ECC_LOW, wifiStr);

        int qrPx = qr.size * QR_SCALE;
        int xOff = (QR_ZONE_W - qrPx) / 2;
        int yOff = (display.height() - qrPx) / 2;

        for (int y = 0; y < qr.size; y++) {
            for (int x = 0; x < qr.size; x++) {
                if (qrcode_getModule(&qr, x, y)) {
                    display.fillRect(
                        xOff + x * QR_SCALE,
                        yOff + y * QR_SCALE,
                        QR_SCALE, QR_SCALE,
                        GxEPD_BLACK
                    );
                }
            }
        }
    } else {
        int xOff = (QR_ZONE_W - LOGO_W) / 2;
        int yOff = (display.height() - LOGO_H) / 2;
        display.drawBitmap(xOff, yOff, logo_bitmap, LOGO_W, LOGO_H, GxEPD_BLACK);
    }
}

// ---- Right zone -------------------------------------------------------------
// Edit this function when redesigning the layout in your external tool.
// All coordinates are absolute display pixels. Text uses Org_01 font.

void drawRightContent(const String& team, const String& callsign)
{
    display.setFont(&Org_01);
    display.setTextWrap(false);

    // --- Shapes ---

    // Callsign badge (filled black)
    display.fillRoundRect(103, 33, 135, 21, 5, GxEPD_BLACK);
    display.drawRoundRect(102, 31, 136, 21, 5, GxEPD_BLACK);

    // Team badge (filled black)
    display.drawRoundRect(103, 66, 136, 21, 5, GxEPD_BLACK);
    display.fillRoundRect(104, 64, 135, 21, 5, GxEPD_BLACK);

    // Callsign label bar
    display.fillRect(182, 47, 56, 8, GxEPD_BLACK);
    display.fillRoundRect(174, 49, 64, 13, 3, GxEPD_BLACK);

    // Team label bar
    display.fillRoundRect(103, 56, 64, 13, 3, GxEPD_BLACK);
    display.drawRect(103, 62, 60, 7, GxEPD_BLACK);

    // Radio/status box
    display.drawRoundRect(103, 90, 76, 23, 4, GxEPD_BLACK);
    display.fillRoundRect(129, 90, 53, 23, 4, GxEPD_BLACK);

    // --- Icons ---
    display.drawBitmap(212, 89, image_celsius_bits,              24, 23, GxEPD_BLACK);
    display.drawBitmap(189, 89, image_battery_67_bits,           16, 24, GxEPD_BLACK);
    display.drawBitmap(109, 94, image_music_radio_broadcast_bits, 15, 16, GxEPD_BLACK);

    // --- Text ---

    // Title (black on white)
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(3);
    display.setCursor(105, 20);
    display.print("AVALON");

    // All remaining text is white on black shapes
    display.setTextColor(GxEPD_WHITE);

    // Callsign value
    display.setTextSize(2);
    display.setCursor(133, 45);
    display.print(callsign);

    // Label: CALLSIGN
    display.setTextSize(1);
    display.setCursor(183, 59);
    display.print("CALLSIGN");

    // Label: TEAM
    display.setCursor(123, 62);
    display.print("TEAM");

    // Team value
    display.setTextSize(2);
    display.setCursor(137, 78);
    display.print(team);

    // Status counter
    display.setCursor(143, 104);
    display.print("1/3");
}
