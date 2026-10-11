#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

extern "C" {
  #include <user_interface.h>
  bool wifi_softap_deauth(uint8 mac[6]);
}

const char *ssid = "iPhone 18 Pro max";
const char *pass = "12345678";
const int channel = 6;
const int maxClients = 8;

const char *adminPin = "0000";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

struct Alias {
  uint8_t mac[6];
  char name[24];
};
#define MAX_ALIASES 16
Alias aliases[MAX_ALIASES];
int aliasCount = 0;
struct Blocked {
  uint8_t mac[6];
  unsigned long until;
};

#define MAX_BLOCKED 16
Blocked blocked[MAX_BLOCKED];
int blockedCount = 0;

WiFiEventHandler onConnect;

// ---------- helpers ----------
String macToString(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// takes "AA:BB:CC:DD:EE:FF" (or with %3A, dashes, spaces) and fills mac[]
bool stringToMac(String s, uint8_t *mac) {
  s.replace("%3A", "");
  s.replace("%3a", "");
  s.replace(":", "");
  s.replace("-", "");
  s.replace(" ", "");

  if (s.length() != 12) return false;

  for (int i = 0; i < 6; i++) {
    char hex[3] = { s[i * 2], s[i * 2 + 1], 0 };
    mac[i] = (uint8_t)strtol(hex, NULL, 16);
  }
  return true;
}

String guessVendor(const uint8_t *mac) {
  // phones use random mac now so this bit is set a lot
  if (mac[0] & 0x02) return "Smartphone (Private MAC)";

  if (mac[0] == 0x3C && mac[1] == 0xD9 && mac[2] == 0x2B) return "Apple iPhone/iPad";
  if (mac[0] == 0xAC && mac[1] == 0xBC && mac[2] == 0x32) return "Apple Device";
  if (mac[0] == 0xF0 && mac[1] == 0x18 && mac[2] == 0x98) return "Apple Device";
  if (mac[0] == 0x00 && mac[1] == 0x12 && mac[2] == 0x47) return "Samsung Galaxy";
  if (mac[0] == 0x18 && mac[1] == 0x3F && mac[2] == 0x47) return "Samsung Galaxy";
  if (mac[0] == 0x28 && mac[1] == 0x6C && mac[2] == 0x07) return "Xiaomi / Redmi";
  if (mac[0] == 0x64 && mac[1] == 0x09 && mac[2] == 0x80) return "Xiaomi Device";
  if (mac[0] == 0x94 && mac[1] == 0x65 && mac[2] == 0x2D) return "OnePlus Phone";
  if (mac[0] == 0x3C && mac[1] == 0x5A && mac[2] == 0x37) return "Google Pixel";

  return "Mobile / Laptop";
}

String getName(const uint8_t *mac, int num) {
  for (int i = 0; i < aliasCount; i++) {
    if (memcmp(aliases[i].mac, mac, 6) == 0 && strlen(aliases[i].name) > 0) {
      return String(aliases[i].name);
    }
  }
  return "Player #" + String(num) + " (" + guessVendor(mac) + ")";
}

void setAlias(const uint8_t *mac, const String &name) {
  // already has one? just update it
  for (int i = 0; i < aliasCount; i++) {
    if (memcmp(aliases[i].mac, mac, 6) == 0) {
      strncpy(aliases[i].name, name.c_str(), sizeof(aliases[i].name) - 1);
      aliases[i].name[sizeof(aliases[i].name) - 1] = 0;
      return;
    }
  }
  if (aliasCount < MAX_ALIASES) {
    memcpy(aliases[aliasCount].mac, mac, 6);
    strncpy(aliases[aliasCount].name, name.c_str(), sizeof(aliases[aliasCount].name) - 1);
    aliases[aliasCount].name[sizeof(aliases[aliasCount].name) - 1] = 0;
    aliasCount++;
  }
}
int findBlocked(const uint8_t *mac) {
  for (int i = 0; i < blockedCount; i++) {
    if (memcmp(blocked[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

void removeBlocked(int idx) {
  for (int j = idx; j < blockedCount - 1; j++) {
    blocked[j] = blocked[j + 1];
  }
  blockedCount--;
}

bool isBlocked(const uint8_t *mac) {
  int idx = findBlocked(mac);
  if (idx == -1) return false;

  // temp block over? remove it
  if (blocked[idx].until > 0 && millis() > blocked[idx].until) {
    removeBlocked(idx);
    return false;
  }
  return true;
}

void blockDevice(const uint8_t *mac, unsigned long ms = 0) {
  int idx = findBlocked(mac);
  unsigned long until = (ms > 0) ? (millis() + ms) : 0;

  if (idx != -1) {
    blocked[idx].until = until;
  } else if (blockedCount < MAX_BLOCKED) {
    memcpy(blocked[blockedCount].mac, mac, 6);
    blocked[blockedCount].until = until;
    blockedCount++;
  }

  wifi_softap_deauth((uint8_t *)mac);
}

void unblockDevice(const uint8_t *mac) {
  int idx = findBlocked(mac);
  if (idx != -1) removeBlocked(idx);
}

bool loggedIn() {
  if (server.hasHeader("Cookie")) {
    String c = server.header("Cookie");
    if (c.indexOf("auth=valid") != -1) return true;
  }
  return false;
}

String loginPage(bool wrong = false) {
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
    "<title>ESP8266</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;}"
    "body{background:#000;color:#fff;display:flex;align-items:center;justify-content:center;min-height:100vh;padding:20px;}"
    ".card{width:100%;max-width:360px;}"
    ".inp{width:100%;padding:14px;background:#141414;border:1px solid #282828;border-radius:8px;color:#fff;font-size:16px;outline:none;margin-bottom:14px;}"
    ".inp:focus{border-color:#fff;}"
    ".btn{width:100%;padding:14px;background:#fff;color:#000;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;}"
    ".err{color:#ff4444;font-size:13px;margin-bottom:14px;}"
    "</style></head><body>"
    "<div class='card'>");

  if (wrong) html += F("<div class='err'>Wrong PIN, try again.</div>");

  html += F("<form method='POST' action='/login'>"
    "<input class='inp' type='password' name='pin' placeholder='PIN' pattern='[0-9]*' inputmode='numeric' autofocus required>"
    "<button class='btn' type='submit'>Continue</button>"
    "</form></div></body></html>");
  return html;
}

String dashboardPage() {
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
    "<title>ESP8266</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;}"
    "body{background:#000;color:#fff;padding:24px 16px 80px;max-width:540px;margin:0 auto;}"
    ".header{display:flex;align-items:center;justify-content:space-between;padding-bottom:20px;border-bottom:1px solid #1a1a1a;margin-bottom:24px;}"
    ".title{font-size:18px;font-weight:600;}"
    ".sub{font-size:12px;color:#777;}"
    ".logout{color:#777;text-decoration:none;font-size:13px;border:1px solid #222;padding:6px 12px;border-radius:6px;}"
    ".stats{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:24px;}"
    ".stat{background:#0d0d0d;border:1px solid #1c1c1c;border-radius:10px;padding:14px;}"
    ".num{font-size:22px;font-weight:600;margin-bottom:2px;}"
    ".lbl{font-size:12px;color:#777;text-transform:uppercase;letter-spacing:0.5px;}"
    ".sec{font-size:14px;font-weight:600;color:#777;text-transform:uppercase;letter-spacing:0.8px;margin-bottom:12px;}"
    ".dev{background:#0d0d0d;border:1px solid #1c1c1c;border-radius:12px;padding:16px;margin-bottom:12px;}"
    ".dev-top{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:12px;}"
    ".dev-name{font-size:16px;font-weight:600;margin-bottom:3px;}"
    ".dev-meta{font-size:12px;color:#777;font-family:ui-monospace,Consolas,monospace;}"
    ".on{font-size:11px;font-weight:600;color:#00e676;background:rgba(0,230,118,0.1);padding:3px 8px;border-radius:4px;}"
    ".off{font-size:11px;font-weight:600;color:#ff5252;background:rgba(255,82,82,0.1);padding:3px 8px;border-radius:4px;}"
    ".rename{display:flex;gap:8px;margin-bottom:12px;}"
    ".rename input[type=text]{flex:1;background:#141414;border:1px solid #282828;border-radius:6px;padding:8px 10px;color:#fff;font-size:13px;outline:none;}"
    ".save{background:#222;color:#fff;border:none;border-radius:6px;padding:8px 12px;font-size:12px;font-weight:600;cursor:pointer;}"
    ".actions{display:flex;gap:8px;}"
    ".act{flex:1;padding:10px;border-radius:8px;font-size:13px;font-weight:600;text-align:center;text-decoration:none;display:inline-block;}"
    ".kick{background:#1c1c1c;color:#ff6b6b;border:1px solid #331f1f;}"
    ".block{background:#fff;color:#000;}"
    ".unblock{background:#1c1c1c;color:#00e676;border:1px solid #1f3325;}"
    ".empty{color:#555;font-size:14px;text-align:center;padding:36px 0;background:#0d0d0d;border-radius:12px;border:1px dashed #1f1f1f;}"
    ".bar{position:fixed;bottom:0;left:0;right:0;background:#000;border-top:1px solid #1a1a1a;padding:14px 20px;display:flex;justify-content:center;}"
    ".refresh{background:#fff;color:#000;font-size:14px;font-weight:600;padding:12px 24px;border-radius:24px;text-decoration:none;}"
    "</style></head><body>");

  html += F("<div class='header'><div>"
    "<div class='title'>ESP8266</div><div class='sub'>160 MHz &bull; Channel 6</div></div>"
    "<a class='logout' href='/logout'>Back</a></div>");

  int connected = WiFi.softAPgetStationNum();

  html += "<div class='stats'>"
    "<div class='stat'><div class='num'>" + String(connected) + "/" + String(maxClients) + "</div><div class='lbl'>Active Devices</div></div>"
    "<div class='stat'><div class='num'>" + String(blockedCount) + "</div><div class='lbl'>Blocked Devices</div></div></div>";

  html += F("<div class='sec'>Connected Devices</div>");

  struct station_info *st = wifi_softap_get_station_info();
  if (st == NULL) {
    html += F("<div class='empty'>No devices connected.</div>");
  } else {
    int n = 1;
    while (st != NULL) {
      String mac = macToString(st->bssid);
      IPAddress ip(st->ip.addr);
      String ipStr = (st->ip.addr == 0) ? "Assigning IP..." : ip.toString();
      String name = getName(st->bssid, n);

      html += "<div class='dev'>"
        "<div class='dev-top'><div>"
        "<div class='dev-name'>" + name + "</div>"
        "<div class='dev-meta'>IP: " + ipStr + " &bull; " + mac + "</div></div>"
        "<span class='on'>Online</span></div>";
      html += "<form class='rename' method='POST' action='/rename'>"
        "<input type='hidden' name='mac' value='" + mac + "'>"
        "<input type='text' name='name' placeholder='Set nickname...'>"
        "<button class='save' type='submit'>Save</button></form>";
      html += "<div class='actions'>"
        "<a class='act kick' href='/kick?mac=" + mac + "'>Kick Player</a>"
        "<a class='act block' href='/block?mac=" + mac + "'>Block</a></div></div>";

      n++;
      st = STAILQ_NEXT(st, next);
    }
    wifi_softap_free_station_info();
  }
  if (blockedCount > 0) {
    html += F("<div class='sec' style='margin-top:32px;'>Blocked Players</div>");
    for (int i = 0; i < blockedCount; i++) {
      String mac = macToString(blocked[i].mac);
      String name = getName(blocked[i].mac, i + 1);
      String status = (blocked[i].until > 0) ? "Kicked (Temp)" : "Blocked";

      html += "<div class='dev'>"
        "<div class='dev-top'><div>"
        "<div class='dev-name'>" + name + "</div>"
        "<div class='dev-meta'>" + mac + "</div></div>"
        "<span class='off'>" + status + "</span></div>"
        "<div class='actions'>"
        "<a class='act unblock' href='/unblock?mac=" + mac + "'>Unblock Player</a></div></div>";
    }
  }
  html += F("<div class='bar'><a class='refresh' href='/'>Refresh Status</a></div></body></html>");
  return html;
}

void goHome() {
  server.sendHeader("Location", "/");
  server.send(303);
}
void handleRoot() {
  if (!loggedIn()) {
    server.send(200, "text/html", loginPage(false));
    return;
  }
  server.send(200, "text/html", dashboardPage());
}
void handleLogin() {
  if (server.hasArg("pin") && server.arg("pin") == adminPin) {
    server.sendHeader("Set-Cookie", "auth=valid; Path=/; Max-Age=86400");
    goHome();
    return;
  }
  server.send(200, "text/html", loginPage(true));
}
void handleLogout() {
  server.sendHeader("Set-Cookie", "auth=logged_out; Path=/; Max-Age=0");
  goHome();
}
void handleKick() {
  if (!loggedIn()) { goHome(); return; }

  uint8_t mac[6];
  if (server.hasArg("mac") && stringToMac(server.arg("mac"), mac)) {
    blockDevice(mac, 600000);   // 10 min so the phone doesnt just auto reconnect
    Serial.printf("[kick] %s\n", macToString(mac).c_str());
  }
  goHome();
}
void handleBlock() {
  if (!loggedIn()) { goHome(); return; }

  uint8_t mac[6];
  if (server.hasArg("mac") && stringToMac(server.arg("mac"), mac)) {
    blockDevice(mac, 0);
    Serial.printf("[block] %s\n", macToString(mac).c_str());
  }
  goHome();
}
void handleUnblock() {
  if (!loggedIn()) { goHome(); return; }

  uint8_t mac[6];
  if (server.hasArg("mac") && stringToMac(server.arg("mac"), mac)) {
    unblockDevice(mac);
    Serial.printf("[unblock] %s\n", macToString(mac).c_str());
  }
  goHome();
}

void handleRename() {
  if (!loggedIn()) { goHome(); return; }

  uint8_t mac[6];
  if (server.hasArg("mac") && server.hasArg("name") && stringToMac(server.arg("mac"), mac)) {
    setAlias(mac, server.arg("name"));
  }
  goHome();
}

void setup() {
  system_update_cpu_freq(160);   // 160MHz, keeps latency low

  Serial.begin(115200);
  delay(100);
  Serial.println("\nStarting ESP8266...");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, pass, channel, 0, maxClients);

  onConnect = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected &evt) {
    uint8_t mac[6];
    memcpy(mac, evt.mac, 6);
    if (isBlocked(mac)) {
      Serial.printf("[blocked] rejected %s\n", macToString(mac).c_str());
      wifi_softap_deauth(mac);
    }
  });

  server.collectHeaders("Cookie");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/kick", HTTP_GET, handleKick);
  server.on("/block", HTTP_GET, handleBlock);
  server.on("/unblock", HTTP_GET, handleUnblock);
  server.on("/rename", HTTP_POST, handleRename);

  server.begin();
  Serial.println("Panel at http://192.168.4.1");
}

void loop() {
  server.handleClient();

  static unsigned long last = 0;
  if (millis() - last > 200) {
    last = millis();
    if (blockedCount > 0) {
      struct station_info *st = wifi_softap_get_station_info();
      while (st != NULL) {
        if (isBlocked(st->bssid)) {
          wifi_softap_deauth(st->bssid);
        }
        st = STAILQ_NEXT(st, next);
      }
      wifi_softap_free_station_info();
    }
  }
}
