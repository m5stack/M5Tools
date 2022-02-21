
#include <M5Unified.h>

static constexpr int i2c_addr = 0x38;
static constexpr int i2c_freq = 400000;

#include "ft6336_fw_v17_app.h"
#define OLD_FIRMWARE_VERSION 16

#define FT6336U_ID         0x64
#define ID_G_CIPHER        0xa3
#define ID_G_FIRMID        0xa6
#define FT_REG_RESET_FW    0x07
#define FT_ERASE_APP_REG   0x61
#define FT_READ_ID_REG     0x90
#define FT_FW_START_REG    0xbf
#define FT_RST_CMD_REG2    0xbc
#define FT_UPGRADE_AA      0xAA
#define FT_UPGRADE_55      0x55
#define FT_FW_PKT_LEN       128
#define FT_FW_PKT_DLY_MS     10

#define DELAY_AA      100
#define DELAY_55       30
#define DELAY_READID  100

#define UPGRADE_ID_1  0x79
#define UPGRADE_ID_2  0x1C

#define LOG(fmt, arg...) ESP_LOGI("FT6336_FW", "[%s]: " fmt , __func__ , ## arg)
#define ERR(fmt, arg...) ESP_LOGE("FT6336_FW", "[%s]: " fmt , __func__ , ## arg)
#ifndef DEBUG
#define DBG(fmt, arg...) {}
#else
#define DBG(fmt, arg...) ESP_LOGD("FT6336_FW", "[%s]: " fmt "\n" , __func__ , ## arg)
#endif

static bool i2c_read(const uint8_t *wrbuf, int wrlen, uint8_t *rdbuf, int rdlen)
{
  if (wrlen > 0)
  {
    if (!M5.In_I2C.start(i2c_addr, false, i2c_freq)
     || !M5.In_I2C.write(wrbuf, wrlen)
     || !M5.In_I2C.restart(i2c_addr, true, i2c_freq))
    {
      ERR("I2C write error");
      return false;
    }
  }
  else
  {
    if (!M5.In_I2C.start(i2c_addr, true, i2c_freq))
    {
      ERR("I2C read error");
      return false;
    }
  }
  if (!M5.In_I2C.read(rdbuf, rdlen)
   || !M5.In_I2C.stop())
  {
    ERR("I2C read error: request:%d", rdlen);
    return false;
  }
  return true;
}

static bool i2c_write(const uint8_t *buf, int len)
{
  if (!M5.In_I2C.start(i2c_addr, false, i2c_freq)
   || !M5.In_I2C.write(buf, len)
   || !M5.In_I2C.stop())
  {
    ERR("I2C write error");
    return false;
  }
  return true;
}

static bool read_id(void)
{
  uint8_t reg_val[2] = {0};
  uint8_t packet_buf[4];

  delay(DELAY_READID);
  packet_buf[0] = FT_READ_ID_REG;
  packet_buf[1] = packet_buf[2] = packet_buf[3] = 0x00;

  i2c_read(packet_buf, 4, reg_val, 2);
  if (reg_val[0] != UPGRADE_ID_1
   || reg_val[1] != UPGRADE_ID_2) {
    ERR("READ-ID not ok: %x %x", reg_val[0], reg_val[1]);
    return false;
  }

  return true;
}

static void ft6x36_fw_upgrade(const uint8_t *data, uint32_t data_len)
{
  int i;
  uint8_t buf[6 + FT_FW_PKT_LEN];

  size_t retry = 16;
  do
  {
    LOG("Reset CTPM");
    buf[0] = FT_RST_CMD_REG2;
    buf[1] = FT_UPGRADE_AA;
    i2c_write(buf, 2);
    delay(DELAY_AA);
    buf[1] = FT_UPGRADE_55;
    i2c_write(buf, 2);
    delay(DELAY_55);

    LOG("Enter upgrade mode");
    buf[0] = FT_UPGRADE_55;
    buf[1] = FT_UPGRADE_AA;
    if (!i2c_write(buf, 2))
    {
      ERR("failed to enter upgrade mode");
      continue;
    }
    LOG("Check READ-ID");
  } while (!read_id() && --retry);

  if (retry == 0)
  {
    return;
  }

  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setCursor(0, 0);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString("Core2 Touch Firmware", 0, 0);
  M5.Display.fillRect(10, 112, M5.Display.width() - 20, 17, TFT_BLACK);
  M5.Display.fillCircle(                   10, 120, 8, TFT_BLACK);
  M5.Display.fillCircle( M5.Display.width() - 10, 120, 8, TFT_BLACK);


  LOG("Erase current app");
  M5.Display.drawString("Erase", 0, 28);
  buf[0] = FT_ERASE_APP_REG;
  if (!i2c_write(buf, 1))
  {
    ERR("Couldn't Erase");
    return;
  }

  for (i = 0; i < 200; ++i) {
    M5.Display.fillCircle( 10 + (M5.Display.width() - 20) * i / 200, 120, 4, TFT_BLUE );
    delay(5);
  }

  M5.Display.drawString("Write ", 0, 28);
  LOG("Write firmware to CTPM flash");
  buf[0] = FT_FW_START_REG;
  buf[1] = 0x00;
  bool fail = false;
  for (i = 0; i < data_len; i += FT_FW_PKT_LEN) {
    uint32_t length = data_len - i;
    if (length > FT_FW_PKT_LEN) { length = FT_FW_PKT_LEN; }

    buf[2] = (uint8_t) (i >> 8);
    buf[3] = (uint8_t) i;
    buf[4] = (uint8_t) (length >> 8);
    buf[5] = (uint8_t) length;
    memcpy(&buf[6], &data[i], length);
    if (!i2c_write(buf, length + 6))
    {
      fail = true;
      ERR("Couldn't Write");
      break;
    }
    delay(FT_FW_PKT_DLY_MS);
    M5.Display.fillCircle( 10 + (M5.Display.width() - 20) * i / data_len, 120, 4, TFT_GREEN );
  }
  if (fail)
  {
    ERR("Failed to flash FW");
    M5.Display.drawString("fail", 0, 56);
  }
  else
  {
    LOG("Success to flash FW");
    M5.Display.drawString("success", 0, 56);
  }
  delay(50);

  buf[0] = FT_REG_RESET_FW;
  i2c_write(buf, 1);
  delay(1000);

  M5.Display.clear();
}

void ft6336_fw_updater(void)
{
  uint8_t wbuf, rbuf;

  wbuf = ID_G_CIPHER;
  size_t retry = 32;
  do
  {
    if (i2c_read(&wbuf, 1, &rbuf, 1) && rbuf == FT6336U_ID) { break; }
    LOG("wait CTPM response");
    delay(128);
  } while (--retry);

  /* Get current firmware version */
  uint8_t fw_ver = 0;
  {
    wbuf = ID_G_FIRMID;
    i2c_read(&wbuf, 1, &fw_ver, 1);
    LOG("Firmware version: %d.0.0", fw_ver);
  }

  if (fw_ver == 0 || fw_ver == OLD_FIRMWARE_VERSION)
  {
    auto &bin = firmware_v17;
    LOG("FW length is %ld", sizeof(bin));
    ft6x36_fw_upgrade(bin, sizeof(bin));
  }
}
