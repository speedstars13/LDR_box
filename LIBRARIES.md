# Environnement Arduino & librairies

## Carte / core

- **IDE** : Arduino IDE 2.x (ou PlatformIO, non testé/documenté ici).
- **Board package** : *esp32* par Espressif Systems, installé via le
  Gestionnaire de cartes (Fichier > Préférences > URL de gestionnaire de
  cartes supplémentaires : `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
- **Version testée pour ce projet** : la Firebase-ESP-Client (voir
  ci-dessous) a eu besoin d'un correctif spécifique pour rester compatible
  avec le core ESP32 **v3.1.x**. Si vous rencontrez des erreurs de
  compilation liées à `Client.h` ou similaire sur une version plus récente
  du core, essayez de revenir à la 3.1.x, ou vérifiez la page des issues du
  dépôt Firebase-ESP-Client pour un correctif plus récent.
- **Board à sélectionner** : celle qui correspond à votre module précis
  (ex. "ESP32S3 Dev Module" pour un ESP32-S3), avec un **Partition Scheme**
  adapté : voir `docs/OTA.md` — c'est important, le schéma par défaut ne
  laisse pas de place pour les mises à jour par Wi-Fi.

## Librairies à installer (Gestionnaire de bibliothèques)

| Librairie | Auteur | Rôle dans le projet |
|---|---|---|
| **TFT_eSPI** | Bodmer | Pilote l'écran tactile (tout l'affichage) |
| **WiFiManager** | tzapu | Portail captif pour configurer le Wi-Fi sans re-flasher |
| **Firebase Arduino Client Library for ESP8266 and ESP32** (`Firebase_ESP_Client.h`) | Mobizt | Lecture/écriture sur la Realtime Database |
| **JPEGDEC** | bitbank2 | Décodage des photos JPEG téléchargées |
| **ArduinoOTA** | Espressif (inclus avec le core ESP32) | Mises à jour du firmware par Wi-Fi |

Toutes ces librairies (sauf ArduinoOTA, déjà incluse) s'installent via
**Croquis > Inclure une bibliothèque > Gérer les bibliothèques**, en
recherchant leur nom exact ci-dessus.

### ⚠️ TFT_eSPI nécessite une configuration manuelle

TFT_eSPI ne se configure pas dans le sketch, mais dans un fichier
`User_Setup.h` (ou un fichier de setup personnalisé sélectionné dans
`User_Setup_Select.h`) situé dans le dossier de la librairie
(`Arduino/libraries/TFT_eSPI/`). C'est là que sont définies les broches SPI
de votre écran (`TFT_MOSI`, `TFT_SCLK`, `TFT_CS`, `TFT_DC`, `TFT_RST`,
`TOUCH_CS`, etc.) et le pilote utilisé (ex. `ILI9488_DRIVER`). Ce fichier
est **spécifique à votre câblage** et n'est pas fourni dans ce dépôt.
Sauvegardez-le une fois qu'il fonctionne — c'est le fichier qu'on oublie
toujours et qu'on regrette de ne pas avoir noté quelque part.

### ⚠️ Firebase-ESP-Client est un projet "deprecated"

L'auteur (Mobizt) a mis cette librairie en pause au profit d'une nouvelle
librairie asynchrone, **FirebaseClient**, avec une API différente. La
version utilisée ici (`Firebase_ESP_Client.h`, `FirebaseData`,
`FirebaseAuth`, `Firebase.RTDB.getJSON()`, etc.) reste fonctionnelle et
stable, et c'est celle sur laquelle ce firmware est basé — ne l'installez
pas par erreur à la place de l'autre lors de la recherche dans le
Gestionnaire de bibliothèques (les deux noms se ressemblent). Migrer vers la
nouvelle librairie demanderait de réécrire une bonne partie des appels
réseau du firmware ; ce n'est pas fait dans ce dépôt.

## Bibliothèque web (app.js)

`webapp/app.js` utilise le **SDK Firebase JS v10 en mode modules**, chargé
directement depuis un CDN (aucune installation locale nécessaire) :

```js
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
```

Si vous voulez une version plus récente du SDK, changez simplement le
numéro de version dans les 3 lignes d'import en haut du fichier — l'API
utilisée ici (`getDatabase`, `ref`, `set`, `push`, `onValue`,
`signInAnonymously`) est stable sur les versions 10.x.

## Résumé express (checklist)

- [ ] Core **esp32** (Espressif) installé, version 3.1.x recommandée
- [ ] Carte sélectionnée correspondant à votre module ESP32
- [ ] **Partition Scheme** avec OTA activé si vous comptez utiliser les mises
      à jour par Wi-Fi (voir docs/OTA.md)
- [ ] `TFT_eSPI` installée **et** son `User_Setup.h` configuré pour votre
      écran
- [ ] `WiFiManager`, `Firebase Arduino Client Library for ESP8266 and ESP32`
      (Mobizt), `JPEGDEC` installées
- [ ] `firmware/LDR_Box.ino` compile sans erreur avant de tenter le premier
      flash
