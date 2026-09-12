/*
  LDR BOX - Style Appli Web
  -----------------------------------
  - Palette couleurs : Violet profond (bg), Gold, Coral, Lavender
  - UI avec encadrés adoucis (Panel)
  - Corrections bugs (Agenda scroll, luminosité au boot, plantage dessin vide)
  - Ajouts : Dessin fluide, Gomme épaisse, Jauge Retrouvailles, Corrections UI

  --- CORRECTIONS DE CETTE VERSION ---
  1) Roue de luminosité : sens de rotation VISUEL inversé (l'encodeur physique
     n'a pas été touché) + centrage vertical corrigé.
  2) Réaction sur une photo : affichage clair "Envoyé !" directement sur le
     bouton pressé (l'ancien toast ne s'affichait jamais car il était bloqué
     tant que la photo est en plein écran).
  3) Mode nuit (assombrissement automatique 23h-7h) supprimé. Seul le mode
     veille par paliers d'inactivité est conservé.
*/
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <JPEGDEC.h>
#include <SPIFFS.h>
#include <time.h>
#include <ArduinoOTA.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// ---- Ecran & Wi-Fi ----
TFT_eSPI tft = TFT_eSPI();
WiFiManager wm;
JPEGDEC jpeg;

// ---- Calibration tactile ----
uint16_t calData[5] = { 334, 3520, 274, 3479, 1 };

// ==========================================================
// ---- NOUVELLE PALETTE DE COULEURS (Inspirée du CSS) ----
// ==========================================================
#define COULEUR_FOND   tft.color565(51, 25, 90)    // --bg-1 (#33195a)
#define COULEUR_PANEL  tft.color565(36, 18, 63)    // --bg-2 (#24123f)
#define COULEUR_TEXTE  tft.color565(243, 236, 251) // --text (#f3ecfb)
#define COULEUR_DIM    tft.color565(201, 188, 221) // --text-dim (#c9bcdd)
#define COULEUR_CADRE  tft.color565(201, 167, 232) // --lavender (#c9a7e8) - Bordures
#define COULEUR_OR     tft.color565(255, 214, 107) // --gold (#ffd66b) - Titres/Boutons
#define COULEUR_CORAL  tft.color565(255, 171, 122) // --coral (#ffab7a) - Events multijours
#define COULEUR_OK     tft.color565(123, 232, 184) // --ok (#7be8b8)
#define COULEUR_BLEU   tft.color565(108, 181, 246) // Taches / dates limites

// ==========================================================
// ---- PALETTE "NATURE" (reservee a la page Plante) ----
// ==========================================================
#define COULEUR_PLANTE_FOND        tft.color565(20, 46, 30)   // vert tres fonce (sain)
#define COULEUR_PLANTE_PANEL       tft.color565(30, 64, 42)   // vert fonce panel (sain)
#define COULEUR_PLANTE_CADRE       tft.color565(122, 189, 100) // vert clair bordure (sain)
#define COULEUR_PLANTE_TEXTE       tft.color565(230, 245, 225) // blanc verdatre (sain)
#define COULEUR_PLANTE_TIGE        tft.color565(76, 153, 68)   // vert tige (sain)
#define COULEUR_PLANTE_FEUILLE     tft.color565(94, 186, 82)   // vert feuille (sain)
#define COULEUR_PLANTE_FLEUR       tft.color565(255, 156, 191) // rose petales (sain)
#define COULEUR_PLANTE_FANE_FOND   tft.color565(46, 38, 28)   // brun-gris fond (fanee)
#define COULEUR_PLANTE_FANE_PANEL  tft.color565(60, 50, 38)   // brun-gris panel (fanee)
#define COULEUR_PLANTE_FANE_CADRE  tft.color565(150, 130, 100) // beige bordure (fanee)
#define COULEUR_PLANTE_FANE_TIGE   tft.color565(120, 100, 70)  // tige seche (fanee)
#define COULEUR_PLANTE_FANE_FEUILLE tft.color565(140, 120, 80) // feuille seche (fanee)
#define COULEUR_PLANTE_POT         tft.color565(150, 90, 60)  // terre cuite du pot

// ==========================================================
// ---- CONFIGURATION A PERSONNALISER (voir docs/SETUP_FIREBASE.md) ----
// ==========================================================
// Identifiants de CE boitier et de son partenaire. Sur le PREMIER boitier,
// laissez tel quel. Sur le DEUXIEME boitier, inversez MON_ID/PARTENAIRE_ID
// et NOM_PROPRIETAIRE/NOM_PARTENAIRE. Ce sont les deux SEULES lignes a
// inverser entre les deux boitiers : tous les chemins Firebase ci-dessous
// sont construits automatiquement a partir d'elles.
#define MON_ID              "appareil_a"
#define PARTENAIRE_ID       "appareil_b"
#define NOM_PROPRIETAIRE    "PersonneA"
#define NOM_PARTENAIRE      "PersonneB"

// Cle API et URL de votre projet Firebase (Realtime Database). Voir
// docs/SETUP_FIREBASE.md pour savoir ou les trouver dans la console.
#define API_KEY       "VOTRE_CLE_API_FIREBASE"
#define DATABASE_URL  "https://VOTRE-PROJET-default-rtdb.VOTRE-REGION.firebasedatabase.app"

// Mot de passe pour les mises a jour a distance (OTA). A changer avant
// le premier flash - voir docs/OTA.md.
#define OTA_PASSWORD "changez-moi"

// ---- Configuration Firebase (chemins generes automatiquement, ne pas toucher) ----
#define CHEMIN_BASE   "/devices/" MON_ID "/current"
#define CHEMIN_AGENDA "/devices/" MON_ID "/agenda"
#define CHEMIN_AUTRE  "/devices/" PARTENAIRE_ID "/current/reaction"
#define CHEMIN_AUTRE_DESSIN "/devices/" PARTENAIRE_ID "/current/dessin"
#define CHEMIN_AUTRE_POKE "/devices/" PARTENAIRE_ID "/current/poke"
#define CHEMIN_RETROUVAILLES "/partage/retrouvaille_timestamp"
#define CHEMIN_START_RETROUVAILLES "/partage/start_retrouvaille"
#define CHEMIN_PARTAGE "/partage"
#define CHEMIN_PLANTE "/partage/plante"
#define CHEMIN_PLANTE_NIVEAU "/partage/plante/niveau"
#define CHEMIN_PLANTE_DERNIERE_INTERACTION "/partage/plante/derniere_interaction"
#define CHEMIN_PLANTE_JOUR_CREDIT "/partage/plante/jour_credit"
#define CHEMIN_PLANTE_CREDITS_JOUR "/partage/plante/credits_jour"
#define CHEMIN_JOURNAL_JOUR "/partage/journal_jour"
#define CHEMIN_JOURNAL_JOUR_META "/partage/journal_jour_meta"
#define CHEMIN_TOUCHER "/partage/toucher"
#define CHEMIN_TOUCHER_MOI "/partage/toucher/" MON_ID
#define CLE_TOUCHER_PARTENAIRE PARTENAIRE_ID

FirebaseData fbdo;
FirebaseData fbdoPlante; // Objet dedie a la Plante (evite tout conflit avec fbdo,
                         // utilise pendant le parsing du JSON principal)
FirebaseData fbdoJournal; // Objet dedie au Journal/Album (meme raison)
FirebaseData fbdoToucher; // Objet dedie au Toucher en Direct (meme raison)
FirebaseAuth auth;
FirebaseConfig config;

// ---- Broches Encodeur & Retroeclairage ----
#define PIN_CLK        4
#define PIN_DT         5
#define PIN_SW         6
#define PIN_BACKLIGHT 14

const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;
volatile int luminosite = 100;
int lastCLK;

// ---- Variables pour les Pop-ups (Luminosité & Notifications) ----
int derniereLuminositeAffichee = 100;
unsigned long timerPopup = 0;
bool affichagePopupLuminosite = false;
String messageToast = "";
bool affichageToast = false;
unsigned long timerToast = 0;

// ---- Horloge Globale ----
int memoHeureAffichage = -1;
int memoMinuteAffichage = -1;

// ---- Structure de donnees pour l'agenda ----
struct EvenementAgenda {
  String date;
  String endDate;
  String temps;
  String titre;
  bool isMultiDay;
  bool task;
  int64_t timestamp;
};

const int MAX_EVENEMENTS = 15;
EvenementAgenda evenements[MAX_EVENEMENTS];
int nbEvenements = 0;

// ---- Structure de donnees pour le dessin ----
struct SegmentDessin {
  int16_t x1, y1, x2, y2;
  uint16_t couleur;
};

const int MAX_SEGMENTS = 700;
SegmentDessin segmentsEnvoi[MAX_SEGMENTS];
int nbSegmentsEnvoi = 0;

SegmentDessin segmentsRecus[MAX_SEGMENTS];
int nbSegmentsRecus = 0;
String signatureDessinRecu = "";

// ---- Variables de contenu Firebase ----
String dernierTexte = "";
String dernierePhotoURL = "";
String dernierEmploiDuTempsURL = "";
String derniereReactionRecue = "";
String dernierPokeRecu = "";

// --- Variables WebApp (Retrouvailles & Capsule Temporelle) ---
int64_t targetRetrouvailles = 0;
int64_t startRetrouvailles = 0; // Ajout pour la jauge de progression
int64_t targetMessageSecret = 0;
String texteMessageSecret = "";

// ---- Variables Page Relation (Pour éviter le clignotement) ----
bool pageRelationInit = false;
int memoJours = -1, memoHeures = -1, memoMinutes = -1;
bool memoSynchro = false, memoCapsuleOuverte = false;
String memoTexteSecret = "", memoDernierTexte = "";

// ---- Variables pour la page Dessin ----
bool modeDessinActif = false;
uint16_t couleurDessin = TFT_WHITE;

// ---- Variables de contenu affiche (pour comparaison) ----
String dernierePhotoAffichee = "###INIT###";
String dernierTexteAffiche = "###INIT###";
String dernierAgendaAccueilAffiche = "###INIT###";

unsigned long dernierCheck = 0;
const long intervalleCheck = 2000;

// ---- Notifications (badge) ----
bool notificationMessage = false;
bool notificationDessin = false;
int pageNotificationCible = 0;

// ---- Variables Plante Virtuelle ----
int plantNiveauBase = 50;              // Niveau (0-100) synchronise depuis Firebase
int64_t plantDerniereInteraction = 0;  // Timestamp de la derniere interaction (Firebase)
bool pagePlanteInit = false;
int memoPlantNiveauAffiche = -1;
bool memoPlantMalade = false;
int memoPlanteAlerteAccueil = -1; // -1 = jamais dessine, 0 = ok, 1 = malade

// ---- Variables Journal / Album du jour ----
struct EntreeJournal {
  String type;     // "message" ou "photo"
  String contenu;  // texte du message, ou URL de la photo
  String auteur;   // nom de la personne qui a envoye
  int64_t timestamp;
};
const int MAX_ENTREES_JOURNAL = 20;
EntreeJournal journalDuJour[MAX_ENTREES_JOURNAL];
int nbEntreesJournal = 0;
int albumIndexCourant = 0;
unsigned long albumDernierChangement = 0;
const unsigned long ALBUM_INTERVALLE_DEFILEMENT = 6000; // 6s entre chaque entree affichee
bool albumAutoAfficheAujourdHui = false;
int64_t albumJourAutoAffiche = -1;

// ---- Variables Poke / Radar (double-clic encodeur) ----
const unsigned long FENETRE_DOUBLE_CLIC = 350;
unsigned long dernierRelachementCourt = 0;
bool clicSimpleEnAttente = false;

// ---- Variables Toucher en Direct ----
bool jeTouchaisAvant = false;      // pour ne notifier Firebase qu'au changement d'etat
bool partenaireTouche = false;
bool synchroDejaSignalee = false;  // evite de re-notifier en boucle tant que ca reste synchrone

// ---- Mode veille par paliers ----
unsigned long derniereInteraction = 0;

// ---- Wi-Fi hors ligne ----
bool wifiPerdu = false;
int tentativesReconnexion = 0;
unsigned long dernierEssaiWifi = 0;
bool portailWifiActif = false;

// ==========================================================
// ---- GEOMETRIE DE LA PAGE ACCUEIL ----
// ==========================================================
const int HEADER_H = 30;
const int FOOTER_H = 22;

const int PHOTO_X = 10;
const int PHOTO_Y = HEADER_H + 5;
const int PHOTO_W = 200;
const int PHOTO_H = 320 - HEADER_H - FOOTER_H - 15;

const int TEXTE_X = 220;
const int TEXTE_Y = HEADER_H + 5;
const int TEXTE_W = 250;
const int TEXTE_H = (PHOTO_H / 2) - 5;

const int AGENDA_X = TEXTE_X;
const int AGENDA_Y = TEXTE_Y + TEXTE_H + 10;
const int AGENDA_W = TEXTE_W;
const int AGENDA_H = TEXTE_H;

// ---- Etat navigation ----
bool photoPleinEcran = false;
int pageActuelle = 0;
const int NB_PAGES = 7;

