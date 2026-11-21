# ADR-0004: Comportamiento y mundo definidos en datos JSON validados

- **Estado:** Aceptada (2026-11-21)
- **Contexto:** El objetivo es una variabilidad muy alta (muchas actividades, subestados, arquetipos, eventos, edificios y
  plantillas de mapa) y que otras personas puedan personalizar la simulación sin compilar ni conocer el motor.
- **Decisión:** Actividades, subestados, pesos de utilidad, edificios, barrios, perfiles, arquetipos, eventos, plantillas de
  mapa, nombres y traducciones viven en `data/` como JSON con un campo `format` y `version`. El motor los carga con un
  parser JSON propio (con comentarios), los valida al arrancar con mensajes concretos, y el código solo depende de unos
  pocos identificadores obligatorios (p. ej. la actividad `travelling` o el subestado `walking_talking`).
  La referencia de la wiki se genera desde esos archivos (`export-docs`).
- **Consecuencias:** Ajustar el comportamiento es editar datos (el perfil horario de la ciudad se ajustó así). Las pruebas
  comprueban la coherencia del catálogo y que todas las traducciones existen. Las reglas que no se pueden expresar en datos
  (contexto de grupos, transporte, emergencias) siguen en C++ y se documentan como subestados automáticos.
