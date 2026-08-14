#pragma once

#include "main.hpp"

#include <memory>
#include <esp_now.h>
#include <esp_idf_version.h>
#if !M5TOOLS_TARGET_ESP32C5
#include <BluetoothSerial.h>
#endif
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#if __has_include(<core_version.h>)
#include <core_version.h>
#endif

#define M5TOOLS_HAS_CLASSIC_BT (!M5TOOLS_TARGET_ESP32C5)

#if M5TOOLS_HAS_CLASSIC_BT
BluetoothSerial SerialBT;
#endif

extern const unsigned char gImage_uartBk[];
extern const unsigned char gImage_uartBps[];
extern const unsigned char gImage_uartPort[];
extern const unsigned char gImage_uartStopStart[];

static constexpr int bpsListMax = 12;
static constexpr int bpsUARTList[bpsListMax] = { 1200,2400,4800,9600,19200,38400,57600,115200,256000,512000,750000,921600 };

enum serialSource
{ ss_usb = 0
, ss_porta
, ss_portb
, ss_portc
, ss_rs485
, ss_ble
, ss_bt
, ss_espnow
};
// 0:USB / 1:PortA / 2:PortB / 3:PortC / 4:RS485 / 5:BLE / 6:BT / 7:ESPNOW

// ボードで使えるソースのみを列挙する (表示順 = この並び順)。
// ToughC5 では PortA が内部 I2C バスと共用のため UART 用途から除外し、
// Classic BT 非搭載のため BT も除外する。USB は HWCDC 経由で扱う。
#if M5TOOLS_TARGET_ESP32C5
static constexpr int8_t sourceIdList[] = { ss_usb, ss_portb, ss_portc, ss_rs485, ss_ble, ss_espnow };
#else
static constexpr int8_t sourceIdList[] = { ss_usb, ss_porta, ss_portb, ss_portc, ss_rs485, ss_ble, ss_bt, ss_espnow };
#endif
static constexpr int sourceCount = sizeof(sourceIdList);

// UART ピンの取得。ポート類は M5Unified のピンテーブルから引く (tx=pin2 / rx=pin1)。
// RS485 と旧機種の USB (UART0 ブリッジ) はピンテーブルに項目が無いため直値。
static void getSourcePins(int src, int8_t& tx, int8_t& rx)
{
  tx = -1; rx = -1;
  switch (src)
  {
#if M5TOOLS_TARGET_ESP32C5
  case ss_rs485: tx = 24; rx = 23; return;
#else
  case ss_usb:   tx =  1; rx =  3; return;
  case ss_rs485: tx = 19; rx = 27; return;
  case ss_porta: tx = M5.getPin(m5::pin_name_t::port_a_pin2); rx = M5.getPin(m5::pin_name_t::port_a_pin1); return;
#endif
  case ss_portb: tx = M5.getPin(m5::pin_name_t::port_b_pin2); rx = M5.getPin(m5::pin_name_t::port_b_pin1); return;
  case ss_portc: tx = M5.getPin(m5::pin_name_t::port_c_txd ); rx = M5.getPin(m5::pin_name_t::port_c_rxd ); return;
  default: return;
  }
}

// ボーレート設定が意味を持つソースか (HWCDC 経由の USB はボーレート不要)
static bool sourceUsesBaud(int src)
{
#if M5TOOLS_TARGET_ESP32C5
  if (src == ss_usb) { return false; }
#endif
  return src < ss_ble;
}

class ringbuf_t
{
public:
  virtual ~ringbuf_t(void)
  {
    release();
  }
  void release(void)
  {
    if (_buffer)
    {
      free(_buffer);
      _buffer = nullptr;
      _buflen = 0;
    }
  }
  void init(size_t buflen)
  {
    release();
    _buflen = buflen;
    _buffer = (uint8_t*)malloc(buflen);
    _writeindex = 0;
    _readindex  = 0;
  }

  size_t available(void)
  {
    return (_writeindex - _readindex) & (_buflen - 1);
  }

