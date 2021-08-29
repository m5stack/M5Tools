#pragma once

#include "main.hpp"

extern const unsigned char gImage_pwrBk[];
extern const unsigned char gImage_pwrNumber[]; // '0123456789 .-' w=8 : h=12 * 13char
extern const unsigned char gImage_pwrInOut[];        // w:60 / h:40 / 2 img
extern const unsigned char gImage_pwrBattery[];
extern const unsigned char gImage_pwrLcdHolder[];     // w:20 / h:20
extern const unsigned char gImage_batteryDirection[]; // w:9 / h:48 / 2 img
extern const unsigned char gImage_pwrCore2Switch[]; // w:44 / h:20 / 2 img

struct PagePWR : public PageBase
{
  bool exten = true;
  bool flickBr = false;
  int slider_level;
  int batlevel;
  int chargeDirection = -1;
  bool flgMotor = false;

  int posToBrightness(int y)
  {
    int br = (127 - (tp[0].y - 64));
    if (br < 0) br = 0;
    else if (br > 127) br = 127;

    br = ((br * (1000-300)) / 127) + 300;
    br = br * 255 / 1000;
    return br;
  }
  int BrightnessToPos(int brightness)
  {
    int y = brightness * 1000 / 255;
    y = std::max(0, std::min(128, (y - 300) * 127 / (1000-300)));
    return 191 - y;
  }

  void setExtEn(bool flg)
  {
    exten = flg;
    if (flg)
    {
      M5.Axp.writeRegister8(0x91, (M5.Axp.readRegister8(0x91) & 0x0F) | 0xF0);
      M5.Axp.writeRegister8(0x90, (M5.Axp.readRegister8(0x90) & 0xF8) | 0x02);
      M5.Axp.setEXTEN(true);
    }
    else
    {
      M5.Axp.setEXTEN(false);
      M5.Axp.writeRegister8(0x90, (M5.Axp.readRegister8(0x90) & 0xF8) | 0x01);
    }
    M5.Lcd.pushImage(145, 123, 60, 40 , (m5gfx::rgb565_t*)gImage_pwrInOut + flg * 60 * 40);
  }

  void setMotorLcdSwitch(bool motor)
  {
    if (M5.getBoard() != m5::board_t::board_M5StackCore2) { return; }

    flgMotor = motor;
    M5.Lcd.pushImage(20, 34, 44, 20, (m5gfx::rgb565_t*)gImage_pwrCore2Switch + motor * 44 * 20);
  }

  void drawSlider(int level)
  {
    int y = BrightnessToPos(slider_level) - 10;
    M5.Lcd.setClipRect(27, y, 20, 20);
    M5.Lcd.pushImage(16, 34, 288, 168 , (m5gfx::rgb565_t*)gImage_pwrBk);
    M5.Lcd.clearClipRect();

    slider_level = level;
    y = BrightnessToPos(level) - 10;
    M5.Lcd.pushImage(27, y, 20, 20, (m5gfx::rgb565_t*)gImage_pwrLcdHolder);
  }

  void setMotor(int level)
  {
    if (M5.getBoard() != m5::board_t::board_M5StackCore2) { return; }
    drawSlider(level);
    M5.Axp.setLDO3(1300 + level * 6);
  }

  void setBrightness(int br)
  {
    drawSlider(br);
    M5.Lcd.setBrightness(br);
  }

  void drawFloat(int x, int y, float value, int int_digit, int fraction_digit)
  {
    x -= 8;
    for (int i = 0; i < fraction_digit; ++i)
    {
      value *= 10.0f;
      ++int_digit;
    }
    int val = value;

    // 低い桁から順に描画
    do
    {
      int num = (val % 10);
      if (!val && fraction_digit < 0)
      {
        num = 10;
      }
      M5.Lcd.pushImage(x, y, 8, 12, (m5gfx::rgb565_t*)gImage_pwrNumber + 8 * 12 * num);
      x -= 8;
      if (0 == --fraction_digit)
      {
        M5.Lcd.pushImage(x, y, 8, 12, (m5gfx::rgb565_t*)gImage_pwrNumber + 8 * 12 * (11));
        x -= 8;
      }
      val = val / 10;
    } while (--int_digit > 0 || val);
  }

  void setup(void) override
  {
    M5.Lcd.pushImage(16, 34, 288, 168 , (m5gfx::rgb565_t*)gImage_pwrBk);

    M5.Axp.bitOn(0x82, 0xFF); // ADC enable

    setBrightness(M5.Lcd.getBrightness());

    setExtEn(M5.Axp.getEXTEN());

    batlevel = -1;

    if (M5.getBoard() == m5::board_t::board_M5StackCore2)
    {
      M5.Lcd.fillRect(18, 36, 54, 12, TFT_WHITE);
      setMotorLcdSwitch(false);
    }
  }

