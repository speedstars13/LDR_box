# Configurer Firebase pour LDR Box

LDR Box utilise **Firebase Realtime Database** comme "colle" entre les deux
boitiers ESP32, l'application web, et le script de synchronisation Google
Agenda. Tout transite par une seule base de données JSON en temps réel.

## 1. Créer le projet Firebase

1. Allez sur [console.firebase.google.com](https://console.firebase.google.com/)
   et connectez-vous avec un compte Google.
2. Cliquez sur **Ajouter un projet**, donnez-lui un nom (ex. `ldr-box`), et
   suivez l'assistant (Google Analytics est optionnel, vous pouvez le
   désactiver).

## 2. Activer l'authentification anonyme

L'application web et le firmware se connectent tous les deux en mode
**anonyme** (pas de mot de passe à gérer) :

1. Dans le menu de gauche, allez dans **Build > Authentication**.
2. Onglet **Sign-in method**, cliquez sur **Anonymous**, activez-le, enregistrez.

## 3. Créer la Realtime Database

1. Menu de gauche : **Build > Realtime Database**.
2. Cliquez sur **Créer une base de données**.
3. Choisissez une région proche de vous (ex. `europe-west1`).
4. Démarrez en **mode test** (on ajustera les règles à l'étape 5).

## 4. Récupérer vos identifiants

1. Cliquez sur l'icône ⚙️ à côté de "Vue d'ensemble du projet" > **Paramètres du projet**.
2. Onglet **Général**, section **Vos applications** : cliquez sur l'icône
   `</>` (Web) pour enregistrer une application web si ce n'est pas déjà fait
   (nom libre, pas besoin de Firebase Hosting).
3. Vous obtenez un objet `firebaseConfig` avec :
   - `apiKey`
   - `authDomain`
   - `databaseURL`
   - `projectId`
4. Reportez ces valeurs :
   - Dans `webapp/app.js`, tout en haut, dans `firebaseConfig`.
   - Dans `firmware/LDR_Box.ino`, dans les macros `API_KEY` et
     `DATABASE_URL` (bloc "CONFIGURATION FIREBASE" en haut du fichier).
   - Dans `google-apps-script/sync_calendar.gs`, dans `API_KEY` et `DATABASE_URL`.

   `API_KEY` et `apiKey` sont la même valeur ; `DATABASE_URL` correspond au
   `databaseURL` de la config web (sans le `/devices/...` à la fin, celui-ci
   est ajouté automatiquement par le code).

## 5. Règles de sécurité

Dans **Realtime Database > Règles**, remplacez le contenu par :

```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}
```

Cela autorise toute personne **authentifiée** (même en anonyme) à lire et
écrire l'intégralité de la base. C'est suffisant pour un projet privé à deux
personnes, mais gardez à l'esprit que :

- N'importe qui connaissant votre `apiKey` peut techniquement se connecter en
  anonyme et lire/écrire vos données — ne réutilisez pas ce projet Firebase
  pour autre chose de sensible.
- Pour un usage plus large ou plus exposé, il faudrait des règles plus fines
  (par exemple restreindre l'écriture à des chemins précis selon l'UID de
  l'utilisateur), ce qui sort du cadre de ce petit projet.

## 6. Comprendre la structure de données

LDR Box organise tout sous deux racines :

```
/devices/<id_appareil>/current/
    text                  → dernier message recu par ce boitier
    photo                 → URL Cloudinary de la derniere photo
    emploi_du_temps       → URL Cloudinary de l'emploi du temps
    reaction              → derniere reaction envoyee PAR l'autre boitier
    dessin                → dernier dessin recu (segments serialises)
    poke                  → dernier "poke" recu
    msg_secret_texte      → texte de la capsule temporelle
    msg_secret_timestamp  → date d'ouverture de la capsule

/devices/<id_appareil>/agenda/
    event_0, event_1, ... → evenements/taches (voir google-apps-script/sync_calendar.gs)

/partage/
    retrouvaille_timestamp  → date cible du compte a rebours (les deux boitiers)
    start_retrouvaille      → date de depart, pour la jauge de progression
    plante/                 → etat de la plante virtuelle (partagee)
    journal_jour/            → journal du jour pour l'Album 22h
    journal_jour_meta/       → compteur d'entrees du journal
    toucher/                 → etat "toucher en direct" de chaque boitier

/history/
    (liste, generee par push()) → flux affiche dans l'appli web
```

`<id_appareil>` correspond à ``MON_ID` / `PARTENAIRE_ID` définis dans
`firmware/LDR_Box.ino` (par défaut `appareil_a` / `appareil_b`) — gardez ces
identifiants cohérents entre le firmware, `app.js` et le script Google
Agenda.

## 7. Dépannage rapide

- **Le boitier n'arrive pas à se connecter à Firebase** : vérifiez
  `API_KEY`/`DATABASE_URL`, et que l'authentification anonyme est bien
  activée (étape 2).
- **L'appli web affiche une erreur d'auth** : ouvrez la console développeur
  du navigateur (F12), l'erreur Firebase y est généralement explicite.
- **Rien ne se synchronise mais pas d'erreur visible** : vérifiez les règles
  de sécurité (étape 5) — une règle `false` bloque silencieusement les
  écritures côté client web.
