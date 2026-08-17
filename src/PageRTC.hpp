#pragma once

#include "main.hpp"

extern const unsigned char gImage_rtcBk[]; // w:288 / h:168
extern const unsigned char gImage_rtcLock[]; // w:25 / h:24 / 2 item
extern const unsigned char gImage_rtcNumber[]; // w:12 / h:20 / 11 item
extern const unsigned char gImage_rtcNumberGray[]; // w:12 / h:20 / 11 item
extern const unsigned char gImage_rtcSetTimer[]; // w:62 / h:40
extern const unsigned char gImage_rtcSetTimerDisable[]; // w:62 / h:40

static constexpr const char weekdays[7][4] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };

struct PageRTC : public PageBase
{
  PageRTC(void)
  {
    _wake_timer.hours = 0;
    _wake_timer.minutes = 0;
    _wake_timer.seconds = 0;
  }

  void setLock(bool lock)
  {
    _lock = lock;
    M5.Lcd.pushImage(16, 88, 25, 24 , (m5gfx::rgb565_t*)gImage_rtcLock + (!lock) * 25 * 24);
  }

  void shakeLock(void)
  {
    for (int i = 0; i < 22; ++i)
    {
      int x = abs((i&7)-3)-2;
      M5.Lcd.pushImage(16+x, 88, 25, 24 , (m5gfx::rgb565_t*)gImage_rtcLock);
      delay(15);
    }
  }

  void setup(void) override
  {
    M5.Lcd.pushImage(16, 34, 288, 168 , (m5gfx::rgb565_t*)gImage_rtcBk);

    M5.Lcd.pushImage(29, 158, 62, 40 , (m5gfx::rgb565_t*)gImage_rtcSetTimerDisable, TFT_WHITE);

    _canvas_base.createSprite(268, 40);

    setLock(true);
    _prev_x = -1;
  }

