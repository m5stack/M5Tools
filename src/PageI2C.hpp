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
#if M5TOOLS_TARGET_ESP32C5
    /// ToughC5 の PortA は内部バス (LP_I2C, G2/G3) のレベルシフタ分配であり、
    /// 独立した外部バスが存在しないため、スキャン元の選択 UI は表示しない。
    M5.Lcd.drawCenterString("LP_I2C  G2 / G3" , 160, 148);
    M5.Lcd.drawCenterString("Internal / PortA", 160, 168);
#else
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
    /// 再初期化はページ表示時とソース切替時のみ。毎ループ行うとドライバ再構築の
    /// オーバーヘッドでループ周期が伸び、タッチ応答が悪化する。
    beginSelectedI2C();
#endif
    /// 全セルを未スキャン表示で初期化し、スキャン結果は loop で 1 アドレスずつ反映する
    M5.Lcd.setFont(&fonts::Font0);
    for (int i = 0x08; i < 0x78; ++i)
    {
      drawCell(i, cell_unknown);
    }
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    scanAddr = 0x08;
  }

  /// 空 write プローブ (SMBus Quick 相当) はファーム実装スレーブ (UnitLCD/OLED 等)
  /// や動作中のタッチコントローラの状態を乱すため、1 バイト read で在否を判定する
  bool probe(int addr)
  {
#if M5TOOLS_TARGET_ESP32C5
    auto wire = &M5.In_I2C;
#else
    auto wire = i2cScanSource ? &M5.In_I2C : &M5.Ex_I2C;
#endif
    std::uint8_t dummy;
    bool ok = wire->start(addr, true, 100000);
    if (ok)
    {
      ok = wire->read(&dummy, 1, true);
      ok = wire->stop() && ok;
    }
    return ok;
  }

  enum cell_state_t { cell_unknown, cell_absent, cell_found };

  void drawCell(int addr, cell_state_t state)
  {
    std::size_t y = addr >> 4;
    std::size_t x = addr & 15;
    M5.Lcd.setCursor(28 + x * 17, 43 + y * 12);
    switch (state)
    {
    case cell_unknown: M5.Lcd.setTextColor(TFT_DARKGRAY , TFT_LIGHTGRAY); break;
    case cell_absent:  M5.Lcd.setTextColor(TFT_LIGHTGRAY, TFT_WHITE    ); break;
    case cell_found:   M5.Lcd.setTextColor(TFT_BLACK    , TFT_GREEN    ); break;
    }
    M5.Lcd.printf("%02x", addr);
  }

#if !M5TOOLS_TARGET_ESP32C5
  void beginSelectedI2C(void)
  {
    if (i2cScanSource)
    {
      M5.In_I2C.begin(I2C_NUM_1, 21, 22);
    }
    else
    {
      M5.Ex_I2C.begin(I2C_NUM_0, 32, 33);
    }
  }
#endif
  void loop(void) override
  {
#if !M5TOOLS_TARGET_ESP32C5
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
#endif
    {
      M5.Lcd.setFont(&fonts::Font0);
      /// 一括スキャンはループ周期が伸びてタッチ応答が悪化するため、
      /// 1 ループ 1 アドレスずつ処理する
      int addr = scanAddr;
      drawCell(addr, probe(addr) ? cell_found : cell_absent);
      scanAddr = (addr + 1 < 0x78) ? addr + 1 : 0x08;
      M5.Lcd.setFont(&fonts::Font2);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    }
  }
  void end(void) override
  {
  }
private:
  int scanAddr = 0x08;
#if !M5TOOLS_TARGET_ESP32C5
  int i2cScanSource = 0;
#endif

};