bool toucheEnCours = false;
int touchStartX = -1, touchStartY = -1;
int touchLastX = -1, touchLastY = -1;
const int SEUIL_TAP = 20;
const int ZONE_NAV_LARG = 60;
int scrollAgendaY = 0;
int maxScrollAgenda = 0;

void chargerAgenda();
void dessinerPageAgenda();
void afficherPageSelonIndex();
void forcerReaffichageAccueil();
void afficherAnimationReaction(String type);
void dessinerPageDessin();
void dessinerPageRelation();
void dessinerBadgeNotif();
void dessinerRoueLuminosite();
void dessinerToast();
void declencherToast(const char* message);
void mettreAJourBadgeNotif();
void ouvrirNotification();
void dessinerStatutWifi(const char* titre, const char* detail);
void masquerCoinsImage(int x, int y, int w, int h, int rayon, uint16_t couleurFond);
void restaurerEnteteApresOverlay();
void dessinerPagePlante();
void dessinerPlantePixelArt(int cx, int solY, int niveau, bool malade);
void dessinerSolPlante(int cx, int solY, int panelX, int panelW, bool malade);
void dessinerDecorPlante(int panelX, int panelY, int panelW, bool malade);
void arroserPlante();
void rafraichirAlertePlanteAccueil();
void dessinerPageAlbum();
void chargerJournalDuJour();
void ajouterAuJournal(const char* type, const String &contenu);
void envoyerPoke();
void declencherAnimationPoke();
uint16_t interpolerCouleur(uint16_t c1, uint16_t c2, float t);
int dessinerCoeur(int x, int y, int taille, uint16_t couleur);

void IRAM_ATTR onEncoderChange() {
  int clkState = digitalRead(PIN_CLK);
  int dtState = digitalRead(PIN_DT);
  if (clkState != lastCLK) {
    if (dtState != clkState) luminosite -= 3;
    else luminosite += 3;
    luminosite = constrain(luminosite, 0, 100);
  }
  lastCLK = clkState;
}

// ==========================================================
// ---- DESSIN DES POP-UPS (ROUE & TOAST) ----
// ==========================================================
void dessinerRoueLuminosite() {
  affichageToast = false;
  affichagePopupLuminosite = true;
  timerPopup = millis();

  const int x = 125, y = 3, w = 140, h = 27;
  tft.fillRoundRect(x, y, w, h, 8, COULEUR_PANEL);
  tft.drawRoundRect(x, y, w, h, 8, COULEUR_CADRE);

  // --- CORRECTION : centrage vertical précis de la roue de points ---
  const int cx = x + 18;
  const int cy = y + (h + 1) / 2; // arrondi correct (avant: tronqué, legerement decale)

  for (int i = 0; i < 10; i++) {
    // --- CORRECTION : sens de rotation VISUEL inversé (on ne touche pas a l'encodeur) ---
    float angle = (-90 + i * 36) * 0.0174533; // sens horaire a l'ecran
    uint16_t couleur = (luminosite >= (i + 1) * 10) ? COULEUR_OR : COULEUR_DIM;
    tft.fillCircle(cx + cos(angle) * 9, cy + sin(angle) * 9, 2, couleur);
  }

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_OR, COULEUR_PANEL);
  tft.setCursor(x + 37, y + 9); tft.print("LUM");
  tft.setTextColor(COULEUR_TEXTE, COULEUR_PANEL);
  tft.setCursor(x + 64, y + 9); tft.printf("%3d%%", luminosite);
}

void declencherToast(const char* message) {
  affichagePopupLuminosite = false;
  if (photoPleinEcran) return; // Ne jamais cacher ni recharger la photo plein ecran.
  messageToast = message;
  affichageToast = true;
  timerToast = millis();
  derniereInteraction = millis();
  dessinerToast();
}

void dessinerToast() {
  int w = 140;
  int h = 27;
  int x = 125;
  int y = 3;

  tft.fillRoundRect(x, y, w, h, 8, COULEUR_PANEL);
  tft.drawRoundRect(x, y, w, h, 8, COULEUR_CADRE);

  tft.setTextColor(COULEUR_TEXTE, COULEUR_PANEL);
  tft.setTextSize(1);
  int cursorX = x + (w - (strlen(messageToast.c_str()) * 6)) / 2;
  tft.setCursor(max(x + 5, cursorX), y + 9);
  tft.print(messageToast);
}

// ==========================================================
// ---- POKE / RADAR (double-clic encodeur) ----
// ==========================================================

// Interpole entre 2 couleurs RGB565 (t=0 -> c1, t=1 -> c2), pour un fondu doux.
uint16_t interpolerCouleur(uint16_t c1, uint16_t c2, float t) {
  if (t < 0) t = 0; if (t > 1) t = 1;
  uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  uint8_t r = r1 + (uint8_t)((r2 - r1) * t);
  uint8_t g = g1 + (uint8_t)((g2 - g1) * t);
  uint8_t b = b1 + (uint8_t)((b2 - b1) * t);
  return (r << 11) | (g << 5) | b;
}

// Petit coeur qui apparait tout en douceur (fondu entree/sortie) dans la
// pastille en haut de l'ecran, sans jamais clignoter ni etre agressif.
void declencherAnimationPoke() {
  affichagePopupLuminosite = false;
  affichageToast = false;
  if (photoPleinEcran) return; // On ne touche jamais a la photo plein ecran

  const int x = 125, y = 3, w = 140, h = 27;
  const int cx = x + w / 2 - 8, cy = y + h / 2 - 6;

  for (int i = 0; i <= 8; i++) {
    float t = i / 8.0;
    tft.fillRoundRect(x, y, w, h, 8, COULEUR_PANEL);
    tft.drawRoundRect(x, y, w, h, 8, COULEUR_CADRE);
    dessinerCoeur(cx, cy, 16, interpolerCouleur(COULEUR_PANEL, TFT_PINK, t));
    delay(35);
  }
  delay(450); // le coeur reste un court instant, bien visible
  for (int i = 8; i >= 0; i--) {
    float t = i / 8.0;
    tft.fillRoundRect(x, y, w, h, 8, COULEUR_PANEL);
    tft.drawRoundRect(x, y, w, h, 8, COULEUR_CADRE);
    dessinerCoeur(cx, cy, 16, interpolerCouleur(COULEUR_PANEL, TFT_PINK, t));
    delay(35);
  }
  restaurerEnteteApresOverlay();
}

// Envoie un "Je pense a toi" a l'autre boitier (sans texte) et affiche la
// meme animation en confirmation locale.
void envoyerPoke() {
  Firebase.RTDB.setString(&fbdo, CHEMIN_AUTRE_POKE, "poke_" + String(millis()));
  declencherAnimationPoke();
}

void dessinerHorloge() {
  if (photoPleinEcran || affichagePopupLuminosite || affichageToast) return;

  time_t maintenant = time(nullptr);
  if (maintenant < 1700000000) return;

  struct tm *infos = localtime(&maintenant);

  if (infos->tm_hour == memoHeureAffichage && infos->tm_min == memoMinuteAffichage) return;
  memoHeureAffichage = infos->tm_hour;
  memoMinuteAffichage = infos->tm_min;

  int xClock = tft.width() - 65;
  int yClock = 5;
  uint16_t bg = (pageActuelle == 2) ? COULEUR_PANEL : COULEUR_FOND;

  tft.fillRect(xClock, yClock, 65, 20, bg);
  tft.setTextColor(COULEUR_DIM, bg);
  tft.setTextSize(2);
  tft.setCursor(xClock, yClock);
  tft.printf("%02d:%02d", infos->tm_hour, infos->tm_min);
}

void restaurerEnteteApresOverlay() {
  if (photoPleinEcran) return;
  uint16_t fond = (pageActuelle == 2) ? COULEUR_PANEL : COULEUR_FOND;
  tft.fillRect(120, 0, 155, HEADER_H, fond);
  if (pageActuelle == 0) {
    tft.setTextColor(COULEUR_OR, COULEUR_FOND);
    tft.setTextSize(2);
    tft.setCursor(tft.width() / 2 - 40, 5); tft.print("Accueil");
  } else if (pageActuelle == 1) {
    tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
    tft.setTextSize(1);
    tft.setCursor(tft.width() / 2 - 40, 5); tft.print("[Clic Haut]");
  }
  memoMinuteAffichage = -1;
  dessinerHorloge();
}

void appliquerLuminosite() {
  int base = luminosite;
  unsigned long inactif = millis() - derniereInteraction;
  int plafondVeille = 100;

  // --- Mode veille par paliers d'inactivite ---
  if (inactif > 9000000UL) plafondVeille = 0;        // > 2h30 : ecran eteint
  else if (inactif > 7200000UL) plafondVeille = 10;   // > 2h : 10%
  else if (inactif > 3600000UL) plafondVeille = 30;   // > 1h : 30%

  base = min(base, plafondVeille);

  // --- CORRECTION : mode nuit (23h-7h) supprime. Seul le mode veille reste. ---

  int pwmValue = map(base, 0, 100, 0, 255);
  ledcWrite(pwmChannel, pwmValue);

  if (luminosite != derniereLuminositeAffichee && !photoPleinEcran && !modeDessinActif) {
    derniereLuminositeAffichee = luminosite;
    derniereInteraction = millis();
    dessinerRoueLuminosite();
  }
}

// ---- Reinitialisation Wi-Fi + retour accueil ----
unsigned long debutAppuiBouton = 0;
bool boutonAppuye = false;
bool resetEnCours = false;
const unsigned long DUREE_APPUI_LONG = 2000;

void gererBoutonEncodeur() {
  bool etat = (digitalRead(PIN_SW) == LOW);

  if (etat && !boutonAppuye) {
    boutonAppuye = true;
    debutAppuiBouton = millis();
    derniereInteraction = millis();
  } else if (etat && boutonAppuye && !resetEnCours) {
    if (millis() - debutAppuiBouton > DUREE_APPUI_LONG) {
      resetEnCours = true;
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(20, 100);
      tft.println("Reinitialisation Wi-Fi...");
      delay(1500);
      wm.resetSettings();
      ESP.restart();
    }
  } else if (!etat && boutonAppuye) {
    unsigned long duree = millis() - debutAppuiBouton;
    boutonAppuye = false;

    if (!resetEnCours && duree < DUREE_APPUI_LONG && duree > 50) {
      // --- POKE : on ne declenche pas l'action simple immediatement, on
      //     attend une courte fenetre pour voir si un 2e clic (double-clic
      //     = poke) arrive. Sinon l'action simple s'execute plus bas. ---
      if (clicSimpleEnAttente && (millis() - dernierRelachementCourt) < FENETRE_DOUBLE_CLIC) {
        clicSimpleEnAttente = false;
        envoyerPoke();
      } else {
        clicSimpleEnAttente = true;
        dernierRelachementCourt = millis();
      }
    }
    resetEnCours = false;
  }

  // La fenetre de double-clic est ecoulee sans 2e clic : on execute l'action
  // simple normale (notification, ou retour a l'accueil).
  if (clicSimpleEnAttente && (millis() - dernierRelachementCourt) > FENETRE_DOUBLE_CLIC) {
    clicSimpleEnAttente = false;
    if (notificationMessage || notificationDessin) {
      ouvrirNotification();
    } else if (pageActuelle != 0 || photoPleinEcran) {
      photoPleinEcran = false;
      pageActuelle = 0;
      afficherPageSelonIndex();
    }
  }
}

// ---- Statut et reconnexion Wi-Fi non bloquants ----
void dessinerStatutWifi(const char* titre, const char* detail) {
  tft.fillScreen(COULEUR_FOND);
  tft.fillRoundRect(55, 78, tft.width() - 110, 160, 16, COULEUR_PANEL);
  tft.drawRoundRect(55, 78, tft.width() - 110, 160, 16, COULEUR_CADRE);
  tft.fillCircle(tft.width() / 2, 112, 16, COULEUR_OR);
  tft.drawCircle(tft.width() / 2, 112, 25, COULEUR_CADRE);
  tft.setTextColor(COULEUR_OR, COULEUR_PANEL);
  tft.setTextSize(2); tft.setCursor(82, 150); tft.println(titre);
  tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
  tft.setTextSize(1); tft.setCursor(76, 184); tft.println(detail);
  tft.setCursor(76, 205); tft.print("LDR_Box_"); tft.println(NOM_PROPRIETAIRE);
}

void verifierWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiPerdu || portailWifiActif) {
      wifiPerdu = false;
      portailWifiActif = false;
      tentativesReconnexion = 0;
      forcerReaffichageAccueil();
      declencherToast("Wi-Fi reconnecte");
    }
    return;
  }
  if (!wifiPerdu) {
    wifiPerdu = true;
    tentativesReconnexion = 0;
    dernierEssaiWifi = 0;
    declencherToast("Wi-Fi : reconnexion...");
  }
  if (portailWifiActif) {
    wm.process();
    return;
  }
  if (millis() - dernierEssaiWifi < 5000) return;
  dernierEssaiWifi = millis();
  tentativesReconnexion++;
  WiFi.reconnect();
  if (tentativesReconnexion >= 3) {
    dessinerStatutWifi("Configuration Wi-Fi", "Connectez-vous au reseau :");
    wm.setConfigPortalBlocking(false);
    wm.startConfigPortal("LDR_Box_" NOM_PROPRIETAIRE);
    portailWifiActif = true;
  }
}

