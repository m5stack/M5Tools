
#include "main.hpp"

#include <esp_log.h>
#include <driver/i2s.h>

#include "PageDefault.hpp"
#include "PageWiFi.hpp"
#include "PageTOUCH.hpp"
#include "PageI2C.hpp"
#include "PageGPIO.hpp"
#include "PageUART.hpp"
#include "PageTF.hpp"
#include "PageRTC.hpp"
#include "PagePWR.hpp"

extern const unsigned char gImage_blankBk[];
extern const unsigned char gImage_fun_unsel[];
extern const unsigned char gImage_fun_sel[];
extern const unsigned char gImage_sleepBk[];
extern const unsigned char gImage_core2Tools[];
extern const unsigned char gImage_toughTools[];

PageWiFi  page0;
PageTOUCH page1;
PageI2C   page2;
PageGPIO  page3;
PageUART  page4;
PageTF    page5;
PageRTC   page6;
PagePWR   page7;
static constexpr uint32_t pageListMax = 8;
PageBase* pageList[pageListMax] = { &page0, &page1, &page2, &page3, &page4, &page5, &page6, &page7 };
PageDefault pageDefault;
PageBase* selectedPage;

uint32_t prevSel, currentSel;

int tlsc6x_tp_dect(void); // M5Tough TouchPanel updater

void drawBackground(void)
{
  prevSel = -1;
  currentSel = -1;

  M5.Lcd.pushImage(0, 0, 320, 240, (m5gfx::rgb565_t*)gImage_blankBk);
  auto logo = (m5gfx::rgb565_t*)(M5.getBoard() == m5::board_t::board_M5StackCore2
            ? gImage_core2Tools
            : gImage_toughTools);
  M5.Lcd.pushImage(8, 0, 170, 26, logo);
  for (int i = 0; i < 8; i++)
  {
    M5.Lcd.pushImage(i * 40 + 1, 208, 38, 28, (m5gfx::rgb565_t*)gImage_fun_unsel + (i * 38 * 28));
  }
  if (selectedPage)
  {
    selectedPage->end();
  }
  selectedPage = &pageDefault;
  selectedPage->setup();
}

void setup(void)
{
  //bool LCDEnable, bool SDEnable, bool SerialEnable, bool I2CEnable, mbus_mode_t mode
  //M5.begin(true, true, true, false, kMBusModeOutput);//
  M5.begin();//

  tlsc6x_tp_dect();

  M5.Lcd.setFont(&fonts::Font2);
  M5.Lcd.setBaseColor(TFT_WHITE);
  M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);

  xTaskCreatePinnedToCore(soundTask, "soundTask", 4096, &soundParam, 0, &soundParam.handle, 0);

  M5.Lcd.startWrite();

  drawBackground();
}