  void read(uint8_t* buf, size_t len)
  {
    do
    {
      size_t l = std::min(len, _buflen - _readindex);
      memcpy(buf, &_buffer[_readindex], l);
      _readindex += l;
      if (_readindex == _buflen)
      {
        _readindex = 0;
        buf += l;
      }
      len -= l;
    } while (len);
  }

  void write(const uint8_t* buf, size_t len)
  {
    do
    {
      size_t l = std::min(len, _buflen - _writeindex);
      memcpy(&_buffer[_writeindex], buf, l);
      _writeindex += l;
      if (_writeindex == _buflen)
      {
        _writeindex = 0;
        buf += l;
      }
      len -= l;
    } while (len);
  }

private:
  uint8_t* _buffer = nullptr;
  size_t _buflen;
  size_t _writeindex;
  size_t _readindex;
};

ringbuf_t _ringbuf_espnow;
ringbuf_t _ringbuf_ble;
bool deviceConnected = false;
bool oldDeviceConnected = false;

struct PageUART : public PageBase
{
  static constexpr int bpsX = 211;
  static constexpr int bpsWidth = 64;
  static constexpr int bpsHeight = 18;
  static constexpr int sourceX = 104;
  static constexpr int sourceWidth = 98;
  static constexpr int sourceHeight = 18;
  static constexpr int colorLogBk = 0xE73C;

  M5Canvas _canvas_source;

  void drawBps(int y, int bpsIdx, bool visible, bool& prevVisible)
  {
    if (prevVisible != visible)
    {
      prevVisible = visible;
      int x = bpsX;
      int tx = bpsX + bpsWidth;
      if (visible) std::swap(x, tx);
      M5.Lcd.setClipRect(bpsX, y, bpsWidth, bpsHeight);
      do
      {
        x = x + (x < tx ? 1 : -1);
        M5.Lcd.pushImage(x, y, bpsWidth, bpsHeight, (m5gfx::rgb565_t*)gImage_uartBps + (bpsIdx * bpsWidth * bpsHeight));
        delay(1);
      } while (x != tx);
      M5.Lcd.clearClipRect();
    }
    if (visible)
    {
      M5.Lcd.pushImage(bpsX, y, bpsWidth, bpsHeight, (m5gfx::rgb565_t*)gImage_uartBps + (bpsIdx * bpsWidth * bpsHeight));
    }
    else
    {
      M5.Lcd.fillRect(bpsX, y, bpsWidth, bpsHeight, TFT_WHITE);
    }
  }

