# Escenarios y partidas

## Escenario (`npc-scenario`)

```jsonc
{
  "format": "npc-scenario", "version": 1,
  "name": "Hora punta con accidente",
  "description": "Lunes 7:00 en una ciudad con río...",
  "config": {
    "seed": 11,                     // semilla de todo (mapa, población, comportamiento, eventos)
    "map_template": "river",        // o "map_file": "maps/custom/aldea.json" (relativo a data/)
    "map_size": "medium",
    "map_seed": 0,                  // 0 = la misma que seed
    "population": 8000,             // 0 = la sugerida por el tamaño
    "start_hour": 7.0,
    "start_weekday": 0,             // 0 = lunes
    "step_seconds": 1.0,            // duración de un tick
    "chaos": 0.5,                   // multiplicador de eventos aleatorios
    "random_events": true
  },
  "events": [
    { "at": "d0 07:50", "event": "traffic_accident", "severity": 0.9, "duration": 60 },
    { "at": "d0 08:15", "event": "heavy_rain", "severity": 0.8 },
    { "at": "d1 12:00", "event": "fire", "x": 800, "y": 560, "radius": 90 }
  ]
}
```

`at` admite `"dN HH:MM"` (día N) o `"HH:MM"` (día 0). Cada evento admite `x`, `y`, `radius`, `duration` (minutos) y
`severity` (0–1); lo que se omita se sortea según `events.json`.

## Partida guardada (`npc-run`)

Generada por *Guardar partida* o por `npc_crowd_routines run`:

```jsonc
{
  "format": "npc-run", "version": 1, "engine": "0.3.0",
  "scenario": "Hora punta con accidente",
  "config": { /* igual que en el escenario */ },
  "commands": [
    { "tick": 3000, "command": { "type": "inject_event", "event": "traffic_accident", "severity": 0.9 } },
    { "tick": 5120, "command": { "type": "set_chaos", "value": 3 } }
  ],
  "end_tick": 21600, "end_time": "d0 13:00",
  "checksum": "8c1f0e2d9a4b7c31"
}
```

`replay` reconstruye la ciudad con `config`, aplica cada orden en su tick y compara el `checksum` al llegar a `end_tick`.

## Órdenes

| `type` | Campos | Efecto |
|---|---|---|
| `inject_event` | `event`, opcionales `x`, `y`, `radius`, `duration`, `severity` | crea un evento |
| `end_event` | `id` | termina un evento activo |
| `set_chaos` | `value` (0–10) | cambia la frecuencia de eventos aleatorios |
| `set_random_events` | `value` (bool) | activa o desactiva los eventos aleatorios |

Pausar, cambiar la velocidad o avanzar no son órdenes de simulación: no cambian el resultado y no se registran.