void loop(void)
{
/*
for (int i = 0; i < 100; ++i)
{
  Serial.printf("count:%d  : ", i);
  delay(50);
  Serial.print("1 begin /");
  Serial1.begin(115200, SERIAL_8N1, 33, 32);
  delay(50);
  Serial.print("2 begin /");
  Serial2.begin(115200, SERIAL_8N1, 36, 26);
  delay(50);
  Serial.print("1 end /");
  //Serial1.end();
  delay(50);
  Serial.print("2 end /");
  //Serial2.end();
  Serial.print(" done \r\n");
}
Serial.print("done");
//*/
  delay(1);
  ++loopCount;
  updateTouch();
/*
  if (touchPoints)
  {
    M5.Lcd.setCursor(30, 30);
    M5.Lcd.printf("Convert X:%03d  Y:%03d", tp[0].x, tp[0].y);
    M5.Lcd.fillRect(tp[0].x-2, tp[0].y-2, 5, 5, random(65536));
  }
//*/

  if (justTouch)
  {
    if (tp[0].y > 200)
    { /// select function.
      uint32_t index = (tp[0].x / 40);
      if (prevSel != index && index < pageListMax)
      {
        currentSel = index;
        clickSound();
        M5.Lcd.pushImage(currentSel * 40 + 1, 208, 38, 28, (m5gfx::rgb565_t*)gImage_fun_sel + (currentSel * 38 * 28));
        if (prevSel < pageListMax)
        M5.Lcd.pushImage(prevSel * 40 + 1, 208, 38, 28, (m5gfx::rgb565_t*)gImage_fun_unsel + (prevSel * 38 * 28));
        prevSel = currentSel;

        selectedPage->end();
        selectedPage = pageList[currentSel];
        M5.Lcd.fillRect(17, 32, 286, 172, TFT_WHITE);
        selectedPage->setup();
        return;
      }
    }
    if ((tp[0].x - tp[0].y) > 240)
    {
      static constexpr int slide_back_color = 0xCE79;
      clickSound();
      M5.Lcd.pushImage(0, 0, 320, 240, (m5gfx::rgb565_t*)gImage_sleepBk);
      auto logo = (m5gfx::rgb565_t*)(M5.getBoard() == m5::board_t::board_M5StackCore2
                ? gImage_core2Tools
                : gImage_toughTools);
      M5.Lcd.pushImage(75, 26, 170, 26, logo);

      M5Canvas _canvas_base, _canvas_bk, _canvas_slide;
      _canvas_slide.createSprite(56, 56);
      _canvas_bk.createSprite(28, 56);
      _canvas_bk.pushImage(-32, -80, 320, 240, (m5gfx::rgb565_t*)gImage_sleepBk);

      float affine[6] = { 0.25f, 0.0f, 0.0f, 0.0f, 0.25f, 0.0f};

      /// make slider UI
      _canvas_base.setColorDepth(2);
      _canvas_base.createSprite(56*4, 56*4);
      _canvas_base.setPaletteColor(0, slide_back_color);
      _canvas_base.fillCircle(112, 112, 112, 1);
      _canvas_base.pushAffineWithAA(&_canvas_bk, affine, 0);
      _canvas_base.fillCircle(112, 112, 100, 3);
      _canvas_base.fillCircle(112, 112,  40, 1);
      _canvas_base.fillCircle(112, 112,  32, 3);
      _canvas_base.fillRect( 92, 112 - 40, 39, 24, 3);
      _canvas_base.fillRect(107, 112 - 50, 9, 45, 1);
      _canvas_base.pushAffineWithAA(&_canvas_slide, affine);
      _canvas_slide.fillArc(28, 28, 28, 29, 94, 266, slide_back_color);
      _canvas_base.deleteSprite();
      _canvas_base.deletePalette();
      _canvas_base.setColorDepth(16);
      _canvas_base.createSprite(256, 56);

      int slide_x = 0;
      int prev_x = -1;
      bool hold = false;
      for (;;)
      {
        if (prev_x != slide_x)
        {
          prev_x = slide_x;
          _canvas_base.pushImage(-32, -80, 320, 240, (m5gfx::rgb565_t*)gImage_sleepBk);
          _canvas_bk.pushSprite(&_canvas_base, 0, 0);
          _canvas_base.fillRect(28, 0, slide_x, 56, 0x555555u);
          _canvas_slide.pushSprite(&_canvas_base, slide_x, 0, slide_back_color);
          _canvas_base.pushSprite(&M5.Lcd, 32, 80);
        }
        if (!updateTouch())
        {
          hold = false;
        }
        else
        if (justTouch)
        {
          if (tp[0].x > 110 && tp[0].x < 220 && tp[0].y > 200 && tp[0].y < 240)
          {
            clickSound();
            drawBackground();
            break;
          }
          if (tp[0].y > 80 && tp[0].y < 136 && tp[0].x > 32 + slide_x && tp[0].x < 32 + 56 + slide_x)
          {
            clickSound();
            hold = true;
          }
        }
        if (hold)
        {
          slide_x = std::max(0, std::min(200, tp[0].x - 60));
        }
        else if (slide_x == 200)
        {
          esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // gpio39 == touch INT
          delay(100);
          M5.Lcd.fillScreen(TFT_BLACK);
          M5.Lcd.sleep();
          M5.Lcd.waitDisplay();
          esp_deep_sleep_start();
        }
        else
        {
          slide_x = slide_x * 7 >> 3;
        }
      }
    }
  }
  selectedPage->loop();
}
