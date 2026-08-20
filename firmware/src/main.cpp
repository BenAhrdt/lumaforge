#include <Arduino.h>
#include <algorithm>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <esp_netif.h>
#include <memory>
#include "generated_web.hpp"
#include "lumaforge/core.hpp"
#include "lumaforge/device_identity.hpp"

namespace {
constexpr uint32_t kConnectTimeoutMs = 15000;
constexpr uint32_t kReconnectIntervalMs = 30000;
constexpr char kApPassword[] = "lumaforge";
constexpr char kProduct[] = "LumaForge";
constexpr char kModel[] = "esp32";
constexpr char kFirmwareVersion[] = "0.2.0-alpha.2";
constexpr uint8_t kApiVersion = 1;

Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket(81);
DNSServer dns;
String deviceId;
String deviceName;
String hostname;
String apSsid;
bool accessPointActive = false;
uint32_t lastReconnectAttempt = 0;
bool updateSucceeded = false;
lumaforge::Renderer renderer;
lumaforge::Scene runtimeScene{"runtime", "Runtime", {}};
struct HardwareOutput { uint8_t gpio; String colorOrder; std::vector<size_t> logical; std::unique_ptr<Adafruit_NeoPixel> pixels; };
std::vector<HardwareOutput> hardwareOutputs;
uint32_t animationEpoch = 0;
uint32_t lastFrameAt = 0;
bool playbackActive = false;
String activeAutomationId;
String activeAutomationSceneId;
size_t activeAutomationStep = 0;
uint32_t automationStepStartedAt = 0;
String automationAdvanceMode;
float automationAdvanceSeconds = 0.0f;
float cpuLoadPercent = 0.0f;
uint64_t cpuBusyMicros = 0;
uint32_t cpuWindowStarted = 0;
String jsonEscape(const String& value);

constexpr char kDefaultLayout[] = "{\"strips\":[]}";
constexpr char kDefaultList[] = "[]";

String readProjectFile(const char* key, const char* fallback) {
  const String path = String("/") + key + ".json";
  File file = LittleFS.open(path, "r");
  if (!file) return fallback;
  String value = file.readString();
  return value.length() ? value : String(fallback);
}

bool writeProjectFile(const char* key, const String& value) {
  JsonDocument document;
  if (deserializeJson(document, value)) return false;
  const String path = String("/") + key + ".json";
  const String temporary = path + ".tmp";
  File file = LittleFS.open(temporary, "w");
  if (!file || file.print(value) != value.length()) return false;
  file.close();
  LittleFS.remove(path);
  return LittleFS.rename(temporary, path);
}

String deviceJson() {
  return "{\"name\":\"" + jsonEscape(deviceName) + "\",\"ledType\":\"WS2812B\",\"ledCount\":0,\"gpio\":4,"
    "\"channels\":\"RGB\",\"colorOrder\":\"RGB\",\"maxBrightness\":1,\"maxCurrentMa\":3000,"
    "\"supplyVoltage\":5,\"outputs\":[{\"id\":\"main\",\"gpio\":4}],\"apiVersion\":" + String(kApiVersion) + "}";
}

String projectJson() {
  return "{\"device\":" + deviceJson() + ",\"layout\":" + readProjectFile("layout", kDefaultLayout) +
    ",\"zones\":" + readProjectFile("zones", kDefaultList) + ",\"scenes\":" + readProjectFile("scenes", kDefaultList) +
    ",\"automations\":" + readProjectFile("automations", kDefaultList) + "}";
}

lumaforge::Direction directionFromJson(const String& value) {
  if (value == "reverse") return lumaforge::Direction::Reverse;
  if (value == "pingpong") return lumaforge::Direction::PingPong;
  if (value == "pingpong-reverse") return lumaforge::Direction::PingPongReverse;
  if (value == "center-out") return lumaforge::Direction::CenterOut;
  if (value == "outside-in") return lumaforge::Direction::OutsideIn;
  if (value == "center-out-and-back") return lumaforge::Direction::CenterOutAndBack;
  if (value == "outside-in-and-back") return lumaforge::Direction::OutsideInAndBack;
  return lumaforge::Direction::Forward;
}

size_t projectLedCount(JsonVariantConst layout) {
  size_t count = 0;
  for (JsonVariantConst strip : layout["strips"].as<JsonArrayConst>()) count += strip["ledCount"] | 0;
  return count;
}

void clearHardwareOutputs() {
  for (auto& output : hardwareOutputs) { output.pixels->clear();output.pixels->show(); }
}

neoPixelType pixelTypeFor(const String& order) {
  if (order == "GRB") return NEO_GRB + NEO_KHZ800;
  if (order == "BRG") return NEO_BRG + NEO_KHZ800;
  if (order == "RBG") return NEO_RBG + NEO_KHZ800;
  if (order == "GBR") return NEO_GBR + NEO_KHZ800;
  if (order == "BGR") return NEO_BGR + NEO_KHZ800;
  return NEO_RGB + NEO_KHZ800;
}

void configureHardwareOutputs(JsonVariantConst layout) {
  struct DesiredOutput { uint8_t gpio;String colorOrder;std::vector<size_t> logical; };
  std::vector<DesiredOutput> desired;
  size_t logicalOffset = 0;
  for (JsonVariantConst section : layout["strips"].as<JsonArrayConst>()) {
    const size_t count = section["ledCount"] | 0;
    if (count) {
      const uint8_t gpio = section["gpio"] | 4;
      const String colorOrder = section["colorOrder"] | "RGB";
      auto found = std::find_if(desired.begin(), desired.end(), [gpio](const auto& output) { return output.gpio == gpio; });
      if (found == desired.end()) { desired.push_back({gpio,colorOrder,{}});found=std::prev(desired.end()); }
      for (size_t index=0;index<count;++index) found->logical.push_back(logicalOffset+index);
    }
    logicalOffset += count;
  }
  bool unchanged = desired.size() == hardwareOutputs.size();
  for (size_t index=0;unchanged && index<desired.size();++index) unchanged=hardwareOutputs[index].gpio==desired[index].gpio&&hardwareOutputs[index].colorOrder==desired[index].colorOrder&&hardwareOutputs[index].logical==desired[index].logical;
  if (unchanged) return;
  clearHardwareOutputs();
  hardwareOutputs.clear();
  for (auto& desiredOutput : desired) {
    HardwareOutput output{desiredOutput.gpio,desiredOutput.colorOrder,std::move(desiredOutput.logical),nullptr};
    output.pixels=std::make_unique<Adafruit_NeoPixel>(output.logical.size(),output.gpio,pixelTypeFor(output.colorOrder));
    output.pixels->begin();output.pixels->clear();output.pixels->show();
    hardwareOutputs.push_back(std::move(output));
  }
}

void configureHardwareOutputsFromStorage() {
  JsonDocument layout;
  if (!deserializeJson(layout,readProjectFile("layout",kDefaultLayout))) configureHardwareOutputs(layout.as<JsonVariantConst>());
}

void writeHardwareFrame(const std::vector<lumaforge::Color>& frame) {
  for (auto& output : hardwareOutputs) {
    for (size_t physical=0;physical<output.logical.size();++physical) {
      const size_t logical=output.logical[physical];
      const auto color=logical<frame.size()?frame[logical]:lumaforge::Color{};
      output.pixels->setPixelColor(physical,output.pixels->Color(color.r,color.g,color.b));
    }
    output.pixels->show();
  }
}

std::vector<size_t> targetLeds(const String& zoneId, size_t count, JsonArrayConst zones) {
  std::vector<size_t> result;
  if (zoneId == "all") {
    result.reserve(count);
    for (size_t index = 0; index < count; ++index) result.push_back(index);
    return result;
  }
  for (JsonVariantConst zone : zones) {
    if (String(zone["id"] | "") != zoneId) continue;
    for (JsonVariantConst led : zone["leds"].as<JsonArrayConst>()) {
      const int index = led.as<int>();
      if (index >= 0 && static_cast<size_t>(index) < count) result.push_back(static_cast<size_t>(index));
    }
    break;
  }
  return result;
}

bool loadSceneForPlayback(const String& sceneId) {
  JsonDocument layoutDocument;
  JsonDocument zonesDocument;
  JsonDocument scenesDocument;
  if (deserializeJson(layoutDocument, readProjectFile("layout", kDefaultLayout)) ||
      deserializeJson(zonesDocument, readProjectFile("zones", kDefaultList)) ||
      deserializeJson(scenesDocument, readProjectFile("scenes", kDefaultList))) return false;
  configureHardwareOutputs(layoutDocument.as<JsonVariantConst>());
  const size_t count = projectLedCount(layoutDocument.as<JsonVariantConst>());
  renderer.resize(count);
  runtimeScene.animations.clear();
  for (JsonVariantConst scene : scenesDocument.as<JsonArrayConst>()) {
    if (String(scene["id"] | "") != sceneId) continue;
    const bool sceneLoop = scene["loop"] | false;
    for (JsonVariantConst item : scene["animations"].as<JsonArrayConst>()) {
      lumaforge::Animation animation;
      animation.id = String(item["id"] | "animation").c_str();
      animation.effect = lumaforge::parseEffect(String(item["effect"] | "solid").c_str());
      animation.target = targetLeds(String(item["zoneId"] | "all"), count, zonesDocument.as<JsonArrayConst>());
      const String color = item["color"] | "#00aeef";
      animation.color = lumaforge::Color::hex(color.c_str());
      animation.colorMode = String(item["colorMode"] | "solid") == "rainbow" ? lumaforge::ColorMode::Rainbow : lumaforge::ColorMode::Solid;
      animation.brightness = item["brightness"] | .8f;
      animation.speed = item["speed"] | .5;
      animation.start = item["start"] | 0.0;
      animation.duration = (sceneLoop || (item["loop"] | false)) ? 0.0 : (item["duration"] | 5.0);
      animation.priority = item["priority"] | 0;
      animation.direction = directionFromJson(String(item["direction"] | "forward"));
      animation.width = item["width"] | .15f;
      runtimeScene.animations.push_back(std::move(animation));
    }
    animationEpoch = millis();
    playbackActive = true;
    return true;
  }
  return false;
}

void stopPlayback() {
  runtimeScene.animations.clear();
  playbackActive = false;
  animationEpoch = millis();
  clearHardwareOutputs();
}

void broadcastAutomationState() {
  JsonDocument state;
  state["type"] = "automation.state";
  if (activeAutomationId.length()) state["automationId"] = activeAutomationId;
  else state["automationId"] = nullptr;
  state["state"] = activeAutomationId.length() ? "running" : "stopped";
  if (activeAutomationId.length()) {
    state["stepIndex"] = activeAutomationStep;
    state["sceneId"] = activeAutomationSceneId;
    state["elapsedSeconds"] = static_cast<float>(millis() - automationStepStartedAt) / 1000.0f;
  }
  String response;
  serializeJson(state, response);
  webSocket.broadcastTXT(response);
}

bool startAutomationStep() {
  JsonDocument automations;
  JsonDocument scenes;
  if (deserializeJson(automations, readProjectFile("automations", kDefaultList)) ||
      deserializeJson(scenes, readProjectFile("scenes", kDefaultList))) return false;
  for (JsonVariantConst automation : automations.as<JsonArrayConst>()) {
    if (String(automation["id"] | "") != activeAutomationId) continue;
    JsonArrayConst steps = automation["steps"].as<JsonArrayConst>();
    String sceneId;
    JsonVariantConst step;
    if (steps && activeAutomationStep < steps.size()) {
      step = steps[activeAutomationStep];
      sceneId = String(step["sceneId"] | "");
      automationAdvanceMode = String(step["advance"] | "manual");
      automationAdvanceSeconds = step["durationSeconds"] | 0.0f;
    } else if (!steps && activeAutomationStep == 0) {
      sceneId = String(automation["sceneId"] | "");
      automationAdvanceMode = "manual";
      automationAdvanceSeconds = 0.0f;
    } else {
      activeAutomationId = "";
      activeAutomationSceneId = "";
      stopPlayback();
      broadcastAutomationState();
      return true;
    }
    if (!loadSceneForPlayback(sceneId)) return false;
    activeAutomationSceneId = sceneId;
    if (automationAdvanceMode == "scene_finished") {
      float duration = 0.0f;
      bool endless = false;
      for (JsonVariantConst scene : scenes.as<JsonArrayConst>()) {
        if (String(scene["id"] | "") != sceneId) continue;
        endless = scene["loop"] | false;
        for (JsonVariantConst animation : scene["animations"].as<JsonArrayConst>()) {
          if (animation["loop"] | false) endless = true;
          duration = std::max(duration, (animation["start"] | 0.0f) + (animation["duration"] | 0.0f));
        }
      }
      automationAdvanceSeconds = endless ? -1.0f : duration;
    }
    automationStepStartedAt = millis();
    broadcastAutomationState();
    return true;
  }
  return false;
}

bool startAutomation(const String& automationId) {
  activeAutomationId = automationId;
  activeAutomationStep = 0;
  if (startAutomationStep()) return true;
  activeAutomationId = "";
  activeAutomationSceneId = "";
  return false;
}

bool nextAutomationStep() {
  if (!activeAutomationId.length()) return false;
  ++activeAutomationStep;
  return startAutomationStep();
}

void stopAutomation() {
  activeAutomationId = "";
  activeAutomationSceneId = "";
  activeAutomationStep = 0;
  stopPlayback();
  broadcastAutomationState();
}

void updateAutomation() {
  if (!activeAutomationId.length()) return;
  const float elapsed = static_cast<float>(millis() - automationStepStartedAt) / 1000.0f;
  if ((automationAdvanceMode == "after_delay" && elapsed >= automationAdvanceSeconds) ||
      (automationAdvanceMode == "scene_finished" && automationAdvanceSeconds >= 0.0f && elapsed >= automationAdvanceSeconds)) {
    nextAutomationStep();
  }
}

void broadcastCurrentFrame() {
  const double seconds = static_cast<double>(millis() - animationEpoch) / 1000.0;
  const std::vector<lumaforge::Color>& frame = renderer.render(runtimeScene, seconds);
  writeHardwareFrame(frame);
  const std::string message = "{\"type\":\"frame\",\"pixels\":" + lumaforge::colorJson(frame) + "}";
  webSocket.broadcastTXT(reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

void updatePreview(JsonVariantConst message) {
  JsonDocument layoutDocument;
  if (deserializeJson(layoutDocument, readProjectFile("layout", kDefaultLayout))) return;
  const size_t count = projectLedCount(layoutDocument.as<JsonVariantConst>());
  configureHardwareOutputs(layoutDocument.as<JsonVariantConst>());
  renderer.resize(count);
  runtimeScene.animations.erase(std::remove_if(runtimeScene.animations.begin(), runtimeScene.animations.end(),
    [](const lumaforge::Animation& animation) { return animation.id == "preview"; }), runtimeScene.animations.end());
  lumaforge::Animation animation;
  animation.id = "preview";
  animation.effect = lumaforge::parseEffect(String(message["effect"] | "solid").c_str());
  for (JsonVariantConst led : message["selection"].as<JsonArrayConst>()) {
    const int index = led.as<int>();
    if (index >= 0 && static_cast<size_t>(index) < count) animation.target.push_back(static_cast<size_t>(index));
  }
  animation.color = lumaforge::Color::hex(String(message["color"] | "#00aeef").c_str());
  animation.brightness = message["brightness"] | .8f;
  animation.speed = message["speed"] | .5;
  animation.duration = 0;
  animation.priority = 1000;
  animation.direction = directionFromJson(String(message["direction"] | "forward"));
  runtimeScene.animations.push_back(std::move(animation));
  animationEpoch = millis();
  playbackActive = true;
}

String jsonEscape(const String& value) {
  String result;
  result.reserve(value.length() + 8);
  for (const char character : value) {
    if (character == '"' || character == '\\') result += '\\';
    if (character == '\n') result += "\\n";
    else if (character != '\r') result += character;
  }
  return result;
}

String htmlEscape(const String& value) {
  String result = value;
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  result.replace(">", "&gt;");
  result.replace("\"", "&quot;");
  return result;
}

String networkOptions() {
  const int count = WiFi.scanNetworks();
  String options;
  for (int index = 0; index < count; ++index) {
    const String ssid = WiFi.SSID(index);
    if (!ssid.length() || options.indexOf("value=\"" + htmlEscape(ssid) + "\"") >= 0) continue;
    options += "<option value=\"" + htmlEscape(ssid) + "\">" + htmlEscape(ssid) + " (" +
      String(WiFi.RSSI(index)) + " dBm)</option>";
  }
  WiFi.scanDelete();
  return options;
}

String setupPage(const String& message = "") {
  const String notice = message.length() ? "<p class=\"notice\">" + htmlEscape(message) + "</p>" : "";
  return String(F("<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>LumaForge Einrichtung</title><script>document.documentElement.dataset.theme=localStorage.getItem('lumaforge.theme')||'light'</script><style>")) +
    F(":root{color-scheme:dark;font-family:system-ui,sans-serif}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#0b1015;color:#eaf2f6}.card{width:min(430px,calc(100% - 32px));background:#11191f;border:1px solid #31414d;border-radius:12px;padding:24px;box-shadow:0 24px 70px #000a}h1{margin:0 0 8px;font-size:22px}.brand{color:#20c8f4}p{color:#91a1ad;line-height:1.5}.notice{color:#55e3a4}label{display:block;margin-top:16px;color:#aab8c1;font-size:12px}input,select,button,.back{box-sizing:border-box;width:100%;margin-top:6px;padding:11px;border:1px solid #34434e;border-radius:7px;background:#162028;color:#eef6fa;font:inherit}.back{display:block;margin-top:16px;text-align:center;text-decoration:none}button{margin-top:22px;background:#0787aa;border-color:#20c8f4;cursor:pointer}small{display:block;margin-top:18px;color:#71818c}html[data-theme=light]{color-scheme:light}html[data-theme=light] body{background:#eef3f6;color:#172630}html[data-theme=light] .card{background:#f8fbfc;border-color:#aebfc9;box-shadow:0 24px 70px #28404c38}html[data-theme=light] p,html[data-theme=light] small{color:#617482}html[data-theme=light] label{color:#526875}html[data-theme=light] input,html[data-theme=light] select,html[data-theme=light] .back{background:#fff;color:#172630;border-color:#bfd0da}html[data-theme=light] button{color:#fff}html[data-theme=light] .notice{color:#14734a}</style></head><body><main class=\"card\"><h1><span class=\"brand\">LF</span> LumaForge einrichten</h1><p>Verbinde das Gerät mit deinem WLAN. Die Zugangsdaten bleiben lokal auf dem ESP32 gespeichert.</p>") + notice +
    F("<form method=\"post\" action=\"/configure\"><label>WLAN<select name=\"ssid\" required><option value=\"\">Netz auswählen …</option>") + networkOptions() +
    F("</select></label><label>WLAN-Passwort<input name=\"password\" type=\"password\" autocomplete=\"current-password\"></label><label>Gerätename<input name=\"name\" maxlength=\"40\" value=\"") + htmlEscape(deviceName) +
    F("\" required></label><button type=\"submit\">Speichern und verbinden</button></form><a class=\"back\" href=\"/\">Zurück zu LumaForge</a><small>Gerät: ") + htmlEscape(deviceId) + F("</small></main></body></html>");
}

String devicePage() {
  return String(F("<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>LumaForge</title><style>")) +
    F(":root{color-scheme:dark;font-family:system-ui,sans-serif}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#0b1015;color:#eaf2f6}.card{width:min(430px,calc(100% - 32px));background:#11191f;border:1px solid #31414d;border-radius:12px;padding:24px;box-shadow:0 24px 70px #000a}h1{margin:0 0 8px;font-size:22px}.brand,.online{color:#20c8f4}p{color:#91a1ad;line-height:1.5}.status{padding:12px;border:1px solid #29414d;border-radius:8px;background:#0d151a}.button{display:block;margin-top:18px;padding:11px;border:1px solid #34434e;border-radius:7px;color:#eef6fa;text-align:center;text-decoration:none}.primary{background:#0787aa;border-color:#20c8f4}small{display:block;margin-top:18px;color:#71818c}</style></head><body><main class=\"card\"><h1><span class=\"brand\">LF</span> ") + htmlEscape(deviceName) +
    F("</h1><p class=\"status\"><span class=\"online\">●</span> Mit dem WLAN verbunden<br>IP-Adresse: ") + WiFi.localIP().toString() +
    F("<br>Hostname: ") + htmlEscape(hostname) + F(".local</p><a class=\"button primary\" href=\"/api/v1/status\">Status-API öffnen</a><a class=\"button\" href=\"/setup\">WLAN und Gerätenamen ändern</a><small>Gerät: ") +
    htmlEscape(deviceId) + F("</small></main></body></html>");
}

String updatePage(const String& message = "", bool success = false) {
  const String notice = message.length() ? "<p class=\"notice " + String(success ? "success" : "error") + "\">" + htmlEscape(message) + "</p>" : "";
  String redirect;
  if (success) redirect = F("<script>let seconds=5;const notice=document.querySelector('.notice');const timer=setInterval(()=>{seconds--;notice.textContent=`Update erfolgreich. LumaForge startet neu … Weiterleitung in ${seconds} s`;if(seconds<=0){clearInterval(timer);location.replace('/')}},1000)</script>");
  return String(F("<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>LumaForge Firmware-Update</title><script>document.documentElement.dataset.theme=localStorage.getItem('lumaforge.theme')||'light'</script><style>")) +
    F(":root{color-scheme:dark;font-family:system-ui,sans-serif}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#0b1015;color:#eaf2f6}.card{width:min(460px,calc(100% - 32px));background:#11191f;border:1px solid #31414d;border-radius:12px;padding:24px;box-shadow:0 24px 70px #000a}h1{margin:0 0 8px;font-size:22px}.brand{color:#20c8f4}p{color:#91a1ad;line-height:1.5}.notice{padding:10px;border-radius:7px}.success{color:#55e3a4;background:#123026}.error{color:#ff8095;background:#321923}input,button,a{box-sizing:border-box;width:100%;margin-top:16px;padding:11px;border:1px solid #34434e;border-radius:7px;background:#162028;color:#eef6fa;font:inherit}button{background:#0787aa;border-color:#20c8f4;cursor:pointer}a{display:block;text-align:center;text-decoration:none}progress{width:100%;margin-top:16px;accent-color:#20c8f4}small{display:block;margin-top:18px;color:#71818c}html[data-theme=light]{color-scheme:light}html[data-theme=light] body{background:#eef3f6;color:#172630}html[data-theme=light] .card{background:#f8fbfc;border-color:#aebfc9;box-shadow:0 24px 70px #28404c38}html[data-theme=light] p,html[data-theme=light] small{color:#617482}html[data-theme=light] input,html[data-theme=light] a{background:#fff;color:#172630;border-color:#bfd0da}html[data-theme=light] button{color:#fff}html[data-theme=light] .success{color:#14734a;background:#dff5e9}html[data-theme=light] .error{color:#b42340;background:#fde8ed}</style></head><body><main class=\"card\"><h1><span class=\"brand\">LF</span> Firmware aktualisieren</h1><p>Wähle eine für dieses ESP32-Modell erstellte <b>firmware.bin</b>. WLAN- und Projektdaten bleiben erhalten.</p>") + notice +
    F("<form id=\"upload\"><input id=\"firmware\" type=\"file\" accept=\".bin,application/octet-stream\" required><progress id=\"progress\" max=\"100\" value=\"0\"></progress><button type=\"submit\">Update installieren</button></form><a href=\"/\">Zurück zu LumaForge</a><small>Während des Updates nicht vom Strom trennen.</small><script>const form=document.querySelector('#upload'),file=document.querySelector('#firmware'),progress=document.querySelector('#progress');form.onsubmit=e=>{e.preventDefault();if(!file.files.length)return;const data=new FormData();data.append('firmware',file.files[0]);const request=new XMLHttpRequest();request.open('POST','/update');request.upload.onprogress=e=>{if(e.lengthComputable)progress.value=e.loaded/e.total*100};request.onload=()=>{document.open();document.write(request.responseText);document.close()};request.onerror=()=>alert('Upload fehlgeschlagen');request.send(data)};</script>") + redirect + F("</main></body></html>");
}

void startMdns() {
  MDNS.end();
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "product", kProduct);
    MDNS.addServiceTxt("http", "tcp", "id", deviceId);
    MDNS.addService("lumaforge", "tcp", 80);
    MDNS.addServiceTxt("lumaforge", "tcp", "id", deviceId);
    MDNS.addServiceTxt("lumaforge", "tcp", "name", deviceName);
    MDNS.addServiceTxt("lumaforge", "tcp", "api", String(kApiVersion));
    MDNS.addServiceTxt("lumaforge", "tcp", "fw", kFirmwareVersion);
    MDNS.addServiceTxt("lumaforge", "tcp", "model", kModel);
    MDNS.addServiceTxt("lumaforge", "tcp", "manufacturer", kProduct);
    MDNS.addServiceTxt("lumaforge", "tcp", "product", kProduct);
    MDNS.addServiceTxt("lumaforge", "tcp", "protocol", "http");
  }
}

void startAccessPoint() {
  if (accessPointActive) return;
  WiFi.mode(WIFI_AP_STA);
  accessPointActive = WiFi.softAP(apSsid.c_str(), kApPassword);
  if (accessPointActive) dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("Einrichtungs-AP: %s | http://192.168.4.1\n", apSsid.c_str());
  Serial.println("AP-Passwort: lumaforge");
}

bool connectStation(uint32_t timeoutMs) {
  const String ssid = preferences.getString("ssid", "");
  if (!ssid.length()) return false;
  const String password = preferences.getString("password", "");
  WiFi.mode(accessPointActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(hostname.c_str());
  if (esp_netif_t* station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
    esp_netif_set_hostname(station, hostname.c_str());
  }
  WiFi.begin(ssid.c_str(), password.c_str());
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) delay(100);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("LumaForge online: http://%s.local | http://%s\n", hostname.c_str(), WiFi.localIP().toString().c_str());
  startMdns();
  return true;
}

void redirectToPortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void registerRoutes() {
  server.on("/", HTTP_GET, [] {
    if (WiFi.status() != WL_CONNECTED) {
      server.send(200, "text/html; charset=utf-8", setupPage());
      return;
    }
    const auto& asset = embedded_web::assets[0];
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, asset.mime, reinterpret_cast<const char*>(asset.data), asset.size);
  });
  server.on("/setup", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", setupPage()); });
  server.on("/update", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", updatePage()); });
  server.on("/update", HTTP_POST, [] {
    if (updateSucceeded && !Update.hasError()) {
      server.send(200, "text/html; charset=utf-8", updatePage("Update erfolgreich. LumaForge startet neu …", true));
      delay(700);
      ESP.restart();
    } else {
      server.send(500, "text/html; charset=utf-8", updatePage("Update fehlgeschlagen. Die bisherige Firmware bleibt aktiv."));
    }
  }, [] {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      updateSucceeded = false;
      Serial.printf("OTA-Update gestartet: %s\n", upload.filename.c_str());
      if (!upload.filename.endsWith(".bin") || !Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (!Update.hasError() && Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      updateSucceeded = !Update.hasError() && Update.end(true);
      if (!updateSucceeded) Update.printError(Serial);
      else Serial.printf("OTA-Update vollständig: %u Bytes\n", upload.totalSize);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
    }
  });
  server.on("/generate_204", HTTP_ANY, redirectToPortal);
  server.on("/hotspot-detect.html", HTTP_ANY, redirectToPortal);
  server.on("/connecttest.txt", HTTP_ANY, redirectToPortal);
  server.on("/ncsi.txt", HTTP_ANY, redirectToPortal);
  server.on("/configure", HTTP_POST, [] {
    const String ssid = server.arg("ssid");
    const String name = server.arg("name");
    if (!ssid.length() || !name.length()) {
      server.send(422, "text/html; charset=utf-8", setupPage("Bitte WLAN und Gerätenamen angeben."));
      return;
    }
    preferences.putString("ssid", ssid);
    preferences.putString("password", server.arg("password"));
    preferences.putString("name", name);
    server.send(200, "text/html; charset=utf-8", setupPage("Gespeichert. Das Gerät startet jetzt neu …"));
    delay(600);
    ESP.restart();
  });
  server.on("/api/v1/device", HTTP_GET, [] {
    const String status = WiFi.status() == WL_CONNECTED ? "online" : accessPointActive ? "provisioning" : "connecting";
    server.send(200, "application/json", "{\"id\":\"" + jsonEscape(deviceId) + "\",\"name\":\"" + jsonEscape(deviceName) +
      "\",\"model\":\"LumaForge ESP32\",\"firmwareVersion\":\"" + String(kFirmwareVersion) + "\",\"apiVersion\":\"" + String(kApiVersion) + "\",\"hostname\":\"" +
      jsonEscape(hostname) + ".local\",\"status\":\"" + status + "\"}");
  });
  server.on("/api/v1/device", HTTP_PUT, [] {
    JsonDocument request;
    if (!server.hasArg("plain") || deserializeJson(request, server.arg("plain"))) {
      server.send(422, "application/json", "{\"error\":\"invalid_json\"}");
      return;
    }
    String name = request["device_name"] | "";
    name.trim();
    if (!name.length() || name.length() > 40) {
      server.send(422, "application/json", "{\"error\":\"invalid_device_name\"}");
      return;
    }
    deviceName = name;
    preferences.putString("name", deviceName);
    if (WiFi.status() == WL_CONNECTED) startMdns();
    server.send(200, "application/json", "{\"device_id\":\"" + jsonEscape(deviceId) + "\",\"device_name\":\"" + jsonEscape(deviceName) + "\"}");
  });
  server.on("/api/v1/info", HTTP_GET, [] {
    const bool online = WiFi.status() == WL_CONNECTED;
    JsonDocument info;
    info["product"] = kProduct;
    info["device_id"] = deviceId;
    info["device_name"] = deviceName;
    info["model"] = kModel;
    info["firmware_version"] = kFirmwareVersion;
    info["api_version"] = kApiVersion;
    info["hostname"] = hostname;
    info["network"]["connected"] = online;
    info["network"]["ip"] = online ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    info["network"]["rssi"] = online ? WiFi.RSSI() : 0;
    JsonArray capabilities = info["capabilities"].to<JsonArray>();
    capabilities.add("led_output");
    capabilities.add("scenes");
    capabilities.add("zones");
    capabilities.add("automations");
    capabilities.add("automation_sequences");
    String response;
    serializeJson(info, response);
    server.send(200, "application/json", response);
  });
  server.on("/api/v1/status", HTTP_GET, [] {
    const bool online = WiFi.status() == WL_CONNECTED;
    server.send(200, "application/json", "{\"wifi\":\"" + String(online ? "connected" : "disconnected") +
      "\",\"ip\":\"" + String(online ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) +
      "\",\"provisioning\":" + String(accessPointActive ? "true" : "false") + ",\"rssi\":" + String(online ? WiFi.RSSI() : 0) +
      ",\"version\":\"" + String(kFirmwareVersion) + "\",\"cpuPercent\":" + String(cpuLoadPercent, 1) + ",\"memoryUsedBytes\":" + String(ESP.getHeapSize() - ESP.getFreeHeap()) +
      ",\"memoryTotalBytes\":" + String(ESP.getHeapSize()) + "}");
  });
  server.on("/api/config", HTTP_GET, [] { server.send(200, "application/json", deviceJson()); });
  server.on("/api/project", HTTP_GET, [] { server.send(200, "application/json", projectJson()); });
  for (const auto& asset : embedded_web::assets) {
    if (String(asset.path) == "/") continue;
    server.on(asset.path, HTTP_GET, [asset] {
      server.sendHeader("Content-Encoding", "gzip");
      server.send_P(200, asset.mime, reinterpret_cast<const char*>(asset.data), asset.size);
    });
  }
  for (const char* key : {"layout", "zones", "scenes", "automations"}) {
    const String route = String("/api/") + key;
    server.on(route.c_str(), HTTP_GET, [key] {
      server.send(200, "application/json", readProjectFile(key, String(key) == "layout" ? kDefaultLayout : kDefaultList));
    });
    server.on(route.c_str(), HTTP_PUT, [key] {
      if (!server.hasArg("plain") || !writeProjectFile(key, server.arg("plain"))) {
        server.send(422, "application/json", "{\"error\":\"invalid_json\"}");
        return;
      }
      server.send(200, "application/json", server.arg("plain"));
      if (String(key) == "layout") configureHardwareOutputsFromStorage();
      webSocket.broadcastTXT("{\"type\":\"" + String(key) + ".updated\"}");
    });
  }
  server.on("/api/project/import", HTTP_POST, [] {
    JsonDocument document;
    if (!server.hasArg("plain") || deserializeJson(document, server.arg("plain"))) {
      server.send(422, "application/json", "{\"error\":\"invalid_project\"}");
      return;
    }
    for (const char* key : {"layout", "zones", "scenes", "automations"}) {
      if (!document[key].is<JsonVariant>()) continue;
      String value;
      serializeJson(document[key], value);
      if (!writeProjectFile(key, value)) {
        server.send(500, "application/json", "{\"error\":\"storage_failed\"}");
        return;
      }
    }
    configureHardwareOutputsFromStorage();
    server.send(200, "application/json", projectJson());
  });
  server.onNotFound([] { accessPointActive ? redirectToPortal() : server.send(404, "application/json", "{\"error\":\"not_found\"}"); });
}

void onWebSocketEvent(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    webSocket.sendTXT(client, "{\"type\":\"hello\",\"apiVersion\":" + String(kApiVersion) + "}");
    JsonDocument state;
    state["type"] = "automation.state";
    if (activeAutomationId.length()) state["automationId"] = activeAutomationId;
    else state["automationId"] = nullptr;
    state["state"] = activeAutomationId.length() ? "running" : "stopped";
    if (activeAutomationId.length()) {
      state["stepIndex"] = activeAutomationStep;
      state["sceneId"] = activeAutomationSceneId;
    }
    String response;
    serializeJson(state, response);
    webSocket.sendTXT(client, response);
    return;
  }
  if (type != WStype_TEXT) return;
  JsonDocument message;
  if (deserializeJson(message, payload, length)) {
    webSocket.sendTXT(client, "{\"type\":\"error\",\"message\":\"invalid JSON\"}");
    return;
  }
  try {
    const String typeName = message["type"] | "";
    if (typeName == "scene.play") {
      activeAutomationId = "";
      activeAutomationSceneId = "";
      if (!loadSceneForPlayback(String(message["sceneId"] | ""))) {
        webSocket.sendTXT(client, "{\"type\":\"error\",\"message\":\"unknown scene\"}");
        return;
      }
    } else if (typeName == "scene.stop") {
      stopAutomation();
    } else if (typeName == "automation.start") {
      if (!startAutomation(String(message["automationId"] | ""))) {
        webSocket.sendTXT(client, "{\"type\":\"error\",\"message\":\"unknown automation\"}");
        return;
      }
    } else if (typeName == "automation.stop") {
      stopAutomation();
    } else if (typeName == "automation.next") {
      if (!nextAutomationStep()) {
        webSocket.sendTXT(client, "{\"type\":\"error\",\"message\":\"no active automation\"}");
        return;
      }
    } else if (typeName == "preview.set") {
      updatePreview(message.as<JsonVariantConst>());
    } else if (typeName == "preview.cancel" || typeName == "preview.apply") {
      runtimeScene.animations.erase(std::remove_if(runtimeScene.animations.begin(), runtimeScene.animations.end(),
        [](const lumaforge::Animation& animation) { return animation.id == "preview"; }), runtimeScene.animations.end());
      playbackActive = !runtimeScene.animations.empty();
      if (!playbackActive) clearHardwareOutputs();
    } else {
      webSocket.sendTXT(client, "{\"type\":\"error\",\"message\":\"unknown message type\"}");
      return;
    }
    broadcastCurrentFrame();
    webSocket.sendTXT(client, "{\"type\":\"" + typeName + ".accepted\"}");
  } catch (...) {
    webSocket.sendTXT(client, "{\"type\":\"error\",\"message\":\"renderer configuration failed\"}");
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  // ESP.getEfuseMac() exposes the factory-programmed base MAC, unlike
  // interface MACs that may depend on Wi-Fi mode. Hashing the complete 48-bit
  // value produces a stable public identity without disclosing the raw MAC.
  deviceId = lumaforge::deviceIdFromHardwareMac(ESP.getEfuseMac()).c_str();
  const String shortId = deviceId.substring(3, 9);
  preferences.begin("lumaforge", false);
  LittleFS.begin(true);
  configureHardwareOutputsFromStorage();
  deviceName = preferences.getString("name", "LumaForge " + shortId);
  hostname = "lumaforge-" + shortId;
  apSsid = "LumaForge-" + shortId;
  registerRoutes();
  if (!connectStation(kConnectTimeoutMs)) startAccessPoint();
  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  cpuWindowStarted = millis();
}

void loop() {
  const uint32_t loopStarted = micros();
  server.handleClient();
  webSocket.loop();
  updateAutomation();
  if (playbackActive && millis() - lastFrameAt >= 50) {
    lastFrameAt = millis();
    broadcastCurrentFrame();
  }
  if (accessPointActive) dns.processNextRequest();
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt >= kReconnectIntervalMs) {
    lastReconnectAttempt = millis();
    connectStation(1000);
  }
  cpuBusyMicros += static_cast<uint32_t>(micros() - loopStarted);
  const uint32_t cpuElapsed = millis() - cpuWindowStarted;
  if (cpuElapsed >= 1000) {
    cpuLoadPercent = std::min(100.0f, static_cast<float>(cpuBusyMicros) * 100.0f / (static_cast<float>(cpuElapsed) * 1000.0f));
    cpuBusyMicros = 0;
    cpuWindowStarted = millis();
  }
  delay(2);
}
