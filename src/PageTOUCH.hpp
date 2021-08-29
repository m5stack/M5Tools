#pragma once

#include "main.hpp"

#include <esp_now.h>

static constexpr uint32_t dummy_prefix = 0xABEC1DEB;
static constexpr int packetlen = 4 + sizeof(m5gfx::touch_point_t);
static constexpr int peerMax = 8;
static constexpr int peerColor[] = { TFT_RED, TFT_DARKGREEN, TFT_DARKCYAN, TFT_PURPLE, TFT_OLIVE, TFT_BROWN, TFT_MAGENTA, TFT_LIGHTGRAY };

struct peer_touch_t
{
  m5gfx::touch_point_t tp;
  uint8_t mac[6];
  bool recv = false;
};
peer_touch_t peer[peerMax];
int peerCount = 0;

struct PageTOUCH : public PageBase
{
  m5gfx::touch_point_t last_tp;
  esp_now_peer_info_t slave;

  // 受信時コールバック
  static void OnDataRecv(const uint8_t *mac, const uint8_t *data, int data_len)
  {
    if (memcmp(data, &dummy_prefix, 4))
    {
      return;
    }
    size_t idx = 0;
    for (; idx < peerCount; ++idx)
    {
      if (0 == memcmp(mac, peer[idx].mac, 6)) break;
    }
    if (idx == peerCount)
    {
      if (idx == peerMax) return;
      ++peerCount;
      memcpy(peer[idx].mac, mac, 6);
    }
    peer[idx].tp = *(const m5gfx::touch_point_t*)&data[4];
    peer[idx].recv = true;
  //  ESP_LOGI("recv", "%x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  void startEspNow(void)
  {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK) { ESP_LOGI("main", "ESPNow Init Success"); }
    esp_now_register_recv_cb(OnDataRecv);

    memset(&slave, 0, sizeof(slave));
    memset(slave.peer_addr, 0xFF, 6);
    slave.channel = 1;
    if (!esp_now_is_peer_exist((const uint8_t*)slave.peer_addr)) {
      esp_now_add_peer((const esp_now_peer_info_t*)&slave);
    }
  }

  void setup(void) override
  {
    startEspNow();
    prevPeerCount = 0;
  }
  void loop(void) override
  {
    M5.Lcd.setClipRect(17, 32, 286, 172);
    if (touchPoints)
    {
      if (justTouch)
      {
        clickSound();
      }
      if (memcmp(&last_tp, tp, sizeof(m5gfx::touch_point_t)))
      {
        last_tp = tp[0];
        M5.Lcd.fillRect(tp[0].x - 1, tp[0].y - 1, 3, 3, TFT_BLUE);
        sendTouchPos(tp);
      }
    }
    if (prevPeerCount != peerCount)
    {
      M5.Lcd.setTextSize(1);
      M5.Lcd.setFont(&fonts::Font0);
      for (int idx = prevPeerCount; idx < peerCount; ++idx)
      {
        M5.Lcd.setCursor(17, 32 + idx * 8);
        M5.Lcd.setTextColor(peerColor[idx], TFT_WHITE);
        M5.Lcd.printf("%02x:%02x:%02x:%02x:%02x:%02x", peer[idx].mac[0], peer[idx].mac[1], peer[idx].mac[2], peer[idx].mac[3], peer[idx].mac[4], peer[idx].mac[5]);
      }
      prevPeerCount = peerCount;
    }
    for (int idx = 0; idx < peerCount; ++idx)
    {
      if (!peer[idx].recv) continue;
      peer[idx].recv = false;
      M5.Lcd.fillRect(peer[idx].tp.x - 1, peer[idx].tp.y - 1, 3, 3, peerColor[idx]);
    }
    M5.Lcd.clearClipRect();
  }
  void end(void) override
  {
    esp_now_del_peer(slave.peer_addr);
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    WiFi.disconnect(true);
  }

private:
  int prevPeerCount;

  void sendTouchPos(m5gfx::touch_point_t* tp)
  {
    auto buf = (uint8_t*)alloca(packetlen);
    memcpy(buf, &dummy_prefix, 4);
    memcpy(&buf[4], tp, sizeof(m5gfx::touch_point_t));
    esp_now_send(slave.peer_addr, buf, packetlen);
  }
};
