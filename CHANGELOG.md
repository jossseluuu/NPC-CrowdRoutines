# Changelog

Todos los cambios relevantes del proyecto. Formato basado en
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y [versionado semántico](https://semver.org/lang/es/).

## [Unreleased]

### Added
- Repositorio git con `.gitattributes` (UTF-8 + LF) y flujo de ramas por módulo.
- `tools/text_lint`: comprobación de corrupción de texto, con *hook* de pre-commit.
- Presets de CMake para WSL (Release / Debug con sanitizers) y Windows.
- Módulo `core`: JSON sin dependencias, RNG determinista portable con flujos independientes, reloj de simulación.

## [0.2.0] - 2026-11-21

### Fixed
- Reparación de la corrupción de archivos (entrada de la aplicación, visor web, acentos, CMake).
- Sistemas de edificios, tráfico y personalidad compilando y probados.