// ---- Dessin JPEG ----
int offsetDessinX = 0;
int offsetDessinY = 0;

int JPEGDraw(JPEGDRAW *pDraw) {
  tft.pushImage(pDraw->x + offsetDessinX, pDraw->y + offsetDessinY, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}

String urlPourTaille(String urlOriginal, int w, int h) {
  String cle = "/upload/";
  int idxUpload = urlOriginal.indexOf(cle);
  if (idxUpload < 0) return urlOriginal;

  int idxDebutTransfo = idxUpload + cle.length();
  int idxFinTransfo = urlOriginal.indexOf('/', idxDebutTransfo);
  if (idxFinTransfo < 0) return urlOriginal;

  String nouvelleTransfo = "w_" + String(w) + ",h_" + String(h) + ",c_fill,fl_progressive:none,f_jpg";
  return urlOriginal.substring(0, idxDebutTransfo) + nouvelleTransfo + urlOriginal.substring(idxFinTransfo);
}

bool afficherPhotoDepuisURL(String url, int x, int y) {
  if (url.length() == 0) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) return false;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    File f = SPIFFS.open("/temp.jpg", FILE_WRITE);
    if (!f) { http.end(); return false; }
    size_t ecrit = http.writeToStream(&f);
    f.close();
    http.end();

    if (ecrit == 0) return false;

    File fLecture = SPIFFS.open("/temp.jpg", FILE_READ);
    size_t taille = fLecture.size();
    uint8_t* buffer = (uint8_t*)malloc(taille);
    if (!buffer) { fLecture.close(); return false; }

    fLecture.read(buffer, taille);
    fLecture.close();

    offsetDessinX = x; offsetDessinY = y;
    int rc = jpeg.openRAM(buffer, taille, JPEGDraw);
    if (rc) {
      int decodeOK = jpeg.decode(0, 0, 0);
      jpeg.close(); free(buffer);
      return decodeOK ? true : false;
    }
    free(buffer);
  }
  http.end();
  return false;
}

// ==========================================================
// ---- GESTION TEXTE & EMOJI ----
// ==========================================================
int dessinerCoeur(int x, int y, int taille, uint16_t couleur) {
  int r = taille / 4;
  tft.fillCircle(x + r, y + r, r, couleur);
  tft.fillCircle(x + taille - r, y + r, r, couleur);
  tft.fillTriangle(x, y + r, x + taille, y + r, x + taille / 2, y + taille, couleur);
  return taille + 4;
}

uint32_t decoderUTF8(const String &texte, int &i) {
  uint8_t c = texte[i];
  uint32_t codepoint = 0;
  int octets = 0;

  if ((c & 0x80) == 0x00) { codepoint = c; octets = 0; }
  else if ((c & 0xE0) == 0xC0) { codepoint = c & 0x1F; octets = 1; }
  else if ((c & 0xF0) == 0xE0) { codepoint = c & 0x0F; octets = 2; }
  else if ((c & 0xF8) == 0xF0) { codepoint = c & 0x07; octets = 3; }
  else { i++; return 0xFFFD; }

  i++;
  for (int k = 0; k < octets && i < (int)texte.length(); k++) {
    codepoint = (codepoint << 6) | (texte[i] & 0x3F); i++;
  }
  return codepoint;
}

bool estEmoji(uint32_t cp) {
  return (cp >= 0x1F300 && cp <= 0x1FAFF) || (cp >= 0x2600 && cp <= 0x27BF) ||
         (cp == 0x2764) || (cp == 0x2665);
}

char convertirAccent(uint32_t cp) {
  switch (cp) {
    case 0xE9: case 0xE8: case 0xEA: case 0xEB: return 'e';
    case 0xC9: case 0xC8: case 0xCA: case 0xCB: return 'E';
    case 0xE0: case 0xE2: case 0xE4: return 'a';
    case 0xC0: case 0xC2: case 0xC4: return 'A';
    case 0xEE: case 0xEF: return 'i';
    case 0xCE: case 0xCF: return 'I';
    case 0xF4: case 0xF6: return 'o';
    case 0xD4: case 0xD6: return 'O';
    case 0xF9: case 0xFB: case 0xFC: return 'u';
    case 0xD9: case 0xDB: case 0xDC: return 'U';
    case 0xE7: return 'c';
    case 0xC7: return 'C';
    default: return 0;
  }
}

int calculerLargeurMot(const String &texte, int depart, int tailleTexte) {
  int i = depart; int largeur = 0;
  while (i < (int)texte.length()) {
    int iLocal = i; uint32_t cp = decoderUTF8(texte, iLocal);
    if (cp == ' ' || cp == '\n') break;
    if (cp >= 0xFE00 && cp <= 0xFE0F) { i = iLocal; continue; }
    if (estEmoji(cp)) largeur += 8 * tailleTexte + 4;
    else largeur += 6 * tailleTexte;
    i = iLocal;
  }
  return largeur;
}

void afficherTexteEmoji(int xDepart, int yDepart, int largeurMax, const String &texte, uint16_t couleur, uint16_t bgColor, int tailleTexte) {
  tft.setTextColor(couleur, bgColor);
  tft.setTextSize(tailleTexte);
  int x = xDepart; int y = yDepart; int hauteurLigne = 8 * tailleTexte + 10;
  int i = 0; bool enDebutDeMot = true;

  while (i < (int)texte.length()) {
    if (enDebutDeMot) {
      int lMot = calculerLargeurMot(texte, i, tailleTexte);
      if (lMot > 0 && x + lMot > xDepart + largeurMax) { x = xDepart; y += hauteurLigne; }
      enDebutDeMot = false;
    }

    uint32_t cp = decoderUTF8(texte, i);
    if (cp >= 0xFE00 && cp <= 0xFE0F) continue;

    if (cp == ' ') { x += 6 * tailleTexte; enDebutDeMot = true; continue; }
    if (cp == '\n') { x = xDepart; y += hauteurLigne; enDebutDeMot = true; continue; }

    if (estEmoji(cp)) {
      dessinerCoeur(x, y, 8 * tailleTexte, COULEUR_CORAL);
      x += (8 * tailleTexte + 4);
    } else {
      char c = (cp < 128) ? (char)cp : convertirAccent(cp);
      if (c != 0) { tft.setCursor(x, y); tft.print(c); x += 6 * tailleTexte; }
    }
  }
}

void dessinerBadgeNotif() {
  if (notificationMessage || notificationDessin) tft.fillCircle(tft.width() - 77, 10, 6, COULEUR_CORAL);
}

void masquerCoinsImage(int x, int y, int w, int h, int rayon, uint16_t couleurFond) {
  for (int ligne = 0; ligne < rayon; ligne++) {
    int distance = rayon - ligne;
    int inset = rayon - (int)sqrt((float)(rayon * rayon - distance * distance));
    if (inset <= 0) continue;
    tft.drawFastHLine(x, y + ligne, inset, couleurFond);
    tft.drawFastHLine(x + w - inset, y + ligne, inset, couleurFond);
    tft.drawFastHLine(x, y + h - 1 - ligne, inset, couleurFond);
    tft.drawFastHLine(x + w - inset, y + h - 1 - ligne, inset, couleurFond);
  }
}

void mettreAJourBadgeNotif() {
  tft.fillCircle(tft.width() - 77, 10, 7, (pageActuelle == 2) ? COULEUR_PANEL : COULEUR_FOND);
  dessinerBadgeNotif();
}

void ouvrirNotification() {
  if (!notificationMessage && !notificationDessin) return;
  photoPleinEcran = false;
  if (notificationDessin) pageActuelle = 2;
  else {
    notificationMessage = false;
    pageActuelle = 0;
  }
  afficherPageSelonIndex();
}

// ==========================================================
// ---- GESTION ACCUEIL ----
// ==========================================================
void dessinerFondAccueil() {
  tft.fillScreen(COULEUR_FOND);

  tft.setTextColor(COULEUR_OR, COULEUR_FOND);
  tft.setTextSize(2);
  tft.setCursor(tft.width() / 2 - 40, 5); tft.println("Accueil");

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
  tft.setCursor(15, tft.height() - FOOTER_H + 8); tft.print("LDR Box - "); tft.println(NOM_PROPRIETAIRE);

  tft.fillRoundRect(PHOTO_X, PHOTO_Y, PHOTO_W, PHOTO_H, 12, COULEUR_PANEL);
  tft.drawRoundRect(PHOTO_X, PHOTO_Y, PHOTO_W, PHOTO_H, 12, COULEUR_CADRE);

  tft.fillRoundRect(TEXTE_X, TEXTE_Y, TEXTE_W, TEXTE_H, 12, COULEUR_PANEL);
  tft.drawRoundRect(TEXTE_X, TEXTE_Y, TEXTE_W, TEXTE_H, 12, COULEUR_CADRE);

  tft.fillRoundRect(AGENDA_X, AGENDA_Y, AGENDA_W, AGENDA_H, 12, COULEUR_PANEL);
  tft.drawRoundRect(AGENDA_X, AGENDA_Y, AGENDA_W, AGENDA_H, 12, COULEUR_CADRE);

  dessinerBadgeNotif();
}

void rafraichirPhotoSiNecessaire() {
  if (dernierePhotoURL == dernierePhotoAffichee) return;
  tft.fillRect(PHOTO_X + 2, PHOTO_Y + 2, PHOTO_W - 4, PHOTO_H - 4, COULEUR_PANEL);
  if (dernierePhotoURL.length() > 0) {
    const int margeImage = 8;
    String urlMiniature = urlPourTaille(dernierePhotoURL, PHOTO_W - margeImage * 2, PHOTO_H - margeImage * 2);
    afficherPhotoDepuisURL(urlMiniature, PHOTO_X + margeImage, PHOTO_Y + margeImage);
    tft.drawRoundRect(PHOTO_X, PHOTO_Y, PHOTO_W, PHOTO_H, 12, COULEUR_CADRE);
  }
  dernierePhotoAffichee = dernierePhotoURL;
}

void rafraichirTexteSiNecessaire() {
  if (dernierTexte == dernierTexteAffiche) return;
  tft.fillRect(TEXTE_X + 2, TEXTE_Y + 2, TEXTE_W - 4, TEXTE_H - 4, COULEUR_PANEL);
  if (dernierTexte.length() > 0) {
    afficherTexteEmoji(TEXTE_X + 8, TEXTE_Y + 10, TEXTE_W - 16, dernierTexte, COULEUR_TEXTE, COULEUR_PANEL, 2);
  } else {
    tft.setTextColor(COULEUR_DIM, COULEUR_PANEL); tft.setTextSize(1);
    tft.setCursor(TEXTE_X + 8, TEXTE_Y + 10); tft.println("(Aucun message)");
  }
  dernierTexteAffiche = dernierTexte;
}

void rafraichirAgendaAccueil() {
  String checkTitle = (nbEvenements > 0) ? evenements[0].titre : "VIDE";
  if (checkTitle == dernierAgendaAccueilAffiche) return;
  tft.fillRect(AGENDA_X + 2, AGENDA_Y + 2, AGENDA_W - 4, AGENDA_H - 4, COULEUR_PANEL);

  tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
  tft.setTextSize(1);
  tft.setCursor(AGENDA_X + 8, AGENDA_Y + 8);
  tft.println("Prochain Evenement:");

  if (nbEvenements > 0) {
    uint16_t couleurTheme = evenements[0].task ? COULEUR_BLEU :
                            (evenements[0].isMultiDay ? COULEUR_CORAL : COULEUR_OR);
    tft.setTextColor(couleurTheme, COULEUR_PANEL);
    tft.setTextSize(2);
    tft.setCursor(AGENDA_X + 8, AGENDA_Y + 25);
    tft.print(evenements[0].date);

    tft.setTextSize(1);
    tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
    tft.setCursor(AGENDA_X + 8, AGENDA_Y + 45);
    if (!evenements[0].task && !evenements[0].isMultiDay) {
      tft.println((evenements[0].temps == "00:00") ? "Toute la journee" : evenements[0].temps);
    } else {
      tft.println("");
    }
    afficherTexteEmoji(AGENDA_X + 8, AGENDA_Y + 58, AGENDA_W - 16, evenements[0].titre, COULEUR_TEXTE, COULEUR_PANEL, 2);
  } else {
    tft.setCursor(AGENDA_X + 8, AGENDA_Y + 30);
    tft.println("Rien de prevu !");
  }
  dernierAgendaAccueilAffiche = checkTitle;
}

void dessinerPageAccueil() {
  rafraichirPhotoSiNecessaire();
  rafraichirTexteSiNecessaire();
  chargerAgenda();
  rafraichirAgendaAccueil();
  rafraichirAlertePlanteAccueil();
}

