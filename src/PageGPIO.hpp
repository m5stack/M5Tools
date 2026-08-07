#pragma once

#include "main.hpp"

#if M5TOOLS_TARGET_ESP32C5
#include <esp_adc/adc_continuous.h>
#include <soc/adc_channel.h>
#endif

extern const unsigned char gImage_gpioPage[];
extern const unsigned char gImage_slideBack1[];
extern const unsigned char gImage_slideGreen[];
extern const unsigned char gImage_slideRed[];

#if M5TOOLS_TARGET_ESP32C5
/// ToughC5 には DAC が無いため、出力は LEDC PWM で代用する。
/// 出力は従来機種の M5BUS DAC 位置に合わせて G6 / G4 を使う。
/// G4 は PM1 の IRQ 出力線 (wakeup ピン) と共有のため、ページ滞在中のみ
/// PM1 側を入力 (Hi-Z) にして線を明け渡してもらい、離脱時に復元する。
static constexpr uint8_t gpioPwmPins[2] = { 6, 4 };  // M5BUS DAC 位置 (G6 = PortB out)
static constexpr uint8_t gpioAdcPins[2] = { 5, 1 };  // ループバック先: G6->G5 / G4->G1
static constexpr uint8_t gpioAdcChs[2] = { ADC1_GPIO5_CHANNEL, ADC1_GPIO1_CHANNEL };
/// PWM 周波数の制約: SAR ADC の T/H は速い矩形波に追従できず、PWM 周期が
/// 変換時間 (数 us) に近いと変換自体が壊れて duty と無関係な値になる
/// (625kHz で実測確認)。周期が十分長い 21kHz とし、ADC のサンプルレート
/// 80kHz と非整数比にすることで位相ロックによる偏りも避ける。
static constexpr uint32_t gpioPwmFreq = 21000;
static constexpr uint8_t gpioPwmBits = 7;   // 128 段階 (XTAL 48MHz で余裕)
/// ADC は連続変換 (DMA) モードで回す。80kHz を 2ch で分け合い各 40kHz。
/// 1 フレームで数百サンプル平均になるため PWM 矩形波の揺らぎが均される。
static constexpr uint32_t gpioAdcFreq = 80000;
#endif

struct PageGPIO : public PageBase
{
  void setup(void) override
  {
    M5.Lcd.pushImage(17, 32, 286, 172, (m5gfx::rgb565_t*)gImage_gpioPage);
    M5.Lcd.pushImage(36, 56, 8, 136, (m5gfx::rgb565_t*)gImage_slideBack1);
    M5.Lcd.pushImage(76, 56, 8, 136, (m5gfx::rgb565_t*)gImage_slideBack1);
#if M5TOOLS_TARGET_ESP32C5
    /// G4 を PWM に使う間、PM1 の IRQ 出力を入力 (Hi-Z) にして線を明け渡してもらう
    M5.Power.M5pm1.setGPIOFunction(m5::M5PM1_Class::gpio1, m5::M5PM1_Class::gpio);
    M5.Power.M5pm1.setGPIOMode(m5::M5PM1_Class::gpio1, m5::M5PM1_Class::input);
    for (int i = 0; i < 2; ++i)
    {
      if (!ledcAttach(gpioPwmPins[i], gpioPwmFreq, gpioPwmBits))
      {
        M5.Lcd.setCursor(24, 40);
        M5.Lcd.printf("PWM G%d attach NG", gpioPwmPins[i]);
      }
    }
    beginAdc();
    delay(10);  /// 最初の平均に十分なサンプルが溜まるのを待つ
#else
    pinMode(35, ANALOG);
    pinMode(36, ANALOG);
#endif
    setGpio(0, _gpioOut[0]);
    setGpio(1, _gpioOut[1]);
    for (int i = 0; i < 2; ++i)
    {
      prev[i] = 128 - (readAdc(i) >> 5);
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
      int pos = 128 - (readAdc(i) >> 5);
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
#if M5TOOLS_TARGET_ESP32C5
    /// PWM を止め、G4 を wakeup ピン (プルアップ入力) へ戻してから
    /// PM1 の IRQ 出力機能を復元する
    for (int i = 0; i < 2; ++i)
    {
      ledcDetach(gpioPwmPins[i]);
    }
    pinMode(4, INPUT_PULLUP);
    M5.Power.M5pm1.setGPIOFunction(m5::M5PM1_Class::gpio1, m5::M5PM1_Class::irq);
    if (_adc)
    {
      adc_continuous_stop(_adc);
      adc_continuous_deinit(_adc);
      _adc = nullptr;
    }
#endif
  }

private:

  int _gpioOut[2] = {0};
  int _editIdx = -1;
  int prev[2];

#if M5TOOLS_TARGET_ESP32C5
  adc_continuous_handle_t _adc = nullptr;
  int _adcAvg[2] = { 0, 0 };

  void beginAdc(void)
  {
    adc_continuous_handle_cfg_t hcfg = {};
    hcfg.max_store_buf_size = 2048;
    hcfg.conv_frame_size = 256;
    if (ESP_OK != adc_continuous_new_handle(&hcfg, &_adc)) { _adc = nullptr; return; }
    adc_digi_pattern_config_t pat[2] = {};
    for (int i = 0; i < 2; ++i)
    {
      pat[i].atten = ADC_ATTEN_DB_12;
      pat[i].channel = gpioAdcChs[i];
      pat[i].unit = ADC_UNIT_1;
      pat[i].bit_width = 12;
    }
    adc_continuous_config_t ccfg = {};
    ccfg.pattern_num = 2;
    ccfg.adc_pattern = pat;
    ccfg.sample_freq_hz = gpioAdcFreq;
    ccfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    ccfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    adc_continuous_config(_adc, &ccfg);
    adc_continuous_start(_adc);
  }

  /// 溜まった変換結果を全て取り込み、チャネルごとの平均を更新する。
  /// サンプル不足のチャネルは前回の平均を保持する。
  void drainAdc(void)
  {
    if (_adc == nullptr) { return; }
    uint32_t sum[2] = { 0, 0 };
    uint32_t cnt[2] = { 0, 0 };
    uint8_t buf[256];
    uint32_t len = 0;
    while (ESP_OK == adc_continuous_read(_adc, buf, sizeof(buf), &len, 0))
    {
      for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= len; i += SOC_ADC_DIGI_RESULT_BYTES)
      {
        auto d = (adc_digi_output_data_t*)&buf[i];
        for (int k = 0; k < 2; ++k)
        {
          if (d->type2.channel == gpioAdcChs[k]) { sum[k] += d->type2.data; ++cnt[k]; }
        }
      }
      if (len < sizeof(buf)) { break; }
    }
    for (int k = 0; k < 2; ++k)
    {
      if (cnt[k] >= 16) { _adcAvg[k] = (int)(sum[k] / cnt[k]); }
    }
  }
#endif

  int readAdc(int index)
  {
#if M5TOOLS_TARGET_ESP32C5
    if (index == 0) { drainAdc(); }  /// フレーム先頭 (ch0 参照時) にまとめて取り込む
    return _adcAvg[index];
#else
    return analogRead(35 + index);
#endif
  }

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
#if M5TOOLS_TARGET_ESP32C5
    ledcWrite(gpioPwmPins[index], value >> 1);  // 0-255 → 0-127 (7bit duty)
#else
    dacWrite(index ? 26 : 25, value);
#endif
  }

};
