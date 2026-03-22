#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_BN.h>

#include "display_layout.h"

#define ELINK_SS     5
#define ELINK_DC    17
#define ELINK_RESET 16
#define ELINK_BUSY   4
#define BUTTON_PIN  39

const char*     AP_SSID = "epaper";
const IPAddress AP_IP(192, 168, 4, 1);

const unsigned long WIFI_TIMEOUT_MS = 30000;
const unsigned long DEBOUNCE_MS     = 200;

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(ELINK_SS, ELINK_DC, ELINK_RESET, ELINK_BUSY)
);

WebServer server(80);
DNSServer dns;

String team     = "FALCON";
String callsign = "ROMEO 11";
int    teamSize = 3;

bool          wifiActive     = false;
unsigned long wifiStartMs    = 0;
bool          rawBtnState    = HIGH;
bool          debouncedBtn   = HIGH;
unsigned long lastDebounceMs = 0;

// ---- Web page ---------------------------------------------------------------

const char PAGE[] PROGMEM = R"EPD(<!DOCTYPE html>
<html><head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ePaper</title>
  <style>
    body{font-family:sans-serif;max-width:360px;margin:32px auto;padding:16px}
    h2{margin-bottom:16px}
    label{display:block;font-size:13px;font-weight:bold;color:#555;margin-top:12px;margin-bottom:4px}
    input{width:100%;padding:10px;font-size:15px;box-sizing:border-box;text-transform:uppercase}
    .actions{display:flex;gap:8px;margin-top:16px}
    .actions button{flex:1;padding:12px;font-size:15px;cursor:pointer;border-radius:4px}
    #submit{background:#333;color:#fff;border:none}
    #reset{background:#eee;border:1px solid #ccc}
  </style>
</head><body>
  <h2>ePaper Display</h2>
  <form id="f" method="POST" action="/update">
    <label>Team</label>
    <input name="team" id="team" placeholder="TEAM">
    <label>Callsign</label>
    <input name="callsign" id="callsign" placeholder="CALLSIGN">
    <label>Team Size</label>
    <input name="teamsize" id="teamsize" placeholder="3" type="number" min="1" max="99" style="text-transform:none">
    <div class="actions">
      <button type="submit" id="submit">Send</button>
      <button type="button" id="reset" onclick="resetFields()">Reset</button>
    </div>
  </form>
  <script>
    fetch('/state')
      .then(function(r) { return r.json(); })
      .then(function(s) {
        document.getElementById('team').value = s.team;
        document.getElementById('callsign').value = s.callsign;
        document.getElementById('teamsize').value = s.teamsize;
      });
    function resetFields() {
      fetch('/reset', {method:'POST'}).then(function() { location.reload(); });
    }
  </script>
</body></html>)EPD";

// ---- Drawing ----------------------------------------------------------------

void drawFull(bool showQR)
{
    Serial.printf("[display] full refresh (showQR=%d)\n", showQR);
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawLeftContent(showQR);
        drawRightContent(team, callsign, teamSize);
    } while (display.nextPage());
}

void drawLeft(bool showQR)
{
    Serial.printf("[display] partial refresh left (showQR=%d)\n", showQR);
    display.setPartialWindow(0, 0, QR_ZONE_W, display.height());
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawLeftContent(showQR);
    } while (display.nextPage());
}

void drawRight()
{
    Serial.println("[display] partial refresh right");
    display.setPartialWindow(QR_ZONE_W, 0, display.width() - QR_ZONE_W, display.height());
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        drawRightContent(team, callsign, teamSize);
    } while (display.nextPage());
}

// ---- WiFi -------------------------------------------------------------------

void enableWifi()
{
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);
    dns.start(53, "*", AP_IP);
    server.begin();
    wifiActive  = true;
    wifiStartMs = millis();
    Serial.println("[wifi] on");
    drawLeft(true);
}

void disableWifi()
{
    dns.stop();
    WiFi.softAPdisconnect(true);
    wifiActive = false;
    Serial.println("[wifi] off");
    drawLeft(false);
}

// ---- Web handlers -----------------------------------------------------------

void handleRoot()
{
    server.send_P(200, "text/html", PAGE);
}

void handleState()
{
    String json = "{\"team\":\"" + team + "\",\"callsign\":\"" + callsign + "\",\"teamsize\":" + String(teamSize) + "}";
    server.send(200, "application/json", json);
}

void handleUpdate()
{
    if (server.hasArg("team")) {
        team = server.arg("team");
        team.toUpperCase();
    }
    if (server.hasArg("callsign")) {
        callsign = server.arg("callsign");
        callsign.toUpperCase();
    }
    if (server.hasArg("teamsize")) {
        int n = server.arg("teamsize").toInt();
        if (n > 0) teamSize = n;
    }
    Serial.printf("[update] team=%s callsign=%s teamSize=%d\n", team.c_str(), callsign.c_str(), teamSize);

    wifiStartMs = millis();
    drawRight();

    server.sendHeader("Location", "/");
    server.send(302);
}

void handleReset()
{
    team     = "FALCON";
    callsign = "ROMEO 11";
    teamSize = 3;
    Serial.println("[reset] defaults restored");
    wifiStartMs = millis();
    drawRight();
    server.sendHeader("Location", "/");
    server.send(302);
}

// ---- Setup / Loop -----------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT);

    display.init();
    display.setRotation(1);
    Serial.println("[setup] display init done");

    drawFull(false);
    Serial.println("[setup] initial draw done");

    server.on("/",       handleRoot);
    server.on("/state",  handleState);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/reset",  HTTP_POST, handleReset);
    server.onNotFound(handleRoot);

    Serial.println("[setup] ready — press button to enable WiFi");
}

void loop()
{
    // Button debounce
    bool reading = digitalRead(BUTTON_PIN);
    if (reading != rawBtnState) {
        Serial.printf("[btn] raw %d -> %d at %lums\n", rawBtnState, reading, millis());
        rawBtnState    = reading;
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
