# ESP8266 Devices Manager

A small ESP8266 project that turns the board into a Wi-Fi access point with a
web dashboard. You can see who's connected, give devices nicknames, kick them
for 10 minutes, or block them until you unblock.

Made by Avadhoot.

## Features

- Runs its own Wi-Fi network (SoftAP, up to 8 devices)
- PIN-protected web dashboard at `192.168.4.1`
- Shows IP and MAC of every connected device
- Guesses the device type from the MAC (Apple, Samsung, Xiaomi, etc.)
- Set a nickname per device
- Kick (10 min temporary block) and permanent block / unblock
- Blocked devices get removed again if they try to reconnect
- Dark mobile-friendly UI

## Hardware

- ESP8266 board (NodeMCU / Wemos D1 Mini)
- USB cable

## Setup

1. Install the ESP8266 board package in Arduino IDE.
2. Open `esp8266_wifi_manager.ino`.
3. Edit the settings at the top:
```cpp
   const char *ssid = "MyESP-AP";
   const char *pass = "changeme123";   // min 8 characters
   const char *adminPin = "0000";      // change this
```
4. Select your board and port, then upload.

## Usage

1. Connect your phone to the Wi-Fi network you named.
2. Open `http://192.168.4.1` in the browser.
3. Enter the PIN.
4. Use the buttons on each device card: **Kick Player**, **Block**, **Save** (nickname).

## Notes

- Blocking works by deauthenticating the client. The ESP8266 has no real MAC
  filter, so a blocked device may reconnect for a split second before it's
  removed again.
- Nicknames and the block list are stored in RAM, so they reset on reboot.
- The login is a simple cookie and the page is plain HTTP. Fine for a hobby
  project, not for anything serious.
- Only use this on a network you own.

## Tech

C++ (Arduino framework), ESP8266WiFi, ESP8266WebServer, HTML/CSS served from the board.

## License

MIT

## Disclaimer

Built for learning and testing on your own network. Don't use it on networks or devices you don't own.