void forcerReaffichageAccueil() {
  dernierePhotoAffichee = "###FORCE###";
  dernierTexteAffiche = "###FORCE###";
  dernierAgendaAccueilAffiche = "###FORCE###";
  memoPlanteAlerteAccueil = -1;
  dessinerFondAccueil();
  dessinerPageAccueil();
}

// ---- Alerte "plante assoiffee" affichee dans le pied de page de l'Accueil ----
void rafraichirAlertePlanteAccueil() {
  time_t maintenant = time(nullptr);
  bool heureSynchro = (maintenant > 1700000000);
  int64_t diffSecondes = 0;
  if (heureSynchro && plantDerniereInteraction > 0) {
    diffSecondes = (int64_t)maintenant - plantDerniereInteraction;
    if (diffSecondes < 0) diffSecondes = 0;
  }
  bool malade = heureSynchro && plantDerniereInteraction > 0 && (diffSecondes / 86400) >= 3;
  int etat = malade ? 1 : 0;
  if (etat == memoPlanteAlerteAccueil) return;
  memoPlanteAlerteAccueil = etat;

  tft.fillRect(13, tft.height() - FOOTER_H + 6, tft.width() - 26, 14, COULEUR_FOND);
  tft.setTextSize(1);
  if (malade) {
    tft.setTextColor(COULEUR_CORAL, COULEUR_FOND);
    tft.setCursor(15, tft.height() - FOOTER_H + 8);
    tft.println("Ta plante a besoin de toi... (page Plante)");
  } else {
    tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
    tft.setCursor(15, tft.height() - FOOTER_H + 8);
    tft.print("LDR Box - "); tft.println(NOM_PROPRIETAIRE);
  }
}

// ==========================================================
// ---- PLEIN ECRAN PHOTO & MENU DE REACTIONS ----
// ==========================================================
void afficherPhotoPleinEcran() {
  tft.fillScreen(COULEUR_PANEL);
  if (dernierePhotoURL.length() > 0) {
    int imgH = 320;
    int imgW = (imgH * 3) / 4;
    int imgX = (410 - imgW) / 2;
    tft.fillRect(imgX, 0, imgW, imgH, TFT_BLACK);
    String urlZoom = urlPourTaille(dernierePhotoURL, imgW, imgH);
    afficherPhotoDepuisURL(urlZoom, imgX, 0);
  }
  tft.fillRect(410, 0, 70, 320, COULEUR_FOND);
  tft.drawFastVLine(410, 0, 320, COULEUR_CADRE);

  tft.setTextSize(2);
  tft.fillRoundRect(415, 10, 60, 50, 8, TFT_PINK);
  tft.setTextColor(TFT_BLACK, TFT_PINK);
  tft.setCursor(433, 27); tft.print("<3");

  tft.fillRoundRect(415, 72, 60, 50, 8, COULEUR_OR);
  tft.setTextColor(TFT_BLACK, COULEUR_OR);
  tft.setCursor(424, 89); tft.print("^w^");

  tft.fillRoundRect(415, 134, 60, 50, 8, COULEUR_CORAL);
  tft.setTextColor(TFT_BLACK, COULEUR_CORAL);
  tft.setCursor(424, 151); tft.print("O_O");

  tft.fillRoundRect(415, 196, 60, 50, 8, TFT_CYAN);
  tft.setTextColor(TFT_BLACK, TFT_CYAN);
  tft.setCursor(424, 213); tft.print("T_T");

  tft.fillRoundRect(415, 258, 60, 50, 8, COULEUR_OK);
  tft.setTextColor(TFT_BLACK, COULEUR_OK);
  tft.setCursor(424, 275); tft.print(">_<");
}

// ==========================================================
// ---- CHARGEMENT AGENDA & DESSIN ----
// ==========================================================
void chargerAgenda() {
  nbEvenements = 0;
  if (!Firebase.RTDB.getJSON(&fbdo, CHEMIN_AGENDA)) return;

  FirebaseJson &json = fbdo.jsonObject();
  FirebaseJsonData jsonData;

  for (int i = 0; i < MAX_EVENEMENTS; i++) {
    String cle = "event_" + String(i);
    if (!json.get(jsonData, cle + "/title")) break;
    evenements[nbEvenements].titre = jsonData.stringValue;
    json.get(jsonData, cle + "/date"); evenements[nbEvenements].date = jsonData.stringValue;
    if (json.get(jsonData, cle + "/dueDate") && jsonData.stringValue.length() > 0) {
      evenements[nbEvenements].date = jsonData.stringValue;
    }
    evenements[nbEvenements].endDate = json.get(jsonData, cle + "/endDate") ? jsonData.stringValue : "";
    json.get(jsonData, cle + "/time"); evenements[nbEvenements].temps = jsonData.stringValue;
    json.get(jsonData, cle + "/isMultiDay"); evenements[nbEvenements].isMultiDay = jsonData.boolValue;
    evenements[nbEvenements].task = json.get(jsonData, cle + "/task") && jsonData.boolValue;
    evenements[nbEvenements].timestamp = json.get(jsonData, cle + "/timestamp") ? (int64_t)jsonData.doubleValue : 0;
    nbEvenements++;
  }
}

int64_t normaliserTimestamp(int64_t valeur) {
  return (valeur > 100000000000LL) ? valeur / 1000 : valeur;
}

void chargerDessinRecu(const String &chaine) {
  nbSegmentsRecus = 0;
  int debut = 0;
  while (debut < (int)chaine.length() && nbSegmentsRecus < MAX_SEGMENTS) {
    int fin = chaine.indexOf(';', debut);
    String segmentTxt = (fin == -1) ? chaine.substring(debut) : chaine.substring(debut, fin);

    int p1 = segmentTxt.indexOf(',');
    int p2 = segmentTxt.indexOf(',', p1 + 1);
    int p3 = segmentTxt.indexOf(',', p2 + 1);
    int p4 = segmentTxt.indexOf(',', p3 + 1);

    if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0) {
      segmentsRecus[nbSegmentsRecus].x1 = segmentTxt.substring(0, p1).toInt();
      segmentsRecus[nbSegmentsRecus].y1 = segmentTxt.substring(p1 + 1, p2).toInt();
      segmentsRecus[nbSegmentsRecus].x2 = segmentTxt.substring(p2 + 1, p3).toInt();
      segmentsRecus[nbSegmentsRecus].y2 = segmentTxt.substring(p3 + 1, p4).toInt();
      segmentsRecus[nbSegmentsRecus].couleur = (uint16_t)segmentTxt.substring(p4 + 1).toInt();
      nbSegmentsRecus++;
    }
    if (fin == -1) break;
    debut = fin + 1;
  }
}

void envoyerDessin() {
  if (nbSegmentsEnvoi == 0) return;
  String chaineSegments = "";
  for (int i = 0; i < nbSegmentsEnvoi; i++) {
    chaineSegments += String(segmentsEnvoi[i].x1) + "," + String(segmentsEnvoi[i].y1) + "," +
                      String(segmentsEnvoi[i].x2) + "," + String(segmentsEnvoi[i].y2) + "," +
                      String(segmentsEnvoi[i].couleur);
    if (i < nbSegmentsEnvoi - 1) chaineSegments += ";";
  }
  Firebase.RTDB.setString(&fbdo, CHEMIN_AUTRE_DESSIN, chaineSegments);
  nbSegmentsEnvoi = 0;
  arroserPlante();
}

// ==========================================================
// ---- PAGES SUIVANTES ----
// ==========================================================
void dessinerPageAgenda() {
  tft.fillScreen(COULEUR_FOND);

  int y = 50 - scrollAgendaY;
  const int hauteurLigne = 65;
  maxScrollAgenda = (nbEvenements * hauteurLigne) - 200;
  if (maxScrollAgenda < 0) maxScrollAgenda = 0;

  if (nbEvenements == 0) {
    tft.setTextColor(COULEUR_DIM);
    tft.setTextSize(1);
    tft.setCursor(15, 60); tft.println("(Aucun evenement)");
  } else {
    for (int i = 0; i < nbEvenements; i++) {
      if (y + hauteurLigne > 40 && y < tft.height() - 20) {

        tft.fillRoundRect(10, y, tft.width() - 20, hauteurLigne - 8, 8, COULEUR_PANEL);
        tft.drawRoundRect(10, y, tft.width() - 20, hauteurLigne - 8, 8, COULEUR_CADRE);

        uint16_t couleurTheme = evenements[i].task ? COULEUR_BLEU :
                                (evenements[i].isMultiDay ? COULEUR_CORAL : COULEUR_OR);

        tft.setTextSize(2);
        tft.setTextColor(couleurTheme, COULEUR_PANEL);
        tft.setCursor(18, y + 8);
        tft.print(evenements[i].date);

        String heureAffichee = "";
        if (!evenements[i].task && !evenements[i].isMultiDay) {
          heureAffichee = (evenements[i].temps == "00:00") ? "Toute la journee" : evenements[i].temps;
        }

        tft.setTextSize(1);
        tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
        tft.setCursor(210, y + 8);
        tft.println(heureAffichee);

        afficherTexteEmoji(18, y + 28, tft.width() - 36, evenements[i].titre, COULEUR_TEXTE, COULEUR_PANEL, 2);
      }
      y += hauteurLigne;
    }
  }
  tft.fillRect(0, 0, tft.width(), 40, COULEUR_FOND);
  tft.drawFastHLine(0, 40, tft.width(), COULEUR_CADRE);

  tft.setTextColor(COULEUR_OR, COULEUR_FOND);
  tft.setTextSize(2);
  tft.setCursor(15, 12);
  tft.println("Agenda");

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
  tft.setCursor(tft.width() / 2 - 40, 5); tft.println("[Clic Haut]");
  tft.setCursor(tft.width() / 2 - 35, tft.height() - 12); tft.println("[Clic Bas]");
}

void dessinerPageDessin() {
  tft.fillScreen(COULEUR_FOND);
  tft.fillRect(0, 0, tft.width(), HEADER_H, COULEUR_PANEL);
  tft.drawFastHLine(0, HEADER_H, tft.width(), COULEUR_CADRE);

  tft.setTextColor(COULEUR_TEXTE, COULEUR_PANEL);
  tft.setTextSize(2);

  if (!modeDessinActif) {
    tft.setCursor(15, 7); tft.print("Dessin recu");

    tft.fillRoundRect(270, 2, 125, 26, 7, COULEUR_CORAL);
    tft.setTextColor(TFT_BLACK, COULEUR_CORAL);
    tft.setCursor(282, 7); tft.print("DESSINER");

    int yTop = HEADER_H + 1;
    int yBottom = tft.height() - 15;
    tft.fillRect(0, yTop, tft.width(), yBottom - yTop, TFT_BLACK);
    tft.drawRect(0, yTop, tft.width(), yBottom - yTop, COULEUR_CADRE);

    if (nbSegmentsRecus > 0) {
      for (int i = 0; i < nbSegmentsRecus; i++) {
        tft.drawLine(segmentsRecus[i].x1, segmentsRecus[i].y1, segmentsRecus[i].x2, segmentsRecus[i].y2, segmentsRecus[i].couleur);
      }
    } else {
      tft.setTextColor(COULEUR_DIM, TFT_BLACK);
      tft.setTextSize(1);
      tft.setCursor(150, yTop + (yBottom - yTop) / 2);
      tft.println("(Aucun dessin recu pour le moment)");
    }
    notificationDessin = false;
    mettreAJourBadgeNotif();
  } else {
    tft.setTextColor(COULEUR_OR, COULEUR_PANEL);
    tft.setCursor(15, 7); tft.print("A toi de jouer !");

    tft.fillRoundRect(280, 2, 115, 26, 7, COULEUR_CADRE);
    tft.setTextColor(COULEUR_PANEL, COULEUR_CADRE);
    tft.setCursor(296, 7); tft.print("RETOUR");

    tft.fillRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 41, TFT_BLACK);
    int yOutils = tft.height() - 40;

    tft.fillRect(0, yOutils, tft.width(), 40, COULEUR_PANEL);

    tft.fillCircle(30, yOutils + 20, 12, TFT_WHITE);
    tft.fillCircle(70, yOutils + 20, 12, TFT_RED);
    tft.fillCircle(110, yOutils + 20, 12, TFT_GREEN);
    tft.fillCircle(150, yOutils + 20, 12, TFT_BLUE);

    tft.fillRoundRect(180, yOutils + 5, 80, 30, 4, TFT_LIGHTGREY);
    tft.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
    tft.setTextSize(2);
    tft.setCursor(192, yOutils + 12); tft.print("GOMME");

    tft.fillRoundRect(350, yOutils + 5, 120, 30, 4, COULEUR_OK);
    tft.setTextColor(TFT_BLACK, COULEUR_OK);
    tft.setCursor(360, yOutils + 12); tft.print("ENVOYER");
  }
}

