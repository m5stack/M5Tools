#pragma once

#include <WiFi.h>
#include <M5Unified.h>

extern const unsigned char gWav_Click[];
extern const unsigned char gWav_Error[];

m5gfx::touch_point_t prev_tp[2];
m5gfx::touch_point_t tp[2];
uint32_t loopCount = 0;
int touchPoints, prev_touchPoints;
bool justTouch;
int flickDiffX, flickDiffY;


#define CONFIG_I2S_BCK_PIN 12
#define CONFIG_I2S_LRCK_PIN 0
#define CONFIG_I2S_DATA_PIN 2
#define CONFIG_I2S_DATA_IN_PIN 34

#define Speak_I2S_NUMBER I2S_NUM_0
#define MODE_MIC 0
#define MODE_SPK 1
#define I2S_DATA_LEN 60

void setSpeaker(int sampleRate = 16000)
{
  i2s_driver_uninstall(Speak_I2S_NUMBER);

  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = sampleRate,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ALL_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 2,
    .dma_buf_len          = I2S_DATA_LEN,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };

  ESP_LOGI("main", "i2s_driver_install:%d", i2s_driver_install(Speak_I2S_NUMBER, &i2s_config, 0, nullptr));

  i2s_pin_config_t tx_pin_config = {
    .bck_io_num     = CONFIG_I2S_BCK_PIN,
    .ws_io_num      = CONFIG_I2S_LRCK_PIN,
    .data_out_num   = CONFIG_I2S_DATA_PIN,
    .data_in_num    = CONFIG_I2S_DATA_IN_PIN,
  };
  ESP_LOGI("main", "i2s_set_pin:%d", i2s_set_pin(Speak_I2S_NUMBER, &tx_pin_config));
  ESP_LOGI("main", "i2s_set_clk:%d", i2s_set_clk(Speak_I2S_NUMBER, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO));
  ESP_LOGI("main", "i2s_zero_dma_buffer:%d", i2s_zero_dma_buffer(Speak_I2S_NUMBER));
}

struct sound_param_t
{
  xTaskHandle handle = nullptr;
  const uint8_t* data = nullptr;
  size_t len = 0;
  size_t rate = 0;
};
sound_param_t soundParam;

static void IRAM_ATTR soundTask(void* sound_param)
{
  auto param = (sound_param_t*)sound_param;
  param->rate = 16000;
  int prevSampleRate = 0;
  // I2S
  int16_t data[I2S_DATA_LEN];
  for (;;)
  {
    sound_param_t p = *param;
//    play(param->data, param->len, param->rate);

    if (prevSampleRate != p.rate)
    {
      prevSampleRate = p.rate;
      setSpeaker(p.rate);
      M5.Axp.bitOn(0x94, 0x04); // speaker on
    }

    // Write Speaker
    size_t bytes_written = 0;

    int index = 0;

  //  i2s_zero_dma_buffer(Speak_I2S_NUMBER);
    memset(data, 0, I2S_DATA_LEN << 1);
    for (int i = 0; i < 2; ++i)
    {
      i2s_write(Speak_I2S_NUMBER, data, I2S_DATA_LEN*2, &bytes_written, portMAX_DELAY);
    }
    for (int i = 0; i < p.len; i++)
    {
      int16_t val = p.data[i];
      data[index] = (val - 128) * 64;
      index += 1;
      if (I2S_DATA_LEN <= index)
      {
        index = 0;
        i2s_write(Speak_I2S_NUMBER, data, I2S_DATA_LEN*2, &bytes_written, portMAX_DELAY);
      }
    }
    memset(&data[index], 0, (I2S_DATA_LEN - index) * 2);
    i2s_write(Speak_I2S_NUMBER, data, I2S_DATA_LEN * 2, &bytes_written, portMAX_DELAY);
    if (index <= I2S_DATA_LEN)
    {
      memset(data, 0, index * 2);
    }

    for (int i = 0; i < 4; ++i)
    {
      i2s_write(Speak_I2S_NUMBER, data, I2S_DATA_LEN*2, &bytes_written, portMAX_DELAY);
    }
    ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
  }
}

void clickSound(void)
{
  // play(gWavClick, 112, 16000);
  //play(wav, 16538, 16000, 16);
  soundParam.data = gWav_Click;
  soundParam.len  = 112;
  soundParam.rate = 16000;

  // soundParam.data = himehinaWav;
  // soundParam.len  = 63667;
  // soundParam.rate = 16000;

  xTaskNotifyGive(soundParam.handle);
}

void errorSound(void)
{
  soundParam.data = gWav_Error;
  soundParam.len  = 3584;
  soundParam.rate = 16000;

  xTaskNotifyGive(soundParam.handle);
}

int updateTouch(void)
{
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

