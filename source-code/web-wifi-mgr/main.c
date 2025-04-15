#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <algorithm>
#include <esp_wifi.h>
#include <esp_netif.h>

WebServer server(80);

struct WiFiOption {
  String ssid;
  String pass;
};

WiFiOption staOptions[] = {
  {"SSID1", "1234321"},
  {"BackupSSID", "backup123"},
  {"AltSSID3", "altpass3"}
};

String ap_ssid = "ESP32_Extender";
String ap_pass = "12345654";

std::vector<String> blockList;
std::vector<String> fullBlockList;

struct ClientInfo {
  String mac;
  String ip;
  size_t dataUsed;
};

std::vector<ClientInfo> clients;
size_t dataLimitMB = 0;

bool isMACBlocked(String mac) {
  return std::find(blockList.begin(), blockList.end(), mac) != blockList.end();
}

bool isMACFullyBlocked(String mac) {
  return std::find(fullBlockList.begin(), fullBlockList.end(), mac) != fullBlockList.end();
}

void blockInternet(String mac) {
  if (!isMACBlocked(mac)) blockList.push_back(mac);
}

void blockAll(String mac) {
  if (!isMACFullyBlocked(mac)) fullBlockList.push_back(mac);
  wifi_sta_list_t sta_list;
  esp_wifi_ap_get_sta_list(&sta_list);
  for (int i = 0; i < sta_list.num; i++) {
    wifi_sta_info_t sta = sta_list.sta[i];
    char buff[18];
    sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X", sta.mac[0], sta.mac[1], sta.mac[2], sta.mac[3], sta.mac[4], sta.mac[5]);
    if (mac.equalsIgnoreCase(buff)) {
      esp_wifi_deauth_sta(sta.mac);
    }
  }
}

void unblock(String mac) {
  blockList.erase(std::remove(blockList.begin(), blockList.end(), mac), blockList.end());
  fullBlockList.erase(std::remove(fullBlockList.begin(), fullBlockList.end(), mac), fullBlockList.end());
}

void connectToSTA() {
  WiFi.mode(WIFI_AP_STA);
  for (WiFiOption option : staOptions) {
    WiFi.begin(option.ssid.c_str(), option.pass.c_str());
    int tries = 15;
    while (WiFi.status() != WL_CONNECTED && tries-- > 0) delay(500);
    if (WiFi.status() == WL_CONNECTED) break;
  }
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
}

String htmlHeader() {
  return "<html><head><title>ESP32 Wi-Fi Extender</title><style>body{font-family:sans-serif;}table,th,td{border:1px solid black;border-collapse:collapse;padding:5px;}a{margin-right:10px;}button{padding:5px;margin:2px;}</style></head><body><h1>ESP32 Wi-Fi Extender</h1>";
}

void handleRoot() {
  String html = htmlHeader();
  html += "<p><b>STA IP:</b> " + WiFi.localIP().toString() + "</p>";
  html += "<p><b>AP IP:</b> " + WiFi.softAPIP().toString() + "</p>";
  html += "<a href='/clients'>Clients</a><a href='/config'>WiFi Config</a><a href='/blocklist'>Blocked List</a><a href='/status'>Status</a></body></html>";
  server.send(200, "text/html", html);
}

void handleClientsPage() {
  wifi_sta_list_t sta_list;
  tcpip_adapter_sta_list_t adapter_sta_list;
  esp_wifi_ap_get_sta_list(&sta_list);
  tcpip_adapter_get_sta_list(&sta_list, &adapter_sta_list);

  String html = htmlHeader();
  html += "<h2>Connected Clients</h2><table><tr><th>MAC</th><th>IP</th><th>Data (MB)</th><th>Action</th></tr>";
  for (int i = 0; i < adapter_sta_list.num; i++) {
    tcpip_adapter_sta_info_t sta = adapter_sta_list.sta[i];
    char mac[18];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            sta.mac[0], sta.mac[1], sta.mac[2],
            sta.mac[3], sta.mac[4], sta.mac[5]);
    String macStr = String(mac);
    String ipStr = IPAddress(sta.ip.addr).toString();
    size_t data = 0;
    for (ClientInfo &c : clients) {
      if (c.mac == macStr) {
        data = c.dataUsed;
        break;
      }
    }
    html += "<tr><td>" + macStr + "</td><td>" + ipStr + "</td><td>" + String(data / 1024.0 / 1024.0, 2) + "</td><td>";
    html += "<form action='/block_inet' method='GET'><input type='hidden' name='mac' value='" + macStr + "'><button>Block Internet</button></form>";
    html += "<form action='/block_all' method='GET'><input type='hidden' name='mac' value='" + macStr + "'><button>Block All</button></form>";
    html += "<form action='/unblock' method='GET'><input type='hidden' name='mac' value='" + macStr + "'><button>Unblock</button></form></td></tr>";
  }
  html += "</table><a href='/'>Back</a></body></html>";
  server.send(200, "text/html", html);
}

