#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_NeoPixel.h>

namespace {

constexpr char kApSsid[] = "bumptima";
constexpr uint8_t kApChannel = 6;
constexpr uint8_t kNrfCePin = 14;
constexpr uint8_t kNrfCsnPin = 10;
constexpr uint8_t kNrfSckPin = 12;
constexpr uint8_t kNrfMosiPin = 11;
constexpr uint8_t kNrfMisoPin = 13;
constexpr uint8_t kStatusLedPin = 48;
constexpr uint32_t kWifiScanIntervalMs = 15000;
constexpr uint32_t kNrfScanIntervalMs = 8000;
constexpr uint32_t kBleScanIntervalMs = 12000;
constexpr uint32_t kBleScanDurationSeconds = 3;

WebServer server(80);
RF24 radio(kNrfCePin, kNrfCsnPin);
BLEScan *bleScan = nullptr;
Adafruit_NeoPixel statusLed(1, kStatusLedPin, NEO_GRB + NEO_KHZ800);

enum class LedMode : uint8_t {
  Boot,
  ApReady,
  WifiScan,
  NrfSweep,
  BleScan,
  Error,
};

struct WifiHit {
  String ssid;
  String bssid;
  int32_t rssi = 0;
  int32_t channel = 0;
  bool open = false;
};

struct BleHit {
  String name;
  String addr;
  int32_t rssi = 0;
  bool connectable = false;
};

WifiHit wifiHits[20];
size_t wifiHitCount = 0;
BleHit bleHits[24];
size_t bleHitCount = 0;
uint8_t nrfHits[126] = {0};
uint8_t fusedHits[126] = {0};

bool wifiScanRunning = false;
uint32_t lastWifiScanMs = 0;
uint32_t lastNrfScanMs = 0;
uint32_t lastBleScanMs = 0;
bool radioReady = false;
bool bleReady = false;

String htmlEscape(const String &input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '\\': out += F("\\\\"); break;
      case '"': out += F("\\\""); break;
      case '\b': out += F("\\b"); break;
      case '\f': out += F("\\f"); break;
      case '\n': out += F("\\n"); break;
      case '\r': out += F("\\r"); break;
      case '\t': out += F("\\t"); break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

uint32_t ledColor(uint8_t r, uint8_t g, uint8_t b) {
  return statusLed.Color(r, g, b);
}

void setStatusLed(LedMode mode) {
  switch (mode) {
    case LedMode::Boot:
      statusLed.setPixelColor(0, ledColor(40, 0, 60));
      break;
    case LedMode::ApReady:
      statusLed.setPixelColor(0, ledColor(0, 50, 18));
      break;
    case LedMode::WifiScan:
      statusLed.setPixelColor(0, ledColor(50, 25, 0));
      break;
    case LedMode::NrfSweep:
      statusLed.setPixelColor(0, ledColor(0, 20, 55));
      break;
    case LedMode::BleScan:
      statusLed.setPixelColor(0, ledColor(42, 0, 50));
      break;
    case LedMode::Error:
      statusLed.setPixelColor(0, ledColor(60, 0, 0));
      break;
  }
  statusLed.show();
}

int indexFromWifiChannel(int channel) {
  if (channel < 1 || channel > 14) {
    return -1;
  }
  if (channel == 14) {
    return 84;
  }
  return 12 + (channel - 1) * 5;
}

void addEnergyToBand(uint8_t *bands, int center, int spread, uint8_t energy) {
  for (int i = center - spread; i <= center + spread; ++i) {
    if (i < 0 || i >= 126) {
      continue;
    }
    uint16_t candidate = static_cast<uint16_t>(bands[i]) + energy;
    bands[i] = static_cast<uint8_t>(min(candidate, static_cast<uint16_t>(9)));
  }
}

void buildFusedView() {
  for (size_t i = 0; i < sizeof(fusedHits); ++i) {
    fusedHits[i] = min<uint8_t>(nrfHits[i], 9);
  }

  for (size_t i = 0; i < wifiHitCount; ++i) {
    const int center = indexFromWifiChannel(wifiHits[i].channel);
    if (center < 0) {
      continue;
    }
    const uint8_t energy = wifiHits[i].rssi >= -55 ? 4 : (wifiHits[i].rssi >= -72 ? 3 : 2);
    addEnergyToBand(fusedHits, center, 10, energy);
  }

  static const int kBleAdvCenters[3] = {2, 26, 80};
  for (size_t i = 0; i < bleHitCount; ++i) {
    for (uint8_t j = 0; j < 3; ++j) {
      addEnergyToBand(fusedHits, kBleAdvCenters[j], 1, 2);
    }
  }
}

void startWifiScan() {
  if (wifiScanRunning) {
    return;
  }
  setStatusLed(LedMode::WifiScan);
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
  wifiScanRunning = true;
}

void completeWifiScan(int count) {
  wifiHitCount = 0;
  if (count > 0) {
    const size_t limit = min(static_cast<size_t>(count), sizeof(wifiHits) / sizeof(wifiHits[0]));
    for (size_t i = 0; i < limit; ++i) {
      wifiHits[i].ssid = WiFi.SSID(i);
      wifiHits[i].bssid = WiFi.BSSIDstr(i);
      wifiHits[i].rssi = WiFi.RSSI(i);
      wifiHits[i].channel = WiFi.channel(i);
      wifiHits[i].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    wifiHitCount = limit;
  }
  WiFi.scanDelete();
  wifiScanRunning = false;
  lastWifiScanMs = millis();
  buildFusedView();
  setStatusLed(LedMode::ApReady);
}

void runNrfSweep() {
  memset(nrfHits, 0, sizeof(nrfHits));

  radio.stopListening();
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.disableDynamicPayloads();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);

  setStatusLed(LedMode::NrfSweep);

  for (uint8_t pass = 0; pass < 3; ++pass) {
    for (uint8_t ch = 0; ch < 126; ++ch) {
      radio.setChannel(ch);
      delayMicroseconds(140);
      if (radio.testRPD()) {
        if (nrfHits[ch] < 3) {
          ++nrfHits[ch];
        }
      }
    }
  }

  lastNrfScanMs = millis();
  buildFusedView();
  if (!wifiScanRunning) {
    setStatusLed(LedMode::ApReady);
  }
}

void runBleScan() {
  if (!bleReady || bleScan == nullptr) {
    return;
  }

  setStatusLed(LedMode::BleScan);
  bleHitCount = 0;

  BLEScanResults results = bleScan->start(kBleScanDurationSeconds, false);
  const size_t maxBle = sizeof(bleHits) / sizeof(bleHits[0]);
  const size_t count = min(static_cast<size_t>(results.getCount()), maxBle);

  for (size_t i = 0; i < count; ++i) {
    BLEAdvertisedDevice device = results.getDevice(i);
    const String addr = String(device.getAddress().toString().c_str());
    String name = device.haveName() ? String(device.getName().c_str()) : String("BLE-") + addr;
    name.trim();
    if (name.length() == 0) {
      name = String("BLE-") + addr;
    }

    bleHits[i].name = name;
    bleHits[i].addr = addr;
    bleHits[i].rssi = device.getRSSI();
    bleHits[i].connectable = device.haveServiceUUID() || device.haveName();
  }

  bleHitCount = count;
  bleScan->clearResults();
  lastBleScanMs = millis();
  buildFusedView();

  if (!wifiScanRunning) {
    setStatusLed(LedMode::ApReady);
  }
}

String renderHtml() {
  String page;
  page.reserve(18000);
  page += F("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>buggedlab.com // bumptima</title>"
            "<style>"
            ":root{--bg:#070b0f;--panel:#0f1720;--panel2:#101b27;--line:#233041;--text:#d8e3ef;--muted:#7f95aa;--accent:#f0b84b;--accent2:#68d391;--danger:#ff6b6b;--fusion:#80e4ff;}"
            "*{box-sizing:border-box}body{margin:0;font-family:'IBM Plex Sans',ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:radial-gradient(circle at top,#11202d 0,#070b0f 55%);color:var(--text)}"
            ".wrap{max-width:1200px;margin:0 auto;padding:24px}header{display:flex;justify-content:space-between;gap:16px;align-items:flex-end;flex-wrap:wrap;padding:20px 0 28px;border-bottom:1px solid var(--line)}"
            ".brand{font-size:42px;font-weight:900;letter-spacing:.08em;text-transform:uppercase;line-height:1}.brand span{color:var(--accent)}"
            ".sub{color:var(--muted);margin-top:8px;max-width:700px}.pill{display:inline-flex;align-items:center;gap:8px;background:rgba(240,184,75,.12);border:1px solid rgba(240,184,75,.35);color:#ffd98a;padding:8px 12px;border-radius:999px;font-size:12px;letter-spacing:.12em;text-transform:uppercase}"
            ".grid{display:grid;grid-template-columns:repeat(12,1fr);gap:16px;margin-top:20px}.card{grid-column:span 12;background:linear-gradient(180deg,var(--panel),var(--panel2));border:1px solid var(--line);border-radius:18px;padding:18px;box-shadow:0 20px 70px rgba(0,0,0,.35)}"
            ".card h2{margin:0 0 12px;font-size:15px;letter-spacing:.14em;text-transform:uppercase;color:#f1f6fb}.meta{display:flex;flex-wrap:wrap;gap:10px;font-size:13px;color:var(--muted)}"
            ".statrow{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px}.stat{padding:14px;border:1px solid #233447;border-radius:14px;background:rgba(255,255,255,.02)}"
            ".label{font-size:11px;text-transform:uppercase;letter-spacing:.14em;color:var(--muted)}.value{font-size:22px;font-weight:800;margin-top:6px}"
            ".bars{display:grid;grid-template-columns:repeat(126,1fr);gap:2px;align-items:end;height:220px;margin-top:14px;padding:14px;background:#071018;border:1px solid #1d2a38;border-radius:14px;overflow:hidden}"
            ".bar{min-height:4px;border-radius:4px 4px 0 0;background:linear-gradient(180deg,#7cf7b0,#18a566)}.bar.nrf{background:linear-gradient(180deg,#ffd166,#c58b15)}"
            ".bar.fused{background:linear-gradient(180deg,var(--fusion),#1f82b7)}"
            ".legend{display:flex;gap:12px;flex-wrap:wrap;font-size:12px;color:var(--muted);margin-top:10px}.dot{width:10px;height:10px;border-radius:999px;display:inline-block;margin-right:6px}"
            ".list{display:grid;gap:10px}.row{display:grid;grid-template-columns:1fr auto auto auto;gap:12px;align-items:center;padding:12px 14px;border:1px solid #233447;border-radius:12px;background:rgba(255,255,255,.02)}"
            ".ssid{font-weight:700}.muted{color:var(--muted)}.good{color:var(--accent2)}.bad{color:var(--danger)}"
            ".footer{padding:18px 0 8px;color:var(--muted);font-size:12px}.blink{animation:blink 1s steps(2,end) infinite}@keyframes blink{50%{opacity:.35}}"
            "@media (orientation: landscape) and (max-width:1100px){.grid{grid-template-columns:repeat(12,1fr)}.card:nth-of-type(1){grid-column:span 12}.card:nth-of-type(2),.card:nth-of-type(3){grid-column:span 6}.bars{height:280px}}"
            "@media (max-width:900px){.statrow{grid-template-columns:repeat(2,minmax(0,1fr))}.row{grid-template-columns:1fr auto}.bars{height:160px}}"
            "</style></head><body><div class='wrap'>"
            "<header><div><div class='brand'>bugged<span>lab</span>.com</div><div class='sub'>open lab access point on ESP32-S3 with live Wi-Fi and nRF24L01+PA+LNA band sweeps.</div></div>"
            "<div class='pill blink'>SSID: bumptima</div></header>"
            "<section class='grid'>"
            "<div class='card'><h2>Status</h2><div class='statrow'>"
            "<div class='stat'><div class='label'>AP SSID</div><div class='value' id='apSsid'>bumptima</div></div>"
            "<div class='stat'><div class='label'>AP IP</div><div class='value' id='apIp'>192.168.4.1</div></div>"
            "<div class='stat'><div class='label'>Wi-Fi clients</div><div class='value' id='clients'>0</div></div>"
            "<div class='stat'><div class='label'>NRF/BLE age</div><div class='value' id='rfAge'>-</div></div>"
            "</div><div class='meta' id='meta'></div></div>"
            "<div class='card'><h2>Fused spectrum (2.4 GHz)</h2><div class='bars' id='fusedBars'></div><div class='legend'><span><i class='dot' style='background:#80e4ff'></i>ESP32 interpreted RF load</span><span><i class='dot' style='background:#ffd166'></i>nRF24 energy</span><span><i class='dot' style='background:#7cf7b0'></i>Wi-Fi/BLE inferred occupancy</span></div></div>"
            "<div class='card'><h2>nRF24L01+ raw sweep</h2><div class='bars' id='nrfBars'></div><div class='legend'><span><i class='dot' style='background:#ffd166'></i>3 hits</span><span><i class='dot' style='background:#e7a52c'></i>2 hits</span><span><i class='dot' style='background:#9d7a23'></i>1 hit</span><span><i class='dot' style='background:#244052'></i>quiet</span></div></div>"
            "<div class='card'><h2>Wi-Fi scan</h2><div class='list' id='wifiList'></div></div>"
            "<div class='card'><h2>BLE scan</h2><div class='list' id='bleList'></div></div>"
            "</section><div class='footer'>This page updates automatically. The ESP32-S3 radio provides the AP and Wi-Fi scan, while the nRF24 sweeps the 2.4 GHz band occupied by the external transceiver.</div>"
            "</div><script>"
            "const fusedBars=document.getElementById('fusedBars');"
            "const nrfBars=document.getElementById('nrfBars');"
            "const wifiList=document.getElementById('wifiList');"
            "const bleList=document.getElementById('bleList');"
            "const meta=document.getElementById('meta');"
            "function ageLabel(ms){if(ms<1000)return ms+' ms';const s=(ms/1000).toFixed(ms<10000?1:0);return s+' s';}"
            "function renderBars(el,data,maxV,klass){el.innerHTML='';for(let i=0;i<data.length;i++){const v=data[i];const b=document.createElement('div');b.className='bar '+klass;b.style.height=(4+(v/maxV)*100)+'%';b.style.opacity=(v>0?Math.min(0.25+v/(maxV*1.2),1):0.16).toString();b.title='Ch '+i+' => '+v;el.appendChild(b);}}"
            "function render(data){"
            "document.getElementById('apSsid').textContent=data.ap_ssid;"
            "document.getElementById('apIp').textContent=data.ap_ip;"
            "document.getElementById('clients').textContent=data.clients;"
            "document.getElementById('rfAge').textContent=ageLabel(Math.min(data.nrf_age_ms,data.ble_age_ms));"
            "meta.innerHTML='<span>Wi-Fi scan age: '+ageLabel(data.wifi_age_ms)+'</span><span>BLE scan age: '+ageLabel(data.ble_age_ms)+'</span><span>AP channel: '+data.ap_channel+'</span><span>Wi-Fi scan running: '+(data.wifi_scan_running?'yes':'no')+'</span>';"
            "renderBars(fusedBars,data.fused,9,'fused');"
            "renderBars(nrfBars,data.nrf,3,'nrf');"
            "wifiList.innerHTML='';"
            "if(!data.wifi.length){wifiList.innerHTML='<div class=\"row\"><div class=\"ssid\">No networks seen yet</div><div class=\"muted\">waiting for scan</div></div>';}"
            "else{for(const net of data.wifi){const row=document.createElement('div');row.className='row';row.innerHTML='<div><div class=\"ssid\">'+(net.ssid||'[hidden]')+'</div><div class=\"muted\">Channel '+net.channel+'</div></div><div>'+net.rssi+' dBm</div><div class=\"'+(net.open?'good':'bad')+'\">'+(net.open?'open':'secured')+'</div><div class=\"muted\">'+net.bssid+'</div>';wifiList.appendChild(row);}}"
            "bleList.innerHTML='';"
            "if(!data.ble.length){bleList.innerHTML='<div class=\"row\"><div class=\"ssid\">No BLE advertisers yet</div><div class=\"muted\">scan pending</div></div>'; }"
            "else{for(const dev of data.ble){const row=document.createElement('div');row.className='row';row.innerHTML='<div><div class=\"ssid\">'+dev.name+'</div><div class=\"muted\">'+dev.addr+'</div></div><div>'+dev.rssi+' dBm</div><div class=\"'+(dev.connectable?'good':'bad')+'\">'+(dev.connectable?'connectable':'broadcast')+'</div><div class=\"muted\">BLE</div>';bleList.appendChild(row);}}"
            "}"
            "async function tick(){const r=await fetch('/api/status',{cache:'no-store'});render(await r.json());}"
            "tick();setInterval(tick,4000);"
            "</script></body></html>");
  return page;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", renderHtml());
}

void handleStatus() {
  String json;
  json.reserve(24000);
  json += F("{");
  json += F("\"ap_ssid\":\"");
  json += jsonEscape(kApSsid);
  json += F("\",\"ap_ip\":\"");
  json += WiFi.softAPIP().toString();
  json += F("\",\"ap_channel\":");
  json += String(kApChannel);
  json += F(",\"clients\":");
  json += String(WiFi.softAPgetStationNum());
  json += F(",\"wifi_scan_running\":");
  json += wifiScanRunning ? F("true") : F("false");
  json += F(",\"wifi_age_ms\":");
  json += String(millis() - lastWifiScanMs);
  json += F(",\"nrf_age_ms\":");
  json += String(millis() - lastNrfScanMs);
  json += F(",\"ble_age_ms\":");
  json += String(millis() - lastBleScanMs);
  json += F(",\"wifi\":[");
  for (size_t i = 0; i < wifiHitCount; ++i) {
    if (i) json += ',';
    json += F("{\"ssid\":\"");
    json += jsonEscape(wifiHits[i].ssid);
    json += F("\",\"rssi\":");
    json += String(wifiHits[i].rssi);
    json += F(",\"channel\":");
    json += String(wifiHits[i].channel);
    json += F(",\"open\":");
    json += wifiHits[i].open ? F("true") : F("false");
    json += F(",\"bssid\":\"");
    json += jsonEscape(wifiHits[i].bssid);
    json += F("\"}");
  }
  json += F("],\"ble\":[");
  for (size_t i = 0; i < bleHitCount; ++i) {
    if (i) json += ',';
    json += F("{\"name\":\"");
    json += jsonEscape(bleHits[i].name);
    json += F("\",\"addr\":\"");
    json += jsonEscape(bleHits[i].addr);
    json += F("\",\"rssi\":");
    json += String(bleHits[i].rssi);
    json += F(",\"connectable\":");
    json += bleHits[i].connectable ? F("true") : F("false");
    json += F("}");
  }
  json += F("],\"nrf\":[");
  for (size_t i = 0; i < sizeof(nrfHits); ++i) {
    if (i) json += ',';
    json += String(nrfHits[i]);
  }
  json += F("],\"fused\":[");
  for (size_t i = 0; i < sizeof(fusedHits); ++i) {
    if (i) json += ',';
    json += String(fusedHits[i]);
  }
  json += F("]}");

  server.send(200, "application/json", json);
}

void configureAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  IPAddress ip(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gw, mask);
  WiFi.softAP(kApSsid, nullptr, kApChannel, false, 8);
  setStatusLed(LedMode::ApReady);
}

void configureRadio() {
  SPI.begin(kNrfSckPin, kNrfMisoPin, kNrfMosiPin, kNrfCsnPin);
  if (!radio.begin()) {
    Serial.println(F("NRF24 init failed"));
    radioReady = false;
    setStatusLed(LedMode::Error);
    return;
  }
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.disableDynamicPayloads();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radioReady = true;
  radio.printPrettyDetails();
}

void configureBle() {
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();
  if (bleScan == nullptr) {
    Serial.println(F("BLE scan init failed"));
    bleReady = false;
    return;
  }

  bleScan->setActiveScan(true);
  bleScan->setInterval(120);
  bleScan->setWindow(90);
  bleReady = true;
}

void serviceScans() {
  if (wifiScanRunning) {
    const int complete = WiFi.scanComplete();
    if (complete == WIFI_SCAN_RUNNING) {
      return;
    }
    completeWifiScan(complete);
  } else if (millis() - lastWifiScanMs >= kWifiScanIntervalMs) {
    startWifiScan();
  }

  if (millis() - lastNrfScanMs >= kNrfScanIntervalMs) {
    runNrfSweep();
  }

  if (millis() - lastBleScanMs >= kBleScanIntervalMs) {
    runBleScan();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println(F("booting bumptima"));

  statusLed.begin();
  statusLed.setBrightness(24);
  statusLed.clear();
  statusLed.show();
  setStatusLed(LedMode::Boot);

  configureAccessPoint();
  configureRadio();
  configureBle();

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.onNotFound(handleRoot);
  server.begin();

  startWifiScan();
  runNrfSweep();
  runBleScan();

  if (radioReady) {
    setStatusLed(LedMode::ApReady);
  }

  Serial.print(F("AP SSID: "));
  Serial.println(kApSsid);
  Serial.print(F("AP IP: "));
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  serviceScans();
  delay(5);
}
