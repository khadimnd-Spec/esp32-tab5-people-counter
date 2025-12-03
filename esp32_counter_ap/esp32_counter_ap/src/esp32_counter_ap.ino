/**********************************************************
 * ESP32 — Point d'accès Wi-Fi + Compteur ENTREE / SORTIE
 * avec 2 capteurs HC-SR04 (A et B)
 *
 * AP:
 *   SSID: ESP32_COUNTER_AP
 *   MDP : 12345678
 *   IP  : 192.168.4.1
 *
 * API:
 *   GET  /status -> JSON:
 *      {
 *        "count_in":   N,
 *        "count_out":  M,
 *        "inside":     N-M (>=0),
 *        "distance_a": xx.x or null,
 *        "distance_b": xx.x or null,
 *        "near_a":     true/false,
 *        "near_b":     true/false,
 *        "uptime_s":   S
 *      }
 *   POST /reset  -> remet count_in / count_out à zéro
 **********************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ======== AP Wi-Fi ========
const char* AP_SSID = "ESP32_COUNTER_AP";
const char* AP_PSW  = "12345678";     // >=8 caractères
const int   AP_CH   = 6;
IPAddress local_IP(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

// ======== HTTP ========
WebServer server(80);

// ======== Capteurs HC-SR04 ========
// Capteur A (côté A)
const int PIN_TRIG_A = 26;
const int PIN_ECHO_A = 25;  // via diviseur 2k/2k

// Capteur B (côté B)
const int PIN_TRIG_B = 18;
const int PIN_ECHO_B = 19;  // via diviseur 2k/2k

// ======== Paramètres de détection ========
float    seuil_cm       = 25.0f;   // seuil "proche"
float    hysteresis     = 5.0f;    // marge pour rester proche
uint32_t passageTimeout = 1200;    // délai max A->B ou B->A (ms)

// ======== États / compteurs ========
long     countIn   = 0;
long     countOut  = 0;
long     inside    = 0;

bool     nearAState = false;
bool     nearBState = false;

float    lastDistanceA = NAN;
float    lastDistanceB = NAN;

// FSM direction
enum PassageState { PS_IDLE, PS_A_FIRST, PS_B_FIRST };
PassageState passState   = PS_IDLE;
uint32_t     stateStartMs = 0;

// ======== Mesure distance ========
static float mesureDistanceCm(int trigPin, int echoPin) {
  auto once = [&](void) -> float {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(3);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    unsigned long us = pulseIn(echoPin, HIGH, 25000UL); // timeout ~25ms
    if (us == 0) return NAN;
    return (us * 0.0343f) / 2.0f; // cm
  };

  float v[3];
  for (int i = 0; i < 3; ++i) {
    v[i] = once();
    delay(5);
  }
  // médiane
  for (int i = 0; i < 3; ++i)
    for (int j = i+1; j < 3; ++j)
      if (v[j] < v[i]) { float t=v[i]; v[i]=v[j]; v[j]=t; }

  return v[1];
}

// ======== HTTP Handlers ========
void handle_root() {
  server.send(200, "text/plain",
              "ESP32 AP 2xHC-SR04 Counter (ENTREE/SORTIE).\nUse /status (JSON) and /reset (POST).");
}

void handle_status() {
  inside = countIn - countOut;
  if (inside < 0) inside = 0;

  String json = "{";

  json += "\"count_in\":";   json += countIn;
  json += ",\"count_out\":"; json += countOut;
  json += ",\"inside\":";    json += inside;

  json += ",\"distance_a\":";
  if (isnan(lastDistanceA)) json += "null";
  else                      json += String(lastDistanceA,1);

  json += ",\"distance_b\":";
  if (isnan(lastDistanceB)) json += "null";
  else                      json += String(lastDistanceB,1);

  json += ",\"near_a\":"; json += (nearAState ? "true" : "false");
  json += ",\"near_b\":"; json += (nearBState ? "true" : "false");

  json += ",\"uptime_s\":"; json += String(millis()/1000);

  json += "}";

  server.send(200, "application/json", json);
}

void handle_reset() {
  countIn   = 0;
  countOut  = 0;
  inside    = 0;
  passState = PS_IDLE;
  server.send(200, "text/plain", "OK");
  Serial.println("[HTTP] RESET -> counts=0");
}

// ======== Setup ========
void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG_A, OUTPUT);
  pinMode(PIN_ECHO_A, INPUT);
  digitalWrite(PIN_TRIG_A, LOW);

  pinMode(PIN_TRIG_B, OUTPUT);
  pinMode(PIN_ECHO_B, INPUT);
  digitalWrite(PIN_TRIG_B, LOW);

  WiFi.mode(WIFI_AP);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  bool ok = WiFi.softAP(AP_SSID, AP_PSW, AP_CH, 0, 4);
  Serial.printf("SoftAP %s\n", ok ? "OK" : "FAILED");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP()); // 192.168.4.1

  server.on("/",       HTTP_GET,  handle_root);
  server.on("/status", HTTP_GET,  handle_status);
  server.on("/reset",  HTTP_POST, handle_reset);
  server.begin();
  Serial.println("HTTP server started");
}

// ======== Loop ========
void loop() {
  server.handleClient();

  static uint32_t lastMeas = 0;
  if (millis() - lastMeas < 50) return; // ~20 Hz
  lastMeas = millis();

  // Mesures
  float dA = mesureDistanceCm(PIN_TRIG_A, PIN_ECHO_A);
  float dB = mesureDistanceCm(PIN_TRIG_B, PIN_ECHO_B);
  lastDistanceA = dA;
  lastDistanceB = dB;

  // near/hystérésis
  bool newNearA = false;
  bool newNearB = false;

  if (!isnan(dA)) {
    if (!nearAState && dA <= seuil_cm)                    newNearA = true;
    else if (nearAState && dA <= seuil_cm + hysteresis)   newNearA = true;
  }
  if (!isnan(dB)) {
    if (!nearBState && dB <= seuil_cm)                    newNearB = true;
    else if (nearBState && dB <= seuil_cm + hysteresis)   newNearB = true;
  }

  static bool prevNearA = false;
  static bool prevNearB = false;
  bool risingA = (!prevNearA && newNearA);
  bool risingB = (!prevNearB && newNearB);

  uint32_t now = millis();

  switch (passState) {
    case PS_IDLE:
      if (risingA && !newNearB) {
        passState = PS_A_FIRST;
        stateStartMs = now;
        Serial.println("[FSM] A first");
      } else if (risingB && !newNearA) {
        passState = PS_B_FIRST;
        stateStartMs = now;
        Serial.println("[FSM] B first");
      }
      break;

    case PS_A_FIRST:
      if (now - stateStartMs > passageTimeout) {
        Serial.println("[FSM] timeout A_FIRST");
        passState = PS_IDLE;
      } else if (risingB) {
        // A puis B -> entrée
        countIn++;
        inside = countIn - countOut;
        if (inside < 0) inside = 0;
        Serial.printf("[COUNT] ENTREE: in=%ld out=%ld inside=%ld\n", countIn, countOut, inside);
        passState = PS_IDLE;
      }
      break;

    case PS_B_FIRST:
      if (now - stateStartMs > passageTimeout) {
        Serial.println("[FSM] timeout B_FIRST");
        passState = PS_IDLE;
      } else if (risingA) {
        // B puis A -> sortie
        countOut++;
        inside = countIn - countOut;
        if (inside < 0) inside = 0;
        Serial.printf("[COUNT] SORTIE: in=%ld out=%ld inside=%ld\n", countIn, countOut, inside);
        passState = PS_IDLE;
      }
      break;
  }

  prevNearA  = newNearA;
  prevNearB  = newNearB;
  nearAState = newNearA;
  nearBState = newNearB;
}
