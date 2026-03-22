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

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(ELINK_SS, ELINK_DC, ELINK_RESET, ELINK_BUSY)
);

WebServer server(80);
DNSServer dns;

void drawScreen();

String keys[MAX_PAIRS];
String vals[MAX_PAIRS];
int    pairCount = 0;

// Pairs fetched via /pairs on load — no dynamic HTML injection needed
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

void drawScreen()
{
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        // --- QR code ---
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

        // --- Dividers ---
        display.drawFastVLine(QR_ZONE_W, 0, display.height(), GxEPD_BLACK);
        display.drawFastVLine(SEP_X, 0, display.height(), GxEPD_BLACK);

        // --- Key:Value rows ---
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

    } while (display.nextPage());
}

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
        if (name == "key[]" && ki < MAX_PAIRS) {
            tmpKeys[ki++] = server.arg(i);
        } else if (name == "val[]" && vi < MAX_PAIRS) {
            tmpVals[vi++] = server.arg(i);
        }
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

    drawScreen();

    server.sendHeader("Location", "/");
    server.send(302);
}

void handleClear()
{
    pairCount = 0;
    drawScreen();
    server.sendHeader("Location", "/");
    server.send(302);
}

void setup()
{
    Serial.begin(115200);

    display.init();
    display.setRotation(1);
    drawScreen();

    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);

    dns.start(53, "*", AP_IP);

    server.on("/", handleRoot);
    server.on("/pairs", handlePairs);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/clear", HTTP_POST, handleClear);
    server.onNotFound(handleRoot);
    server.begin();

    Serial.printf("AP: %s  IP: %s\n", AP_SSID, AP_IP.toString().c_str());
}

void loop()
{
    dns.processNextRequest();
    server.handleClient();
}
