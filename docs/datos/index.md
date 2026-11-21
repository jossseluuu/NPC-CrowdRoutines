# Formatos de datos

Todo el comportamiento configurable vive en `data/` como JSON (se admiten comentarios `//` y `/* */` y comas finales).
El motor valida cada archivo al arrancar y, si algo está mal, dice el archivo, la línea y el motivo
(por ejemplo: `activities.json 'social' substate 'visiting_friend': unknown venue 'home_frend'`).

| Archivo | Formato (`"format"`) | Contenido |
|---|---|---|
| `activities.json` | `npc-activities` v1 | necesidades, deriva base, actividades y subestados |
| `buildings.json` | `npc-buildings` v1 | tipos de edificio, tipos de barrio y servicios mínimos |
| `population.json` | `npc-population` v1 | perfiles de rutina, arquetipos, tamaños de hogar |
| `events.json` | `npc-events` v1 | tipos de evento |
| `maps/templates.json` | `npc-map-templates` v1 | plantillas y tamaños del generador |
| `maps/names.json` | — | nombres de calles y barrios |
| `names.json` | — | nombres y apellidos de los residentes |
| `maps/custom/*.json` | `npc-city-map` v1 | [mapas personalizados](mapas-personalizados.md) |
| `scenarios/*.json` | `npc-scenario` v1 | [escenarios](escenarios.md) |
| `i18n/<idioma>.json` | — | traducciones (cualquier idioma nuevo aparece en el selector) |

Los campos `version` permiten evolucionar los formatos: un archivo de una versión más nueva que la soportada se rechaza
con un mensaje claro.

## Actividades (`activities.json`)

```jsonc
{
  "id": "eating_out", "color": "#ffca70", "icon": "☕", "choosable": true,
  "location": ["cafe", "restaurant", "bar"],          // categorías de edificio (o "home", "work", "school", "home_friend"...)
  "min_minutes": 15, "max_minutes": 120,
  "needs": { "hunger": 1.25, "social": 0.5 },          // pesos de utilidad
  "base": -0.7,
  "hours": [[7, 10, 0.3], [13, 15.5, 0.55]],           // [inicio, fin, bonificación]; puede cruzar medianoche
  "weekend_bonus": 0.4,
  "traits": { "thrift": -0.5, "sociability": 0.35 },   // multiplicadores: 1 + k·(rasgo − 0.5)·2
  "effects": { "hunger": -1.6, "social": -0.25 },      // cambio de necesidades por hora
  "substates": [
    { "id": "breakfast_cafe", "venue": ["cafe"], "hours": [6, 11] },
    { "id": "drinks", "venue": ["bar"] },
    { "id": "tapas", "venue": ["bar", "restaurant"], "weight": 0.5 }
  ]
}
```

Otras claves: `scheduled` (solo dentro de un bloque de agenda), `outdoor` (le afecta el mal tiempo), `weekday_only`.
En los subestados: `weight`, `hours`, `needs`, `traits`, `venue` y `auto`.

## Edificios y barrios (`buildings.json`)

```jsonc
{ "id": "cafe", "category": "cafe", "color": "#8b6a4f", "size": [8, 12, 8, 12], "floors": [1, 1],
  "jobs": [2, 6], "visitors": 35,
  "hours": { "weekdays": [7, 20.5], "saturday": [8, 21], "sunday": [9, 20] } }
```

`households` (viviendas), `students` (plazas escolares), `open24h`, y horarios por día (`monday`…`sunday` sobrescriben
`weekdays`; `null` = cerrado; cierre menor que apertura = cruza la medianoche). Los barrios definen `density` y `weights`
(probabilidad de cada tipo de edificio).

## Población (`population.json`)

- **Perfil**: `weight`, `age`, `workplaces` (categorías; `"home"` = teletrabajo), `days`
  (`weekdays`, `six`, `rotating`, `random`, `none`), `start` [media, desviación] o `shifts`, `duration`, `commute`
  [a pie, bici, bus, coche], `student`, `care`.
- **Arquetipo**: `t` (12 rasgos en el orden indicado en el archivo), `profiles` (afinidad con perfiles),
  `prefs` (multiplicador por actividad), `commute` opcional.

## Eventos (`events.json`)

`base_rate`, `hours`, `weekend`, `location` (`road`, `building` + `categories`, `district` + `districts`, `anywhere`,
`citywide`), intervalos `radius`, `duration` (min), `severity`, `recovery`, y los bloques `effects`, `response` y
`perception` descritos en [Eventos](../conceptos/eventos.md) y comentados en el propio archivo.

## Traducciones

Cada id nuevo necesita su nombre en `i18n/es.json` y `i18n/en.json` (secciones `activity`, `substate`, `building`,
`district`, `profile`, `archetype`, `event`...). La prueba `data/translations cover every id in both languages` falla si
falta alguno. La sección `ui` contiene los textos de la interfaz.

Tras cambiar datos, regenera la referencia de la wiki con `npc_crowd_routines export-docs`.
