#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>

// Constants
const IPAddress apIP(192, 168, 4, 1);
const IPAddress gatewayIP(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);
const IPAddress BlynkServer(13, 212, 90, 60);
const int port = 8118;
const int metaAddress = 0;
const int metaLenght = 4;
const int jsonAddress = 4;

// Working variables
bool fallback = true;
ESP8266WebServer server(80);
RTC_DS3231 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.pool.ntp.org", 7 * 3600);
DynamicJsonDocument config(2048);
BlynkTimer timer;
WidgetTerminal stateWg(V0);
WidgetTerminal logWg(V7);

// Functions
void syslog(String text) {
  Serial.println(text);
  logWg.println(text);
  logWg.flush();
}

// EEPROM
String EEPROM_read(int index, int length) {
  String text = "";
  char ch = 1;
  for (int i = index; (i < (index + length)) && ch; ++i) {
    if (ch = EEPROM.read(i)) text.concat(ch);
  }
  return text;
}

int EEPROM_write(int index, String text) {
  for (int i = index; i < text.length() + index; ++i) EEPROM.write(i, text[i - index]);
  EEPROM.write(index + text.length(), 0);
  EEPROM.commit();
  return text.length() + 1;
}

DynamicJsonDocument getEEPROM_JSON() {
  String jsonRead = EEPROM_read(jsonAddress, EEPROM_read(metaAddress, metaLenght).toInt());
  DynamicJsonDocument jsonDoc(2048);
  deserializeJson(jsonDoc, jsonRead);
  return jsonDoc;
}

void setEEPROM_JSON(DynamicJsonDocument jsonDoc) {
  String jsonWriteString;
  serializeJson(jsonDoc, jsonWriteString);
  EEPROM_write(metaAddress, (String)EEPROM_write(jsonAddress, jsonWriteString));
}

// Switch
class SWC {
private:
  bool state_on = false;
  int end[2] = {-1, -1};

public:
  SWC() {
    pinMode(16, OUTPUT);
    off();
  }

  void on() {
    digitalWrite(16, LOW);
    state_on = true;
  }

  void off() {
    digitalWrite(16, HIGH);
    state_on = false;
    setend(-1, -1);
  }

  bool isopen() {return state_on;}

  void setend(int h, int m) {
    end[0] = h;
    end[1] = m;
    if (h != 1) syslog("Close time has just been set.");
  }

  void check(int h, int m, int weekday) {
    if (end[0] == h && end[1] == m) {
      off();
      syslog("Switch turned off by schedule.");
      return; 
    }
    if (end[0] == -1 && end[1] == -1 && config["sch"]) {
      if (config["sch0"][7] == h && config["sch0"][8] == m && config["sch0"][weekday] == 1) {
        on();
        syslog("Switch turned on by schedule 1.");
        setend(config["sch0"][9], config["sch0"][10]);
      }
      else if (config["sch1"][7] == h && config["sch1"][8] == m && config["sch1"][weekday] == 1) {
        on();
        syslog("Switch turned on by schedule 2.");
        setend(config["sch1"][9], config["sch1"][10]);
      }
    }
  }
};

SWC sw = SWC();

// Setup functions
void defaultConfigSetup(){
  String defaultConfig = "\
{\
  \"ssid\": \"June8th\",\
  \"password\": \"80808181\",\
  \"webpwd\": \"admin\",\
  \"sch\": false,\
  \"sch0\": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],\
  \"sch1\": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]\
}";
  deserializeJson(config, defaultConfig);
  /*
  Schedule layout
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}

  0 -> 6: Active weekdays (0 = Inactive, 1 = Active)
  0 = Sun, 1 = Mon, 2 = Tue, 3 = Wed, 4 = Thu, 5 = Fri, 6 = Sat
  7: Start hour (0-23)
  8: Start minute (0-59)
  9: End hour (0-23)
  10: End minute (0-59)
*/
}

void EEPROMsetup(){
  EEPROM.begin(2048);
  if (getEEPROM_JSON().isNull()) {
    syslog("No saved configuration found. Using default config.");
    return;
  }
  config = getEEPROM_JSON();
  syslog("Config loaded.");
}

void WiFisetup(){
  WiFi.softAPConfig(apIP, gatewayIP, subnetMask);
  WiFi.softAP("ESP8266");
  WiFi.begin();
  syslog("Connecting to WiFi...");
}