  void drawBps(void)
  {
    drawBps( 98, bpsUART1, sourceUsesBaud(sourceIdList[sourceUART1]), visibleBps1);
    drawBps(120, bpsUART2, sourceUsesBaud(sourceIdList[sourceUART2]), visibleBps2);

    // if (sourceUART2 >= ss_ble)
    // {
    //   M5.Lcd.fillRect(bpsX, 120, bpsWidth, bpsHeight, TFT_WHITE);
    // }
    // else
    // {
    //   M5.Lcd.pushImage(bpsX, 120, bpsWidth, bpsHeight, (m5gfx::rgb565_t*)gImage_uartBps + (bpsUART2 * bpsWidth * bpsHeight));
    // }
  }
  void setup(void) override
  {
    static constexpr char format[] = "%02x%02x%02x%02x%02x%02x";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    _canvas_source.setPsram(true);
    _canvas_source.createSprite(sourceWidth, sourceHeight * sourceCount);
    _canvas_source.setTextSize(1,2);
    _canvas_source.setTextColor(TFT_BLACK, TFT_WHITE);
    for (int i = 0; i < sourceCount; ++i)
    { /// 画像素材から有効なソースの行だけを抜き出して並べる
      int id = sourceIdList[i];
      _canvas_source.pushImage(0, i * sourceHeight, sourceWidth, sourceHeight, (m5gfx::swap565_t*)gImage_uartPort + id * sourceWidth * sourceHeight);
      if (id == ss_ble || id == ss_bt || id == ss_espnow)
      {
        _canvas_source.setCursor(24, sourceHeight * i + 2);
        _canvas_source.printf(format, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      }
    }

    M5.Lcd.pushImage(17, 34, 286, 168 , (m5gfx::rgb565_t*)gImage_uartBk);
    M5.Lcd.pushImage(sourceX,  98, sourceWidth, sourceHeight, (m5gfx::rgb565_t*)_canvas_source.getBuffer() + (sourceUART1 * sourceWidth * sourceHeight));
    M5.Lcd.pushImage(sourceX, 120, sourceWidth, sourceHeight, (m5gfx::rgb565_t*)_canvas_source.getBuffer() + (sourceUART2 * sourceWidth * sourceHeight));
    drawBps();
    setEnable(false);
    serialMon1.init( 38);
    serialMon2.init(140);
  }
  void end(void) override
  {
    serialMon1.release();
    serialMon2.release();
    _seri1.reset(nullptr);
    _seri2.reset(nullptr);
    _canvas_source.deletePalette();
    _canvas_source.deleteSprite();
  }
  void loop(void) override
  {
    if (enableUART)
    {
      if (justTouch)
      {
        if ((tp[0].x > 20) && (tp[0].x < sourceX) && (tp[0].y > 98) && (tp[0].y < 136))
        {
          clickSound();
          setEnable(false);
          return;
        }
      }
      bool s1, s2;
      do
      {
        s1 = serialMon1.update();
        s2 = serialMon2.update();
      } while (s1 || s2);
    }
    else
    {
      if (justTouch)
      {
        if ((tp[0].y > 118 - 56) && (tp[0].y < 118 + 56))
        {
          if (tp[0].x > bpsX)
          {
            if (tp[0].y < 118)
            {
              if (visibleBps1)
              {
                flickSelect((m5gfx::rgb565_t*)gImage_uartBps, bpsUART1, bpsX, 98, bpsWidth, bpsHeight, bpsListMax);
              }
            }
            else
            {
              if (visibleBps2)
              {
                flickSelect((m5gfx::rgb565_t*)gImage_uartBps, bpsUART2, bpsX, 120, bpsWidth, bpsHeight, bpsListMax);
              }
            }
          }
          else
          if (tp[0].x > sourceX)
          {
            if (tp[0].y < 118)
            {
              flickSelect((m5gfx::rgb565_t*)_canvas_source.getBuffer(), sourceUART1, sourceX, 98, sourceWidth, sourceHeight, sourceCount, sourceUART2);
            }
            else
            {
              flickSelect((m5gfx::rgb565_t*)_canvas_source.getBuffer(), sourceUART2, sourceX, 120, sourceWidth, sourceHeight, sourceCount, sourceUART1);
            }
            drawBps();
          }
          else
          {
            if (sourceUART1 != sourceUART2)
            {
              clickSound();
              M5.Lcd.fillRect(20,  36, 280, 60, colorLogBk);
              M5.Lcd.fillRect(20, 140, 280, 60, colorLogBk);
              setEnable(true);
            }
            else
            {
              errorSound();
            }
          }
        }
      }
    }
  }

private:

  struct ISerial
  {
    virtual ~ISerial() { release(); }
    virtual void release(void) {};
    virtual int available(void) = 0;
    virtual void read(uint8_t*, size_t) = 0;
    virtual void write(const uint8_t*, size_t) = 0;
  };

  struct HwSerial : public ISerial
  {
    HwSerial(HardwareSerial* seri, int baudrate, int rx, int tx)
     : _seri { seri }
    {
      if (rx >= 0 && tx >= 0)
      {
        seri->begin(baudrate, SERIAL_8N1, rx, tx);
      }
    }
    void release(void) override
    {
      _seri->end();
    }
    int available(void) override
    {
      return _seri->available();
    }
    void read(uint8_t* buf, size_t len) override
    {
#if defined ( ARDUINO_ESP32_RELEASE_1_0_4 )
      do
      {
        *buf++ = _seri->read();
      } while (--len);
#else
      _seri->read(buf, len);
#endif
    }
    void write(const uint8_t* buf, size_t len) override
    {
      _seri->write(buf, len);
    }
  private:
    HardwareSerial* _seri;
  };

  struct NullSerial : public ISerial
  {
    int available(void) override { return 0; }
    void read(uint8_t*, size_t) override {}
    void write(const uint8_t*, size_t) override {}
  };

#if M5TOOLS_TARGET_ESP32C5
  /// USB は USB-Serial-JTAG (HWCDC) でありピン指定の UART が存在しない
  struct UsbCdcSerial : public ISerial
  {
    int available(void) override
    {
      return Serial.available();
    }
    void read(uint8_t* buf, size_t len) override
    {
      Serial.readBytes(buf, len);
    }
    void write(const uint8_t* buf, size_t len) override
    {
      Serial.write(buf, len);
    }
  };
#endif

  struct ESPNOWSerial : public ISerial
  {
    esp_now_peer_info_t slave;

    static void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len)
    {
      _ringbuf_espnow.write(data, len);
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void OnDataRecvV5(const esp_now_recv_info_t *info, const uint8_t *data, int len)
    {
      (void)info;
      OnDataRecv(nullptr, data, len);
    }
#endif

    ESPNOWSerial(void)
    {
      _ringbuf_espnow.init(512);
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      if (esp_now_init() == ESP_OK) { ESP_LOGI("main", "ESPNow Init Success"); }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
      esp_now_register_recv_cb(OnDataRecvV5);
#else
      esp_now_register_recv_cb(OnDataRecv);
#endif

      memset(&slave, 0, sizeof(slave));
      memset(slave.peer_addr, 0xFF, 6);
      slave.channel = 1;
      if (!esp_now_is_peer_exist((const uint8_t*)slave.peer_addr)) {
        esp_now_add_peer((const esp_now_peer_info_t*)&slave);
      }
    }
    void release(void) override
    {
      esp_now_del_peer(slave.peer_addr);
      esp_now_unregister_recv_cb();
      esp_now_deinit();
      WiFi.disconnect(true);
      _ringbuf_espnow.release();
    }

    int available(void) override
    {
      return _ringbuf_espnow.available();
    }

    void read(uint8_t* buf, size_t len) override
    {
      _ringbuf_espnow.read(buf, len);
    }

    void write(const uint8_t* buf, size_t len) override
    {
      esp_now_send(slave.peer_addr, buf, len);
    }
  };

  struct BTSerial : public ISerial
  {
    BTSerial()
    {
#if M5TOOLS_HAS_CLASSIC_BT
      SerialBT.begin();
#endif
    }
    void release(void) override
    {
#if M5TOOLS_HAS_CLASSIC_BT
      SerialBT.disconnect();
      SerialBT.end();
#endif
    }
    int available(void) override
    {
#if M5TOOLS_HAS_CLASSIC_BT
      return SerialBT.available();
#else
      return 0;
#endif
    }
    void read(uint8_t* buf, size_t len) override
    {
#if M5TOOLS_HAS_CLASSIC_BT
      SerialBT.readBytes(buf, len);
#else
      (void)buf;
      (void)len;
#endif
    }
    void write(const uint8_t* buf, size_t len) override
    {
#if M5TOOLS_HAS_CLASSIC_BT
      SerialBT.write(buf, len);
      SerialBT.flush();
#else
      (void)buf;
      (void)len;
#endif
    }
  };

  struct BLESerial : public ISerial
  {
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
uint8_t txValue = 0;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      auto rxValue = pCharacteristic->getValue();
      size_t len = rxValue.length();
      if (len)
      {
        _ringbuf_ble.write((const uint8_t*)rxValue.c_str(), len);
      }
    }
};