void handleBlocklist() {
  String html = htmlHeader();
  html += "<h2>Block Lists</h2><b>Internet Block:</b><ul>";
  for (String mac : blockList) html += "<li>" + mac + "</li>";
  html += "</ul><b>Full Block:</b><ul>";
  for (String mac : fullBlockList) html += "<li>" + mac + "</li>";
  html += "</ul><a href='/'>Back</a></body></html>";
  server.send(200, "text/html", html);
}

void handleConfig() {
  String html = htmlHeader();
  html += "<h2>WiFi Config</h2><form method='POST' action='/save'>";
  html += "STA1 SSID:<input name='ssid1'><br>PASS:<input name='pass1'><br>";
  html += "STA2 SSID:<input name='ssid2'><br>PASS:<input name='pass2'><br>";
  html += "STA3 SSID:<input name='ssid3'><br>PASS:<input name='pass3'><br>";
  html += "AP SSID:<input name='ap_ssid'><br>PASS:<input name='ap_pass'><br>";
  html += "Data Limit (MB):<input name='dlimit'><br>";
  html += "<input type='submit'></form><a href='/'>Back</a></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid1")) staOptions[0].ssid = server.arg("ssid1");
  if (server.hasArg("pass1")) staOptions[0].pass = server.arg("pass1");
  if (server.hasArg("ssid2")) staOptions[1].ssid = server.arg("ssid2");
  if (server.hasArg("pass2")) staOptions[1].pass = server.arg("pass2");
  if (server.hasArg("ssid3")) staOptions[2].ssid = server.arg("ssid3");
  if (server.hasArg("pass3")) staOptions[2].pass = server.arg("pass3");
  if (server.hasArg("ap_ssid")) ap_ssid = server.arg("ap_ssid");
  if (server.hasArg("ap_pass")) ap_pass = server.arg("ap_pass");
  if (server.hasArg("dlimit")) dataLimitMB = server.arg("dlimit").toInt();
  server.send(200, "text/html", "Saved. Restarting...");
  delay(1000);
  ESP.restart();
}

void handleBlockInet() {
  blockInternet(server.arg("mac"));
  server.sendHeader("Location", "/clients");
  server.send(303);
}

void handleBlockAll() {
  blockAll(server.arg("mac"));
  server.sendHeader("Location", "/clients");
  server.send(303);
}

void handleUnblock() {
  unblock(server.arg("mac"));
  server.sendHeader("Location", "/clients");
  server.send(303);
}

void handleStatus() {
  String html = htmlHeader();
  html += "<h2>Status Info</h2>";
  html += "<p><b>Connected SSID:</b> " + WiFi.SSID() + "</p>";
  html += "<p><b>BSSID:</b> " + WiFi.BSSIDstr() + "</p>";
  html += "<p><b>RSSI:</b> " + String(WiFi.RSSI()) + " dBm</p>";
  html += "<p><b>Data Limit:</b> " + String(dataLimitMB) + " MB</p>";
  html += "<a href='/'>Back</a></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  connectToSTA();
  server.on("/", handleRoot);
  server.on("/clients", handleClientsPage);
  server.on("/config", handleConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/block_inet", handleBlockInet);
  server.on("/block_all", handleBlockAll);
  server.on("/unblock", handleUnblock);
  server.on("/blocklist", handleBlocklist);
  server.on("/status", handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();
}
