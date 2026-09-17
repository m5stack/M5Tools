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

#if M5TOOLS_TARGET_ESP32C5

// ToughC5 は I2S スピーカーを持たず、ブザーが PMIC (M5PM1) の PWM ch1 に
// ぶら下がっている。制御は内部 I2C 越しのレジスタ書き込みのみなので波形再生は
// できず、操作音は単音のトーンで代用する。
// 鳴動長を数えるタイマは M5Unified 側に無いため、期限は buzzerService() が見る。

// 周波数と鳴動長は既存 WAV 素材 (src/wav) の解析から求めた。
//   gWav_Click : 有音 3.9ms  / 基本周波数 約 1.8kHz (DFT・ゼロ交差・自己相関が一致)
//   gWav_Error : 有音 215ms  / 基音 約 850Hz (ただし自己相関が低くノイズ性が強い)
//
// 実機実測で分かったこと:
//   - 周波数はレジスタに書いた Hz がそのまま出る。
//   - 2〜3kHz 付近が共振域で極端に大きくなり、操作音には耳障りすぎる。
//     上記の 1.8kHz と 850Hz はどちらも共振域から外れており具合が良い。
//   - duty は音色がわずかに変わるだけで音量にはほぼ効かない。よって音量調整の
//     手段は実質的に周波数の選び方しかなく、包絡の再現もできない。
//     duty は矩形波として素直な 50% を使う。
//   - 数 ms でも十分聞こえるため、click は波形どおりの長さで足りる。
// 周波数は解析値 (1.8kHz / 850Hz) より低めが好ましいという実機評価を受けて下げた。
// 素材との一致より体感を優先するが、click と error の音程差は保つ。
static constexpr uint16_t buzzerClickHz = 1200;
static constexpr uint16_t buzzerClickMs = 4;
static constexpr uint16_t buzzerErrorHz = 600;
static constexpr uint16_t buzzerErrorMs = 200;
static constexpr uint8_t  buzzerDuty    = 50;

// 0 = 鳴動していない。millis() は 0 を取り得るので下記の 1 補正で避ける。
static uint32_t buzzerStopMs = 0;

static void buzzerStop(void)
{
  buzzerStopMs = 0;
  M5.Power.M5pm1.setPwmDutyPercent(m5::M5PM1_Class::pwm_ch1, 0, m5::pwm_polarity_t::normal, false);
}

static void buzzerTone(uint16_t hz, uint8_t duty, uint32_t ms)
{
  auto& pm1 = M5.Power.M5pm1;
  // 鳴動中に周波数を直接書き換えるとグリッチの原因になるため、一度止めてから設定する。
  pm1.setPwmDutyPercent(m5::M5PM1_Class::pwm_ch1, 0, m5::pwm_polarity_t::normal, false);
  if (hz == 0 || duty == 0 || ms == 0)
  {
    buzzerStopMs = 0;
    return;
  }
  pm1.setPwmFrequency(hz);
  pm1.setPwmDutyPercent(m5::M5PM1_Class::pwm_ch1, duty);
  uint32_t stop = millis() + ms;
  buzzerStopMs = stop ? stop : 1;
}

// 毎ループ呼ぶ。期限が来たブザーを止めるだけで、鳴っていなければ I2C は発生しない。
static void buzzerService(void)
{
  if (buzzerStopMs && (int32_t)(millis() - buzzerStopMs) >= 0)
  {
    buzzerStop();
  }
}

// 鳴動中なら期限まで待ってから止める。
// buzzerService() はメインループからしか呼ばれないので、描画が長いページに入ると
// 停止が遅れて短い音が伸びてしまう。長い処理を始める前と、確実に鳴らし切りたい
// 短い音の直後に使う。
// 別タスクで止める案は採らない。m5::I2C_Class に排他制御が無く、内部 I2C バスは
// タッチ・RTC・IOE・PM1 が共有していて、タッチはメインループが毎周ポーリングして
// いるため、保護なしに別タスクから割り込むとバスを奪い合う。
static void buzzerFlush(void)
{
  while (buzzerStopMs)
  {
    buzzerService();
    if (buzzerStopMs)
    {
      delay(1);
    }
  }
}

#endif

static void prepareForPowerDown(void)
{
#if M5TOOLS_TARGET_ESP32C5
  buzzerStop();
  M5.Power.M5pm1.setLedEnLevel(false);
#endif
}

void clickSound(void)
{
#if M5TOOLS_TARGET_ESP32C5
  // click は呼び出し箇所が多く、そのどれが長い描画の直前なのかを追い続けるのは
  // 現実的でない。数 ms しかないので鳴らし切ってから戻り、構造的に伸びないようにする。
  buzzerTone(buzzerClickHz, buzzerDuty, buzzerClickMs);
  buzzerFlush();
#else
  // play(gWavClick, 112, 16000);
  //play(wav, 16538, 16000, 16);
  playSound(gWav_Click, 112, 16000);
#endif
}

void errorSound(void)
{
#if M5TOOLS_TARGET_ESP32C5
  buzzerTone(buzzerErrorHz, buzzerDuty, buzzerErrorMs);
#else
  playSound(gWav_Error, 3584, 16000);
#endif
}

int updateTouch(void)
{
#if M5TOOLS_TARGET_ESP32C5
  buzzerService();
#endif
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
