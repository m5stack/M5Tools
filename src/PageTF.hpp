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
#if M5TOOLS_TARGET_ESP32C5
      /// ToughC5 の TF は電源 (IOE PYG6) とカード検出 (IOE PYG14) が
      /// IO エキスパンダ経由。SPI ピンと CS は M5Unified のテーブルから引く。
      /// (IOE クラスのピン番号は 0 始まりのため PYGn は n-1 になる)
      auto& ioe = M5.getIOExpander(0);
      ioe.setHighImpedance(5, false); // Hi-Z を解除しないと出力が駆動されない
      ioe.setDirection(5, true);
      ioe.digitalWrite(5, true);      // PYG6 = PYB_TF_EN
      ioe.setDirection(13, false);
      ioe.setPullMode(13, m5::IOExpander_Base::pull_up); // PYG14 = PYB_TF_DET (プルアップ入力)
      delay(100);                     // TF 電源の安定待ち
      M5.Lcd.printf("TF_DET=%d TF_EN=%d\r\n",
                    (int)ioe.digitalRead(13), (int)ioe.digitalRead(5));
      const int tf_ss_pin = M5.getPin(m5::pin_name_t::sd_spi_ss);
#else
      const int tf_ss_pin = CONFIG_TF_SS_PIN;
#endif
      int retry = 5;
      do
      {
        SD.end();
      } while (!SD.begin(tf_ss_pin, SPI, 25000000) && --retry);
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
#if M5TOOLS_TARGET_ESP32C5
    /// ページを離れる時は TF をアンマウントして電源を切る。
    /// SD へのアクセスは LCD と共有の SPI バスを使うため、保持中の
    /// 描画トランザクションを解放してから行う。
    M5.Lcd.endWrite();
    SD.end();
    M5.getIOExpander(0).digitalWrite(5, false); // PYG6 = PYB_TF_EN
    M5.Lcd.startWrite();
#endif
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
