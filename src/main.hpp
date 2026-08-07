#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <M5Unified.h>

#if defined(CONFIG_IDF_TARGET_ESP32C5)
#define M5TOOLS_TARGET_ESP32C5 1
#else
#define M5TOOLS_TARGET_ESP32C5 0
#endif

extern const unsigned char gWav_Click[];
extern const unsigned char gWav_Error[];

m5gfx::touch_point_t prev_tp[2];
m5gfx::touch_point_t tp[2];
uint32_t loopCount = 0;
int touchPoints, prev_touchPoints;
bool justTouch;
int flickDiffX, flickDiffY;


static bool beginSpeaker(void)
{
  if (!M5.Speaker.isEnabled())
  {
    return false;
  }
  return M5.Speaker.isRunning() || M5.Speaker.begin();
}

static void playSound(const uint8_t* data, size_t len, uint32_t sampleRate)
{
  if (beginSpeaker())
  {
    M5.Speaker.playRaw(data, len, sampleRate, false, 1, 0, true);
  }
}

void clickSound(void)
{
  // play(gWavClick, 112, 16000);
  //play(wav, 16538, 16000, 16);
  playSound(gWav_Click, 112, 16000);
}

void errorSound(void)
{
  playSound(gWav_Error, 3584, 16000);
}

int updateTouch(void)
{
  if (touchPoints)
  {
    memcpy(prev_tp, tp, sizeof(m5gfx::touch_point_t) * touchPoints);
  }
  prev_touchPoints = touchPoints;
  touchPoints = M5.Lcd.getTouch(tp, 2);
  justTouch = (prev_touchPoints == 0 && touchPoints != 0);
  if (touchPoints)
  {
    if (justTouch)
    {
      flickDiffX = 0;
      flickDiffY = 0;
    }
    else
    {
      flickDiffX += tp[0].x - prev_tp[0].x;
      flickDiffY += tp[0].y - prev_tp[0].y;
    }
  }

  return touchPoints;
}

bool contain(int x, int y, int w, int h)
{
  return x <= tp->x && tp->x < (x + w)
      && y <= tp->y && tp->y < (y + h);
}

struct PageBase
{
  virtual void setup(void) {}
  virtual void loop(void) {}
  virtual void end(void) {}
};
