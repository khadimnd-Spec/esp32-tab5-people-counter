# Firmware ESP32 – AP + Compteur ENTREE/SORTIE

## Rôle
- Crée le point d'accès `ESP32_COUNTER_AP` (192.168.4.1).
- Mesure les deux capteurs HC-SR04 (A/B).
- Calcule IN, OUT, INSIDE.
- Expose l'API HTTP:
  - `GET /status` → JSON
  - `POST /reset` → remet les compteurs à zéro.

## Brochage
- TRIG_A = 26, ECHO_A = 25 (via diviseur 2k/2k)
- TRIG_B = 18, ECHO_B = 19 (via diviseur 2k/2k)

## Compilation
- Carte: "ESP32 Dev Module" (ou M5Stack/équivalent).
- IDE: Arduino IDE (ou PlatformIO).