  void end(void) override
  {
    setMotor(0);
  }

  void loop(void) override
  {
    if (justTouch)
    {
      if (tp[0].x >= 145 && tp[0].x < 205 && tp[0].y >= 123 && tp[0].y < 163)
      {
        clickSound();
        setExtEn(!exten);
      }
      else
      if (tp[0].x < 80 && tp[0].y > 56 && tp[0].y < 200)
      {
        clickSound();
        flickBr = true;
      }
      else
      if (M5.getBoard() == m5::board_t::board_M5StackCore2
       && tp[0].x < 80 && tp[0].y > 20 && tp[0].y < 56)
      {
        clickSound();
        setMotorLcdSwitch(!flgMotor);
        setMotor(0);
        if (!flgMotor)
        {
          setBrightness(M5.Lcd.getBrightness());
        }
      }
    }
    if (flickBr)
    {
      if (!touchPoints)
      {
        flickBr = false;
      }
      else
      {
        int val = posToBrightness(tp[0].y);
        if (!flgMotor)
        {
          if (val != slider_level)
          {
            setBrightness(val);
          }
        }
        else
        {
          setMotor(val);
        }
      }
    }

    float batVolt = M5.Axp.getBatteryVoltage();

    int level = std::max<int>(0, std::min<int>(100, (batVolt - 3.2f) * 100));
    if (batlevel != level)
    {
      batlevel = level;
      if (M5.Axp.getBatState())
      {
        int w = level * 46 / 100;
        M5.Lcd.setClipRect(238, 169, w, 17);
        M5.Lcd.pushImage(238, 169, 46, 17 , (m5gfx::rgb565_t*)gImage_pwrBattery);
        M5.Lcd.setClipRect(238 + w, 169, 46 - w, 17);
        M5.Lcd.pushImage(238, 169, 46, 17 , (m5gfx::rgb565_t*)gImage_pwrBattery + 46 * 17);
        M5.Lcd.clearClipRect();
      }
      else
      { // no battery
        M5.Lcd.pushImage(238, 169, 46, 17 , (m5gfx::rgb565_t*)gImage_pwrBattery + 46 * 17);
        M5.Lcd.drawLine(238 + 28, 169, 238 + 16, 169 + 16, TFT_BLACK);
      }
    }

    drawFloat(277, 36, M5.Axp.getInternalTemperature(), 2, 1);

    bool dir = M5.Axp.isCharging();
    if (chargeDirection != dir)
    {
      chargeDirection = dir;
      M5.Lcd.pushImage(252, 118, 9, 48 , (m5gfx::rgb565_t*)gImage_batteryDirection + dir * 9 * 48);
    }
    float batCurrent = dir
                     ? M5.Axp.getBatteryChargeCurrent()
                     : M5.Axp.getBatteryDischargeCurrent();
    drawFloat(282, 135, batCurrent, 3, 0);

    drawFloat(277, 189, batVolt, 1, 2);

    drawFloat(191, 39, M5.Axp.getACINCurrent(), 4, 1);
    drawFloat(122, 66, M5.Axp.getACINVolatge(), 1, 2);
    drawFloat(191, 78, M5.Axp.getVBUSCurrent(), 4, 1);
    drawFloat(122, 112, M5.Axp.getVBUSVoltage(), 1, 2);
    drawFloat(122, 188, M5.Axp.getAPSVoltage(), 1, 2);
/*
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);

    M5.Lcd.setCursor(100,  40); M5.Lcd.printf("bat power: %f", M5.Axp.getBatteryPower());
    M5.Lcd.setCursor(100,  60); M5.Lcd.printf("bat volt: %f", M5.Axp.getBatteryVoltage());
    M5.Lcd.setCursor(100,  80); M5.Lcd.printf("vbus current: %f", M5.Axp.getVbusCurrent());
    M5.Lcd.setCursor(100, 100); M5.Lcd.printf("vbus volt: %f", M5.Axp.getVbusVoltage());
    M5.Lcd.setCursor(100, 120); M5.Lcd.printf("acin current: %f", M5.Axp.getAcinCurrent());
    M5.Lcd.setCursor(100, 140); M5.Lcd.printf("acin volt: %f", M5.Axp.getAcinVolatge());
    M5.Lcd.setCursor(100, 160); M5.Lcd.printf("aps volt: %f", M5.Axp.getApsVoltage());
//*/
  }
};
