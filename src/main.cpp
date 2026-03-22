#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_BN.h>
#include <Fonts/FreeSans9pt7b.h>
#include <qrcode.h>
#include "logo.h"

#define ELINK_SS     5
#define ELINK_DC    17
#define ELINK_RESET 16
#define ELINK_BUSY   4
#define BUTTON_PIN  39

const char*     AP_SSID = "epaper";
const IPAddress AP_IP(192, 168, 4, 1);

const int QR_VERSION = 3;
const int QR_SCALE   = 3;
const int QR_ZONE_W  = 95;   // QR code is 87px; 4px padding each side

// Right-side layout
const int TEXT_X    = QR_ZONE_W + 2;
const int KEY_COL_W = 65;
const int SEP_X     = TEXT_X + KEY_COL_W + 2;
const int VAL_X     = SEP_X + 3;
const int LINE_H    = 15;
const int START_Y   = 16;
const int MAX_PAIRS = 7;

const unsigned long WIFI_TIMEOUT_MS = 30000;
const unsigned long DEBOUNCE_MS     = 200;

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(ELINK_SS, ELINK_DC, ELINK_RESET, ELINK_BUSY)
);

WebServer server(80);
DNSServer dns;

String keys[MAX_PAIRS];
String vals[MAX_PAIRS];
int    pairCount = 0;

bool          wifiActive     = false;
unsigned long wifiStartMs    = 0;
bool          rawBtnState    = HIGH;
bool          debouncedBtn   = HIGH;
unsigned long lastDebounceMs = 0;

void drawFull(bool showQR);  // forward declaration
void drawLeft(bool showQR);  // forward declaration
void drawRight();            // forward declaration

const char PAGE[] PROGMEM = R"EPD(<!DOCTYPE html>
<html><head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ePaper</title>
  <style>
    body{font-family:sans-serif;max-width:420px;margin:32px auto;padding:16px}
    h2{margin-bottom:16px}
    .row{display:flex;gap:8px;margin-bottom:8px}
    .row input{flex:1;padding:10px;font-size:15px;box-sizing:border-box}
    .row button{padding:10px 14px;font-size:15px;cursor:pointer}
    .actions{display:flex;gap:8px;margin-top:8px}
    .actions button{flex:1;padding:12px;font-size:15px;cursor:pointer;border-radius:4px}
    .header{display:flex;gap:8px;margin-bottom:4px;font-weight:bold;font-size:13px;color:#555}
    .header span{flex:1}
    #submit{background:#333;color:#fff;border:none}
    #clear{background:#eee;border:1px solid #ccc}
  </style>
</head><body>
  <h2>ePaper Display</h2>
  <form id="f" method="POST" action="/update">
    <div class="header">
      <span>Key</span><span>Value</span>
      <span style="flex:0;width:42px"></span>
    </div>
    <div id="rows"></div>
    <div class="actions">
      <button type="button" onclick="addRow()">+ Row</button>
      <button type="submit" id="submit">Send</button>
      <button type="button" id="clear" onclick="clearAll()">Clear</button>
    </div>
  </form>
  <script>
    function addRow(k, v) {
      var d = document.createElement('div');
      d.className = 'row';
      d.innerHTML =
        '<input name="key[]" placeholder="Key" value="' + (k || '') + '">' +
        '<input name="val[]" placeholder="Value" value="' + (v || '') + '">' +
        '<button type="button" onclick="this.parentNode.remove()">x</button>';
      document.getElementById('rows').appendChild(d);
    }
    function clearAll() {
      fetch('/clear', {method:'POST'}).then(function() { location.reload(); });
    }
    fetch('/pairs')
      .then(function(r) { return r.json(); })
      .then(function(pairs) {
        if (pairs.length) pairs.forEach(function(p) { addRow(p[0], p[1]); });
        else addRow();
      });
  </script>
</body></html>)EPD";

// ---- Drawing ---------------------------------------------------------------

void drawLogoContent()
{
    int xOff = (QR_ZONE_W - LOGO_W) / 2;
    int yOff = (display.height() - LOGO_H) / 2;
    display.drawBitmap(xOff, yOff, logo_bitmap, LOGO_W, LOGO_H, GxEPD_BLACK);
}

void drawQRContent()
{
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
}

void drawRightContent()
{
    display.drawFastVLine(SEP_X, 0, display.height(), GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);
    for (int i = 0; i < pairCount; i++) {
        int y = START_Y + i * LINE_H;
        display.setCursor(TEXT_X, y);
        display.print(keys[i]);
        display.setCursor(VAL_X, y);
        display.print(vals[i]);
    }
}

