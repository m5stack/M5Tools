#pragma once

#include "main.hpp"

#include <memory>
#include <esp_now.h>
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#if __has_include(<core_version.h>)
#include <core_version.h>
#endif

BluetoothSerial SerialBT;

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
static constexpr int serialSourceMax = 8;
static constexpr int8_t sourceUARTList[serialSourceMax][2] = { { 1, 3 }, { 32, 33 }, { 26, 36 }, { 14, 13 }, { 19, 27 }, {-1, -1}, {-1, -1} }; // tx,rx

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
    drawBps( 98, bpsUART1, sourceUART1 < ss_ble, visibleBps1);
    drawBps(120, bpsUART2, sourceUART2 < ss_ble, visibleBps2);

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
    esp_read_mac(mac, ESP_MAC_BT);
    _canvas_source.setPsram(true);
    _canvas_source.createSprite(sourceWidth, sourceHeight * serialSourceMax);
    _canvas_source.pushImage(0, 0, sourceWidth, sourceHeight * serialSourceMax, (m5gfx::swap565_t*)gImage_uartPort);
    _canvas_source.setTextSize(1,2);
    _canvas_source.setTextColor(TFT_BLACK, TFT_WHITE);
    _canvas_source.setCursor(24, sourceHeight * ss_ble + 2);
    _canvas_source.printf(format, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _canvas_source.setCursor(24, sourceHeight * ss_bt + 2);
    _canvas_source.printf(format, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _canvas_source.setCursor(24, sourceHeight * ss_espnow + 2);
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    _canvas_source.printf(format, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

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
              flickSelect((m5gfx::rgb565_t*)_canvas_source.getBuffer(), sourceUART1, sourceX, 98, sourceWidth, sourceHeight, serialSourceMax, sourceUART2);
            }
            else
            {
              flickSelect((m5gfx::rgb565_t*)_canvas_source.getBuffer(), sourceUART2, sourceX, 120, sourceWidth, sourceHeight, serialSourceMax, sourceUART1);
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
      seri->begin(baudrate, SERIAL_8N1, rx, tx);
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

  struct ESPNOWSerial : public ISerial
  {
    esp_now_peer_info_t slave;

    static void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len)
    {
      _ringbuf_espnow.write(data, len);
    }

    ESPNOWSerial(void)
    {
      _ringbuf_espnow.init(512);
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
      SerialBT.begin();
    }
    void release(void) override
    {
      SerialBT.disconnect();
      SerialBT.end();
    }
    int available(void) override
    {
      return SerialBT.available();
    }
    void read(uint8_t* buf, size_t len) override
    {
      SerialBT.readBytes(buf, len);
    }
    void write(const uint8_t* buf, size_t len) override
    {
      SerialBT.write(buf, len);
      SerialBT.flush();
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
      std::string rxValue = pCharacteristic->getValue();
      size_t len = rxValue.size();
      if (len)
      {
        _ringbuf_ble.write((uint8_t*)rxValue.c_str(), len);
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

  serial_monitor_t serialMon1;
  serial_monitor_t serialMon2;
  std::unique_ptr<ISerial> _seri1;
  std::unique_ptr<ISerial> _seri2;
  int bpsUART1 = 7; // 115200
  int bpsUART2 = 7; // 115200
  int sourceUART1 = serialSource::ss_usb;
  int sourceUART2 = serialSource::ss_porta;
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
        switch (sourceUART1)
        {
        case ss_espnow:
          _seri1.reset(new ESPNOWSerial());
          break;
        case ss_bt:
          _seri1.reset(new BTSerial());
          break;
        case ss_ble:
          _seri1.reset(new BLESerial());
          break;
        default:
          _seri1.reset(new HwSerial(&Serial1, bpsUARTList[bpsUART1], sourceUARTList[sourceUART1][1], sourceUARTList[sourceUART1][0]));
          break;
        }

        switch (sourceUART2)
        {
        case ss_espnow:
          _seri2.reset(new ESPNOWSerial());
          break;
        case ss_bt:
          _seri2.reset(new BTSerial());
          break;
        case ss_ble:
          _seri2.reset(new BLESerial());
          break;
        default:
          _seri2.reset(new HwSerial(&Serial2, bpsUARTList[bpsUART2], sourceUARTList[sourceUART2][1], sourceUARTList[sourceUART2][0]));
          break;
        }
      // _seri_in->begin(bpsUARTList[bps], SERIAL_8N1, sourceUARTList[source][1], sourceUARTList[source][0]);

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
