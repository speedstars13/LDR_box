// =========================================================================
// --- 1. INITIALISATION DE LA BASE DE DONNÉES EN MODE MODULE ---
// =========================================================================
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { getDatabase, ref, set, push, onValue } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";
import { getAuth, signInAnonymously } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js";

// À COMPLÉTER avec les infos de ta console Firebase
const firebaseConfig = {
  apiKey: "VOTRE_CLE_API_FIREBASE",
  authDomain: "VOTRE-PROJET.firebaseapp.com",
  databaseURL: "https://VOTRE-PROJET-default-rtdb.VOTRE-REGION.firebasedatabase.app",
  projectId: "VOTRE-PROJET",
};

// Initialisation directe
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);
const auth = getAuth(app);

// =========================================================================
// --- 2. GESTION DES ESPACES COMPTES ---
// =========================================================================
const authSection = document.getElementById('auth-section');
const interfaceSection = document.getElementById('interface-section');
const userBadge = document.getElementById('user-badge');
const btnLogout = document.getElementById('btn-logout');

let monProfil = localStorage.getItem('ldr_profile');

async function verifierAuthentification() {
    if (monProfil) {
        try {
            if (!auth.currentUser) {
                await signInAnonymously(auth);
            }
            if (authSection) authSection.style.display = 'none';
            if (interfaceSection) interfaceSection.style.display = 'block';
            if (userBadge) userBadge.textContent = `[PROFIL: ${monProfil.toUpperCase()}]`;
            initialiserHistorique();
        } catch (erreur) {
            console.error("[AUTH] Erreur connexion anonyme :", erreur);
        }
    } else {
        if (authSection) authSection.style.display = 'block';
        if (interfaceSection) interfaceSection.style.display = 'none';
    }
}

const btnPersonB = document.getElementById('btn-person-b');
if (btnPersonB) {
    btnPersonB.addEventListener('click', async () => {
        localStorage.setItem('ldr_profile', 'personB');
        monProfil = 'personB';
        await verifierAuthentification();
    });
}

const btnPersonA = document.getElementById('btn-person-a');
if (btnPersonA) {
    btnPersonA.addEventListener('click', async () => {
        localStorage.setItem('ldr_profile', 'personA');
        monProfil = 'personA';
        await verifierAuthentification();
    });
}

if (btnLogout) {
    btnLogout.addEventListener('click', () => {
        localStorage.removeItem('ldr_profile');
        monProfil = null;
        verifierAuthentification();
    });
}

// =========================================================================
// --- 3. PIPELINE TRANSMISSION TEXTE ---
// =========================================================================
const messageInput = document.getElementById('message-input');
const btnSendTxt = document.getElementById('btn-send-txt');

if (btnSendTxt) {
    btnSendTxt.addEventListener('click', () => {
        const texte = messageInput.value.trim();
        if (!texte) return;

        const cibleAppareil = (monProfil === 'personB') ? 'appareil_a' : 'appareil_b';

        set(ref(db, `devices/${cibleAppareil}/current/text`), texte);
        set(ref(db, `devices/${cibleAppareil}/current/last_update`), "text");

        push(ref(db, 'history'), {
            from: monProfil,
            type: "text",
            value: texte,
            timestamp: Date.now()
        });

        messageInput.value = "";
        alert("Message envoyé au serveur !");
    });
}

// =========================================================================
// --- 4. PIPELINE TRANSMISSION MEDIA (CLOUDINARY) ---
// =========================================================================
const photoInput = document.getElementById('photo-input');
const photoStatus = document.getElementById('photo-status');
const btnSwitchCamera = document.getElementById('btn-switch-camera');
const btnSendPhoto = document.getElementById('btn-send-photo');
const btnOpenCamera = document.getElementById('btn-open-camera');
const btnCapturePhoto = document.getElementById('btn-capture-photo');
const btnCloseCamera = document.getElementById('btn-close-camera');
const cameraContainer = document.getElementById('camera-container');
const cameraPreview = document.getElementById('camera-preview');
const cameraCanvas = document.getElementById('camera-canvas');

let cameraStream = null;
let photoPrise = null;
let cameraFacingMode = "environment";

const CLOUDINARY_CLOUD_NAME = "VOTRE_CLOUD_NAME";
const CLOUDINARY_UPLOAD_PRESET = "VOTRE_UPLOAD_PRESET";

if (photoInput) {
    photoInput.addEventListener('change', () => {
        photoPrise = null;
        if (photoInput.files[0]) {
            photoStatus.textContent = `📁 ${photoInput.files[0].name}`;
        } else {
            photoStatus.textContent = "Aucune photo sélectionnée";
        }
    });
}

async function ouvrirCamera() {
    try {
        cameraStream = await navigator.mediaDevices.getUserMedia({
            video: { facingMode: cameraFacingMode }
        });
        cameraPreview.srcObject = cameraStream;
        cameraContainer.style.display = "block";
    } catch (err) {
        console.error(err);
        alert("Impossible d'accéder à la caméra.");
    }
}

