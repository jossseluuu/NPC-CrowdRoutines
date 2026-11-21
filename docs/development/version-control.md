# Control de versiones

El repositorio usa **git** con un flujo basado en ramas cortas por módulo, commits convencionales con
ámbito de módulo, versionado semántico y comprobaciones automáticas de texto.

## Por qué tan estricto

En noviembre de 2026 el proyecto sufrió corrupción masiva sin posibilidad de recuperación: archivos
reescritos a través de cadenas de PowerShell (`\"` dentro del C++/HTML, escapes `` `t `` y `` `b ``
convertidos en caracteres de control), acentos sustituidos por `U+FFFD`, un HTML guardado en Latin‑1 y
`src/app/main.cpp` sobrescrito con otro archivo. Con git cualquiera de esos daños se deshace con
`git restore`. Las reglas de este documento existen para que no vuelva a ocurrir.

## Módulos y ámbitos

Cada commit lleva el ámbito del módulo que toca. Los ámbitos coinciden con las carpetas:

| Ámbito | Carpeta | Contenido |
|---|---|---|
| `core` | `src/core/` | JSON, RNG determinista, reloj, geometría |
| `data` | `src/data/`, `data/` | Catálogos (actividades, arquetipos, edificios, eventos, i18n) |
| `world` | `src/world/` | Modelo de mapa, formato de mapas, navegación |
| `mapgen` | `src/world/generators/` | Generadores por plantilla |
| `population` | `src/population/` | Residentes, hogares, horarios, relaciones |
| `behavior` | `src/behavior/` | Necesidades, IA por utilidad, subestados, grupos, movimiento |
| `events` | `src/events/` | Catálogo de eventos, generación aleatoria, percepción |
| `sim` | `src/sim/` | Coordinador, escenarios, repetición, telemetría |
| `server` | `src/server/` | Servidor HTTP y API |
| `app` | `src/app/` | Línea de comandos |
| `web` | `web/` | Cliente de visualización |
| `bench` | `bench/` | Benchmarks y generación de informes |
| `docs` | `docs/` | Documentación / wiki |
| `ci` | `.github/` | GitHub Actions |
| `build` | `CMakeLists.txt`, `CMakePresets.json`, `tools/` | Compilación y herramientas |

## Commits

Formato [Conventional Commits](https://www.conventionalcommits.org/es/):

```
<tipo>(<ámbito>): <resumen en imperativo, minúsculas>

<cuerpo opcional: qué y por qué>
```

Tipos: `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`.
Ejemplos:

```
feat(mapgen): add river template with bridges
fix(behavior): keep group followers on the leader's route
docs(data): document the custom map format
```

Un commit = un cambio coherente que compila y pasa las pruebas.

## Ramas

- `main`: siempre compila y pasa `ctest`. No se trabaja directamente en ella.
- Ramas de trabajo: `<tipo>/<ámbito>-<descripción>`, por ejemplo `feat/mapgen-coastal`,
  `fix/server-frame-race`. Las fases grandes usan `feat/phaseN-<tema>`.
- Se integran en `main` con `git merge --no-ff` (o *pull request* en GitHub) cuando la CI está en verde,
  para que el historial conserve qué commits forman cada funcionalidad.

## Versiones

[Versionado semántico](https://semver.org/lang/es/): `MAJOR.MINOR.PATCH`.

- `MAJOR`: cambios incompatibles en formatos de datos (mapas, escenarios, repeticiones) o en la API HTTP.
- `MINOR`: funcionalidades nuevas compatibles.
- `PATCH`: correcciones.

La versión vive en `project(... VERSION x.y.z)` de `CMakeLists.txt`; cada versión publicada se etiqueta
`vX.Y.Z` y se resume en [`CHANGELOG.md`](../../CHANGELOG.md) (formato *Keep a Changelog*).
Los formatos de archivo tienen además su propio número `version` interno (ver
[formatos de datos](../data/index.md)), de modo que un escenario guardado sabe con qué formato se creó.

## Comprobaciones automáticas

`tools/text_lint` (C++) rechaza los síntomas de corrupción: UTF‑8 inválido, BOM, `U+FFFD`, caracteres de
control, finales de línea CRLF y comillas escapadas `\"` en marcado o fuera de literales C++.

Se ejecuta:

1. En local, como *hook* de pre-commit. Activarlo una vez:
   ```sh
   git config core.hooksPath tools/hooks
   ```
2. En la CI de GitHub sobre todos los archivos versionados.

`.gitattributes` fuerza **UTF‑8 con LF** en el repositorio. Nunca escribas archivos del proyecto con
`Set-Content`/`Out-File` ni cadenas interpoladas de PowerShell; usa el editor o git.

## Recuperar un archivo dañado

```sh
git diff -- ruta/al/archivo          # ver qué cambió
git restore ruta/al/archivo          # volver a la última versión confirmada
git log --follow -- ruta/al/archivo  # historial del archivo
git restore --source=<commit> ruta   # recuperar una versión concreta
```
