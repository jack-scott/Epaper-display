#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_BN.h>
#include <Fonts/FreeSans9pt7b.h>
#include <qrcode.h>

#define ELINK_SS     5
#define ELINK_DC    17
#define ELINK_RESET 16
#define ELINK_BUSY   4

const char*       AP_SSID = "epaper";
const IPAddress   AP_IP(192, 168, 4, 1);

// QR version 3 (29x29 modules) fits "WIFI:T:nopass;S:epaper;;" at ECC_LOW
const int QR_VERSION  = 3;
const int QR_SCALE    = 3;
const int QR_ZONE_W   = 115;   // pixels reserved for QR on the left

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(ELINK_SS, ELINK_DC, ELINK_RESET, ELINK_BUSY)
);

WebServer  server(80);
DNSServer  dns;

String currentText = "";

const char PAGE[] PROGMEM = R"(<!DOCTYPE html>
<html><head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ePaper</title>
  <style>
    body{font-family:sans-serif;max-width:360px;margin:40px auto;padding:16px}
    input{width:100%;padding:12px;font-size:16px;box-sizing:border-box;margin-top:8px}
  </style>
</head><body>
  <h2>Send text to display</h2>
  <form method="POST" action="/update">
    <input type="text" name="msg" placeholder="Enter text..." maxlength="60">
    <input type="submit" value="Send">
  </form>
</body></html>)";

void drawScreen(const String& text)
{
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        // --- QR code ---
        char wifiStr[32];
        snprintf(wifiStr, sizeof(wifiStr), "WIFI:T:nopass;S:%s;;", AP_SSID);

        QRCode qr;
        uint8_t qrBuf[110];  // sufficient for version 3
        qrcode_initText(&qr, qrBuf, QR_VERSION, ECC_LOW, wifiStr);

        int qrPx  = qr.size * QR_SCALE;
        int xOff  = (QR_ZONE_W - qrPx) / 2;
        int yOff  = (display.height() - qrPx) / 2;

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

        // --- Divider ---
        display.drawFastVLine(QR_ZONE_W, 0, display.height(), GxEPD_BLACK);

        // --- Text ---
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setTextWrap(true);
        display.setCursor(QR_ZONE_W + 6, 20);

        if (text.length() > 0) {
            display.print(text);
        } else {
            display.setTextColor(0x8888);  // grey placeholder
            display.print("Waiting for\ninput...");
        }
    } while (display.nextPage());
}

void handleRoot()
{
    server.send_P(200, "text/html", PAGE);
}

void handleUpdate()
{
    if (server.method() == HTTP_POST && server.hasArg("msg")) {
        currentText = server.arg("msg");
        drawScreen(currentText);
        server.send(200, "text/html",
            "<p>Done! <a href='/'>Back</a></p>");
    } else {
        server.sendHeader("Location", "/");
        server.send(302);
    }
}

void setup()
{
    Serial.begin(115200);

    display.init();
    display.setRotation(1);
    drawScreen("");

    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);

    dns.start(53, "*", AP_IP);

    server.on("/", handleRoot);
    server.on("/update", handleUpdate);
    server.onNotFound(handleRoot);
    server.begin();

    Serial.printf("AP: %s  IP: %s\n", AP_SSID, AP_IP.toString().c_str());
}

void loop()
{
    dns.processNextRequest();
    server.handleClient();
}