function fermerCamera() {
    if (cameraStream) {
        cameraStream.getTracks().forEach(track => track.stop());
        cameraStream = null;
    }
    cameraContainer.style.display = "none";
}

if (btnOpenCamera) {
    btnOpenCamera.addEventListener('click', async () => {
        photoInput.value = "";
        photoPrise = null;
        photoStatus.textContent = "📷 Caméra ouverte";
        await ouvrirCamera();
    });
}

if (btnCloseCamera) {
    btnCloseCamera.addEventListener('click', () => fermerCamera());
}

if (btnSwitchCamera) {
    btnSwitchCamera.addEventListener('click', async () => {
        fermerCamera();
        cameraFacingMode = (cameraFacingMode === "environment") ? "user" : "environment";
        await ouvrirCamera();
    });
}

if (btnCapturePhoto) {
    btnCapturePhoto.addEventListener('click', () => {
        cameraCanvas.width = cameraPreview.videoWidth;
        cameraCanvas.height = cameraPreview.videoHeight;
        const ctx = cameraCanvas.getContext('2d');
        ctx.drawImage(cameraPreview, 0, 0, cameraCanvas.width, cameraCanvas.height);

        cameraCanvas.toBlob(blob => {
            photoPrise = new File([blob], `photo_${Date.now()}.jpg`, { type: 'image/jpeg' });
            if (photoStatus) {
                photoStatus.textContent = "📷 Photo capturée : prête à être envoyée";
            }
            fermerCamera();
        }, 'image/jpeg', 0.9);
    });
}

if (btnSendPhoto) {
    btnSendPhoto.addEventListener('click', async () => {
        const fichier = photoInput.files[0] || photoPrise;
        if (!fichier) {
            alert("Sélectionnez d'abord un fichier image.");
            return;
        }

        btnSendPhoto.textContent = "CHARGEMENT...";
        btnSendPhoto.disabled = true;

        const formData = new FormData();
        formData.append('file', fichier);
        formData.append('upload_preset', CLOUDINARY_UPLOAD_PRESET);

        try {
            const reponse = await fetch(`https://api.cloudinary.com/v1_1/${CLOUDINARY_CLOUD_NAME}/image/upload`, {
                method: 'POST',
                body: formData
            });

            if (!reponse.ok) throw new Error("Erreur Cloudinary HTTP");

            const donneesCloudinary = await reponse.json();
            const urlBrute = donneesCloudinary.secure_url;
            const urlOptimisee = urlBrute.replace('/upload/', '/upload/w_480,h_320,c_fill,fl_progressive:none,f_jpg/');
            const cibleAppareil = (monProfil === 'personB') ? 'appareil_a' : 'appareil_b';

            await set(ref(db, `devices/${cibleAppareil}/current/photo`), urlOptimisee);
            await set(ref(db, `devices/${cibleAppareil}/current/last_update`), "photo");

            await push(ref(db, 'history'), {
                from: monProfil,
                type: "photo",
                value: urlOptimisee,
                timestamp: Date.now()
            });

            alert("Fichier image synchronisé !");
            photoInput.value = "";
            photoPrise = null;
            photoStatus.textContent = "Aucune photo sélectionnée";

        } catch (erreur) {
            console.error(erreur);
            alert("Erreur lors du traitement de l'image.");
        } finally {
            btnSendPhoto.textContent = "UPLOADER";
            btnSendPhoto.disabled = false;
        }
    });
}

// =========================================================================
// --- 4.5 PIPELINE EMPLOI DU TEMPS ---
// =========================================================================
const scheduleInput = document.getElementById('schedule-input');
const scheduleStatus = document.getElementById('schedule-status');
const btnSendSchedule = document.getElementById('btn-send-schedule');

if (scheduleInput) {
    scheduleInput.addEventListener('change', () => {
        if (scheduleInput.files[0]) {
            scheduleStatus.textContent = `📁 ${scheduleInput.files[0].name}`;
        } else {
            scheduleStatus.textContent = "Aucune image sélectionnée";
        }
    });
}

