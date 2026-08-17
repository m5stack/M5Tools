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
#if M5TOOLS_TARGET_ESP32C5
  // ToughC5 グラフィック版のトグル位置 (60x40 の pwrInOut 画像、AXP 版と同位置)
  static constexpr int C5_EXT_X = 145, C5_EXT_Y = 123;
#endif
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
#if M5TOOLS_TARGET_ESP32C5
    M5.Power.setExtOutput(flg);
    M5.Lcd.pushImage(C5_EXT_X, C5_EXT_Y, 60, 40 , (m5gfx::rgb565_t*)gImage_pwrInOut + flg * 60 * 40);
#else
    M5.Power.setExtPower(flg);
    M5.Lcd.pushImage(145, 123, 60, 40 , (m5gfx::rgb565_t*)gImage_pwrInOut + flg * 60 * 40);
#endif
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
    M5.Power.Axp192.setLDO3(1300 + level * 6);
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

    // 数字画像に負号のグリフが無いため負値は表示できない。加えて C++ では負数の
    // 剰余が負になり、そのまま画像配列の添字にすると範囲外を読んで表示が壊れる。
    // 値を出せない場合は空白で埋める。
    const bool blank = (val < 0);

    // 低い桁から順に描画
    do
    {
      int num = (val % 10);
      if (blank || (!val && fraction_digit < 0))
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
      val = blank ? 0 : val / 10;
    } while (--int_digit > 0 || val);
  }

  void setup(void) override
  {
#if M5TOOLS_TARGET_ESP32C5
    setupC5();
    return;
#endif

    M5.Lcd.pushImage(16, 34, 288, 168 , (m5gfx::rgb565_t*)gImage_pwrBk);

    M5.Power.Axp192.bitOn(0x82, 0xFF); // ADC enable

    setBrightness(M5.Lcd.getBrightness());

    setExtEn(M5.Power.Axp192.getEXTEN());

    batlevel = -1;

    if (M5.getBoard() == m5::board_t::board_M5StackCore2)
    {
      M5.Lcd.fillRect(18, 36, 54, 12, TFT_WHITE);
      setMotorLcdSwitch(false);
    }
  }

  void end(void) override
  {
#if M5TOOLS_TARGET_ESP32C5
    return;
#endif
    setMotor(0);
  }

  void loop(void) override
  {
#if M5TOOLS_TARGET_ESP32C5
    loopC5();
    return;
#endif

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

    float batVolt = M5.Power.Axp192.getBatteryVoltage();

    int level = std::max<int>(0, std::min<int>(100, (batVolt - 3.2f) * 100));
    if (batlevel != level)
    {
      batlevel = level;
      if (M5.Power.Axp192.getBatState())
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

    drawFloat(277, 36, M5.Power.Axp192.getInternalTemperature(), 2, 1);

    bool dir = M5.Power.Axp192.isCharging();
    if (chargeDirection != dir)
    {
      chargeDirection = dir;
      M5.Lcd.pushImage(252, 118, 9, 48 , (m5gfx::rgb565_t*)gImage_batteryDirection + dir * 9 * 48);
    }
    float batCurrent = dir
                     ? M5.Power.Axp192.getBatteryChargeCurrent()
                     : M5.Power.Axp192.getBatteryDischargeCurrent();
    drawFloat(282, 135, batCurrent, 3, 0);

    drawFloat(277, 189, batVolt, 1, 2);

    drawFloat(191, 39, M5.Power.Axp192.getACINCurrent(), 4, 1);
    drawFloat(122, 66, M5.Power.Axp192.getACINVolatge(), 1, 2);
    drawFloat(191, 78, M5.Power.Axp192.getVBUSCurrent(), 4, 1);
    drawFloat(122, 112, M5.Power.Axp192.getVBUSVoltage(), 1, 2);
    drawFloat(122, 188, M5.Power.Axp192.getAPSVoltage(), 1, 2);
/*
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);

    M5.Lcd.setCursor(100,  40); M5.Lcd.printf("bat power: %f", M5.Power.Axp192.getBatteryPower());
    M5.Lcd.setCursor(100,  60); M5.Lcd.printf("bat volt: %f", M5.Power.Axp192.getBatteryVoltage());
    M5.Lcd.setCursor(100,  80); M5.Lcd.printf("vbus current: %f", M5.Power.Axp192.getVbusCurrent());
    M5.Lcd.setCursor(100, 100); M5.Lcd.printf("vbus volt: %f", M5.Power.Axp192.getVbusVoltage());
    M5.Lcd.setCursor(100, 120); M5.Lcd.printf("acin current: %f", M5.Power.Axp192.getAcinCurrent());
    M5.Lcd.setCursor(100, 140); M5.Lcd.printf("acin volt: %f", M5.Power.Axp192.getAcinVolatge());
    M5.Lcd.setCursor(100, 160); M5.Lcd.printf("aps volt: %f", M5.Power.Axp192.getApsVoltage());
//*/
  }

private:
#if M5TOOLS_TARGET_ESP32C5
  // 機種依存の値収集はこの構造体と readMetrics に閉じる。UI 側は構造体だけを読む。
  struct pwr_metrics_t
  {
    int bat_mv;     // >0 = 電圧 / 0 = バッテリー無し / -1 = 在否未確定
    int bat_level;  // -1 = バッテリー無し or 未確定
    int bat_raw_mv; // PM1 VBAT 生値 (在否ゲート無し) / -1 = 読み取り失敗
    int chg_en;     // CHG_EN (PWR_CFG bit0) / -1 = 読み取り失敗
    int vbus_mv;    // -1 = N/A
    int v5out_mv;   // 0 = 読み取り失敗
    int pwr_src;    // PWR_SRC(0x04) 生値 / -1 = 読み取り失敗
    int temp_dC;    // 0.1℃ 単位 / -1 = 未取得
    m5::Power_Class::is_charging_t charging;
  };

  int temp_dC = -1;
  int chgEnState = -1;
  bool tempKicked = false;
  uint32_t lastDrawMs = 0;

  void readMetrics(pwr_metrics_t& m)
  {
    auto& pm1 = M5.Power.M5pm1;

    m.bat_mv    = M5.Power.getBatteryVoltage();
    m.bat_level = M5.Power.getBatteryLevel();
    m.vbus_mv   = M5.Power.getVBUSVoltage();
    m.v5out_mv  = pm1.get5VoutVoltage();
    m.charging  = M5.Power.isCharging();

    uint16_t raw = 0;
    m.bat_raw_mv = pm1.getBatteryVoltage(&raw) ? raw : -1;
    bool en;
    m.chg_en = pm1.getBatteryCharge(&en) ? (int)en : -1;

    uint8_t v;
    m.pwr_src = pm1.readRegister(0x04, &v, 1) ? v : -1;

    // 内部温度は共用 ADC の ch6。変換完了は待たず、次回 tick で結果を回収する。
    uint8_t ctrl = pm1.readRegister8(0x2A);
    if (!(ctrl & 1))
    {
      uint8_t res[2];
      if (tempKicked && pm1.readRegister(0x28, res, 2))
      {
        temp_dC = ((res[1] & 0x0F) << 8) | res[0];
      }
      tempKicked = pm1.writeRegister8(0x2A, (6 << 1) | 1);
    }
    m.temp_dC = temp_dC;
  }

  // --- ToughC5 グラフィック版 ---
  // AXP 版の背景画像 (gImage_pwrBk) をそのまま貼り、ToughC5 に無い要素
  // (電流表示・DC/DC 系) を消してから実測値を重ねる。座標は AXP 版準拠。
  // PWR_SRC の各ビットは対応する配線への緑オーバーレイで表現する。
  // 幅は 41 まで。ページ切り替え時の消去矩形が x=302 までしか消さないため、
  // 42 にすると右端の 1 列が次のページに残る。
  static constexpr int C5_CHG_X = 262, C5_CHG_Y = 122, C5_CHG_W = 41, C5_CHG_H = 44;
  static constexpr int C5_BAT_X = 238, C5_BAT_Y = 169;   // 電池アイコン 46x17 (AXP 版と同位置)
  static constexpr int C5_ARROW_X = 252, C5_ARROW_Y = 118; // 充電矢印 9x48 (AXP 版と同位置)

  // 変化検出用 (再描画を状態変化時に限る)
  int c5_src = -2;
  int c5_chg_en = -2;
  int c5_arrow = -2;     // 0=なし / 1=充電 / 2=放電
  int c5_batstate = -9;  // 1=有り / 0=無し / -1=未確定
  int c5_battxt = -9;    // 電池テキスト行の描画済み状態

  // 背景画像の一部を原画で復元する
  void c5Restore(int x, int y, int rw, int rh)
  {
    M5.Lcd.setClipRect(x, y, rw, rh);
    M5.Lcd.pushImage(16, 34, 288, 168, (m5gfx::rgb565_t*)gImage_pwrBk);
    M5.Lcd.clearClipRect();
  }

  // 配線描画 (緑=通電 / 黒=非通電)。y は配線の上端。
  // 原画の配線は mA 表記と近接しているため、復元ではなく描き直しで管理する。
  // 配線描画。通電時は緑 + 矢頭、非通電時は黒の線のみ (向きを主張しない)。
  // 線幅と矢頭は原画の作画 (1px 線 + 小さな矢頭) に合わせる。y は配線の中心。
  void c5Wire(int y, bool live, bool into_pmu)
  {
    M5.Lcd.fillRect(139, y - 7, 87, 15, TFT_WHITE); // PMU 箱の縁 (x228〜) には近づけない
    if (!live)
    {
      M5.Lcd.fillRect(141, y, 78, 1, TFT_BLACK);
      return;
    }
    if (into_pmu)
    {
      M5.Lcd.fillRect(141, y, 71, 1, TFT_DARKGREEN);
      M5.Lcd.fillTriangle(211, y - 4, 211, y + 4, 218, y, TFT_DARKGREEN);
    }
    else
    {
      M5.Lcd.fillRect(148, y, 71, 1, TFT_DARKGREEN);
      M5.Lcd.fillTriangle(148, y - 4, 148, y + 4, 141, y, TFT_DARKGREEN);
    }
  }

  void c5DrawChgButton(bool available, bool en)
  {
    uint16_t body = (available && en) ? (uint16_t)0x8C40 /* olive */ : TFT_WHITE;
    uint16_t line = available ? TFT_BLACK : 0xC618;
    uint16_t text = (available && en) ? TFT_WHITE : line;
    M5.Lcd.fillRoundRect(C5_CHG_X, C5_CHG_Y, C5_CHG_W, C5_CHG_H, 8, body);
    M5.Lcd.drawRoundRect(C5_CHG_X, C5_CHG_Y, C5_CHG_W, C5_CHG_H, 8, line);
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextDatum(m5gfx::middle_center);
    M5.Lcd.setTextColor(text, body);
    M5.Lcd.drawString("CHG", C5_CHG_X + C5_CHG_W / 2, C5_CHG_Y + 13);
    M5.Lcd.drawString(!available ? "?" : en ? "ON" : "OFF", C5_CHG_X + C5_CHG_W / 2, C5_CHG_Y + 30);
    M5.Lcd.setTextDatum(m5gfx::top_left);
  }

  void setupC5(void)
  {
    M5.Lcd.pushImage(16, 34, 288, 168, (m5gfx::rgb565_t*)gImage_pwrBk);

    // ToughC5 に無い要素 (電流表示・DC/DC 系) を背景色で消す。
    // PMU 箱の縁 (x228〜、アンチエイリアス含む) には触れない。
    const uint16_t wipe = TFT_WHITE;
    M5.Lcd.fillRect(139,  38,  87, 65, wipe); // USB/M-BUS の電流表記と配線帯 (MODE 表記には触れない)
    M5.Lcd.fillRect( 88, 126,  62, 76, wipe); // DC/DC 本体と縦矢印
    M5.Lcd.fillRect(126, 124,  82, 78, wipe); // 曲線矢印 (OUTPUT トグルの下)
    M5.Lcd.fillRect(208, 118,  96, 84, wipe); // 曲線右側・電池電流・CHG ボタン下地

    setExtEn(M5.Power.getExtOutput());
    setBrightness(M5.Lcd.getBrightness());

    temp_dC = -1;
    tempKicked = false;
    lastDrawMs = 0;
    c5_src = -2;
    c5_chg_en = -2;
    c5_arrow = -2;
    c5_batstate = -9;
    c5_battxt = -9;
    batlevel = -1;
  }

  void loopC5(void)
  {
    if (justTouch)
    {
      if (tp[0].x >= C5_EXT_X && tp[0].x < C5_EXT_X + 60
       && tp[0].y >= C5_EXT_Y && tp[0].y < C5_EXT_Y + 40)
      {
        clickSound();
        setExtEn(!exten);
        c5_src = -2; // 矢印の向きが変わるので配線を描き直す
        lastDrawMs = 0;
      }
      else
      if (tp[0].x >= C5_CHG_X - 4 && tp[0].x < C5_CHG_X + C5_CHG_W + 4
       && tp[0].y >= C5_CHG_Y - 4 && tp[0].y < C5_CHG_Y + C5_CHG_H + 4
       && chgEnState >= 0)
      {
        clickSound();
        M5.Power.setBatteryCharge(!chgEnState);
        lastDrawMs = 0;
      }
      else
      if (tp[0].x < 80 && tp[0].y > 56 && tp[0].y < 200)
      {
        clickSound();
        flickBr = true;
      }
    }
    if (flickBr)
    {
      if (!touchPoints) { flickBr = false; }
      else
      {
        int val = posToBrightness(tp[0].y);
        if (val != slider_level) { setBrightness(val); }
      }
    }

    uint32_t now = millis();
    if (lastDrawMs && now - lastDrawMs < 250) { return; }
    lastDrawMs = now ? now : 1;

    pwr_metrics_t m;
    readMetrics(m);
    chgEnState = m.chg_en;

    // 配線の通電状態と向きは設定値でなく実動作で示す:
    // PWR_SRC bit1 が立っていれば OUTPUT 設定でも実際はバスから受電している
    // (真に出力onlyなら電力源を失って落ちるため)。出力方向の通電は PWR_SRC に
    // 現れないので、OUTPUT 設定かつレール電圧の実測で判定する。
    int src = m.pwr_src < 0 ? 0 : m.pwr_src;
    bool mbus_in  = (src & 2) != 0;
    bool mbus_out = !mbus_in && exten && (m.v5out_mv > 4000);
    int wires = ((src & 1) ? 1 : 0) | (mbus_in ? 2 : 0) | (mbus_out ? 4 : 0);
    if (c5_src != wires)
    {
      c5_src = wires;
      c5Wire(54, src & 1, true);              // USB → PMU
      c5Wire(92, mbus_in || mbus_out, !mbus_out); // 受電中は →PMU / 出力中は PMU→
    }

    // 充電矢印: 充電中=充電向き / 電池駆動 (5VIN なし)=放電向き / それ以外=なし
    int arrow = (m.charging == m5::Power_Class::is_charging) ? 1
              : (m.bat_mv > 0 && !(src & 1)) ? 2 : 0;
    if (c5_arrow != arrow)
    {
      c5_arrow = arrow;
      if (arrow)
      {
        M5.Lcd.pushImage(C5_ARROW_X, C5_ARROW_Y, 9, 48,
                         (m5gfx::rgb565_t*)gImage_batteryDirection + (arrow == 1) * 9 * 48);
      }
      else
      { /// アイドル時は矢印画像の軸 1px 列を切り出して線として流用する。
        /// CHG ボタンの左枠 (x262) に触れないよう消去は x260 まで。
        M5.Lcd.fillRect(C5_ARROW_X - 2, C5_ARROW_Y, 11, 48, TFT_WHITE);
        M5.Lcd.setClipRect(C5_ARROW_X + 4, C5_ARROW_Y, 1, 48);
        M5.Lcd.pushImage(C5_ARROW_X, C5_ARROW_Y, 9, 48, (m5gfx::rgb565_t*)gImage_batteryDirection);
        M5.Lcd.clearClipRect();
      }
    }

    // CHG_EN トグルボタン
    if (c5_chg_en != m.chg_en)
    {
      c5_chg_en = m.chg_en;
      c5DrawChgButton(m.chg_en >= 0, m.chg_en > 0);
    }

    // 電池アイコン: 有り=残量塗り / 無し=空+斜線 / 未確定=空+オレンジ枠 "?"
    int batstate = (m.bat_mv > 0) ? 1 : (m.bat_mv == 0) ? 0 : -1;
    int level = (batstate == 1) ? m.bat_level : -1;
    if (c5_batstate != batstate || batlevel != level)
    {
      c5_batstate = batstate;
      batlevel = level;
      if (batstate == 1)
      {
        int w = std::max(0, std::min(100, level)) * 46 / 100;
        M5.Lcd.setClipRect(C5_BAT_X, C5_BAT_Y, w, 17);
        M5.Lcd.pushImage(C5_BAT_X, C5_BAT_Y, 46, 17, (m5gfx::rgb565_t*)gImage_pwrBattery);
        M5.Lcd.setClipRect(C5_BAT_X + w, C5_BAT_Y, 46 - w, 17);
        M5.Lcd.pushImage(C5_BAT_X, C5_BAT_Y, 46, 17, (m5gfx::rgb565_t*)gImage_pwrBattery + 46 * 17);
        M5.Lcd.clearClipRect();
      }
      else
      {
        M5.Lcd.pushImage(C5_BAT_X, C5_BAT_Y, 46, 17, (m5gfx::rgb565_t*)gImage_pwrBattery + 46 * 17);
        if (batstate == 0)
        { // 無し: 斜線
          M5.Lcd.drawLine(C5_BAT_X + 28, C5_BAT_Y, C5_BAT_X + 16, C5_BAT_Y + 16, TFT_BLACK);
        }
        else
        { // 未確定: オレンジ枠 + "?"
          M5.Lcd.drawRect(C5_BAT_X, C5_BAT_Y, 43, 17, TFT_ORANGE);
          M5.Lcd.setFont(&fonts::Font2);
          M5.Lcd.setTextDatum(m5gfx::middle_center);
          M5.Lcd.setTextColor(TFT_ORANGE, TFT_WHITE);
          M5.Lcd.drawString("?", C5_BAT_X + 21, C5_BAT_Y + 8);
          M5.Lcd.setTextDatum(m5gfx::top_left);
        }
      }
    }

    // 数値 (ビットマップ数字): USB / M-BUS。単位 "V" は背景の焼き込みを使う
    if (m.vbus_mv  >= 0) { drawFloat(122,  66, m.vbus_mv  * 0.001f, 1, 2); }
    if (m.v5out_mv >  0) { drawFloat(122, 112, m.v5out_mv * 0.001f, 1, 2); }

    // PM1 温度 (raw 値、単位は暫定で背景の摂氏表記を流用)
    if (m.temp_dC >= 0) { drawFloat(277, 36, (float)m.temp_dC, 3, -1); }

    char buf[24];
    M5.Lcd.setFont(&fonts::Font2);
    M5.Lcd.setTextDatum(m5gfx::top_left);

    // 電池電圧行: 有りは確定値+残量、無し/未確定は raw ノード電圧を灰/橙で。
    // 状態が変わったら行全体を原画で復元してから描き直す (消し残り防止)。
    if (c5_battxt != batstate)
    {
      c5_battxt = batstate;
      c5Restore(188, 187, 116, 15);
    }
    if (batstate == 1)
    {
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
      M5.Lcd.setTextDatum(m5gfx::top_right);
      M5.Lcd.setTextPadding(36);
      snprintf(buf, sizeof buf, "%d%%", m.bat_level);
      M5.Lcd.drawString(buf, 228, 187); // Font2 と数字スプライトの見た目の高さを揃える
      M5.Lcd.setTextDatum(m5gfx::top_left);
      drawFloat(277, 189, m.bat_mv * 0.001f, 1, 2);
    }
    else
    { /// raw も同じスプライト数字で描き、"raw" の色だけで状態を示す
      M5.Lcd.setTextColor(batstate == 0 ? 0x8410 : TFT_ORANGE, TFT_WHITE);
      M5.Lcd.setTextPadding(0);
      M5.Lcd.drawString("raw", 206, 187);
      if (m.bat_raw_mv >= 0) { drawFloat(277, 189, m.bat_raw_mv * 0.001f, 1, 2); }
      else
      {
        M5.Lcd.setTextPadding(40);
        M5.Lcd.drawString("N/A", 240, 187);
      }
    }

    // PWR_SRC 生値 (診断用)
    M5.Lcd.setTextColor(0x8410, TFT_WHITE);
    M5.Lcd.setTextPadding(56);
    if (m.pwr_src >= 0) { snprintf(buf, sizeof buf, "SRC 0x%02X", m.pwr_src); }
    else                { snprintf(buf, sizeof buf, "SRC ?"); }
    M5.Lcd.drawString(buf, 132, 187);
    M5.Lcd.setTextPadding(0);
  }
#endif
};
