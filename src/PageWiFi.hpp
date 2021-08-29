#pragma once

#include "main.hpp"

extern const unsigned char gImage_ScanWiFi[];

struct PageWiFi : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.pushImage(220, 120, 80, 80 , (m5gfx::rgb565_t*)gImage_ScanWiFi);
  }
  void loop(void) override
  {
    if (justTouch && (tp[0].y > 120) && (tp[0].y < 200) && (tp[0].x > 220) && (tp[0].x < 320))
    {
      clickSound();
      M5.Lcd.setFont(&fonts::Font2);
      M5.Lcd.fillRect(20, 35, 200, 165, TFT_WHITE);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
      M5.Lcd.setCursor(20, 32);
      M5.Lcd.printf("Scanning ......");

      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      int n = WiFi.scanNetworks();
      M5.Lcd.setCursor(20, 32);
      M5.Lcd.printf("Total : %d found.", n);
      for (int i = 0; i < 10; i++)
      {
        M5.Lcd.setCursor(20, 50 + 15 * i);
        M5.Lcd.setTextColor((WiFi.RSSI(i) > -70) ? TFT_BLUE : TFT_RED, TFT_WHITE);
        M5.Lcd.printf("%d. %s : (%d)", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      }
    }
  }
  void end(void) override
  {
  }
};