// ---- PAGE RELATION (Mise a jour intelligente & Jauge de progression) ----
void dessinerPageRelation() {
  time_t maintenant = time(nullptr);
  bool heureSynchro = (maintenant > 1700000000);
  int64_t cibleRetrouvailles = targetRetrouvailles;

  int jours = 0, heures = 0, minutes = 0;
  if (heureSynchro && cibleRetrouvailles > 0) {
    int64_t diff = cibleRetrouvailles - (int64_t)maintenant;
    if (diff < 0) diff = 0;
    jours = diff / 86400;
    heures = (diff % 86400) / 3600;
    minutes = (diff % 3600) / 60;
  }

  bool capsuleOuverte = (heureSynchro && targetMessageSecret > 0 && (int64_t)maintenant >= targetMessageSecret);

  // --- Premier affichage : dessin des 3 cadres fixes (fond/bordure/etiquette),
  //     une seule fois. Les valeurs variables sont ensuite gerees section par
  //     section plus bas, pour ne jamais reflasher toute la page. ---
  if (!pageRelationInit) {
    tft.fillScreen(COULEUR_FOND);
    tft.setTextColor(COULEUR_OR, COULEUR_FOND);
    tft.setTextSize(2);
    tft.setCursor(15, 10); tft.println("Relation");
    tft.drawFastHLine(10, 34, tft.width() - 20, COULEUR_CADRE);

    tft.fillRoundRect(10, 42, tft.width() - 20, 56, 8, COULEUR_PANEL);
    tft.drawRoundRect(10, 42, tft.width() - 20, 56, 8, COULEUR_CADRE);
    tft.setTextSize(1);
    tft.setTextColor(COULEUR_CADRE, COULEUR_PANEL);
    tft.setCursor(18, 48); tft.println("Prochaines retrouvailles :");

    tft.fillRoundRect(10, 106, tft.width() - 20, 60, 8, COULEUR_PANEL);
    tft.drawRoundRect(10, 106, tft.width() - 20, 60, 8, COULEUR_CADRE);
    tft.setTextColor(COULEUR_CADRE, COULEUR_PANEL);
    tft.setCursor(18, 112); tft.println("Capsule temporelle :");

    tft.fillRoundRect(10, 174, tft.width() - 20, 120, 8, COULEUR_PANEL);
    tft.drawRoundRect(10, 174, tft.width() - 20, 120, 8, COULEUR_CADRE);
    tft.setTextColor(COULEUR_CADRE, COULEUR_PANEL);
    tft.setCursor(18, 180); tft.println("Dernier message :");

    pageRelationInit = true;
    // Valeurs "impossibles" pour forcer le premier dessin de chaque section ci-dessous
    memoJours = -999; memoHeures = -999; memoMinutes = -999;
    memoSynchro = !heureSynchro;
    memoCapsuleOuverte = !capsuleOuverte;
    memoTexteSecret = "###FORCE###";
    memoDernierTexte = "###FORCE###";
  }

  // --- SECTION 1 : Compteur + barre de progression (uniquement si ca a change) ---
  if (memoJours != jours || memoHeures != heures || memoMinutes != minutes || memoSynchro != heureSynchro) {
    tft.fillRect(12, 58, tft.width() - 24, 36, COULEUR_PANEL);

    if (!heureSynchro) {
      tft.setTextSize(1);
      tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
      tft.setCursor(18, 68); tft.println("(Synchronisation de l'heure...)");
    } else if (cibleRetrouvailles == 0) {
      tft.setTextSize(1);
      tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
      tft.setCursor(18, 68); tft.println("(Aucune date programmee)");
    } else {
      tft.setTextSize(2);
      tft.setTextColor(COULEUR_OR, COULEUR_PANEL);
      tft.setCursor(18, 62);
      tft.printf("%d j  %02d h  %02d m", jours, heures, minutes);

      int barX = 18, barY = 82, barW = tft.width() - 56, barH = 8;
      tft.fillRoundRect(barX, barY, barW, barH, 4, COULEUR_FOND);
      tft.drawRoundRect(barX, barY, barW, barH, 4, COULEUR_DIM);

      // Jauge : start_retrouvailles = 0%, retrouvailles_timestamp = 100%.
      // Tant que start_retrouvailles n'est pas defini, la jauge reste vide.
      int fillW = 0;
      if (startRetrouvailles > 0 && cibleRetrouvailles > startRetrouvailles) {
        int64_t totalDuration = cibleRetrouvailles - startRetrouvailles;
        int64_t elapsed = (int64_t)maintenant - startRetrouvailles;
        if (elapsed < 0) elapsed = 0;
        if (elapsed > totalDuration) elapsed = totalDuration;
        fillW = (elapsed * barW) / totalDuration;
      }
      if (fillW > 0) tft.fillRoundRect(barX, barY, fillW, barH, 4, COULEUR_CORAL);
    }

    memoJours = jours; memoHeures = heures; memoMinutes = minutes; memoSynchro = heureSynchro;
  }

  // --- SECTION 2 : Capsule temporelle / mot secret (uniquement si ca a change) ---
  if (memoCapsuleOuverte != capsuleOuverte || memoTexteSecret != texteMessageSecret) {
    tft.fillRect(12, 122, tft.width() - 24, 42, COULEUR_PANEL);
    tft.setTextSize(1);

    if (targetMessageSecret > 0 && texteMessageSecret.length() > 0) {
      if (!heureSynchro) {
        tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
        tft.setCursor(18, 132); tft.println("(Attente de l'horloge...)");
      } else if (capsuleOuverte) {
        afficherTexteEmoji(18, 132, tft.width() - 36, texteMessageSecret, COULEUR_TEXTE, COULEUR_PANEL, 2);
      } else {
        time_t tSecret = (time_t)targetMessageSecret;
        struct tm *tmSecret = localtime(&tSecret);
        char dateOuverture[30];
        sprintf(dateOuverture, "%02d/%02d/%04d a %02d:%02d", tmSecret->tm_mday, tmSecret->tm_mon + 1, tmSecret->tm_year + 1900, tmSecret->tm_hour, tmSecret->tm_min);

        tft.setTextColor(COULEUR_OR, COULEUR_PANEL);
        tft.setCursor(18, 132); tft.println("[Message en attente]");
        tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
        tft.setCursor(18, 146); tft.printf("Ouverture : %s", dateOuverture);
      }
    } else {
      tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
      tft.setCursor(18, 132); tft.println("(Aucune capsule temporelle)");
    }

    memoCapsuleOuverte = capsuleOuverte;
    memoTexteSecret = texteMessageSecret;
  }

  // --- SECTION 3 : Dernier message (uniquement si le texte a change) ---
  if (memoDernierTexte != dernierTexte) {
    tft.fillRect(12, 190, tft.width() - 24, 100, COULEUR_PANEL);
    tft.setTextSize(1);

    if (dernierTexte.length() > 0) {
      afficherTexteEmoji(18, 198, tft.width() - 36, dernierTexte, COULEUR_TEXTE, COULEUR_PANEL, 3);
    } else {
      tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
      tft.setCursor(18, 198); tft.println("(Aucun message)");
    }

    memoDernierTexte = dernierTexte;
  }
}

void dessinerPageEmploiDuTemps() {
  tft.fillScreen(COULEUR_FOND);
  tft.setTextColor(COULEUR_OR, COULEUR_FOND);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  tft.println("Emploi du temps");

  int imgX = 10;
  int imgY = HEADER_H + 5;
  int imgW = tft.width() - 20;
  int imgH = tft.height() - HEADER_H - 15;

  tft.fillRoundRect(imgX, imgY, imgW, imgH, 12, COULEUR_PANEL);
  tft.drawRoundRect(imgX, imgY, imgW, imgH, 12, COULEUR_CADRE);

  if (dernierEmploiDuTempsURL.length() > 0) {
    String urlZoom = urlPourTaille(dernierEmploiDuTempsURL, imgW - 4, imgH - 4);
    afficherPhotoDepuisURL(urlZoom, imgX + 2, imgY + 2);
    masquerCoinsImage(imgX + 2, imgY + 2, imgW - 4, imgH - 4, 10, COULEUR_PANEL);
    tft.drawRoundRect(imgX, imgY, imgW, imgH, 12, COULEUR_CADRE);
  } else {
    tft.setTextColor(COULEUR_DIM, COULEUR_PANEL);
    tft.setTextSize(1);
    tft.setCursor(imgX + 40, imgY + (imgH / 2) - 5);
    tft.println("(Aucun emploi du temps charge)");
  }
}

// ==========================================================
// ---- PLANTE VIRTUELLE (Tamagotchi de couple) ----
// ==========================================================

// Petit soleil (page saine) ou nuage terne (page malade), coin haut-droit du cadre.
void dessinerDecorPlante(int panelX, int panelY, int panelW, bool malade) {
  int sx = panelX + panelW - 30;
  int sy = panelY + 24;
  if (!malade) {
    tft.fillCircle(sx, sy, 9, COULEUR_OR);
    for (int i = 0; i < 8; i++) {
      float a = (i * 45) * 0.0174533;
      int x1 = sx + (int)(cos(a) * 13), y1 = sy + (int)(sin(a) * 13);
      int x2 = sx + (int)(cos(a) * 17), y2 = sy + (int)(sin(a) * 17);
      tft.drawLine(x1, y1, x2, y2, COULEUR_OR);
    }
  } else {
    uint16_t cNuage = COULEUR_PLANTE_FANE_CADRE;
    tft.fillCircle(sx - 7, sy, 6, cNuage);
    tft.fillCircle(sx + 3, sy - 3, 7, cNuage);
    tft.fillCircle(sx + 11, sy, 5, cNuage);
    tft.fillRect(sx - 11, sy, 26, 5, cNuage);
  }
}

// Ligne de sol : brins d'herbe (sain) ou terre craquelee (malade), en laissant
// un espace libre au centre pour le pot.
void dessinerSolPlante(int cx, int solY, int panelX, int panelW, bool malade) {
  uint16_t cSol = malade ? COULEUR_PLANTE_FANE_CADRE : COULEUR_PLANTE_CADRE;
  tft.drawFastHLine(panelX + 8, solY + 22, panelW - 16, cSol);

  if (!malade) {
    for (int gx = panelX + 18; gx < panelX + panelW - 18; gx += 16) {
      if (abs(gx - cx) < 36) continue; // laisse la place au pot
      tft.drawLine(gx, solY + 22, gx - 2, solY + 15, COULEUR_PLANTE_FEUILLE);
      tft.drawLine(gx, solY + 22, gx + 2, solY + 15, COULEUR_PLANTE_FEUILLE);
    }
  } else {
    for (int gx = panelX + 22; gx < panelX + panelW - 22; gx += 30) {
      if (abs(gx - cx) < 36) continue;
      tft.drawLine(gx, solY + 20, gx + 8, solY + 25, COULEUR_PLANTE_FANE_TIGE);
    }
  }
}

// Dessine la plante en pixel art. niveau: 0-100 (croissance). malade: bascule
// vers l'aspect fane (tige courbee, feuilles et fleur ternies).
void dessinerPlantePixelArt(int cx, int solY, int niveau, bool malade) {
  uint16_t cTige    = malade ? COULEUR_PLANTE_FANE_TIGE    : COULEUR_PLANTE_TIGE;
  uint16_t cFeuille = malade ? COULEUR_PLANTE_FANE_FEUILLE : COULEUR_PLANTE_FEUILLE;
  uint16_t cFleur   = malade ? COULEUR_PLANTE_FANE_FEUILLE : COULEUR_PLANTE_FLEUR;
  uint16_t cCoeur   = malade ? COULEUR_PLANTE_FANE_CADRE   : COULEUR_OR;

  // Pot en terre cuite : corps + reflet + rebord ombre + petit monticule de terre
  tft.fillTriangle(cx - 28, solY, cx + 28, solY, cx + 20, solY + 22, COULEUR_PLANTE_POT);
  tft.fillTriangle(cx - 28, solY, cx - 20, solY + 22, cx + 20, solY + 22, COULEUR_PLANTE_POT);
  tft.fillTriangle(cx - 11, solY + 3, cx - 3, solY + 3, cx - 7, solY + 17, tft.color565(184, 124, 90));
  tft.fillRect(cx - 32, solY - 4, 64, 6, COULEUR_PLANTE_POT);
  tft.drawFastHLine(cx - 32, solY - 4, 64, tft.color565(110, 60, 40));
  tft.fillCircle(cx, solY - 3, 5, tft.color565(90, 55, 35));

  int niv = constrain(niveau, 0, 100);
  int hautTige = map(niv, 0, 100, 8, 92);
  int inclinaison = malade ? 22 : 0; // affaissement horizontal si malade

  int sommetX = cx + inclinaison;
  int sommetY = solY - hautTige;

  // Tige (courbee si malade, bien droite sinon)
  if (malade) {
    int milieuX = cx + inclinaison / 2;
    int milieuY = solY - hautTige / 2;
    tft.drawLine(cx, solY, milieuX, milieuY, cTige);
    tft.drawLine(cx + 1, solY, milieuX + 1, milieuY, cTige);
    tft.drawLine(milieuX, milieuY, sommetX, sommetY, cTige);
    tft.drawLine(milieuX + 1, milieuY, sommetX + 1, sommetY, cTige);
  } else {
    tft.drawFastVLine(cx, sommetY, hautTige, cTige);
    tft.drawFastVLine(cx + 1, sommetY, hautTige, cTige);
  }

  if (niv < 18) {
    // Simple pousse : un petit bourgeon vert au sommet
    tft.fillCircle(sommetX, sommetY, 3, cFeuille);
    return;
  }

  // Feuilles disposees le long de la tige, avec une pointe arrondie
  int nbPaires = (niv < 45) ? 1 : 2;
  for (int p = 0; p < nbPaires; p++) {
    int fy = solY - (hautTige * (p + 1)) / (nbPaires + 1);
    int fx = cx + (inclinaison * (p + 1)) / (nbPaires + 1);
    int pente = malade ? 10 : -6; // les feuilles pointent vers le bas si malade
    int tipLx = fx - 16, tipLy = fy + pente;
    int tipRx = fx + 16, tipRy = fy + pente;
    tft.fillTriangle(fx, fy, tipLx, tipLy, fx - 3, fy + (malade ? 12 : -3), cFeuille);
    tft.fillCircle(tipLx, tipLy, 3, cFeuille);
    tft.fillTriangle(fx, fy, tipRx, tipRy, fx + 3, fy + (malade ? 12 : -3), cFeuille);
    tft.fillCircle(tipRx, tipRy, 3, cFeuille);
  }

  if (niv < 70) return; // pas encore de fleur

  if (niv < 90) {
    // Bouton floral ferme
    tft.fillCircle(sommetX, sommetY, 6, cFleur);
    return;
  }

  // Pleine floraison : petales autour d'un coeur dore
  int rayonPetale = malade ? 6 : 10;
  for (int i = 0; i < 6; i++) {
    float a = (i * 60) * 0.0174533;
    int px = sommetX + (int)(cos(a) * rayonPetale);
    int py = sommetY + (int)(sin(a) * rayonPetale);
    tft.fillCircle(px, py, malade ? 4 : 7, cFleur);
  }
  tft.fillCircle(sommetX, sommetY, malade ? 3 : 5, cCoeur);

  if (!malade) {
    // Petites etincelles pour une pleine floraison eclatante
    tft.drawPixel(sommetX - 15, sommetY - 11, COULEUR_OR);
    tft.drawPixel(sommetX + 15, sommetY - 9, COULEUR_OR);
    tft.drawPixel(sommetX - 11, sommetY + 15, COULEUR_OR);
  }
}

