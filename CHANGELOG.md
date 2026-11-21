# Changelog

Todos los cambios relevantes del proyecto. Formato basado en
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y [versionado semántico](https://semver.org/lang/es/).

## [Unreleased]

## [0.3.1] - 2026-11-21

### Fixed

- **Autobuses**: un autobús que llegaba a la última parada de su línea se quedaba allí para siempre, porque al dar la
  vuelta volvía a detectar la misma parada. Sus pasajeros no llegaban nunca a su destino y el número de viajeros solo
  crecía con el tiempo.
- **Autobuses**: al cambiar de plan en pleno trayecto (por ejemplo, para huir de un evento) o al unirse a un grupo, el
  residente no liberaba su plaza en el autobús.
- **Grupos**: nunca llegaban a formarse, porque el edificio de salida se leía después de que el residente hubiera salido.
  Ahora familias y amigos salen juntos como estaba previsto.
- **Interfaz**: la pestaña *Decisión* mostraba números sin redondear (`0.170000002`) y un doble signo menos; la distancia
  recorrida se redondea a una décima.
- **Interfaz**: la velocidad media de coches o peatones muestra «—» cuando no hay ninguno en marcha, en lugar de 0 km/h.
- **Interfaz**: el eje de las gráficas muestra «0» en lugar de «0.00», y los botones junto a un campo de texto ya no
  parten su etiqueta en varias líneas.

### Added

- Pruebas: contadores de plazas de autobús coherentes con los pasajeros reales, ningún viaje de más de 3 horas en una
  ciudad mediana con eventos, y formación de grupos a lo largo del día.
- Documentación: README reescrito con capturas, galería de plantillas, diagrama de arquitectura, rendimiento,
  consideraciones y hoja de ruta; páginas nuevas de *Consideraciones y limitaciones*, *Preguntas frecuentes* y
  *Glosario*; capturas y diagramas Mermaid en la wiki.

### Changed

- Informe de rendimiento regenerado: el motor hace ahora más trabajo real (grupos y viajes en autobús), así que el coste
  por tick con 25.000 residentes pasa de unos 1,3 ms a unos 1,7 ms de mediana.
- La documentación de reproducibilidad precisa que la garantía bit a bit se aplica al mismo binario, no entre
  compiladores o sistemas distintos.

## [0.3.0] - 2026-11-21

Reescritura completa: una única simulación en C++ con cliente web fino.

### Added

- **Control de versiones**: repositorio git con `.gitattributes` (UTF-8 + LF), ramas por fase/módulo,
  commits convencionales con ámbito, `tools/text_lint` (C++) en pre-commit y CI.
- **Motor C++** (`npc_engine`): JSON y RNG deterministas propios, catálogo de datos validado, mapas, población,
  comportamiento, movimiento, eventos, coordinador de paso fijo, telemetría.
- **Estados**: 14 actividades principales y 85 subestados en datos; subestados automáticos por contexto
  (grupos, transporte, prisa, semáforos, conversaciones, emergencias).
- **Población**: 16 perfiles de rutina, 48 arquetipos combinables (dos por residente) sobre 12 rasgos, horarios,
  empleo y estudios, red social (familia, pareja, vecinos, compañeros, amistades, rivales), agendas diarias.
- **IA por utilidad** con 8 necesidades, elección de lugar (horario, aforo, cierres, lugares favoritos), grupos que salen
  juntos, acompañamiento al colegio, conversaciones y discusiones en la calle.
- **Mapas**: 6 plantillas procedimentales (cuadrícula, casco antiguo, río, afueras, centro financiero, costa) en 3 tamaños,
  barrios con nombre, 36 tipos de edificio con horarios, autobuses, semáforos; formato `npc-city-map` para mapas
  personalizados con derivación automática y validación.
- **Movimiento**: aceras, bicis, coches con congestión y semáforos, líneas de autobús con paradas.
- **Eventos**: 18 tipos con generación aleatoria (caos), ciclo de vida, cortes de calles, cierres, tiempo, multitudes,
  percepción, avisos públicos, boca a boca y reacciones según la personalidad; peleas emergentes.
- **Escenarios y partidas**: formatos `npc-scenario` y `npc-run`, repetición verificada por *checksum*; 4 escenarios de ejemplo.
- **Servidor HTTP propio** y API; línea de comandos `serve`, `run`, `replay`, `benchmark`, `generate-map`,
  `validate-map`, `export-docs`.
- **Interfaz web** (es/en): zoom, minimapa, capas, 7 modos de color, árbol de estados desplegable, filtros combinados
  evaluados en C++, inspector (resumen, agenda, historial, relaciones, acciones, decisión), gráficas, gestión de
  escenarios, partidas y mapas, enlaces compartibles.
- **Wiki** en `docs/` (MkDocs) con referencia generada desde los datos; GitHub Actions para CI (Linux GCC y sanitizers,
  Windows MSVC) y para publicar la wiki e informes de rendimiento en GitHub Pages.

### Changed

- La compilación principal es en WSL por Smart App Control (ADR-0003).
- El benchmark OOP/SoA original pasa a `bench/legacy_storage` (`storage_benchmark`).

### Removed

- Modelo de simulación en JavaScript del visor antiguo y módulos de andamiaje sin uso (sustituidos por el motor).

## [0.2.0] - 2026-11-21

### Fixed

- Reparación de la corrupción de archivos (entrada de la aplicación, visor web, acentos, CMake).
- Sistemas de edificios, tráfico y personalidad compilando y probados.
