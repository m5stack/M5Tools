#pragma once

#include "main.hpp"

struct PageI2C : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.setTextColor(TFT_LIGHTGRAY);
    M5.Lcd.setColor(TFT_LIGHTGRAY);
    // for (int i = 0x00; i < 0x80; ++i)
    // {
    //   std::size_t y = i >> 4;
    //   std::size_t x = i & 15;
    //   M5.Lcd.drawRect(24 + x * 17, 40 + y * 12, 16, 11);
    // }
    for (int i = 0; i < 9; ++i)
    {
      M5.Lcd.drawFastHLine(24, 40 + i * 12, 272);
    }
    for (int i = 0; i < 17; ++i)
    {
      M5.Lcd.drawFastVLine(24 + i * 17, 40, 96);
    }

    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Lcd.drawString(" External"   ,  64, 144);
    M5.Lcd.drawString("I2C0(Wire)" ,  64, 160);
    M5.Lcd.drawString("G33 / G32"   ,  64, 176);
    M5.Lcd.drawString(" Internal"   , 192, 144);
    M5.Lcd.drawString("I2C1(Wire1)", 192, 160);
    M5.Lcd.drawString("G22 / G21"   , 192, 176);

    M5.Lcd.drawCircle(52,166,6,TFT_BLACK);
    M5.Lcd.drawCircle(180,166,6,TFT_BLACK);
    M5.Lcd.fillCircle( 52, 166, 4, i2cScanSource == 0 ? TFT_BLACK : TFT_WHITE);
    M5.Lcd.fillCircle(180, 166, 4, i2cScanSource == 1 ? TFT_BLACK : TFT_WHITE);
    if (i2cScanSource)
    {
      M5.In_I2C.begin(i2c_port_t::I2C_NUM_1, 21, 22);
    }
    else
    {
      M5.Ex_I2C.begin(i2c_port_t::I2C_NUM_0, 32, 33);
    }
  }
  void loop(void) override
  {
    if (justTouch && (tp[0].x > 50 && tp[0].x < 280 && tp[0].y > 144 && tp[0].y < 184))
    {
      int tmp = tp[0].x < 160 ? 0 : 1;
      if (i2cScanSource != tmp)
      {
        i2cScanSource = tmp;
        clickSound();
        setup();
      }
    }
    {
      M5.Lcd.setFont(&fonts::Font0);
      bool result[0x80];
      auto scanWire = i2cScanSource ? &M5.In_I2C : &M5.Ex_I2C;
      scanWire->scanID(result);
      for (int i = 0x08; i < 0x78; ++i)
      {
        bool hit = result[i];
        std::size_t y = i >> 4;
        std::size_t x = i & 15;
        M5.Lcd.setCursor(28 + x * 17, 43 + y * 12);
        M5.Lcd.setTextColor(hit ? TFT_BLACK : TFT_LIGHTGRAY , hit ? TFT_GREEN : TFT_WHITE);
        M5.Lcd.printf("%02x", i);
      }
      M5.Lcd.setFont(&fonts::Font2);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    }
  }
  void end(void) override
  {
  }
private:
  int i2cScanSource = 0;

};
