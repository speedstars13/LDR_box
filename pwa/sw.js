const CACHE_NAME = 'ldr-box-cache-v1';

// Événement d'installation
self.addEventListener('install', (e) => {
  self.skipWaiting();
});

// Événement d'activation
self.addEventListener('activate', (e) => {
  e.waitUntil(clients.claim());
});

// Stratégie réseau d'abord pour le temps réel
self.addEventListener('fetch', (e) => {
  e.respondWith(
    fetch(e.request).catch(() => {
      return caches.match(e.request);
    })
  );
});