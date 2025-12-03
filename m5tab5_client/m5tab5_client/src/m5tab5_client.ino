/**********************************************************
 * M5 Tab5 — Compteur ENTREE / SORTIE (ESP32 AP + 2 HC-SR04)
 * - Ecran d'accueil avec bouton "START"
 * - Ecran mesures avec:
 *     INSIDE (personnes présentes)
 *     IN  (entrées)
 *     OUT (sorties)
 * - Bouton HOME (retour à l'accueil, coupe le Wi-Fi)
 * - Bouton RESET (demande reset au serveur)
 * - Bip à chaque nouveau passage (entrée ou sortie)
 **********************************************************/
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ====== PARAM RÉSEAU (AP ESP32) ======
const char* WIFI_SSID   = "ESP32_COUNTER_AP";
const char* WIFI_PSW    = "12345678";
const char* SERVER_IP   = "192.168.4.1";
const uint16_t SERVER_PORT = 80;

// ====== UI / STRUCTS ======
auto& display = M5.Display;
struct Rect { int x,y,w,h; };
Rect resetBtn;
Rect startBtn;
Rect homeBtn;

// ====== ÉTAT GÉNÉRAL ======
enum AppState { STATE_HOME, STATE_RUN };
AppState appState = STATE_HOME;

// Compteurs
long countIn   = 0;
long countOut  = 0;
long inside    = 0;
long lastBeepTotal = 0;   // pour déclencher le bip

// Infos capteurs (facultatif)
float distA = NAN;
float distB = NAN;
bool  nearA = false;
bool  nearB = false;

// Réseau / erreurs
bool   wifi_ok     = false;
String lastErr     = "";
uint32_t wifiLastTry   = 0;
uint32_t wifiBackoffMs = 3000;

// ====== BIP ======
void beepOnce() {
  M5.Speaker.setVolume(180);
  M5.Speaker.tone(1200, 80); // 1200 Hz, 80 ms
}

// ====== HTTP helpers ======
bool httpGET(const String& path, String& payload) {
  if (WiFi.status() != WL_CONNECTED) { lastErr = "WIFI_OFF"; return false; }
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + path;
  http.setConnectTimeout(900);
  http.setTimeout(900);
  if (!http.begin(url)) { lastErr = "HTTP_BEGIN_FAIL"; return false; }
  int code = http.GET();
  if (code == 200) {
    payload = http.getString();
    http.end();
    return true;
  } else {
    lastErr = "HTTP_" + String(code);
    http.end();
    return false;
  }
}

bool httpPOST(const String& path) {
  if (WiFi.status() != WL_CONNECTED) { lastErr = "WIFI_OFF"; return false; }
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + path;
  http.setConnectTimeout(900);
  http.setTimeout(900);
  if (!http.begin(url)) { lastErr = "HTTP_BEGIN_FAIL"; return false; }
  int code = http.POST((uint8_t*)nullptr, 0);
  http.end();
  if (code == 200) return true;
  lastErr = "HTTP_" + String(code);
  return false;
}

// ====== UI : ACCUEIL ======
void drawHomeScreen() {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(textdatum_t::middle_center);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(3);
  display.drawString("Compteur ENTREE / SORTIE", display.width()/2, 60);
  display.setTextSize(2);
  display.drawString("ESP32 AP + 2x HC-SR04", display.width()/2, 95);

  int bw = display.width() - 80;
  int bh = 80;
  int bx = 40;
  int by = display.height()/2 - bh/2;
  startBtn = { bx, by, bw, bh };

  display.fillRoundRect(bx, by, bw, bh, 18, TFT_DARKGREEN);
  display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  display.setTextSize(4);
  display.drawString("START", display.width()/2, by + bh/2);

  display.setTextSize(2);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString("Toucher START pour lancer les mesures", display.width()/2, by + bh + 40);
}

