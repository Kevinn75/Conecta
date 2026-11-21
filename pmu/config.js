// Configuracion local offline-first del PMU hacia el puente USB de la ESP32-S3 Concentrador.
// Usa el mismo origen desde el que se sirve pmu.html (ej. http://localhost:8000).
window.CONECTA_CONFIG = {
  firebaseUrl: window.location.origin
};
