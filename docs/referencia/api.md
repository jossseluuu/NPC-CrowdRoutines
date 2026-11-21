# API HTTP

`npc_crowd_routines serve` expone esta API (y sirve `web/`). Respuestas JSON salvo donde se indica. Pensada para uso
local; escucha en `127.0.0.1` por defecto.

## Lectura

| Método y ruta | Respuesta |
|---|---|
| `GET /api/info` | versión, configuración, resumen del mapa, plantillas, tamaños, idiomas, escenarios, mapas personalizados, partidas y estado del ejecutor |
| `GET /api/catalog` | jerarquía de actividades/subestados con colores, edificios, barrios, eventos, perfiles, arquetipos, necesidades, rasgos |
| `GET /api/i18n?lang=es` | archivo de traducciones |
| `GET /api/map` | mapa actual en formato `npc-city-map` |
| `GET /api/residents` | atributos fijos por residente: `profile`, `age`, `sex`, `mode`, `district` |
| `GET /api/frame` | **binario**: posiciones y estado de todos los residentes, autobuses y carga de tráfico (ver abajo) |
| `GET /api/state` | reloj, recuento por actividad y subestado, estadísticas, tiempo, caos, eventos activos e historial, estado del ejecutor (velocidad, ms por tick) |
| `GET /api/agent?id=N` | detalle de un residente: identidad, rasgos, necesidades, horario, agenda, historial, acciones, relaciones, trayecto y desglose de la decisión |
| `GET /api/building?id=N` | detalle de un edificio y quién está dentro |
| `GET /api/timeseries?since=T&max=N` | series temporales desde el segundo simulado T, diezmadas a N puntos |
| `GET /api/telemetry.csv` | telemetría completa en CSV |
| `GET /api/map/export` | mapa actual como descarga |

## Consultas

| Método y ruta | Cuerpo | Respuesta |
|---|---|---|
| `POST /api/query` | filtro | **binario**: un bit por residente; cabecera `X-Matches` con el total |
| `POST /api/export` | filtro | CSV de los residentes que coinciden |

Filtro: `{"logic": "all"|"any", "rules": [regla...]}`. Regla: `{"field": ..., "value": ...}`, con `min`/`max` en campos
numéricos, `name` en `need`/`trait` y `negate: true` opcional. Se pueden anidar grupos `{"any": [...]}` / `{"all": [...]}`.
Campos: `activity`, `substate`, `profile`, `archetype`, `district`, `sex`, `age`, `need`, `trait`, `mode`, `place`,
`work`, `outdoors`, `moving`, `group`, `emergency`, `relation`, `relations`, `knows_event`, `distance_km`, `id`.

## Control y órdenes

| Método y ruta | Cuerpo |
|---|---|
| `POST /api/control` | `{"action": "pause"|"resume"|"toggle"}`, `{"action": "step", "ticks": 60}`, `{"action": "speed", "value": 300}` |
| `POST /api/command` | orden de simulación (ver [Escenarios y partidas](../datos/escenarios.md#ordenes)); se aplica en el siguiente tick |
| `POST /api/reset` | configuración (`map_template`, `map_size`, `seed`, `population`, `map_file`, `start_hour`, `start_weekday`, `chaos`, `random_events`) |
| `POST /api/scenario/load` | `{"file": "rush_hour.json"}` o `{"scenario": {...}}` |
| `POST /api/scenario/save` | `{"name": "...", "description": "..."}` |
| `POST /api/run/save` | `{"name": "..."}` |
| `POST /api/run/load` | `{"file": "...", "replay": true}` o `{"run": {...}}`; con `replay` devuelve `checksum_ok` |
| `POST /api/map/upload` | mapa `npc-city-map` (o `{"map": {...}}`); se valida y se guarda en `data/maps/custom/` |

Errores: código 4xx/5xx con `{"error": "mensaje"}`.

## Formato del frame binario

Little endian:

```
"NPCF"  u32 versión  f64 segundos  u32 n
n × { f32 x, f32 y, u8 actividad, u8 subestado, u8 flags, u8 estrés(0-255) }
u32 nb  nb × { f32 x, f32 y, u16 línea, u16 pasajeros }
u32 ne  ne × u8 carga de tráfico (0-250 = carga×100, 255 = calle cortada)
```

Flags: 1 en la calle, 2 en movimiento, 4 en grupo, 8 emergencia, 16 coche, 32 autobús, 64 bici.
