# Changelog

Todos los cambios relevantes del proyecto. Formato basado en
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y [versionado semántico](https://semver.org/lang/es/).

## [Unreleased]

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