  void loop(void) override
  {
    if (_editIdx < 0 || _editIdx >= 6)
    {
      // bool 版 getter は失敗時に out-param を更新しないので、読み失敗や
      // 不正データの際は前回の正常値が保持される。1 回の呼び出しに纏めるのは
      // date だけ成功・time だけ失敗という新旧混在を避けるため。
      // (戻り値版は失敗を無視して負のセンチネル値を返すため使わない)
      M5.Rtc.getDateTime(&_date, &_time);
    }
    if (justTouch)
    {
      if (contain(0, 72, 64, 56))
      {
        clickSound();
        setLock(!_lock);
      }
      else
      {
        if (     contain( 92,  48, 80, 54)) { _editIdx = 0; _editVal = _date.year   ; _editMin = 1900; _editMax = 2099; }
        else if (contain(175,  48, 56, 54)) { _editIdx = 1; _editVal = _date.month  ; _editMin =    1; _editMax =   12; }
        else if (contain(235,  48, 56, 54)) { _editIdx = 2; _editVal = _date.date   ; _editMin =    1; _editMax =   31; }
        else if (contain( 95, 101, 56, 54)) { _editIdx = 3; _editVal = _time.hours  ; _editMin =    0; _editMax =   23; }
        else if (contain(165, 101, 56, 54)) { _editIdx = 4; _editVal = _time.minutes; _editMin =    0; _editMax =   59; }
        else if (contain(235, 101, 56, 54)) { _editIdx = 5; _editVal = _time.seconds; _editMin =    0; _editMax =   59; }
        else if (contain( 95, 152, 56, 54)) { _editIdx = 6; _editVal = _wake_timer.hours  ; _editMin = 0; _editMax =  4; }
        else if (contain(165, 152, 56, 54)) { _editIdx = 7; _editVal = _wake_timer.minutes; _editMin = 0; _editMax = 59; }
        else if (contain(235, 152, 56, 54)) { _editIdx = 8; _editVal = _wake_timer.seconds; _editMin = 0; _editMax = 59; }
        if (_editIdx >= 0)
        {
          if (_lock && _editIdx <= 5)
          {
            errorSound();
            _editIdx = -1;
            shakeLock();
          }
          else
          {
            clickSound();
          }
        }
      }
    }
    if (_editIdx >= 0)
    {
      bool res = flickValue();
      switch (_editIdx)
      {
      case 0: _date.year    = _editVal; break;
      case 1: _date.month   = _editVal; break;
      case 2: _date.date    = _editVal; break;
      case 3: _time.hours   = _editVal; break;
      case 4: _time.minutes = _editVal; break;
      case 5: _time.seconds = _editVal; break;
      case 6: _wake_timer.hours   = _editVal; break;
      case 7: _wake_timer.minutes = _editVal; break;
      case 8: _wake_timer.seconds = _editVal; break;
      default: break;
      }
      if (!res)
      {
        if (_editIdx < 6)
        {
          M5.Rtc.setDate(_date);
          M5.Rtc.setTime(_time);
        }
        _editIdx = -1;
      }
    }

    auto img = (const m5gfx::rgb565_t*)gImage_rtcNumber;
    drawNumber(&M5.Lcd, img, 156, 65, _date.year, 4);
    drawNumber(&M5.Lcd, img, 215, 65, _date.month, 2);
    drawNumber(&M5.Lcd, img, 275, 65, _date.date, 2);
    drawNumber(&M5.Lcd, img, 135, 117, _time.hours, 2);
    drawNumber(&M5.Lcd, img, 205, 117, _time.minutes, 2);
    drawNumber(&M5.Lcd, img, 275, 117, _time.seconds, 2);

    bool slider_enabled = _wake_timer.hours || _wake_timer.minutes || _wake_timer.seconds;

    if (_prev_x != _slide_x || _editIdx >= 6)
    {
      _prev_x = _slide_x;
      _canvas_base.pushImage(-13, -124, 288, 168, (m5gfx::rgb565_t*)gImage_rtcBk);
      auto img = (const m5gfx::rgb565_t*)gImage_rtcNumberGray;
      drawNumber(&_canvas_base, img, 106, 11, _wake_timer.hours, 2);
      drawNumber(&_canvas_base, img, 176, 11, _wake_timer.minutes, 2);
      drawNumber(&_canvas_base, img, 246, 11, _wake_timer.seconds, 2);
      if (_slide_x > 62)
      {
        _canvas_base.fillRect(16+62, 0, _slide_x-62, 40, 0xE73C);
      }

      _canvas_base.pushImage(_slide_x, 0, 62, 40, (m5gfx::rgb565_t*)(slider_enabled ? gImage_rtcSetTimer : gImage_rtcSetTimerDisable), TFT_WHITE);
      _canvas_base.pushSprite(&M5.Lcd, 29, 158);
    }
    if (!touchPoints)
    {
      _hold = false;
    }
    else
    if (justTouch)
    {
      if (tp[0].y > 158 && tp[0].y < 198 && tp[0].x > 29 + _slide_x && tp[0].x < 29 + 62 + _slide_x)
      {
        if (slider_enabled)
        {
          clickSound();
          _hold = true;
        }
        else
        {
          errorSound();
        }
      }
    }
    if (_hold)
    {
      _slide_x = std::max(0, std::min(206, tp[0].x - 60));
    }
    else
    if (_slide_x == 206)
    {
      int timer = _wake_timer.hours * 3600 + _wake_timer.minutes * 60 + _wake_timer.seconds;
      M5.Power.timerSleep(timer);
    }
    else
    {
      _slide_x = _slide_x * 7 >> 3;
    }
  }
  void end(void) override
  {
    _canvas_base.deleteSprite();
  }

private:
  M5Canvas _canvas_base;
  int _slide_x = 0;
  int _prev_x = -1;
  bool _hold = false;

  bool _lock;
  m5::rtc_date_t _date;
  m5::rtc_time_t _time;
  int _editIdx = -1;
  int _editVal = -1;
  int _editMin = -1;
  int _editMax = -1;

  m5::rtc_time_t _wake_timer;

  void drawNumber(LovyanGFX* gfx, const m5gfx::rgb565_t* img, int x, int y, int val, int int_digit)
  {
    // 初回読み取り前の初期値など、負値は空白 (グリフ10) で埋める。
    // 旧実装は size_t 受けだったため負値が大きな符号なし値になり、
    // フィールド外まで余分な桁を描いて残留していた。
    const bool blank = (val < 0);
    do
    { // 低い桁から順に描画
      x -= 12;
      int num = blank ? 10 : (val % 10);
      gfx->pushImage(x, y, 12, 20, img + 12 * 20 * num);
      val = blank ? 0 : val / 10;
    } while (--int_digit > 0 || val > 0);
  }

  bool flickValue(void)
  {
    if (!updateTouch())
    {
      return false;
    }
    int tmp = _editVal;
    if (flickDiffY < -10)
    {
      flickDiffY += 10;
      if (++tmp > _editMax)
      {
        tmp = _editMin;
      }
    }
    else if (flickDiffY > 10)
    {
      flickDiffY -= 10;
      if (--tmp < _editMin)
      {
        tmp = _editMax;
      }
    }
    if (_editVal != tmp)
    {
      _editVal = tmp;
      clickSound();
    }
    return true;
  }
};
