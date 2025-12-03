Description du projet :

Le projet People Counter RF met en œuvre un système de comptage d’occupation intérieur combinant :

une détection de passage basée sur deux capteurs ultrasoniques HC-SR04 disposés en barrière (A/B) de part et d’autre d’une porte ;

un module ESP32 jouant le rôle de contrôleur de capteurs et de point d’accès Wi-Fi ;

une tablette M5Stack Tab5 assurant l’interface de supervision (IHM).

L’ESP32 mesure en continu les distances des capteurs A et B, détermine le sens de passage (A→B = entrée, B→A = sortie), met à jour les compteurs IN, OUT et INSIDE, et expose ces données via une petite API HTTP/JSON.
La Tab5 se connecte automatiquement au point d’accès Wi-Fi 2,4 GHz créé par l’ESP32 et affiche en temps quasi réel l’occupation de la salle.

Ce projet se concentre sur la communication RF, le développement embarqué et l’intégration matérielle autour d’un cas d’usage concret de comptage de personnes.

Architecture système :
Capteurs US A/B (HC-SR04) → ESP32 (AP Wi-Fi + logique de comptage)
                                 ⇅ Wi-Fi 2,4 GHz
                           M5Stack Tab5 (STA Wi-Fi + IHM)
                           
ESP32 en mode SoftAP (ESP32_COUNTER_AP, IP 192.168.4.1)

M5Stack Tab5 en mode station (STA)

Communication applicative via HTTP + JSON


Composants :

HC-SR04 (A/B) : capteurs ultrasoniques mesurant la distance et permettant de détecter le passage d’une personne dans une zone donnée.

ESP32 (module ESP32-WROOM-32) :

Acquisition des distances A/B

Logique de comptage (FSM A→B / B→A)

Point d’accès Wi-Fi 2,4 GHz

Serveur HTTP (/status, /reset)

M5Stack Tab5 :

Client Wi-Fi (STA)

Client HTTP/JSON

Interface graphique affichant IN, OUT, INSIDE et les distances dA/dB

Retour sonore (bip) à chaque nouveau passage

Alimentation 5 V SELV : bloc secteur ou alimentation de labo, régulation 3,3 V pour l’ESP32 et les capteurs.



Caractéristiques principales :

Comptage de personnes 

IN : nombre d’entrées

OUT : nombre de sorties

INSIDE : occupation courante (IN − OUT, saturé à 0)

Détection de passage :

2 capteurs HC-SR04 en barrière A/B

Seuil de distance + hystérésis pour stabiliser la détection

Machine d’état (FSM) pour distinguer A→B (entrée) et B→A (sortie) avec timeout

Communication RF :

Wi-Fi 2,4 GHz (IEEE 802.11 b/g/n)

ESP32 en point d’accès (SoftAP), Tab5 en station

Portée typique 10–20 m en intérieur (bande ISM 2,4 GHz)

API HTTP/JSON :

GET /status →
{
  "count_in":   N,
  "count_out":  M,
  "inside":     K,
  "distance_a": xx.x or null,
  "distance_b": yy.y or null,
  "near_a":     true/false,
  "near_b":     true/false,
  "uptime_s":   S
}


Interface Tab5 :

Écran d’accueil avec bouton START

Écran “dashboard” : INSIDE (en grand), IN, OUT, distances A/B

Bouton RESET (appelle /reset)

Bouton HOME (retour accueil, coupe le Wi-Fi)

Bip sonore à chaque nouveau passage (entrée ou sortie)


Usage :

Alimenter l’ESP32 (et les capteurs) en 5 V SELV.

Démarrer la Tab5.

Sur la Tab5, appuyer sur START :

La Tab5 active le Wi-Fi, se connecte à ESP32_COUNTER_AP.

Une fois connectée, elle interroge périodiquement http://192.168.4.1/status.

Lorsque des personnes passent devant les capteurs A et B :

l’ESP32 met à jour les compteurs IN / OUT / INSIDE ;

la Tab5 met à jour l’affichage et émet un bip à chaque nouveau passage.

En cas de besoin, l’utilisateur peut :

appuyer sur RESET pour remettre les compteurs à zéro ;

revenir à HOME pour arrêter la supervision et couper le Wi-Fi.


Tests et performances :

Portée RF :

Tests en intérieur réalisés à 5, 10, 15 et 20 m entre Tab5 et ESP32 SoftAP.

Taux de réponses HTTP /status > 99 % jusqu’à ~10 m en environnement classique (LOS ou cloison légère).

Latence d’affichage :

Période de polling /status ≈ 700 ms

Latence perçue pour la mise à jour des compteurs < 1 seconde.

Précision de comptage :

Fonctionnement correct pour des passages individuels et modérément rapprochés, grâce à la FSM et au timeout entre A et B.

Puissance RF :

EIRP typique de l’ESP32 ≈ 15 dBm, inférieure à la limite de 20 dBm (100 mW) imposée pour la bande ISM 2,4 GHz en Europe.



Conformité réglementaire (de principe) :

Le projet s’appuie sur l’utilisation de la bande ISM 2,4 GHz dans le cadre de la directive RED 2014/53/UE.
Les textes et normes suivants sont pris en compte à titre de référence :

Sécurité électrique : EN 62368-1

Compatibilité électromagnétique (CEM) : EN 301 489-1 / EN 301 489-17

Utilisation efficace du spectre : EN 300 328 (Wi-Fi 2,4 GHz)

Exposition RF (basse puissance, usage grand public) : EN 62479 / EN 62311

Cybersécurité IoT (référence de bonnes pratiques) : ETSI EN 303 645

Le projet est un prototype académique qui s’appuie sur des modules radio pré-certifiés (ESP32-WROOM-32, M5Stack Tab5) et dont la puissance RF reste dans les limites réglementaires de 100 mW EIRP pour la bande 2,4 GHz.


Sources et références techniques :

Fiche technique du module ESP32-WROOM-32 (Espressif)

Fiches techniques des capteurs HC-SR04

Documentation M5Stack Tab5 (ESP32-P4 / ESP32-C6)

Documentation ESP-IDF / Arduino-ESP32

Normes ETSI EN 300 328, EN 301 489-1/-17 (Wi-Fi 2,4 GHz)

Directive européenne RED 2014/53/UE et recommandation CEPT ERC/REC 70-03


Fait par Khadim NDOUR
