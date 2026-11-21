# Primeros pasos

## Requisitos

- Windows 10/11 con **WSL 2** y Ubuntu (o cualquier Linux / macOS).
- En Ubuntu: compilador C++20, CMake ≥ 3.20 y Ninja:
  ```sh
  sudo apt install build-essential cmake ninja-build git
  ```
- Un navegador moderno (Chrome, Edge, Firefox).

!!! info "¿Por qué WSL y no un `.exe` de Windows?"
    Windows 11 trae **Smart App Control**, que bloquea cualquier ejecutable que no esté firmado con un
    certificado de confianza y que Microsoft no conozca. Cada vez que compilas se genera un `.exe` nuevo y sin
    firmar, así que puede quedar bloqueado (“Una directiva de Control de aplicaciones bloqueó este archivo”).
    Los programas de Linux que se ejecutan dentro de WSL no pasan por ese control, así que compilar y ejecutar en
    WSL evita el problema sin desactivar la protección. Detalles y alternativas en
    [ADR-0003](decisiones/ADR-0003-wsl-smart-app-control.md).

## Compilar y probar

Desde la raíz del repositorio, dentro de WSL:

```sh
cmake --preset wsl-release
cmake --build --preset wsl-release
ctest --preset wsl-release
```

O con el atajo `sh tools/wsl.sh test`.

## Abrir la simulación

```sh
./build-wsl/npc_crowd_routines serve
```

y abre **http://localhost:8080** en el navegador de Windows (WSL 2 reenvía `localhost` automáticamente).

Opciones útiles:

```sh
# Ciudad con río, tamaño mediano, 8.000 residentes, empezando a las 7:00
./build-wsl/npc_crowd_routines serve --template river --size medium --population 8000 --start-hour 7

# Cargar un escenario
./build-wsl/npc_crowd_routines serve --scenario data/scenarios/rush_hour.json

# Mapa personalizado
./build-wsl/npc_crowd_routines serve --map-file data/maps/custom/aldea.json --population 80
```

Todas las opciones: [Línea de comandos](referencia/cli.md).

## Sin interfaz (experimentos y CI)

```sh
# 24 horas simuladas; escribe telemetry.csv, summary.json y run.json (repetible)
./build-wsl/npc_crowd_routines run --scenario data/scenarios/evacuation_drill.json --hours 24 --out runs/simulacro

# Verificar que la partida se reproduce exactamente igual
./build-wsl/npc_crowd_routines replay runs/simulacro/run.json

# Informe de rendimiento
./build-wsl/npc_crowd_routines benchmark --agents 2500,10000,25000 --markdown reports/benchmark.md
```

## Activar las comprobaciones de git

Una vez por clon:

```sh
git config core.hooksPath tools/hooks
```

Así cada commit pasa por `text_lint` (ver [Control de versiones](desarrollo/control-de-versiones.md)).
