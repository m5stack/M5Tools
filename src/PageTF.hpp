#pragma once

#include "main.hpp"

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
#endif
      const int tf_ss_pin = M5.getPin(m5::pin_name_t::sd_spi_ss);
      constexpr int retry_max = 5;
      bool opened = false;
      bool canceled = false;
      for (int attempt = 1; ; ++attempt)
      {
        SD.end();
        opened = SD.begin(tf_ss_pin, SPI, 25000000);
        if (opened || attempt >= retry_max) { break; }
        /// 何度目の試行かを見せる。試行の合間はタッチで中断できる
        M5.Lcd.printf("retry %d/%d ...\r\n", attempt + 1, retry_max);
        updateTouch();
        if (prev_touchPoints < touchPoints)
        {
          canceled = true;
          break;
        }
      }
      if (opened)
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
        M5.Lcd.println(canceled ? "TF card open canceled ." : "TF card open failure .");
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
    bool abort = false;
    while (true)
    {
      File fp = dir.openNextFile();
      if (!fp) { break; }

      updateTouch();
      abort = (prev_touchPoints < touchPoints);
      if (abort)
      {
        fp.close();
        break;
      }
      M5.Lcd.print(fp.name());
      if (fp.isDirectory())
      {
        M5.Lcd.println("/");
        if (!showFiles(fp))
        {
          fp.close();
          abort = true;
          break;
        }
      }
      else
      {
        M5.Lcd.println();
      }
      fp.close();
    }
    dir.rewindDirectory();
    return !abort;
  }

};
