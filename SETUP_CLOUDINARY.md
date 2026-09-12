# Configurer Cloudinary pour LDR Box

Les photos et l'emploi du temps ne transitent pas par Firebase (trop lourd
pour la Realtime Database) : l'appli web les envoie à **Cloudinary**, qui
héberge l'image et permet de demander des versions redimensionnées à la
volée via l'URL. C'est cette URL (pas le fichier lui-même) qui est ensuite
stockée dans Firebase, et c'est elle que l'ESP32 télécharge pour l'afficher.

## 1. Créer un compte

1. Allez sur [cloudinary.com](https://cloudinary.com/) et créez un compte
   gratuit (le plan gratuit est largement suffisant pour cet usage).
2. Une fois connecté, votre **Dashboard** affiche en haut de page votre
   **Cloud name** — notez-le, c'est `CLOUDINARY_CLOUD_NAME` dans `app.js`.

## 2. Créer un "Upload Preset" non signé

L'appli web envoie les photos directement depuis le navigateur, sans passer
par un serveur qui signerait la requête. Cloudinary a besoin d'un **upload
preset non signé** pour autoriser ça :

1. Dans le Dashboard, allez dans **Settings** (roue crantée) > **Upload**.
2. Descendez jusqu'à **Upload presets**, cliquez sur **Add upload preset**.
3. **Signing Mode** : choisissez **Unsigned**.
4. Donnez-lui un nom simple (ex. `ldrbox_unsigned`) — c'est ce nom qui va
   dans `CLOUDINARY_UPLOAD_PRESET` dans `app.js`.
5. Optionnel mais recommandé, dans les options du preset :
   - **Folder** : imposez un dossier fixe (ex. `ldrbox/`) pour garder vos
     uploads organisés.
   - **Allowed formats** : limitez à `jpg,png,heic` par exemple, pour éviter
     qu'un tiers qui découvrirait votre cloud name + preset n'y dépose
     n'importe quel type de fichier.
6. Enregistrez.

⚠️ Un preset non signé est utilisable par **quiconque connaît son nom et
votre cloud name** (les deux sont visibles dans le code source de la page
web). Ce n'est pas un problème pour un usage privé entre deux personnes,
mais évitez d'y stocker quoi que ce soit de sensible, et surveillez de temps
en temps votre quota/dashboard Cloudinary.

## 3. Renseigner les valeurs dans le code

Dans `webapp/app.js` :

```js
const CLOUDINARY_CLOUD_NAME = "votre_cloud_name";
const CLOUDINARY_UPLOAD_PRESET = "votre_preset";
```

C'est la **seule** modification Cloudinary à faire côté code : le firmware
ESP32 ne connaît pas Cloudinary directement, il se contente de télécharger
l'URL qu'il trouve dans Firebase.

## 4. Comprendre le redimensionnement à la volée

Après l'upload, Cloudinary renvoie une URL du type :

```
https://res.cloudinary.com/votre_cloud_name/image/upload/v1234567890/nom.jpg
```

`app.js` insère automatiquement des paramètres de transformation juste après
`/upload/` :

```
.../upload/w_480,h_320,c_fill,fl_progressive:none,f_jpg/v1234567890/nom.jpg
```

- `w_480,h_320` : largeur/hauteur cible en pixels.
- `c_fill` : recadre pour remplir exactement ce format (peut rogner l'image).
- `fl_progressive:none` : désactive le JPEG progressif (le décodeur ESP32
  ne le supporte pas).
- `f_jpg` : force le format JPEG en sortie.

Le firmware redemande lui-même une taille différente selon la page où
l'image s'affiche (miniature, plein écran, portrait dans l'Album...) via la
fonction `urlPourTaille()` — inutile de changer quoi que ce soit ici, sauf
si votre écran a une résolution très différente de 480×320.

## 5. Dépannage rapide

- **"Erreur Cloudinary HTTP" dans la console du navigateur** : vérifiez que
  le preset est bien en mode **Unsigned**, et que le nom est identique des
  deux côtés (Cloudinary et `app.js`).
- **L'ESP32 n'arrive pas à afficher l'image** : vérifiez que
  `fl_progressive:none` et `f_jpg` sont bien présents dans l'URL générée
  (visibles dans Firebase, champ `photo`) — sans ça, certains JPEG
  progressifs font planter le décodeur embarqué.
- **Quota dépassé** : le plan gratuit Cloudinary a une limite mensuelle de
  stockage/bande passante ; largement suffisante pour un usage à deux, mais
  surveillable depuis le Dashboard.
