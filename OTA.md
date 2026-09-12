# Mises à jour à distance (OTA)

Le firmware embarque `ArduinoOTA`, ce qui permet de renvoyer une nouvelle version du code par Wi-Fi une fois le boîtier installé, sans avoir à le démonter pour rebrancher un câble USB.

## Prérequis avant le premier flash

**Le tout premier flash doit obligatoirement se faire par USB** — OTA ne sert qu'aux mises à jour *suivantes*.

Dans Arduino IDE, menu **Outils → Partition Scheme**, choisis un schéma dont le nom contient **"OTA"** (par exemple *"Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"*). Un schéma qui indique explicitement *"No OTA"* dans son nom ne réserve pas de deuxième emplacement mémoire pour recevoir le nouveau firmware : la mise à jour par Wi-Fi échouera même si le code est correct.

⚠️ **Compromis à connaître** : les schémas avec OTA laissent en général beaucoup moins de place pour SPIFFS (le stockage utilisé temporairement pour les photos téléchargées) que les schémas sans OTA. Si l'upload de certaines photos échoue après ce changement, c'est probablement la cause — il faudrait alors réduire la taille demandée aux photos dans `app.js`/le firmware, ou choisir un schéma avec un peu plus de SPIFFS si ta carte a assez de flash.

Avant de flasher, personnalise dans `firmware/LDR_Box.ino` :

```cpp
#define OTA_PASSWORD "changez-moi"
```

## Utiliser OTA au quotidien

1. Assure-toi que le boîtier est allumé et connecté à ton réseau Wi-Fi (l'écran affiche normalement l'interface).
2. Ouvre Arduino IDE, va dans **Outils → Port**. Après quelques secondes, un port réseau doit apparaître, du type *"LDR-Box-\<NOM_PROPRIETAIRE\> at 192.168.x.x"*.
3. Sélectionne ce port réseau (au lieu du port série USB habituel).
4. Clique sur **Téléverser** comme d'habitude. L'IDE demande le mot de passe OTA : entre celui défini dans `OTA_PASSWORD`.
5. L'écran du boîtier affiche une barre de progression pendant le transfert ("Mise a jour OTA..."), puis redémarre automatiquement une fois terminé.

Ne coupe pas l'alimentation pendant le transfert.

## Si le port réseau n'apparaît pas

- Vérifie que ton ordinateur et le boîtier sont bien sur le **même réseau Wi-Fi** (pas sur le réseau invité, pas sur un VPN qui isolerait le trafic local).
- **Sous Windows**, la découverte du port réseau dépend de mDNS/Bonjour : installe *Bonjour Print Services* (gratuit, éditeur Apple) si le port n'apparaît jamais après plusieurs redémarrages du boîtier et de l'IDE.
- En dernier recours, certains outils permettent d'envoyer un firmware OTA en ligne de commande en visant directement l'adresse IP du boîtier (via le script `espota.py` fourni avec le core ESP32) — plus avancé, à réserver si le port réseau ne se résout vraiment jamais.

## Limite connue

Le firmware contient plusieurs `delay()` bloquants assez courts (confirmations visuelles, animations). Si un transfert OTA démarre pendant l'un de ces instants précis, il peut ralentir ou, dans de rares cas, échouer — relance simplement le téléversement si ça arrive. Évite de lancer une mise à jour OTA en même temps que tu manipules activement l'écran tactile.