if (btnSendSchedule) {
    btnSendSchedule.addEventListener('click', async () => {
        const fichier = scheduleInput.files[0];
        if (!fichier) {
            alert("Sélectionnez d'abord une image de l'emploi du temps.");
            return;
        }

        btnSendSchedule.textContent = "CHARGEMENT...";
        btnSendSchedule.disabled = true;

        const formData = new FormData();
        formData.append('file', fichier);
        formData.append('upload_preset', CLOUDINARY_UPLOAD_PRESET); 

        try {
            const reponse = await fetch(`https://api.cloudinary.com/v1_1/${CLOUDINARY_CLOUD_NAME}/image/upload`, {
                method: 'POST',
                body: formData
            });

            if (!reponse.ok) throw new Error("Erreur Cloudinary HTTP");

            const donneesCloudinary = await reponse.json();
            const urlBrute = donneesCloudinary.secure_url;
            const urlOptimisee = urlBrute.replace('/upload/', '/upload/w_480,h_320,c_fill,fl_progressive:none,f_jpg/');
            const cibleAppareil = (monProfil === 'personB') ? 'appareil_a' : 'appareil_b';

            // Envoi dans la nouvelle clé 'emploi_du_temps'
            await set(ref(db, `devices/${cibleAppareil}/current/emploi_du_temps`), urlOptimisee);

            await push(ref(db, 'history'), {
                from: monProfil,
                type: "emploi_du_temps", // Nouveau type pour l'historique
                value: urlOptimisee,
                timestamp: Date.now()
            });

            alert("Emploi du temps synchronisé !");
            scheduleInput.value = "";
            scheduleStatus.textContent = "Aucune image sélectionnée";

        } catch (erreur) {
            console.error(erreur);
            alert("Erreur lors de l'envoi de l'emploi du temps.");
        } finally {
            btnSendSchedule.textContent = "ENVOYER L'EMPLOI DU TEMPS";
            btnSendSchedule.disabled = false;
        }
    });
}

// =========================================================================
// --- 5. SYNCHRONISATION DU FLUX RÉSEAU ---
// =========================================================================
const historyFeed = document.getElementById('history-feed');

function initialiserHistorique() {
    if (!historyFeed) return;

    onValue(ref(db, 'history'), (snapshot) => {
        const donnees = snapshot.val();
        historyFeed.innerHTML = "";

        if (donnees) {
            Object.keys(donnees).forEach(cle => {
                const message = donnees[cle];
                const item = document.createElement('div');
                item.classList.add('feed-item');
                const expediteur = message.from.toUpperCase();

                if (message.type === "photo") {
                    item.innerHTML = `<span class="feed-user">[${expediteur}]</span> 🖼️ [Photo transférée]`;
                } else if (message.type === "emploi_du_temps") {
                    item.innerHTML = `<span class="feed-user">[${expediteur}]</span> 📅 [Emploi du temps mis à jour]`;
                } else {
                    item.innerHTML = `<span class="feed-user">[${expediteur}]</span> ${message.value}`;
                }

                historyFeed.appendChild(item);
            });
            historyFeed.scrollTop = historyFeed.scrollHeight;
        } else {
            historyFeed.innerHTML = `<div class="feed-item"><span class="feed-user">[SYSTEM]</span> Aucun enregistrement sur le cloud.</div>`;
        }
    });
}

// =========================================================================
// --- 6. PIPELINE RELATION (COMPTEUR & CAPSULE TEMPORELLE) ---
// =========================================================================
const btnSendRetrouvailles = document.getElementById('btn-send-retrouvailles');
const btnSendProgrammed = document.getElementById('btn-send-programmed');

// Envoi de la date de retrouvailles et du début du compteur (GLOBAL pour les deux appareils)
if (btnSendRetrouvailles) {
    btnSendRetrouvailles.addEventListener('click', async () => {
        const dateVal = document.getElementById('retrouvailles-date').value;
        if (!dateVal) { alert("Choisis une date d'abord !"); return; }
        
        // 1. Timestamp cible (la date de retrouvailles choisie)
        const targetTimestamp = Math.floor(new Date(dateVal).getTime() / 1000); 
        
        // 2. Timestamp de début (l'instant présent où on clique, pour la jauge)
        const startTimestamp = Math.floor(Date.now() / 1000);

        // On écrit les deux dans le nœud global /partage/
        await set(ref(db, `partage/retrouvaille_timestamp`), targetTimestamp);
        await set(ref(db, `partage/start_retrouvaille`), startTimestamp);
        
        push(ref(db, 'history'), {
            from: monProfil, 
            type: "retrouvailles", 
            value: `Nouveau compte à rebours global lancé !`, 
            timestamp: Date.now()
        });
        alert("Compte à rebours et jauge mis à jour pour tout le monde !");
    });
}

// Envoi de la capsule temporelle (Message + Date)
if (btnSendProgrammed) {
    btnSendProgrammed.addEventListener('click', async () => {
        const texte = document.getElementById('programmed-msg-input').value.trim();
        const dateVal = document.getElementById('programmed-date').value;
        
        if (!texte || !dateVal) { alert("Il faut un message ET une date !"); return; }
        
        const timestamp = Math.floor(new Date(dateVal).getTime() / 1000);
        const cibleAppareil = (monProfil === 'personB') ? 'appareil_a' : 'appareil_b';
        
        // On envoie le texte ET la date d'ouverture à Firebase
        await set(ref(db, `devices/${cibleAppareil}/current/msg_secret_texte`), texte);
        await set(ref(db, `devices/${cibleAppareil}/current/msg_secret_timestamp`), timestamp);
        
        push(ref(db, 'history'), {
            from: monProfil, 
            type: "message_programme", 
            value: `Capsule temporelle scellée !`, 
            timestamp: Date.now()
        });
        
        document.getElementById('programmed-msg-input').value = "";
        alert("Capsule temporelle envoyée et verrouillée !");
    });
}
// Exécution au démarrage
verifierAuthentification();