#pragma once

#include "main.hpp"

#define CONFIG_TF_SS_PIN 4

struct PageTF : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.setTextDatum(textdatum_t::middle_center);
    M5.Lcd.drawString("Tap to scan TF.", 160, 120, &fonts::Font4);
    M5.Lcd.setTextDatum(textdatum_t::top_left);
  }
  void loop(void) override
  {
    if (justTouch)
    {
      M5.Lcd.endWrite();
      M5.Lcd.setFont(&fonts::Font2);
      M5.Lcd.setTextColor(TFT_BLACK);
      M5.Lcd.setTextScroll(true);
      M5.Lcd.fillRect(20, 36, 280, 160, TFT_WHITE);
      M5.Lcd.setScrollRect(20, 36, 280, 160, TFT_WHITE);
      M5.Lcd.setCursor(20, 36);
      M5.Lcd.println("TF card open...");
      int retry = 5;
      do
      {
        SD.end();
      } while (!SD.begin(CONFIG_TF_SS_PIN, SPI, 25000000) && --retry);
      if (retry)
      {
        auto root = SD.open("/");
        if (!showFiles(root))
        {
          M5.Lcd.println("\r\n break !");
        }
        root.close();
      }
      else
      {
        M5.Lcd.println("TF card open failure .");
      }
      M5.Lcd.clearScrollRect();
      M5.Lcd.setTextScroll(false);
      M5.Lcd.startWrite();
    }
  }
  void end(void) override
  {
  }
private:
  bool showFiles(File dir)
  {
    File fp =  dir.openNextFile();
    bool abort = false;
    while ((bool)(fp = dir.openNextFile()))
    {
      updateTouch();
      abort = (prev_touchPoints < touchPoints);
      if (abort) break;
      M5.Lcd.print(fp.name());
      if (fp.isDirectory())
      {
        M5.Lcd.println("/");
        if (!showFiles(fp))
        {
          abort = true;
          break;
        }
      }
      else
      {
        M5.Lcd.println();
      }
    }
    dir.rewindDirectory();
    return !abort;
  }

};
