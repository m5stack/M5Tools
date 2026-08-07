# M5Tools
### M5Stack Tough and Core2 tools .

# Support framework
 - Arduino for ESP32 1.0.6
 - PlatformIO espressif32 4.4.0 / Arduino for ESP32 2.0.3
 - PlatformIO pioarduino espressif32 / Arduino for ESP32 3.x for ESP32-C5

# PlatformIO build

```
pio run -e m5tools_core2_tough
pio run -e m5tools_tough_c5
```

The `m5tools_core2_tough` environment uses the `m5stack-core2` board definition
and builds the same firmware for M5Stack Core2 and M5Stack Tough.

The `m5tools_tough_c5` environment uses the pioarduino ESP32-C5 platform and
the `develop` branches of M5Unified and M5GFX. It uses a no-OTA 3MB application
partition because the firmware does not fit in the default 4MB board app slot.

On ESP32-C5, classic Bluetooth and DAC GPIO output are not available and are
disabled. Click and error sounds are played through `M5.Speaker`.

# Support device
 - M5Stack Core2 / Tough
 - M5Stack ToughC5

# License
 - [MIT](LICENSE)