void dessinerPagePlante() {
  time_t maintenant = time(nullptr);
  bool heureSynchro = (maintenant > 1700000000);
  int64_t diffSecondes = 0;
  if (heureSynchro && plantDerniereInteraction > 0) {
    diffSecondes = (int64_t)maintenant - plantDerniereInteraction;
    if (diffSecondes < 0) diffSecondes = 0;
  }
  int joursInactif = diffSecondes / 86400;
  // Fanaison rapide : des le 2e jour sans aucune interaction, on perd du niveau
  int perteFanaison = (joursInactif > 1) ? (joursInactif - 1) * 12 : 0;
  int niveauAffiche = constrain(plantNiveauBase - perteFanaison, 0, 100);
  bool malade = heureSynchro && plantDerniereInteraction > 0 && joursInactif >= 3;

  // Evite de redessiner si rien n'a change (anti-clignotement)
  if (pagePlanteInit && memoPlantNiveauAffiche == niveauAffiche && memoPlantMalade == malade) return;

  uint16_t fond  = malade ? COULEUR_PLANTE_FANE_FOND  : COULEUR_PLANTE_FOND;
  uint16_t panel = malade ? COULEUR_PLANTE_FANE_PANEL : COULEUR_PLANTE_PANEL;
  uint16_t cadre = malade ? COULEUR_PLANTE_FANE_CADRE : COULEUR_PLANTE_CADRE;

  tft.fillScreen(fond);
  tft.setTextColor(COULEUR_PLANTE_TEXTE, fond);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  tft.println(malade ? "Plante (a besoin d'eau)" : "Plante");
  tft.drawFastHLine(10, 34, tft.width() - 20, cadre);

  int panelX = 10, panelY = 42;
  int cadreH = tft.height() - 90;
  tft.fillRoundRect(panelX, panelY, tft.width() - 20, cadreH, 10, panel);
  tft.drawRoundRect(panelX, panelY, tft.width() - 20, cadreH, 10, cadre);

  dessinerDecorPlante(panelX, panelY, tft.width() - 20, malade);

  int cx = tft.width() / 2;
  int solY = panelY + cadreH - 26;
  dessinerSolPlante(cx, solY, panelX, tft.width() - 20, malade);
  dessinerPlantePixelArt(cx, solY, niveauAffiche, malade);

  // Jauge de vitalite, avec des reperes de palier (pousse / feuillue / bouton / fleur)
  int barX = 18, barY = tft.height() - 40, barW = tft.width() - 36, barH = 10;
  tft.fillRoundRect(barX, barY, barW, barH, 5, fond);
  tft.drawRoundRect(barX, barY, barW, barH, 5, cadre);
  int fillW = (niveauAffiche * barW) / 100;
  if (fillW > 0) {
    tft.fillRoundRect(barX, barY, fillW, barH, 5, malade ? COULEUR_PLANTE_FANE_FEUILLE : COULEUR_PLANTE_FEUILLE);
  }
  const int seuilsEtapes[3] = { 18, 45, 90 };
  for (int i = 0; i < 3; i++) {
    int tx = barX + (seuilsEtapes[i] * barW) / 100;
    tft.drawFastVLine(tx, barY - 2, barH + 4, cadre);
  }

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_PLANTE_TEXTE, fond);
  tft.setCursor(barX, barY - 12);
  if (!heureSynchro) {
    tft.println("(Synchronisation de l'heure...)");
  } else if (malade) {
    tft.printf("Vitalite : %d%%  (%d jours sans interaction)", niveauAffiche, joursInactif);
  } else {
    tft.printf("Vitalite : %d%%", niveauAffiche);
  }

  pagePlanteInit = true;
  memoPlantNiveauAffiche = niveauAffiche;
  memoPlantMalade = malade;
}

// Appelee a chaque message / photo / reaction / dessin echange : "arrose" la plante.
// La croissance est volontairement lente : au maximum PLANTE_CROISSANCE_MAX_PAR_JOUR
// points par JOUR CALENDAIRE, ce plafond etant PARTAGE entre les deux boitiers (on
// relit l'etat Firebase avant chaque arrosage, sinon chaque boitier accordait son
// propre plafond independamment, ce qui doublait la vitesse reelle). Il faut donc
// plusieurs messages ce jour-la pour atteindre le maximum, et plusieurs semaines de
// journees actives pour monter d'un niveau visuel de la fleur.
// La fanaison (voir dessinerPagePlante) reste rapide en comparaison : quelques jours
// sans aucun message suffisent a perdre un niveau.
const int PLANTE_CROISSANCE_MAX_PAR_JOUR = 2;
const int PLANTE_CROISSANCE_PAR_INTERACTION = 1;

void arroserPlante() {
  time_t maintenant = time(nullptr);
  if (maintenant > 1700000000) {
    int64_t jourActuel = (int64_t)maintenant / 86400;

    // On relit l'etat partage AVANT d'ecrire : le plafond quotidien doit etre
    // commun aux deux boitiers, pas seulement local a celui-ci.
    int creditsJourPartages = 0;
    if (Firebase.RTDB.getJSON(&fbdoPlante, CHEMIN_PLANTE)) {
      FirebaseJson &jsonPlante = fbdoPlante.jsonObject();
      FirebaseJsonData jd;
      if (jsonPlante.get(jd, "niveau")) plantNiveauBase = jd.intValue;
      int64_t jourCreditPartage = -1;
      if (jsonPlante.get(jd, "jour_credit")) jourCreditPartage = (int64_t)jd.doubleValue;
      if (jourCreditPartage == jourActuel && jsonPlante.get(jd, "credits_jour")) {
        creditsJourPartages = jd.intValue;
      }
    }

    if (creditsJourPartages < PLANTE_CROISSANCE_MAX_PAR_JOUR) {
      int credit = min(PLANTE_CROISSANCE_PAR_INTERACTION, PLANTE_CROISSANCE_MAX_PAR_JOUR - creditsJourPartages);
      plantNiveauBase = constrain(plantNiveauBase + credit, 0, 100);
      creditsJourPartages += credit;
    }
    plantDerniereInteraction = (int64_t)maintenant;

    // On utilise fbdoPlante (et non fbdo) : fbdo peut etre en cours de lecture
    // (parsing JSON) au moment ou arroserPlante() est appelee, et y ecrire
    // ecraserait ce buffer et casserait la suite du parsing.
    Firebase.RTDB.setInt(&fbdoPlante, CHEMIN_PLANTE_NIVEAU, plantNiveauBase);
    Firebase.RTDB.setInt(&fbdoPlante, CHEMIN_PLANTE_DERNIERE_INTERACTION, (int)plantDerniereInteraction);
    Firebase.RTDB.setInt(&fbdoPlante, CHEMIN_PLANTE_JOUR_CREDIT, (int)jourActuel);
    Firebase.RTDB.setInt(&fbdoPlante, CHEMIN_PLANTE_CREDITS_JOUR, creditsJourPartages);
  }
  if (pageActuelle == 5) {
    pagePlanteInit = false;
    dessinerPagePlante();
  }
  if (pageActuelle == 0 && !photoPleinEcran) {
    rafraichirAlertePlanteAccueil();
  }
}

// ==========================================================
// ---- JOURNAL DU JOUR / ALBUM 22H ----
// ==========================================================

// Ajoute une entree au journal partage (/partage/journal_jour), avec
// reinitialisation automatique si un nouveau jour calendaire a commence.
// N'enregistre que ce que CE boitier recoit (donc envoye par NOM_PARTENAIRE) :
// pour voir les deux cotes de la conversation dans l'Album, le DEUXIEME
// boitier doit tourner ce meme firmware avec MON_ID/PARTENAIRE_ID et
// NOM_PROPRIETAIRE/NOM_PARTENAIRE inverses (voir bloc CONFIGURATION en haut).
void ajouterAuJournal(const char* type, const String &contenu) {
  time_t maintenant = time(nullptr);
  if (maintenant <= 1700000000) return; // horloge pas encore synchronisee

  int64_t jourActuel = (int64_t)maintenant / 86400;
  int indexEntree = 0;

  if (Firebase.RTDB.getJSON(&fbdoJournal, CHEMIN_JOURNAL_JOUR_META)) {
    FirebaseJson &meta = fbdoJournal.jsonObject();
    FirebaseJsonData jd;
    int64_t jourJournal = -1;
    if (meta.get(jd, "jour")) jourJournal = (int64_t)jd.doubleValue;
    if (jourJournal == jourActuel && meta.get(jd, "compte")) {
      indexEntree = jd.intValue;
    } else {
      // Nouveau jour : on efface le journal de la veille avant de reecrire
      Firebase.RTDB.deleteNode(&fbdoJournal, CHEMIN_JOURNAL_JOUR);
      indexEntree = 0;
    }
  }

  if (indexEntree >= MAX_ENTREES_JOURNAL) return; // journal du jour deja plein

  // --- CORRECTION : ecriture via setString/setInt directs (comme pour la
  //     Plante), au lieu de FirebaseJson::set() + setJSON() qui ne
  //     persistait / ne se relisait pas correctement (le compteur "compte"
  //     restait bloque, donc chaque message ecrasait la meme entree). ---
  String base = String(CHEMIN_JOURNAL_JOUR) + "/entry_" + String(indexEntree);
  Firebase.RTDB.setString(&fbdoJournal, (base + "/type").c_str(), type);
  Firebase.RTDB.setString(&fbdoJournal, (base + "/contenu").c_str(), contenu);
  Firebase.RTDB.setString(&fbdoJournal, (base + "/auteur").c_str(), NOM_PARTENAIRE);
  Firebase.RTDB.setInt(&fbdoJournal, (base + "/timestamp").c_str(), (int)maintenant);

  Firebase.RTDB.setInt(&fbdoJournal, (String(CHEMIN_JOURNAL_JOUR_META) + "/jour").c_str(), (int)jourActuel);
  Firebase.RTDB.setInt(&fbdoJournal, (String(CHEMIN_JOURNAL_JOUR_META) + "/compte").c_str(), indexEntree + 1);
}

// Charge les entrees du jour (deja dans l'ordre chronologique, puisqu'ecrites
// dans cet ordre au fil de la journee).
void chargerJournalDuJour() {
  nbEntreesJournal = 0;
  if (!Firebase.RTDB.getJSON(&fbdoJournal, CHEMIN_JOURNAL_JOUR)) return;

  FirebaseJson &json = fbdoJournal.jsonObject();
  FirebaseJsonData jd;
  for (int i = 0; i < MAX_ENTREES_JOURNAL; i++) {
    String cle = "entry_" + String(i);
    if (!json.get(jd, cle + "/type")) break;
    journalDuJour[nbEntreesJournal].type = jd.stringValue;
    json.get(jd, cle + "/contenu");  journalDuJour[nbEntreesJournal].contenu = jd.stringValue;
    json.get(jd, cle + "/auteur");   journalDuJour[nbEntreesJournal].auteur = jd.stringValue;
    json.get(jd, cle + "/timestamp"); journalDuJour[nbEntreesJournal].timestamp = (int64_t)jd.doubleValue;
    nbEntreesJournal++;
  }
}