// ====== UI : HEADER + ERREURS ======
void drawHeader() {
  display.fillRect(0,0, display.width(), 18, TFT_BLACK);
  display.setTextDatum(textdatum_t::top_left);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(12, 0);
  String wifi = wifi_ok ? ("Wi-Fi OK " + WiFi.localIP().toString())
                        : "Wi-Fi NOK (reco...)";
  display.print("Counter | "); display.print(wifi);
}

void drawError() {
  uint16_t c = lastErr.length() ? TFT_MAROON : TFT_BLACK;
  display.fillRect(0, 20, display.width(), 18, c);
  display.setTextColor(TFT_WHITE, c);
  display.setTextDatum(textdatum_t::top_left);
  display.setTextSize(2);
  display.setCursor(12, 20);
  display.print(lastErr);
}

// ====== UI : DASHBOARD ======
void drawStaticUI() {
  display.fillScreen(TFT_BLACK);
  drawHeader();
  drawError();

  // HOME
  homeBtn = { 10, 44, 90, 40 };
  display.fillRoundRect(homeBtn.x, homeBtn.y, homeBtn.w, homeBtn.h, 10, TFT_DARKGREY);
  display.setTextDatum(textdatum_t::middle_center);
  display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.setTextSize(2);
  display.drawString("HOME", homeBtn.x + homeBtn.w/2, homeBtn.y + homeBtn.h/2);

  // Barres INSIDE / IN / OUT
  display.fillRoundRect(10, 90,  display.width()-20, 70, 14, TFT_DARKGREEN);
  display.fillRoundRect(10, 170, display.width()-20, 70, 14, TFT_DARKCYAN);
  display.fillRoundRect(10, 250, display.width()-20, 70, 14, TFT_DARKGREY);

  // RESET
  resetBtn = { 10, display.height()-70, display.width()-20, 60 };
  display.fillRoundRect(resetBtn.x, resetBtn.y, resetBtn.w, resetBtn.h, 16, TFT_MAROON);
  display.setTextColor(TFT_WHITE, TFT_MAROON);
  display.setTextSize(3);
  display.setTextDatum(textdatum_t::middle_left);
  display.setCursor(resetBtn.x + 20, resetBtn.y + resetBtn.h/2);
  display.print("RESET");
}

void drawDynamicUI() {
  drawHeader();
  drawError();

  char buf[64];
  display.setTextDatum(textdatum_t::middle_center);

  // INSIDE
  display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  display.setTextSize(4);
  snprintf(buf, sizeof(buf), "INSIDE: %ld", inside);
  display.drawString(buf, display.width()/2, 90 + 35);

  // IN
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  display.setTextSize(3);
  snprintf(buf, sizeof(buf), "IN: %ld", countIn);
  display.drawString(buf, display.width()/2, 170 + 28);

  // OUT
  display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.setTextSize(3);
  snprintf(buf, sizeof(buf), "OUT: %ld", countOut);
  display.drawString(buf, display.width()/2, 250 + 28);

  // Ligne distances
  display.setTextSize(2);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setTextDatum(textdatum_t::bottom_left);
  int y = display.height() - 74;
  display.fillRect(0, y-16, display.width(), 16, TFT_BLACK);

  String line = "dA=";
  if (isnan(distA)) line += "--.-"; else line += String(distA,1);
  line += "cm  dB=";
  if (isnan(distB)) line += "--.-"; else line += String(distB,1);
  line += "cm";

  display.setCursor(12, y);
  display.print(line);
}

// ====== Wi-Fi non bloquant ======
void wifiService() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifi_ok) { wifi_ok = true; drawDynamicUI(); }
    return;
  }

  wifi_ok = false;
  uint32_t now = millis();
  if (now - wifiLastTry < wifiBackoffMs) return;
  wifiLastTry = now;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID, WIFI_PSW);

  wifiBackoffMs = min<uint32_t>(wifiBackoffMs * 2, 30000);
}

