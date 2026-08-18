#pragma once

#include "main.hpp"

extern const unsigned char gImage_ScanWiFi[];

struct PageWiFi : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.pushImage(220, 120, 80, 80 , (m5gfx::rgb565_t*)gImage_ScanWiFi);
    _scanning = false;
  }
  void loop(void) override
  {
    if (!_scanning
     && justTouch && (tp[0].y > 120) && (tp[0].y < 200) && (tp[0].x > 220) && (tp[0].x < 320))
    {
      clickSound();
      M5.Lcd.setFont(&fonts::Font2);
      M5.Lcd.fillRect(20, 35, 200, 165, TFT_WHITE);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);

      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      /// スキャンは非同期で開始し、完了は loop 側でポーリングする。
      /// 同期版はバンド全チャンネル分ブロックし (5GHz 対応機は特に長い)、
      /// その間タッチも表示も止まってしまう
      WiFi.scanNetworks(true);
      _scanning = true;
      _anim = 0;
      _anim_ms = millis() - 1000; // 最初のフレームを即描く
    }
    if (_scanning)
    {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING)
      {
        if (millis() - _anim_ms >= 250)
        { /// 待機中であることが分かるようドットを回す
          _anim_ms = millis();
          M5.Lcd.setFont(&fonts::Font2);
          M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
          M5.Lcd.setCursor(20, 32);
          M5.Lcd.printf("Scanning %-6.*s", (int)(_anim + 1), "......");
          _anim = (_anim + 1) % 6;
        }
        return;
      }
      _scanning = false;
      M5.Lcd.setFont(&fonts::Font2);
      M5.Lcd.setCursor(20, 32);
      if (n < 0)
      {
        M5.Lcd.setTextColor(TFT_RED, TFT_WHITE);
        M5.Lcd.printf("Scan failed.         ");
        return;
      }
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
      M5.Lcd.printf("Total : %d found.     ", n);
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

private:
  bool _scanning = false;
  uint32_t _anim_ms = 0;
  uint8_t _anim = 0;
};
