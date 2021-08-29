#pragma once

#include "main.hpp"

extern const unsigned char gImage_arrow[];
extern const unsigned char gImage_pleaseSelect[];

struct PageDefault : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.pushImage(37, 96, 246, 25, (m5gfx::rgb565_t*)gImage_pleaseSelect);
  }

  void loop(void) override
  {
    float f = abs((int)(loopCount & 255) - 128);
    M5.Lcd.pushImageRotateZoom(160, 160 - (f / 6.0), 14, 0, 0, f / 24.0, 1, 28, 40, (m5gfx::rgb565_t*)gImage_arrow);
  }

  void end(void) override
  {
  }
};
