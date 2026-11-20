# Diseño de la interfaz de emergencia

## Objetivo y criterio

Permitir que una persona reporte una necesidad desde el teléfono y comprenda qué ocurrió con su envío. Diseñar para prisa, conectividad inestable y poca familiaridad digital. El orden es: comprensión, uso, accesibilidad, rendimiento y coherencia visual.

La dirección visual propuesta es sobria y clara: superficies blancas, azul profundo para estructura, verde azulado para acciones; ámbar y rojo reservados para advertencias y peligro. Antes de programar, Claude entrega 4–6 tokens de color con contraste comprobado, escala tipográfica, espaciado y un esquema breve del flujo. No declarar una paleta accesible sin medirla.

Usar fuentes del sistema al principio; cuerpo de al menos 16 px, etiquetas visibles, foco claro y objetivos táctiles de 44 px como criterio interno. Identificar estados con texto y forma además de color. Evitar animación decorativa, carruseles, mapas obligatorios, CDN y fuentes remotas.

## Flujo propuesto

1. Inicio: explicar el propósito y el estado de conexión sin prometer ayuda inmediata.
2. Ubicación: sector y referencia; permitir “No sé el barrio” con una referencia alternativa.
3. Necesidad: opciones claras, validación junto al campo y revisión antes de enviar.
4. Comprobante: distinguir simulación, pendiente local, recepción de estación y confirmación del PMU. Una respuesta simulada nunca debe parecer un acuse real.

Estas pantallas son una propuesta para el siguiente trabajo, no funciones existentes.

## Criterios de revisión

- Anchuras de 360, 390, 768 y 1280 px; sin scroll horizontal ni controles cortados.
- Navegación completa con teclado, orden lógico de foco, nombres accesibles y errores asociados al campo.
- Contraste WCAG AA como objetivo: medir colores y revisar texto, foco y componentes; una prueba automatizada sola no certifica accesibilidad.
- Ejercitar validación, doble envío, respuesta lenta, error y pérdida de conexión. Mantener el contenido ante un reintento cuando el diseño lo permita.
- Probar sin internet externo, manteniendo la red local. Perder internet, perder Wi-Fi y cerrar la pestaña son situaciones distintas.
- No prometer persistencia tras recarga, cierre o reinicio hasta que exista y esté probada.
- Capturas de los estados relevantes con datos ficticios y consola sin errores inesperados.
- Definir y medir el presupuesto de recursos sobre el dispositivo objetivo antes de integrar al firmware; no asumir que un build de escritorio cabe o sirve correctamente en ESP32.

## Reparto de trabajo

Claude escribe el brief y revisa capturas y flujo. Codex implementa los tokens, componentes y pruebas de la tarea. `frontend-design` ayuda con la dirección visual; `web-design-guidelines` aporta una auditoría, y Playwright permite observar el comportamiento. Ninguna herramienta reemplaza la validación con personas o el hardware.
