/* CONFIGURACION DE CONECTA (PMU local offline-first v7)
   - backendUrl apunta al servidor puente local (puente_usb_pmu.py) en este portatil.
   - firebaseUrl queda vacio para operar 100% offline y cumplir Ley 1581.
   - torres: estaciones (ESP32) E01 a E09 con sus coordenadas aproximadas en Manizales. */
window.CONECTA_CONFIG = {
  backendUrl: window.location.origin,
  firebaseUrl: '',
  torres: [
    { id: 'E01', nombre: 'La Enea',           lat: 5.0318, lon: -75.4700 },
    { id: 'E02', nombre: 'Villamaría',        lat: 5.0445, lon: -75.5145 },
    { id: 'E03', nombre: 'Chipre',            lat: 5.0730, lon: -75.5265 },
    { id: 'E04', nombre: 'Centro',            lat: 5.0687, lon: -75.5174 },
    { id: 'E05', nombre: 'Bosques del Norte', lat: 5.0815, lon: -75.4990 },
    { id: 'E06', nombre: 'Villa Carmenza',    lat: 5.0575, lon: -75.5115 },
    { id: 'E07', nombre: 'El Tablazo',        lat: 5.1150, lon: -75.4750 },
    { id: 'E08', nombre: 'Palogrande',        lat: 5.0565, lon: -75.4925 },
    { id: 'E09', nombre: 'El Cable',          lat: 5.0555, lon: -75.4878 }
  ]
};