// Full refresh — redraws everything (boot only)
void drawFull(bool showQR)
{
    Serial.printf("[display] full refresh (showQR=%d)\n", showQR);
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        if (showQR) drawQRContent();
        else        drawLogoContent();
        display.drawFastVLine(QR_ZONE_W, 0, display.height(), GxEPD_BLACK);
        drawRightContent();
    } while (display.nextPage());
}

// Partial refresh — updates only the left side (logo ↔ QR swap)
void drawLeft(bool showQR)
{
    Serial.printf("[display] partial refresh left (showQR=%d)\n", showQR);
    display.setPartialWindow(0, 0, QR_ZONE_W, display.height());
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        if (showQR) drawQRContent();
        else        drawLogoContent();
    } while (display.nextPage());
}

// Partial refresh — updates only the right side (used when key:value changes)
void drawRight()
{
    Serial.println("[display] partial refresh (right side)");
    display.setPartialWindow(QR_ZONE_W, 0, display.width() - QR_ZONE_W, display.height());
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawFastVLine(QR_ZONE_W, 0, display.height(), GxEPD_BLACK);
        drawRightContent();
    } while (display.nextPage());
}

// ---- WiFi ------------------------------------------------------------------

void enableWifi()
{
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);
    dns.start(53, "*", AP_IP);
    server.begin();
    wifiActive  = true;
    wifiStartMs = millis();
    drawLeft(true);
    Serial.println("WiFi on");
}

void disableWifi()
{
    dns.stop();
    WiFi.softAPdisconnect(true);
    wifiActive = false;
    drawLeft(false);
    Serial.println("WiFi off");
}

// ---- Web handlers ----------------------------------------------------------

void handleRoot()
{
    server.send_P(200, "text/html", PAGE);
}

void handlePairs()
{
    String json = "[";
    for (int i = 0; i < pairCount; i++) {
        if (i > 0) json += ",";
        json += "[\"" + keys[i] + "\",\"" + vals[i] + "\"]";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleUpdate()
{
    String tmpKeys[MAX_PAIRS], tmpVals[MAX_PAIRS];
    int ki = 0, vi = 0;

    for (int i = 0; i < server.args(); i++) {
        String name = server.argName(i);
        if (name == "key[]" && ki < MAX_PAIRS)      tmpKeys[ki++] = server.arg(i);
        else if (name == "val[]" && vi < MAX_PAIRS) tmpVals[vi++] = server.arg(i);
    }

    pairCount = 0;
    int total = min(ki, vi);
    for (int i = 0; i < total; i++) {
        if (tmpKeys[i].length() > 0 || tmpVals[i].length() > 0) {
            keys[pairCount] = tmpKeys[i];
            vals[pairCount] = tmpVals[i];
            pairCount++;
        }
    }

    // Reset WiFi timer so user has time after submitting
    wifiStartMs = millis();

    drawRight();

    server.sendHeader("Location", "/");
    server.send(302);
}

void handleClear()
{
    pairCount = 0;
    wifiStartMs = millis();
    drawRight();
    server.sendHeader("Location", "/");
    server.send(302);
}

// ---- Setup / Loop ----------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT);

    display.init();
    display.setRotation(1);
    drawFull(false);

    server.on("/", handleRoot);
    server.on("/pairs", handlePairs);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/clear",  HTTP_POST, handleClear);
    server.onNotFound(handleRoot);

    Serial.println("Ready. Press button to enable WiFi.");
}

void loop()
{
    // Button debounce — track raw state separately from debounced state
    bool reading = digitalRead(BUTTON_PIN);
    if (reading != rawBtnState) {
        Serial.printf("[btn] raw changed: %d -> %d at %lums\n", rawBtnState, reading, millis());
        rawBtnState  = reading;
        lastDebounceMs = millis();
    }
    if ((millis() - lastDebounceMs) > DEBOUNCE_MS && reading != debouncedBtn) {
        debouncedBtn = reading;
        Serial.printf("[btn] debounced -> %d\n", debouncedBtn);
        if (debouncedBtn == LOW) {
            Serial.println("[btn] press confirmed");
            if (!wifiActive) enableWifi();
            else             wifiStartMs = millis();
        }
    }

    if (wifiActive) {
        if (millis() - wifiStartMs >= WIFI_TIMEOUT_MS) {
            disableWifi();
        } else {
            dns.processNextRequest();
            server.handleClient();
        }
    }
}
