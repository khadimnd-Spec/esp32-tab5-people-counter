# Firmware M5 Tab5 – Client IHM

## Rôle
- Se connecte au Wi-Fi `ESP32_COUNTER_AP`.
- Interroge régulièrement `GET /status`.
- Affiche INSIDE / IN / OUT et les distances dA/dB.
- Envoie `POST /reset` sur appui du bouton RESET.
- Génère un bip à chaque nouveau passage détecté.

## Compilation
- Plateforme: M5Unified (M5 Tab5).
- Bibliothèques: M5Unified, WiFi, HTTPClient, ArduinoJson.

