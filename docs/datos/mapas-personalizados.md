# Mapas personalizados

Cualquiera puede crear su propia ciudad como archivo JSON con formato `npc-city-map`. Solo son obligatorios los
**nodos**, las **calles** y los **edificios**; el resto se calcula automáticamente.

## Ejemplo mínimo

`data/maps/custom/aldea.json` (incluido):

```jsonc
{
  "format": "npc-city-map", "version": 1, "name": "Aldea de ejemplo",
  "street_names": ["Calle Mayor", "Calle del Río", "Calle Nueva", "Avenida del Parque"],
  "nodes": [[20, 20], [120, 20], [220, 20], [320, 20], [20, 120], [120, 120] /* ... */],
  "edges": [[0, 1, "avenue", 3], [1, 2, "avenue", 3], [4, 5, "street", 0], [10, 11, "path"] /* ... */],
  "districts": [{ "name": "Centro", "kind": "old_town" }, { "name": "La Vega", "kind": "residential_low" }],
  "buildings": [
    { "type": "apartment", "center": [70, 70], "district": 0 },
    { "type": "school", "center": [95, 172], "size": [40, 30], "district": 1 },
    { "type": "park", "center": [290, 185], "size": [40, 40] }
  ]
}
```

## Campos

| Campo | Obligatorio | Descripción |
|---|---|---|
| `nodes` | sí | intersecciones `[x, y]` en metros (origen arriba a la izquierda, valores ≥ 0) |
| `edges` | sí | `[a, b, tipo?, calle?]`: índices de nodo, tipo `street` (por defecto), `avenue`, `path` (solo peatones), `bridge`, `promenade`; índice opcional en `street_names` |
| `buildings` | sí | `type` (id de `buildings.json`) y `polygon` `[[x,y],...]` o `center` + `size` opcional; opcionales: `district`, `floors`, `households`, `jobs`, `students`, `name` |
| `street_names` | no | nombres de calles |
| `districts` | no | `name`, `kind` (tipo de barrio) y `center` opcional; si no hay, se crea uno |
| `blocks` | no | manzanas para colorear el uso del suelo: `polygon`, `kind`, `district` |
| `water` | no | `kind` (`river`, `sea`, `lake`) y `polygon` (solo visual) |
| `lights` | no | índices de nodos con semáforo; si se omite, se colocan en cruces con avenidas |
| `bus_lines` | no | `name`, `color`, `path` (nodos) y `stops`; si se omite, se generan por las avenidas |
| `width`, `height`, `seed`, `suggested_population` | no | se deducen si faltan |

## Qué se calcula al cargar

1. Se eliminan tramos duplicados y partes de la red **desconectadas** (se conserva el componente mayor).
2. Se comprueba que haya calles para coches y viviendas para los residentes.
3. Viviendas, empleos y plazas escolares que falten se deducen del tipo de edificio, sus plantas y su tamaño.
4. Cada edificio se conecta a la intersección más cercana.
5. Semáforos y autobuses si no se dan.

Los errores son concretos: `edge #12 references a missing node`, `building #3: unknown type 'castle'`,
`edge #4: unknown road kind 'highway'`...

## Herramientas

```sh
# Validar y ver un resumen (avisa de servicios importantes que faltan)
npc_crowd_routines validate-map data/maps/custom/mi_ciudad.json

# Partir de una ciudad generada y editarla
npc_crowd_routines generate-map --template old_town --size small --seed 3 --out data/maps/custom/mi_ciudad.json

# Usarla
npc_crowd_routines serve --map-file data/maps/custom/mi_ciudad.json --population 3000
```

En la interfaz, pestaña **Escenarios**: *Subir mapa* valida y guarda el archivo en `data/maps/custom/`, y aparece en el
selector *Mapa personalizado* de *Nueva ciudad*. *Exportar mapa actual* descarga la ciudad en este mismo formato.