// ====== Setup / Loop ======
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  display.setRotation(1);
  display.setColorDepth(16);
  display.setBrightness(60);

  M5.Speaker.begin();
  M5.Speaker.setVolume(180);

  appState = STATE_HOME;
  drawHomeScreen();
}

void loop() {
  M5.update();

  if (appState == STATE_HOME) {
    if (M5.Touch.getCount()) {
      auto t = M5.Touch.getDetail(0);
      if (t.wasPressed()
          && t.x >= startBtn.x && t.x <= startBtn.x + startBtn.w
          && t.y >= startBtn.y && t.y <= startBtn.y + startBtn.h) {

        appState = STATE_RUN;
        lastErr = "";
        wifi_ok = false;

        countIn  = 0;
        countOut = 0;
        inside   = 0;
        lastBeepTotal = 0;
        distA = NAN; distB = NAN;
        nearA = nearB = false;

        wifiLastTry   = 0;
        wifiBackoffMs = 3000;
        WiFi.disconnect(true, true);

        drawStaticUI();
        drawDynamicUI();
        wifiService();
      }
    }
    return;
  }

  // MODE RUN
  wifiService();

  if (M5.Touch.getCount()) {
    auto t = M5.Touch.getDetail(0);
    if (t.wasPressed()) {
      // RESET
      if (t.x >= resetBtn.x && t.x <= resetBtn.x+resetBtn.w &&
          t.y >= resetBtn.y && t.y <= resetBtn.y+resetBtn.h) {
        lastErr = "";
        if (httpPOST("/reset")) {
          countIn  = 0;
          countOut = 0;
          inside   = 0;
          lastBeepTotal = 0;
        }
        drawDynamicUI();
      }
      // HOME
      else if (t.x >= homeBtn.x && t.x <= homeBtn.x+homeBtn.w &&
               t.y >= homeBtn.y && t.y <= homeBtn.y+homeBtn.h) {
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        wifi_ok = false;
        lastErr = "";
        appState = STATE_HOME;
        drawHomeScreen();
        return;
      }
    }
  }

  // Poll /status
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= 700) {
    lastPoll = millis();
    if (WiFi.status() == WL_CONNECTED) {
      String payload;
      if (httpGET("/status", payload)) {
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
          lastErr = "";

          long  newIn     = countIn;
          long  newOut    = countOut;
          long  newInside = inside;
          float newDistA  = distA;
          float newDistB  = distB;
          bool  newNearA  = nearA;
          bool  newNearB  = nearB;

          if (doc["count_in"].is<long>())    newIn     = doc["count_in"].as<long>();
          if (doc["count_out"].is<long>())   newOut    = doc["count_out"].as<long>();
          if (doc["inside"].is<long>())      newInside = doc["inside"].as<long>();

          if (doc["distance_a"].is<float>()) newDistA  = doc["distance_a"].as<float>();
          else if (doc["distance_a"].is<int>()) newDistA = doc["distance_a"].as<int>();
          else newDistA = NAN;

          if (doc["distance_b"].is<float>()) newDistB  = doc["distance_b"].as<float>();
          else if (doc["distance_b"].is<int>()) newDistB = doc["distance_b"].as<int>();
          else newDistB = NAN;

          if (doc.containsKey("near_a")) newNearA = doc["near_a"];
          if (doc.containsKey("near_b")) newNearB = doc["near_b"];

          // Bip si nouveau passage
          long newTotal = newIn + newOut;
          if (newTotal > lastBeepTotal) {
            beepOnce();
            lastBeepTotal = newTotal;
          }

          countIn  = newIn;
          countOut = newOut;
          inside   = newInside;
          distA    = newDistA;
          distB    = newDistB;
          nearA    = newNearA;
          nearB    = newNearB;
        } else {
          lastErr = "JSON_ERR";
        }
        drawDynamicUI();
      } else {
        drawDynamicUI();
      }
    }
  }
}
