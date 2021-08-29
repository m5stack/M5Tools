#pragma once

#include "main.hpp"

extern const unsigned char gImage_gpioPage[];
extern const unsigned char gImage_slideBack1[];
extern const unsigned char gImage_slideGreen[];
extern const unsigned char gImage_slideRed[];

struct PageGPIO : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.pushImage(17, 32, 286, 172, (m5gfx::rgb565_t*)gImage_gpioPage);
    M5.Lcd.pushImage(36, 56, 8, 136, (m5gfx::rgb565_t*)gImage_slideBack1);
    M5.Lcd.pushImage(76, 56, 8, 136, (m5gfx::rgb565_t*)gImage_slideBack1);
    pinMode(35, ANALOG);
    pinMode(36, ANALOG);
    setGpio(0, _gpioOut[0]);
    setGpio(1, _gpioOut[1]);
    for (int i = 0; i < 2; ++i)
    {
      prev[i] = 128 - (analogRead(35 + i) >> 5);
    }
  }
  void loop(void) override
  {
    if (justTouch)
    {
      _editIdx = -1;
      if (tp[0].x > 32 && tp[0].x < 104 && tp[0].y >= 56 && tp[0].y < 200)
      {
        clickSound();
        _editIdx = tp[0].x < 64 ? 0 : 1;
      }
    }
    if (_editIdx >= 0)
    {
      if (touchPoints == 0)
      {
        _editIdx = -1;
      }
      else
      {
        int val = (128 - (tp[0].y - 64)) * 2;
        if (val < 0) val = 0;
        else if (val > 255) val = 255;
        if (val != _gpioOut[_editIdx])
        {
          setGpio(_editIdx, val);
        }
      }
    }

    M5.Lcd.copyRect(118, 53, 173, 130, 119, 53);
    uint16_t buf[130];
    for (int y = 0; y < 130; ++y)
    {
      buf[y] = (uint16_t)(((loopCount) & 15) && ((y - 1) & 15) ? TFT_WHITE : TFT_LIGHTGRAY);
    }
    for (int i = 0; i < 2; ++i)
    {
      int pos = 128 - (analogRead(35 + i) >> 5);
      int maxpos = prev[i];
      prev[i] = pos;
      if (maxpos < pos) { std::swap(pos, maxpos); }
      buf[pos   -1] &= i ? 0xFAE7 : 0x1BFF;
      buf[maxpos+1] &= i ? 0xFAE7 : 0x1BFF;
      for (int y = pos; y <= maxpos; ++y)
      {
        buf[y] &= i ? 0xF8E3 : 0x18FF;
      // RRRR Rggg GGGb bbBB
      // rrrR Rggg GGGB BBBB
      // RRRR RGGG GGGB BBBB
      }
    }
    M5.Lcd.pushImage(291, 53, 1, 130, (m5gfx::rgb565_t*)buf);
/*
    M5.Lcd.drawFastVLine(291, 54, 128, loopCount & 15 ? TFT_WHITE : TFT_LIGHTGRAY);
    if (loopCount & 15)
    {
      for (int y = 0; y < 128; y += 16)
      {
        M5.Lcd.writePixel(291, 54 + y, TFT_LIGHTGRAY);
      }
    }
    for (int i = 0; i < 2; ++i)
    {
      static int prev[2];
      int pos = analogRead(35 + i) >> 5;
      int maxpos = prev[i] < pos ? pos : prev[i];
      int minpos = prev[i] > pos ? pos : prev[i];
      prev[i] = pos;
      M5.Lcd.writeFastVLine(291, 181 - (maxpos), (maxpos - minpos) + 1 , i ? TFT_RED : TFT_BLUE);
      M5.Lcd.writePixel(291, 180 - (maxpos), i ? 0xFFCF00u : 0x00CFFFu);
      M5.Lcd.writePixel(291, 182 - (minpos));
    }
*/
  }
  void end(void) override
  {
  }

private:

  int _gpioOut[2] = {0};
  int _editIdx = -1;
  int prev[2];

  void setGpio(int index, int value)
  {
    int x = 24 + index * 40;

    M5.Lcd.setClipRect(x, 174 - (_gpioOut[index] >> 1), 32, 16);
    M5.Lcd.fillScreen(TFT_WHITE);
    M5.Lcd.pushImage(36 + index * 40, 56, 8, 136, (m5gfx::rgb565_t*)gImage_slideBack1);
    M5.Lcd.clearClipRect();

    _gpioOut[index] = value;

    M5.Lcd.pushImage(x, 174 - (value >> 1), 32, 16, (m5gfx::rgb565_t*)(index ? gImage_slideRed : gImage_slideGreen));

  /*
    M5.Lcd.fillRect(x, 179 - (_gpioOut[index] >> 1), 32, 9, TFT_WHITE);
    M5.Lcd.drawRect(x+15, 55, 4, 130, TFT_BLACK);
    _gpioOut[index] = value;
    M5.Lcd.fillRect(x+1, 180 - (value >> 1), 30, 7, TFT_BLACK);
    M5.Lcd.drawFastHLine(x+1, 183 - (value >> 1), 30, TFT_WHITE);
    M5.Lcd.drawRect(x, 179 - (value >> 1), 32, 9, TFT_DARKGRAY);
  //*/
    dacWrite(index ? 26 : 25, value);
  }

};
