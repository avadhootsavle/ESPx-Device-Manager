# ESP32 Devices Manager

A small ESP32 project that turns the board into a Wi-Fi access point with a
web dashboard. You can see who's connected, give devices nicknames, kick them
for 10 minutes, or block them until you unblock.

Originally built on the ESP8266, now ported to the ESP32 for more clients,
more power and more headroom.

Made by Avadhoot.

## Features

- Runs its own Wi-Fi network (SoftAP, up to 10 devices on ESP32)
- PIN-protected web dashboard at `192.168.4.1`
- Shows IP and MAC of every connected device
- Guesses the device type from the MAC (Apple, Samsung, Xiaomi, etc.)
- Set a nickname per device
- Kick (10 min temporary block) and permanent block / unblock
- Blocked devices get removed again if they try to reconnect
- Dark mobile-friendly UI

## Why ESP32 instead of ESP8266?

The first version ran on the ESP8266. It worked, but it has limits that
showed up quickly once more devices joined. Moving to the ESP32 fixes most of them.

| | ESP8266 | ESP32 |
|---|---|---|
| Max SoftAP clients | 8 | **10** |
| CPU | Single core, 80 / 160 MHz | **Dual core, up to 240 MHz** |
| RAM | ~80 KB usable | **~520 KB SRAM** |
| Max TX power | 20.5 dBm | 19.5 dBm (more stable under load) |
| Wi-Fi | 802.11 b/g/n | 802.11 b/g/n, WPA3 support in newer cores |
| Bluetooth | None | **Classic + BLE** |
| ADC | 1 pin, 10-bit | **Up to 18 channels, 12-bit** |
| GPIO | Very limited (~11 usable) | **Many more (~25+ usable)** |
| Hardware crypto | Minimal | **AES, SHA, RSA, RNG accelerators** |
| Extras | Basic | Touch pins, DAC, hall sensor, RTC memory, better deep sleep |
| Web server under load | Slows down and can stall with many clients | **Much smoother, since the web server and Wi-Fi can use separate cores** |

### Problems with the ESP8266 (why it was replaced)

- Only **8 clients** at once, so the AP fills up fast
- Single core and low RAM, so the dashboard can lag or crash when many devices
  are connected and the page is large
- Heap fragmentation: building big HTML `String`s can run it out of memory
- Only one ADC pin and few GPIOs, which limits future add-ons (sensors, LEDs, displays)
- No Bluetooth
- Weaker hardware security and crypto
- Older SDK with fewer updates and a smaller community going forward

### Advantages of the ESP32 (what this version gets)

- **10 clients** instead of 8
- Dual core 240 MHz CPU, so the web dashboard and Wi-Fi handling don't fight each other
- Around 6x more RAM, so bigger pages and bigger device/block lists are no problem
- Lots more GPIOs and ADC channels for adding hardware later (status LEDs, OLED, buzzer, buttons)
- Built-in Bluetooth for future features
- Hardware crypto, so it's better suited if HTTPS or stronger auth is added later
- Actively developed ESP-IDF / Arduino core, so it's more future proof

## Hardware

- ESP32 board (ESP32 DevKit V1 or similar)
- USB cable

## Setup

1. Install the **ESP32 board package (core 3.x or newer)** in Arduino IDE
   (Boards Manager → "esp32" by Espressif).
2. Open `esp32_ap_panel.ino`.
3. Edit the settings at the top:
```cpp
   const char *ssid = "MyESP-AP";
   const char *pass = "changeme123";   // min 8 characters
   const char *adminPin = "0000";      // change this
   const int maxClients = 10;          // ESP32 supports up to 10
```
4. Select **ESP32 Dev Module** (or your exact board) and the port, then upload.

## Usage

1. Connect your phone to the Wi-Fi network you named.
2. Open `http://192.168.4.1` in the browser.
3. Enter the PIN.
4. Use the buttons on each device card: **Kick Player**, **Block**, **Save** (nickname).

## Migrating from the ESP8266 version

If you're looking at the old code, these are the main changes:

- `ESP8266WiFi.h` / `ESP8266WebServer.h` → `WiFi.h` / `WebServer.h`
- `wifi_softap_deauth(mac)` → look up the AID, then `esp_wifi_deauth_sta(aid)`
- `wifi_softap_get_station_info()` → `esp_wifi_ap_get_sta_list()` + `esp_netif_get_sta_list()`
- `WiFiEventHandler` → `WiFi.onEvent(..., ARDUINO_EVENT_WIFI_AP_STACONNECTED)`
- `system_update_cpu_freq(160)` → `setCpuFrequencyMhz(240)`
- `WiFi.setOutputPower(20.5)` → `WiFi.setTxPower(WIFI_POWER_19_5dBm)`

## Notes

- Blocking works by deauthenticating the client. The SoftAP has no real MAC
  filter, so a blocked device may reconnect for a split second before it's
  removed again (the loop checks every 200 ms).
- Nicknames and the block list are stored in RAM, so they reset on reboot.
- The login is a simple cookie and the page is plain HTTP. Fine for a hobby
  project, not for anything serious.
- The Wi-Fi event callback runs in a different task from `loop()`. Fine for this
  project, but add a mutex if you extend the block list logic.
- Only use this on a network you own.

## Tech

C++ (Arduino framework), WiFi, WebServer, ESP-IDF Wi-Fi/netif APIs, HTML/CSS served from the board.

## License

MIT

## Disclaimer

Built for learning and testing on your own network. Don't use it on networks or devices you don't own.