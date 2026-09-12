# LDR_box
# LDR Box

A connected device for long-distance couples (or any pair of people): an ESP32 touchscreen displaying messages, photos, a shared calendar, schedules, a reunion countdown, a time capsule, a small virtual plant that grows as you interact, an evening album, and a subtle signal ("poke") to say "I'm thinking of you" without typing a word.

Two identical devices (one per person) communicate via a shared Firebase database. A small web app serves as the sending interface (text, photos, time capsules...), and a Google Apps Script synchronizes Google Calendar and Google Tasks data to each device.

## Architecture

```
[Web App (app.js)] ---> [Firebase Realtime Database] <--- [ESP32 Device #1 (Person A)]
[Google Calendar/Tasks] -> [   via Apps Script       ]  --> [ESP32 Device #2 (Person B)]
                                                            [Cloudinary (photo hosting)]
```

- The **web app** (`webapp/app.js`) sends messages, photos, and time capsules to Firebase and uploads photos to Cloudinary (which returns a URL used by the ESP32).
- Each **ESP32 device** reads its own `/devices/<my_id>/current` node, writes reactions, drawings, or pokes to the partner's node, and reads/writes shared data in `/partage/...` (countdown, virtual plant, daily log, "live touch").
- The **Google Apps Script** pushes calendar and Google Tasks data to `/devices/<my_id>/agenda` during each scheduled execution. ## Repository Contents

```
firmware/
  LDR_Box.ino           ESP32 firmware (TFT touchscreen + Firebase + OTA)
webapp/
  index.html             Web app page (forms + history)
  app.js                 Web app logic (Firebase + Cloudinary)
google-apps-script/
  sync_calendar.gs       Script to paste into script.google.com, linked to your calendar
docs/
  SETUP_FIREBASE.md      Create the Firebase project, database, and rules
  SETUP_CLOUDINARY.md    Create the Cloudinary account and upload preset
  LIBRARIES.md           Arduino libraries and their versions
  OTA.md                 Update firmware via Wi-Fi after installation
```

## Quick Start

1. Read `docs/SETUP_FIREBASE.md` and create your Firebase project.
2. Read `docs/SETUP_CLOUDINARY.md` and create your account/preset.
3. In `webapp/app.js`, replace the `firebaseConfig` block and the `CLOUDINARY_CLOUD_NAME` / `CLOUDINARY_UPLOAD_PRESET` values ​​with your own.
4. In `firmware/LDR_Box.ino`, replace the **CONFIGURATION TO CUSTOMIZE** block at the top of the file (API_KEY, DATABASE_URL, device identity). For the second device, swap `MON_ID`/`PARTENAIRE_ID` and `NOM_PROPRIETAIRE`/`NOM_PARTENAIRE`.
5. Install the Arduino libraries listed in `docs/LIBRARIES.md`, configure `TFT_eSPI` for your screen wiring, select a **Partition Scheme with OTA** from the Tools menu if you want Wi-Fi updates (see `docs/OTA.md`), then flash via USB.
6. In `google-apps-script/sync_calendar.gs`, update the `API_KEY`/`DATABASE_URL`/device ID, paste the code into [script.google.com](https://script.google.com), authorize access to Calendar and Tasks, and add a time-based trigger (Triggers → Time-driven).

## Notes

- The exact Firebase data schema (all keys used) is detailed in `docs/SETUP_FIREBASE.md`.
- `webapp/index.html` includes minimal self-contained styling (no external CSS dependencies)—feel free to customize colors, "Person A/B" labels, etc.