    BLESerial()
    {
      // Create the BLE Device
      BLEDevice::init("UART Service");

      // Create the BLE Server
      pServer = BLEDevice::createServer();
      pServer->setCallbacks(new MyServerCallbacks());

      // Create the BLE Service
      BLEService *pService = pServer->createService(SERVICE_UUID);

      // Create a BLE Characteristic
      pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
                          
      pTxCharacteristic->addDescriptor(new BLE2902());

      BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_RX,
                          BLECharacteristic::PROPERTY_WRITE
                        );

      pRxCharacteristic->setCallbacks(new MyCallbacks());

      // Start the service
      pService->start();

      // Start advertising
      pServer->getAdvertising()->start();
      // Serial.println("Waiting a client connection to notify...");
      
      _ringbuf_ble.init(256);
    }

    void release(void) override
    {
      _ringbuf_ble.release();
    }

    int available(void) override
    {
      if (oldDeviceConnected != deviceConnected) {
        oldDeviceConnected = deviceConnected;
        // disconnecting
        if (!deviceConnected) {
            // delay(500); // give the bluetooth stack the chance to get things ready
            pServer->startAdvertising(); // restart advertising
            // Serial.println("start advertising");
        }
        else
        { // connecting
        // do stuff here on connecting
        }
      }
      return _ringbuf_ble.available();
      // return blespp_buflen[blespp_readindex];
    }

    void read(uint8_t* buf, size_t len) override
    {
      _ringbuf_ble.read(buf, len);
    }

    void write(const uint8_t* buf, size_t len) override
    {
      if (!deviceConnected) { return; }

      do
      {
        size_t sendlen = std::min<size_t>(20u, len);
        pTxCharacteristic->setValue((uint8_t*)buf, len);
        pTxCharacteristic->notify();
        buf += sendlen;
        len -= sendlen;
      } while (len);
    }
  };

  struct serial_monitor_t
  {
    void init(int ypos)
    {
      _ypos = ypos;
      _canvas.setColorDepth(8);
      _canvas.createSprite(280, 58);
      _canvas.createPalette();
      _canvas.setPaletteColor(1, colorLogBk);
      _canvas.setTextColor(0, 1);
    }
    void release(void)
    {
      _canvas.deleteSprite();
    }
    void setBridge(ISerial* in, ISerial* out)
    {
      _seri_in = in;
      _seri_out = out;
      _xcount = 0;
      _canvas.clear(1);
    }
    bool update(void)
    {
      static constexpr char hex[] = "0123456789ABCDEF";
      static constexpr int bufferlen = ESP_NOW_MAX_DATA_LEN;

      int len = _seri_in->available();
      if (len)
      {
        uint8_t buf[bufferlen];
        len = std::min(len, bufferlen);
        _seri_in->read(buf, len);
        _seri_out->write(buf, len);
        for (int i = 0; i < len; ++i)
        {
          if (_xcount == 0)
          {
            _canvas.scroll(0, - 10);
          }
          int x = _xcount * 14 + (_xcount >> 2);
          _canvas.drawChar(hex[buf[i] >> 4], x  , 51);
          _canvas.drawChar(hex[buf[i] & 15], x+6, 51);
          _canvas.drawChar(buf[i], 181 + _xcount * 8 + ((_xcount >> 2) << 1), 51);

          ++_xcount;
          if (_xcount == 12)
          {
            _xcount = 0;
          }
        }
        _mod = true;
        return true;
      }

      if (_mod)
      {
        _mod = false;
        _canvas.pushSprite(&M5.Lcd, 20, _ypos);
      }
      return false;
    }

  private:
    int _xcount;
    int _ypos;
    bool _mod;
    ISerial* _seri_in;
    ISerial* _seri_out;
    M5Canvas _canvas;
  };

  ISerial* createSerial(int id, HardwareSerial* hws, int baudrate)
  {
    switch (id)
    {
    case ss_espnow: return new ESPNOWSerial();
    case ss_ble:    return new BLESerial();
#if M5TOOLS_HAS_CLASSIC_BT
    case ss_bt:     return new BTSerial();
#endif
#if M5TOOLS_TARGET_ESP32C5
    case ss_usb:    return new UsbCdcSerial();
#endif
    default:
      {
        int8_t tx, rx;
        getSourcePins(id, tx, rx);
        if (tx >= 0 && rx >= 0)
        {
#if !M5TOOLS_TARGET_ESP32C5
          /// USB はコンソールが保持している UART0 をそのまま使う
          /// (core 3.x では別 UART へ TX ピンを付け替えても奪取できない)
          if (id == ss_usb) { hws = &Serial0; }
#endif
          return new HwSerial(hws, baudrate, rx, tx);
        }
        return new NullSerial();
      }
    }
  }

  serial_monitor_t serialMon1;
  serial_monitor_t serialMon2;
  std::unique_ptr<ISerial> _seri1;
  std::unique_ptr<ISerial> _seri2;
  int bpsUART1 = 7; // 115200
  int bpsUART2 = 7; // 115200
  int sourceUART1 = 0; // sourceIdList の表示 index
  int sourceUART2 = 1;
  bool visibleBps1 = true;
  bool visibleBps2 = true;
  bool enableUART = false;

  void setEnable(bool enable)
  {
    // M5.Lcd.pushImage(46,88,60,60, (m5gfx::rgb565_t*)( enable ? gImage_UARTon : gImage_UARToff));
    M5.Lcd.pushImage(18, 101, 80, 34, (m5gfx::rgb565_t*)gImage_uartStopStart + (enable * 80 * 34));
    if (enableUART != enable)
    {
      enableUART = enable;
      if (enable)
      {
        /// C5 は HP UART が 2 本 (Serial2 は LP UART でピン固定のため使えない)。
        /// コンソールは USB-JTAG なので UART0 (Serial0) が空いている。
#if M5TOOLS_TARGET_ESP32C5
        auto& hws2 = Serial0;
#else
        auto& hws2 = Serial2;
#endif
        _seri1.reset(createSerial(sourceIdList[sourceUART1], &Serial1, bpsUARTList[bpsUART1]));
        _seri2.reset(createSerial(sourceIdList[sourceUART2], &hws2  , bpsUARTList[bpsUART2]));

        serialMon1.setBridge(_seri1.get(), _seri2.get());
        serialMon2.setBridge(_seri2.get(), _seri1.get());
      }
      else
      {
        serialMon1.setBridge(nullptr, nullptr);
        serialMon2.setBridge(nullptr, nullptr);

        _seri1.reset(nullptr);
        _seri2.reset(nullptr);
        // if (_seri1.get()) { _seri1->release(); }
        // if (_seri2.get()) { _seri2->release(); }
        Serial.begin(115200);
      }
    }
  }

  void flickSelect(m5gfx::rgb565_t* srcimg, int &source, int x, int y, int width, int height, int itemCount, int noselectindex = -1)
  {
    const int totalHeight = height * itemCount;

    int prev_source = -1;
    int pos = source * height;
    M5.Lcd.setClipRect(x, y, width, height);
    while (updateTouch())
    {
      pos -= flickDiffY;
      flickDiffY = 0;
      if ((uint32_t)pos >= totalHeight)
      {
        pos += (pos < 0) ? totalHeight : -totalHeight;
      }
      M5.Lcd.pushImage(x, y - pos              , width, totalHeight, srcimg);
      M5.Lcd.pushImage(x, y - pos + totalHeight, width, totalHeight, srcimg);

      source = ((pos + (height >> 1)) / height) % itemCount;
      if (source == noselectindex)
      {
        source += (source * height <= pos && pos < (source + 1) * height) ? 1 : -1;
        source = (source + itemCount) % itemCount;
      }
      if (prev_source != source)
      {
        prev_source = source;
        clickSound();
      }
    }

    int py = source * height;
    if (abs(pos - py) > abs(pos - (py + totalHeight)))
    {
      py += totalHeight;
    }

    int add = (pos < py) ? 1 : -1; 

    while (pos != py)
    {
      pos += add;
      M5.Lcd.pushImage(x, y - pos              , width, totalHeight, srcimg);
      M5.Lcd.pushImage(x, y - pos + totalHeight, width, totalHeight, srcimg);
      delay(10);
    }
    M5.Lcd.clearClipRect();
  }

};