void dessinerPageAlbum() {
  time_t maintenant = time(nullptr);
  bool heureSynchro = (maintenant > 1700000000);
  int heureLocale = -1;
  if (heureSynchro) {
    struct tm *infos = localtime(&maintenant);
    heureLocale = infos->tm_hour;
  }
  bool debloque = heureSynchro && heureLocale >= 22;

  tft.fillScreen(COULEUR_FOND);
  tft.setTextColor(COULEUR_OR, COULEUR_FOND);
  tft.setTextSize(2);
  tft.setCursor(15, 10);
  tft.println("Album du jour");
  tft.drawFastHLine(10, 34, tft.width() - 20, COULEUR_CADRE);

  if (!debloque) {
    tft.setTextSize(1);
    tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
    tft.setCursor(20, 80);
    tft.println(heureSynchro ? "Rendez-vous a 22h pour revivre votre journee !" : "(Synchronisation de l'heure...)");
    return;
  }

  if (nbEntreesJournal == 0) {
    tft.setTextSize(1);
    tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
    tft.setCursor(20, 80);
    tft.println("(Rien d'echange aujourd'hui)");
    return;
  }

  if (albumIndexCourant >= nbEntreesJournal) albumIndexCourant = 0;
  EntreeJournal &e = journalDuJour[albumIndexCourant];

  time_t tEntree = (time_t)e.timestamp;
  struct tm *infosEntree = localtime(&tEntree);
  tft.fillRoundRect(10, 42, tft.width() - 20, 24, 8, COULEUR_PANEL);
  tft.drawRoundRect(10, 42, tft.width() - 20, 24, 8, COULEUR_CADRE);

  // Petite icone de type : bulle pour un message, cadre photo pour une photo
  if (e.type == "photo") {
    tft.drawRoundRect(18, 47, 13, 10, 2, COULEUR_OR);
    tft.fillCircle(22, 51, 1, COULEUR_OR);
    tft.drawLine(19, 55, 24, 50, COULEUR_OR);
  } else {
    tft.fillRoundRect(18, 46, 15, 9, 3, COULEUR_OR);
    tft.fillTriangle(20, 55, 25, 55, 21, 58, COULEUR_OR);
  }

  tft.setTextSize(1);
  tft.setTextColor(COULEUR_OR, COULEUR_PANEL);
  tft.setCursor(40, 49);
  tft.printf("%s - %02d:%02d", e.auteur.c_str(), infosEntree->tm_hour, infosEntree->tm_min);

  int zoneY = 74;
  int zoneH = tft.height() - zoneY - 24;
  tft.fillRoundRect(10, zoneY, tft.width() - 20, zoneH, 10, COULEUR_PANEL);
  tft.drawRoundRect(10, zoneY, tft.width() - 20, zoneH, 10, COULEUR_CADRE);

  if (e.type == "photo") {
    // La plupart des photos sont en portrait : on demande un cadre PORTRAIT
    // (ratio 3:4, comme pour la photo plein ecran des reactions) plutot
    // qu'un cadre large, pour ne pas couper le haut/bas de la photo. On
    // centre ensuite horizontalement dans le panneau.
    int photoH = zoneH - 16;
    int photoW = (photoH * 3) / 4;
    if (photoW > tft.width() - 40) photoW = tft.width() - 40;
    int photoX = 10 + (tft.width() - 20 - photoW) / 2;
    String urlZoom = urlPourTaille(e.contenu, photoW, photoH);
    afficherPhotoDepuisURL(urlZoom, photoX, zoneY + 8);
  } else {
    afficherTexteEmoji(20, zoneY + 14, tft.width() - 40, e.contenu, COULEUR_TEXTE, COULEUR_PANEL, 2);
  }

  // Petits points indiquant la position dans le defilement du jour
  int dotsY = tft.height() - 14;
  int totalW = nbEntreesJournal * 12;
  int dotsX = (tft.width() - totalW) / 2;
  for (int i = 0; i < nbEntreesJournal; i++) {
    uint16_t c = (i == albumIndexCourant) ? COULEUR_OR : COULEUR_DIM;
    tft.fillCircle(dotsX + i * 12, dotsY, 3, c);
  }
}

void afficherPageSelonIndex() {
  memoMinuteAffichage = -1; // Force l'horloge a se redessiner
  switch (pageActuelle) {
    case 0: forcerReaffichageAccueil(); break;
    case 1:
      chargerAgenda();
      scrollAgendaY = 0;
      dessinerPageAgenda();
      break;
    case 2:
      modeDessinActif = false;
      dessinerPageDessin();
      break;
    case 3:
      pageRelationInit = false;
      dessinerPageRelation();
      break;
    case 4: dessinerPageEmploiDuTemps(); break;
    case 5:
      pagePlanteInit = false;
      dessinerPagePlante();
      break;
    case 6:
      chargerJournalDuJour();
      albumIndexCourant = 0;
      albumDernierChangement = millis();
      dessinerPageAlbum();
      break;
  }
}

bool dansRect(int x, int y, int rx, int ry, int rw, int rh) {
  return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

// ==========================================================
// ---- GESTION TACTILE ----
// ==========================================================
void gererTactile() {
  uint16_t x, y;
  bool touche = tft.getTouch(&x, &y);
  bool debutTouche = !toucheEnCours;

  if (touche) {
    derniereInteraction = millis();
    if (!toucheEnCours) {
      toucheEnCours = true;
      touchStartX = x; touchStartY = y;
      touchLastX = x; touchLastY = y;
    }

    // --- AMELIORATION DU DESSIN (Fluidité et Gomme épaisse) ---
    if (pageActuelle == 2 && modeDessinActif) {
      int yMin = HEADER_H + 2;
      int yMax = tft.height() - 40;
      if (y > yMin && y < yMax) {
        if (debutTouche) {
          if (couleurDessin == TFT_BLACK) tft.fillCircle(x, y, 6, TFT_BLACK);
          else tft.fillCircle(x, y, 2, couleurDessin);
        } else if (abs(touchLastX - (int)x) + abs(touchLastY - (int)y) >= 1) {
          tft.startWrite();
          if (couleurDessin == TFT_BLACK) {
            tft.fillCircle(x, y, 6, TFT_BLACK);
          } else {
            tft.fillCircle(x, y, 2, couleurDessin);
            tft.drawLine(touchLastX, touchLastY, x, y, couleurDessin);
            tft.drawLine(touchLastX + 1, touchLastY, x + 1, y, couleurDessin);
            tft.drawLine(touchLastX, touchLastY + 1, x, y + 1, couleurDessin);
          }
          tft.endWrite();

          if (nbSegmentsEnvoi < MAX_SEGMENTS) {
            segmentsEnvoi[nbSegmentsEnvoi].x1 = touchLastX;
            segmentsEnvoi[nbSegmentsEnvoi].y1 = touchLastY;
            segmentsEnvoi[nbSegmentsEnvoi].x2 = x;
            segmentsEnvoi[nbSegmentsEnvoi].y2 = y;
            segmentsEnvoi[nbSegmentsEnvoi].couleur = couleurDessin;
            nbSegmentsEnvoi++;
          }
        }
      }
    }
    touchLastX = x; touchLastY = y;

  } else if (toucheEnCours) {
    int dx = abs(touchLastX - touchStartX);
    int dy = abs(touchLastY - touchStartY);

    if (dx < SEUIL_TAP && dy < SEUIL_TAP) {
      if (photoPleinEcran) {
        if (touchStartX >= 420) {
          String reactionChoisie = "";
          int idxBouton = 0;
          if (touchStartY < 64)       { reactionChoisie = "heart"; idxBouton = 0; }
          else if (touchStartY < 128) { reactionChoisie = "star";  idxBouton = 1; }
          else if (touchStartY < 192) { reactionChoisie = "shock"; idxBouton = 2; }
          else if (touchStartY < 256) { reactionChoisie = "cry";   idxBouton = 3; }
          else                        { reactionChoisie = "poop";  idxBouton = 4; }

          Firebase.RTDB.setString(&fbdo, CHEMIN_AUTRE, reactionChoisie + "_" + String(millis()));
          arroserPlante();

          // --- CORRECTION : confirmation visuelle claire ---
          // (l'ancien toast ne s'affichait jamais ici car declencherToast()
          // est bloque tant que photoPleinEcran est actif : on affiche donc
          // le message directement sur le bouton presse)
          const int yBoutons[5]         = { 10, 72, 134, 196, 258 };
          const uint16_t coulBoutons[5] = { TFT_PINK, COULEUR_OR, COULEUR_CORAL, TFT_CYAN, COULEUR_OK };
          const char* iconBoutons[5]    = { "<3", "^w^", "O_O", "T_T", ">_<" };
          const int cursorXBoutons[5]   = { 433, 424, 424, 424, 424 };
          int yBtn = yBoutons[idxBouton];

          tft.fillRoundRect(415, yBtn, 60, 50, 8, COULEUR_OK);
          tft.setTextColor(COULEUR_PANEL, COULEUR_OK);
          tft.setTextSize(1);
          tft.setCursor(420, yBtn + 15); tft.println("Envoye");
          tft.setCursor(430, yBtn + 28); tft.print("!");
          delay(600);

          // On restaure l'icone d'origine du bouton
          tft.setTextSize(2);
          tft.fillRoundRect(415, yBtn, 60, 50, 8, coulBoutons[idxBouton]);
          tft.setTextColor(TFT_BLACK, coulBoutons[idxBouton]);
          tft.setCursor(cursorXBoutons[idxBouton], yBtn + 17); tft.print(iconBoutons[idxBouton]);

        } else {
          photoPleinEcran = false;
          forcerReaffichageAccueil();
        }
      }
      else if (pageActuelle == 2 && modeDessinActif) {
        if (touchStartX > 275 && touchStartX < 400 && touchStartY < HEADER_H) {
          modeDessinActif = false;
          dessinerPageDessin();
        }
        else if (touchStartY > tft.height() - 40) {
          if (touchStartX < 50) couleurDessin = TFT_WHITE;
          else if (touchStartX < 90) couleurDessin = TFT_RED;
          else if (touchStartX < 130) couleurDessin = TFT_GREEN;
          else if (touchStartX < 170) couleurDessin = TFT_BLUE;
          else if (touchStartX > 180 && touchStartX < 260) couleurDessin = TFT_BLACK;
          else if (touchStartX > 350) {
            tft.fillRoundRect(350, tft.height() - 35, 120, 30, 4, COULEUR_OR);
            tft.setTextColor(TFT_BLACK, COULEUR_OR);
            tft.setCursor(370, tft.height() - 28);
            tft.print("ENVOI...");
            envoyerDessin();
            dessinerPageDessin();
          }
        }
      }
      else {
        if (pageActuelle == 2 && !modeDessinActif && touchStartX > 270 && touchStartX < 400 && touchStartY < HEADER_H) {
          modeDessinActif = true;
          dessinerPageDessin();
        }
        else if (touchStartX < ZONE_NAV_LARG) {
          pageActuelle--;
          if (pageActuelle < 0) pageActuelle = NB_PAGES - 1;
          afficherPageSelonIndex();
        }
        else if (touchStartX > tft.width() - ZONE_NAV_LARG) {
          pageActuelle++;
          if (pageActuelle >= NB_PAGES) pageActuelle = 0;
          afficherPageSelonIndex();
        }
        else if (pageActuelle == 0 && dansRect(touchStartX, touchStartY, PHOTO_X, PHOTO_Y, PHOTO_W, PHOTO_H)) {
          photoPleinEcran = true;
          afficherPhotoPleinEcran();
        }
        else if (pageActuelle == 1) {
          // --- SCROLL AGENDA PLUS RAPIDE ---
          if (touchStartY < tft.height() / 2) scrollAgendaY -= 140;
          else scrollAgendaY += 140;

          if (scrollAgendaY < 0) scrollAgendaY = 0;
          if (scrollAgendaY > maxScrollAgenda) scrollAgendaY = maxScrollAgenda;
          dessinerPageAgenda();
        }
      }
    }
    toucheEnCours = false;
    touchStartX = -1;
  }
}

// ==========================================================
// ---- SETUP & LOOP ----
// ==========================================================
void afficherAnimationReaction(String type) {
  if (type.startsWith("heart")) declencherToast("Reaction : coeur");
  else if (type.startsWith("star")) declencherToast("Reaction : etoile");
  else if (type.startsWith("shock")) declencherToast("Reaction recue !");
  else if (type.startsWith("cry")) declencherToast("Reaction recue !");
  else if (type.startsWith("poop")) declencherToast("Reaction recue !");
  else declencherToast("Reaction recue !");
}

void afficherMessageStatus(const char* ligne1, const char* ligne2 = "") {
  tft.fillScreen(COULEUR_FOND);
  tft.setTextColor(COULEUR_OR, COULEUR_FOND);
  tft.setTextSize(2);
  tft.setCursor(15, 40); tft.println(ligne1);
  if (strlen(ligne2) > 0) {
      tft.setTextColor(COULEUR_TEXTE, COULEUR_FOND);
      tft.setTextSize(1);
      tft.setCursor(15, 75);
      tft.println(ligne2);
  }
}

void setup() {
  Serial.begin(115200); delay(500);
  if (!SPIFFS.begin(true)) Serial.println("Erreur SPIFFS");

  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);
  tft.setTouch(calData);

  pinMode(PIN_CLK, INPUT_PULLUP); pinMode(PIN_DT, INPUT_PULLUP); pinMode(PIN_SW, INPUT_PULLUP);
  lastCLK = digitalRead(PIN_CLK);
  attachInterrupt(digitalPinToInterrupt(PIN_CLK), onEncoderChange, CHANGE);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(PIN_BACKLIGHT, pwmChannel);
  appliquerLuminosite();

  dessinerStatutWifi("Configuration Wi-Fi", "Connectez-vous au reseau :");
  if (!wm.autoConnect("LDR_Box_" NOM_PROPRIETAIRE)) { delay(2000); ESP.restart(); }

  dessinerStatutWifi("Wi-Fi connecte", "Connexion a Firebase...");
  // IMPORTANT : on synchronise l'horloge systeme en UTC pur (0, 0).
  // Tous les timestamps stockes dans Firebase (retrouvailles, plante,
  // capsule...) sont des epoch UTC. Avant, configTime(3600, 3600, ...)
  // decalait l'horloge SYSTEME elle-meme de +2h en permanence : time(nullptr)
  // ne correspondait plus a un vrai epoch UTC, ce qui faussait tous les
  // comptes a rebours de 1 a 2h (donc parfois un jour de trop/de moins).
  // Le fuseau horaire France (avec passage heure d'ete/hiver automatique)
  // n'est applique qu'a l'AFFICHAGE via localtime(), qui lit la variable TZ.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  config.api_key = API_KEY; config.database_url = DATABASE_URL;
  Firebase.signUp(&config, &auth, "", "");
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // --- OTA : mises a jour a distance par Wi-Fi (nom + mot de passe a
  //     personnaliser). Necessite un Partition Scheme avec OTA active
  //     (voir commentaire pres de la definition du schema dans le menu
  //     Outils de l'IDE Arduino) sinon la compilation ou l'upload OTA echoue.
  ArduinoOTA.setHostname("LDR-Box-" NOM_PROPRIETAIRE);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    tft.fillScreen(COULEUR_FOND);
    tft.setTextColor(COULEUR_OR, COULEUR_FOND);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("Mise a jour OTA...");
    tft.setTextSize(1);
    tft.setTextColor(COULEUR_DIM, COULEUR_FOND);
    tft.setCursor(20, 130);
    tft.println("Ne pas eteindre le boitier.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = (progress * 100) / total;
    tft.fillRect(20, 155, 240, 20, COULEUR_FOND);
    tft.setTextColor(COULEUR_OR, COULEUR_FOND);
    tft.setTextSize(2);
    tft.setCursor(20, 155);
    tft.printf("%d %%", pct);
  });
  ArduinoOTA.onEnd([]() {
    tft.setTextColor(COULEUR_OK, COULEUR_FOND);
    tft.setCursor(20, 190);
    tft.println("Termine, redemarrage...");
  });
  ArduinoOTA.onError([](ota_error_t erreur) {
    tft.setTextColor(TFT_RED, COULEUR_FOND);
    tft.setCursor(20, 190);
    tft.println("Erreur de mise a jour !");
  });
  ArduinoOTA.begin();

  derniereInteraction = millis();
  forcerReaffichageAccueil();
}

