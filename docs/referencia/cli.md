# Línea de comandos

```
npc_crowd_routines <orden> [opciones]
```

Ejecútalo desde la raíz del repositorio (por defecto usa `./data`, `./web` y `./runs`; cámbialo con `--data`, `--web`, `--runs`).

| Orden | Uso |
|---|---|
| `serve` | Simulación con interfaz web. `--port 8080`, `--host 127.0.0.1` (usa `0.0.0.0` para la red local), `--paused`, `--speed 60`, `--scenario archivo` o las opciones de configuración |
| `run` | Sin interfaz. `--hours 24`, `--out runs/nombre`; escribe `telemetry.csv`, `summary.json` y `run.json` |
| `replay <archivo>` | Repite una partida guardada y verifica el *checksum* (código de salida 0 si coincide, 2 si no) |
| `benchmark` | `--agents 2500,10000,25000`, `--ticks 1800`, `--warmup 600`, `--size large`, `--out informe.json`, `--markdown informe.md` |
| `generate-map` | `--template`, `--size`, `--seed`, `--name`, `--out mapa.json` |
| `validate-map <archivo>` | Valida un mapa personalizado y muestra un resumen |
| `export-docs` | `--out docs/referencia/generado`: genera las tablas de referencia de la wiki desde `data/` |

## Opciones de configuración

| Opción | Por defecto | Descripción |
|---|---|---|
| `--seed` | 42 | semilla |
| `--population` | según tamaño | número de residentes (máx. 60.000) |
| `--template` | `grid` | `grid`, `old_town`, `river`, `suburban`, `financial`, `coastal` o una plantilla propia |
| `--size` | `small` | `small`, `medium`, `large` |
| `--map-seed` | = seed | semilla solo del mapa |
| `--map-file` | — | mapa personalizado (sustituye a plantilla y tamaño) |
| `--start-hour` | 7 | hora de inicio |
| `--weekday` | 0 | día de inicio (0 = lunes) |
| `--chaos` | 1 | multiplicador de eventos aleatorios |
| `--no-random-events` | — | desactiva los eventos aleatorios |
| `--step` | 1 | segundos simulados por tick |

## Herramientas adicionales

| Ejecutable | Uso |
|---|---|
| `storage_benchmark` | micro-benchmark original de disposición de memoria OOP frente a SoA (ver [ADR-0001](../decisiones/ADR-0001-storage-backends.md)) |
| `text_lint` | detecta corrupción de texto; lo usa el *hook* de pre-commit y la CI |
| `engine_tests` | pruebas del motor; acepta un filtro: `engine_tests world` |