// Webserver
const String loginHTML = "<!DOCTYPE html><html><head><title>Login</title><style>"
"body {display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0;}"
".login-box {background-color: #fff; border-radius: 10px; padding: 20px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); width: 300px;}"
"input {width: 90%; padding: 10px; margin-bottom: 10px; border: 1px solid #ccc; border-radius: 5px;}"
"button {background-color: #007bff; color: #fff; border: none; border-radius: 5px; padding: 10px; width: 97%; cursor: pointer;}"
"input::placeholder {color: gray; font-style: italic;}"
"</style></head><body><div class=\"login-box\"><form id=\"login-form\">"
"<h1 style=\"text-align: center; margin-top: 0;\">Login</h1>"
"<input placeholder=\"Username\" type=\"text\" id=\"username\" name=\"username\" required><br>"
"<input placeholder=\"Password\" type=\"password\" id=\"password\" name=\"password\" required><br>"
"<button type=\"submit\">OK</button></form></div><script>"
"document.getElementById('login-form').addEventListener('submit', async function(event) {"
"event.preventDefault();"
"const username = document.getElementById('username').value;"
"const password = document.getElementById('password').value;"
"const response = await fetch('/api/login', {"
"method: 'POST',"
"headers: {'Content-Type': 'application/json'},"
"body: JSON.stringify({ username, password })"
"});"
"if (response.status === 200) {"
"const data = await response.json();"
"const Authorization = data.Authorization;"
"document.cookie = `Authorization=${Authorization}; path=/`;"
"window.location.href = '';"
"} else {"
"alert('Login failed!');"
"}});"
"</script></body></html>";

String TOKEN = "";

void resetToken() {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String randomString = "";
  for (int i = 0; i < 32; ++i) {
    randomString += charset[random(0, sizeof(charset) - 1)];
  }
  TOKEN = randomString;
  syslog("New webserver token generated.");
}

void handleRoot() {
  if (!server.hasHeader("Authorization") or server.header("Authorization") != TOKEN) {
    server.sendHeader("Location", "/login");
    server.send(302);
    return;
  }
  String html = "<html>"
  "<body>"
  "<h1>ESP8266</h1>"
  "</body>"
  "</html>";
  server.send(200, "text/html", html);
}

void handleLogin() {
  server.send(200, "text/html", loginHTML);
}

void apiLogin() {
  String jsonBody = server.arg("plain");
  DynamicJsonDocument jsonDoc(1024);
  DeserializationError error = deserializeJson(jsonDoc, jsonBody);
  if (error) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  if (jsonDoc.containsKey("username") && jsonDoc.containsKey("password")){
    if (jsonDoc["username"] == "admin" && jsonDoc["password"] == config["webpwd"]){
      resetToken();
      server.send(200, "application/json", "{\"success\": true, \"Authorization\": \"" + TOKEN + "\"}");
    }
    else server.send(401, "text/plain", "Unauthorized");
  }
  else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void webserverSetup() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_GET, handleLogin);
  server.on("/api/login", HTTP_POST, apiLogin);
  server.begin();
  syslog("Webserver started.");
}

// Blynk client
void fetchState() {
  stateWg.clear();
  DateTime time = rtc.now();
  stateWg.print("Thoi gian: ");
  if (time.dayOfTheWeek() == 1) stateWg.print("CN ");
  else stateWg.print("T" + String(time.dayOfTheWeek()) + " ");
  stateWg.print(String(time.hour()) + ":" + String(time.minute()) + ":" + String(time.second()));
  stateWg.println(" " + String(time.day()) + "/" + String(time.month()) + "/" + String(time.year()));
  stateWg.println("Trang thai: " + String(sw.isopen() ? "Mo" : "Dong"));
  bool schedule = config["sch"];
  if (schedule) {
    stateWg.print("Che do: Len lich");
    stateWg.println();
    stateWg.println("Lich 1: " + String(config["sch0"][7]) + ":" + String(config["sch0"][8]) + " -> " + String(config["sch0"][9]) + ":" + String(config["sch0"][10]));
    for (int i = 0; i < 7; ++i) {
      if (config["sch0"][i]) {
        if (i == 0) stateWg.print("CN ");
        else stateWg.print("T" + String(i + 1) + " ");
      }
    }
    stateWg.println();
    stateWg.println("Lich 2: " + String(config["sch1"][7]) + ":" + String(config["sch1"][8]) + " -> " + String(config["sch1"][9]) + ":" + String(config["sch1"][10]));
    for (int i = 0; i < 7; ++i) {
      if (config["sch1"][i]) {
        if (i == 0) stateWg.print("CN ");
        else stateWg.print("T" + String(i + 1) + " ");
      }
    }
  }
  else stateWg.println("Che do: Thu cong");
  stateWg.println();
  stateWg.flush();
}