void loop() {
  ArduinoOTA.handle();
  appliquerLuminosite();
  gererTactile();
  gererBoutonEncodeur();
  verifierWifi();
  dessinerHorloge();

  // --- Toucher en Direct : on ne previent Firebase QUE quand l'etat change
  //     (pas a chaque tick), pour ne pas spammer la base. ---
  if (toucheEnCours != jeTouchaisAvant) {
    jeTouchaisAvant = toucheEnCours;
    Firebase.RTDB.setBool(&fbdoToucher, CHEMIN_TOUCHER_MOI, toucheEnCours);
  }

  if (affichagePopupLuminosite && (millis() - timerPopup > 1500)) {
    affichagePopupLuminosite = false;
    restaurerEnteteApresOverlay();
  }

  if (affichageToast && (millis() - timerToast > 3000)) {
    affichageToast = false;
    restaurerEnteteApresOverlay();
  }

  // --- Defilement automatique de l'Album (page 6) ---
  if (pageActuelle == 6 && nbEntreesJournal > 0 && (millis() - albumDernierChangement > ALBUM_INTERVALLE_DEFILEMENT)) {
    albumDernierChangement = millis();
    albumIndexCourant = (albumIndexCourant + 1) % nbEntreesJournal;
    dessinerPageAlbum();
  }

  // --- Ouverture automatique de l'Album a 22h (une seule fois par jour, et
  //     seulement si on n'est pas deja en train d'utiliser le boitier) ---
  {
    time_t maintenantAuto = time(nullptr);
    if (maintenantAuto > 1700000000) {
      struct tm *infosAuto = localtime(&maintenantAuto);
      int64_t jourAuto = (int64_t)maintenantAuto / 86400;
      if (jourAuto != albumJourAutoAffiche) {
        albumJourAutoAffiche = jourAuto;
        albumAutoAfficheAujourdHui = false;
      }
      if (!albumAutoAfficheAujourdHui && infosAuto->tm_hour >= 22 &&
          pageActuelle != 6 && !photoPleinEcran && !modeDessinActif &&
          (millis() - derniereInteraction) > 15000UL) {
        albumAutoAfficheAujourdHui = true;
        pageActuelle = 6;
        afficherPageSelonIndex();
      }
    }
  }

  if (!modeDessinActif && Firebase.ready() && (millis() - dernierCheck > intervalleCheck)) {
    dernierCheck = millis();
    if (Firebase.RTDB.getJSON(&fbdo, CHEMIN_BASE)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData jsonData;
      bool nouvelleInfo = false;

      if (json.get(jsonData, "text")) {
        if (dernierTexte != jsonData.stringValue && dernierTexte != "") {
          nouvelleInfo = true;
          declencherToast("Nouveau Message !");
          ajouterAuJournal("message", jsonData.stringValue);
        }
        dernierTexte = jsonData.stringValue;
      }

      if (json.get(jsonData, "photo")) {
        if (dernierePhotoURL != jsonData.stringValue && dernierePhotoURL != "") {
          nouvelleInfo = true;
          declencherToast("Nouvelle Photo !");
          ajouterAuJournal("photo", jsonData.stringValue);
        }
        dernierePhotoURL = jsonData.stringValue;
      }

      if (json.get(jsonData, "emploi_du_temps")) dernierEmploiDuTempsURL = jsonData.stringValue;

      if (json.get(jsonData, "msg_secret_timestamp")) targetMessageSecret = (int64_t)jsonData.doubleValue;
      if (json.get(jsonData, "msg_secret_texte")) texteMessageSecret = jsonData.stringValue;

      if (nouvelleInfo) {
        notificationMessage = true;
        pageNotificationCible = 0;
        derniereInteraction = millis(); // Rallume l'écran à fond
        mettreAJourBadgeNotif();
        arroserPlante();

        // Si l'Album est deja affiche (apres 22h), on le rafraichit tout de
        // suite avec la nouvelle entree, au lieu d'attendre une navigation.
        if (pageActuelle == 6) {
          chargerJournalDuJour();
          if (albumIndexCourant >= nbEntreesJournal) albumIndexCourant = 0;
          dessinerPageAlbum();
        }
      }

      if (json.get(jsonData, "reaction")) {
        String nouvelleReaction = jsonData.stringValue;
        if (nouvelleReaction != derniereReactionRecue && derniereReactionRecue != "") {
          derniereReactionRecue = nouvelleReaction;
          derniereInteraction = millis(); // Rallume l'écran à fond
          afficherAnimationReaction(nouvelleReaction);
          arroserPlante();
        } else if (derniereReactionRecue == "") {
          derniereReactionRecue = nouvelleReaction;
        }
      }

      if (json.get(jsonData, "poke")) {
        String nouveauPoke = jsonData.stringValue;
        if (nouveauPoke != dernierPokeRecu && dernierPokeRecu != "") {
          dernierPokeRecu = nouveauPoke;
          derniereInteraction = millis();
          declencherAnimationPoke();
        } else if (dernierPokeRecu == "") {
          dernierPokeRecu = nouveauPoke;
        }
      }

      if (json.get(jsonData, "dessin")) {
        String nouveauDessin = jsonData.stringValue;
        if (nouveauDessin != signatureDessinRecu && signatureDessinRecu != "") {
          signatureDessinRecu = nouveauDessin;
          chargerDessinRecu(nouveauDessin);
          notificationDessin = true;
          pageNotificationCible = 2;
          derniereInteraction = millis(); // Rallume l'écran à fond
          declencherToast("Nouveau Dessin !");
          mettreAJourBadgeNotif();
          arroserPlante();
        } else if (signatureDessinRecu == "") {
          signatureDessinRecu = nouveauDessin;
          chargerDessinRecu(nouveauDessin);
        }
      }

      if (pageActuelle == 0 && !photoPleinEcran) dessinerPageAccueil();
      if (pageActuelle == 3) dessinerPageRelation();
      if (pageActuelle == 5) dessinerPagePlante();
    }

    // --- Compteur "Prochaines retrouvailles" : lecture via getJSON (plus fiable
    //     que getInt avec cette librairie) de tout /partage en une fois. ---
    if (Firebase.RTDB.getJSON(&fbdo, CHEMIN_PARTAGE)) {
      FirebaseJson &jsonPartage = fbdo.jsonObject();
      FirebaseJsonData jdPartage;
      bool retrouvaillesMiseAJour = false;

      if (jsonPartage.get(jdPartage, "retrouvaille_timestamp")) {
        int64_t nouvelleCible = normaliserTimestamp((int64_t)jdPartage.doubleValue);
        if (nouvelleCible != targetRetrouvailles) {
          targetRetrouvailles = nouvelleCible;
          retrouvaillesMiseAJour = true;
        }
      }

      if (jsonPartage.get(jdPartage, "start_retrouvaille")) {
        int64_t nouveauDebut = normaliserTimestamp((int64_t)jdPartage.doubleValue);
        if (nouveauDebut != startRetrouvailles) {
          startRetrouvailles = nouveauDebut;
          retrouvaillesMiseAJour = true;
        }
      }

      if (retrouvaillesMiseAJour && pageActuelle == 3) {
        pageRelationInit = false;
        dessinerPageRelation();
      }
    }

    // --- Synchronisation de la Plante Virtuelle (partagee entre les deux boitiers) ---
    if (Firebase.RTDB.getJSON(&fbdoPlante, CHEMIN_PLANTE)) {
      FirebaseJson &jsonPlante = fbdoPlante.jsonObject();
      FirebaseJsonData jdPlante;
      bool plantMiseAJour = false;
      if (jsonPlante.get(jdPlante, "niveau") && jdPlante.intValue != plantNiveauBase) {
        plantNiveauBase = jdPlante.intValue;
        plantMiseAJour = true;
      }
      if (jsonPlante.get(jdPlante, "derniere_interaction")) {
        int64_t nouvelleInteraction = (int64_t)jdPlante.doubleValue;
        if (nouvelleInteraction != plantDerniereInteraction) {
          plantDerniereInteraction = nouvelleInteraction;
          plantMiseAJour = true;
        }
      }
      // Si la valeur vient de changer (ex: arrosee depuis l'autre boitier),
      // on force le redessin immediat de la page concernee.
      if (plantMiseAJour) {
        if (pageActuelle == 5) {
          pagePlanteInit = false;
          dessinerPagePlante();
        }
        if (pageActuelle == 0 && !photoPleinEcran) {
          rafraichirAlertePlanteAccueil();
        }
      }
    }

    // --- Toucher en Direct : on regarde si le partenaire touche AUSSI son
    //     ecran en ce moment. Juste un toast doux (comme une notif), non
    //     bloquant : on peut continuer a utiliser le boitier normalement. ---
    if (Firebase.RTDB.getJSON(&fbdoToucher, CHEMIN_TOUCHER)) {
      FirebaseJson &jsonToucher = fbdoToucher.jsonObject();
      FirebaseJsonData jdToucher;
      if (jsonToucher.get(jdToucher, CLE_TOUCHER_PARTENAIRE)) {
        partenaireTouche = jdToucher.boolValue;
      }
      if (toucheEnCours && partenaireTouche) {
        if (!synchroDejaSignalee) {
          synchroDejaSignalee = true;
          declencherToast("Vous pensez l'un a l'autre !");
        }
      } else {
        synchroDejaSignalee = false;
      }
    }
  }
}
