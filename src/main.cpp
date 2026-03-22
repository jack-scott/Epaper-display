#include <GxEPD2_BW.h>
#include <epd/GxEPD2_213_BN.h>
#include <Fonts/FreeSans9pt7b.h>

#define ELINK_SS     5
#define ELINK_DC    17
#define ELINK_RESET 16
#define ELINK_BUSY   4

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(ELINK_SS, ELINK_DC, ELINK_RESET, ELINK_BUSY)
);

void setup()
{
    Serial.begin(115200);

    display.init();
    display.setRotation(1);  // landscape
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(GxEPD_BLACK);

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(10, 20);
        display.print("Hello, world!");
    } while (display.nextPage());

    Serial.println("Done.");
}

void loop() {}