void BlynkSyncEvent() {
  Blynk.virtualWrite(V1, sw.isopen());
  fetchState();
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V3, 0);
  Blynk.virtualWrite(V4, config["sch"] ? 2 : 1);
}

BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    sw.on();
    syslog("Switch turned on by Blynk.");
  }
  else {
    sw.off();
    syslog("Switch turned off by Blynk.");
  }
}

BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    config["sch"] = false;
    syslog("Switched to manual mode.");
  }
  else {
    config["sch"] = true;
    syslog("Switched to schedule mode.");
  }
  fetchState();
}

BLYNK_WRITE(V5) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    config["sch0"][7] = t.getStartHour();
    config["sch0"][8] = t.getStartMinute();
    syslog("Schedule 1 start time changed.");
  }
  if (t.hasStopTime()) {
    config["sch0"][9] = t.getStopHour();
    config["sch0"][10] = t.getStopMinute();
    syslog("Schedule 1 end time changed.");
  }
  for (int i = 0; i < 7; ++i) {
    config["sch0"][i] = t.isWeekdaySelected(i + 1) ? 1 : 0;
  }
}

BLYNK_WRITE(V6) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    config["sch1"][7] = t.getStartHour();
    config["sch1"][8] = t.getStartMinute();
    syslog("Schedule 2 start time changed.");
  }
  if (t.hasStopTime()) {
    config["sch1"][9] = t.getStopHour();
    config["sch1"][10] = t.getStopMinute();
    syslog("Schedule 2 end time changed.");
  }
  for (int i = 0; i < 7; ++i) {
    config["sch1"][i] = t.isWeekdaySelected(i + 1) ? 1 : 0;
  }
}

BLYNK_WRITE(V15) {
  // NTP time synchorization
  if (param.asInt() == 1) {
    rtc.adjust(DateTime(timeClient.getEpochTime()));
    syslog("Performed NTP time synchorization");
  }
}

BLYNK_WRITE(V16) {
  if (param.asInt() == 1) {
    setEEPROM_JSON(config);
    ESP.restart();
  }
}

void setupBlynk() {
  const char* wifiSSID = "June8th";
  const char* wifiPWD = "80808181";
  const char* authToken = "mq3FvWV9hXYMmsK4uLjBO2adpiX30Z10";
  Blynk.begin(authToken, wifiSSID, wifiPWD, BlynkServer, port);
}

// Setup
void setup() {
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();
  timeClient.begin();
  DateTime now = rtc.now();
  if (now.year() < 2023) {
    Serial.println("[WARN] RTC module power loss detected. Schedule features will not work correctly");
    rtc.adjust(DateTime(2023, 1, 1, 0, 0, 0));
  }
  Serial.println("\n\n\n\n");
  syslog("Starting system...");
  defaultConfigSetup();
  EEPROMsetup();
  WiFisetup();
  webserverSetup();
  setupBlynk();
  timer.setInterval(3000L, BlynkSyncEvent);
  syslog("System started.");
}

// Loop functions
void connectionCheck() {
  if (Blynk.connected() && fallback) {
    syslog("Connected to Blynk server. Disabling fallback AP...");
    fallback = false;
    WiFi.softAPdisconnect();
    return;
  }
  if (!Blynk.connected() && !fallback) {
    syslog("Disconnected from Blynk server. Enabling fallback AP...");
    fallback = true;
    WiFi.softAPConfig(apIP, gatewayIP, subnetMask);
    WiFi.softAP("ESP8266");
    return;
  }
}

// Main loop
int interval = 0;
int h, m, wd;
void loop() {
  Blynk.run();
  timer.run();
  server.handleClient();
  connectionCheck();
  if (interval == 20) {
    DateTime time = rtc.now();
    h = time.hour();
    m = time.minute();
    wd = time.dayOfTheWeek() - 1;
    sw.check(h, m, wd);
    interval = 0;
  }
  else interval ++;
  delay(25);
}