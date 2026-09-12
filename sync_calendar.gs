// Configuration de ta LDR Box
// - API_KEY et le prefixe de l'URL : voir docs/SETUP_FIREBASE.md
// - Remplace "appareil_a" par MON_ID (l'ID utilise dans le firmware de CE
//   boitier) : ce script doit tourner une fois par boitier/agenda.
const API_KEY = "VOTRE_CLE_API_FIREBASE";
const DATABASE_URL = "https://VOTRE-PROJET-default-rtdb.VOTRE-REGION.firebasedatabase.app/devices/appareil_a/agenda.json";

// Fonction pour se connecter anonymement et obtenir un jeton d'accès
function getFirebaseAuthToken() {
  const url = "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=" + API_KEY;
  const options = {
    method: "post",
    contentType: "application/json",
    payload: JSON.stringify({ returnSecureToken: true })
  };
  
  const response = UrlFetchApp.fetch(url, options);
  const json = JSON.parse(response.getContentText());
  return json.idToken;
}

function syncCalendarToFirebase() {
  const tz = Session.getScriptTimeZone();
  const now = new Date();
  const oneMonthFromNow = new Date();
  oneMonthFromNow.setMonth(now.getMonth() + 1);
  
  const allItems = [];

  // ==========================================
  // 1. RÉCUPÉRATION DES ÉVÉNEMENTS CALENDRIER
  // ==========================================
  const calendar = CalendarApp.getDefaultCalendar();
  const events = calendar.getEvents(now, oneMonthFromNow);
  
  events.forEach((event) => {
    const startTime = event.getStartTime();
    const endTime = event.getEndTime();
    const isAllDay = event.isAllDayEvent();
    
    let startDateStr = Utilities.formatDate(startTime, tz, "dd/MM");
    
    // Pour les événements "Toute la journée", Google fixe la fin au lendemain à 00:00.
    let trueEndTime = isAllDay ? new Date(endTime.getTime() - 1000) : endTime;
    let endDateStr = Utilities.formatDate(trueEndTime, tz, "dd/MM");
    
    let isMultiDay = (startDateStr !== endDateStr);
    let dateTexte = isMultiDay ? ("Du " + startDateStr + " au " + endDateStr) : startDateStr;
    let heureTexte = isAllDay ? "Jour entier" : Utilities.formatDate(startTime, tz, "HH:mm");

    allItems.push({
      title: event.getTitle(),
      date: dateTexte,
      time: heureTexte,
      isMultiDay: isMultiDay,
      timestamp: startTime.getTime(),
      task: false // Événement de calendrier
    });
  });

  // ==========================================
  // 2. RÉCUPÉRATION DES TÂCHES GOOGLE TASKS
  // ==========================================
  try {
    const taskLists = Tasks.Tasklists.list().items;

    if (taskLists && taskLists.length > 0) {
      taskLists.forEach(taskList => {
        const tasks = Tasks.Tasks.list(taskList.id, {
          showCompleted: false,
          showHidden: false
        }).items;

        if (tasks) {
          tasks.forEach(task => {
            let taskDate = task.due ? new Date(task.due) : null;
            let dateTexte = taskDate ? Utilities.formatDate(taskDate, tz, "dd/MM") : "À faire";
            // Si pas de date limite, on place la tâche après les événements datés
            let timestamp = taskDate ? taskDate.getTime() : (now.getTime() + 9999999999);

            allItems.push({
              title: task.title,
              date: dateTexte,
              time: "Tâche",
              isMultiDay: false,
              timestamp: timestamp,
              task: true // Tâche Google Tasks
            });
          });
        }
      });
    }
  } catch (errorTasks) {
    Logger.log("Erreur Google Tasks (pensez à activer le service Tasks API) : " + errorTasks.toString());
  }

  // ==========================================
  // 3. TRI PAR ORDRE CHRONOLOGIQUE
  // ==========================================
  allItems.sort((a, b) => a.timestamp - b.timestamp);

  // ==========================================
  // 4. CRÉATION DES CLÉS event_0, event_1...
  // ==========================================
  const eventsList = {};
  allItems.forEach((item, index) => {
    eventsList["event_" + index] = item;
  });

  // ==========================================
  // 5. ENVOI À FIREBASE
  // ==========================================
  try {
    const idToken = getFirebaseAuthToken();
    const authenticatedUrl = DATABASE_URL + "?auth=" + idToken;

    const options = {
      method: "put", 
      contentType: "application/json",
      payload: JSON.stringify(eventsList)
    };
    
    UrlFetchApp.fetch(authenticatedUrl, options);
    Logger.log("Synchronisation réussie ! " + Object.keys(eventsList).length + " éléments (événements + tâches) envoyés.");
  } catch (error) {
    Logger.log("Erreur lors de la synchronisation : " + error.toString());
  }
}