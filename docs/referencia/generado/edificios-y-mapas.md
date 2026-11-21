<!-- Generado por `npc_crowd_routines export-docs` a partir de data/. No editar a mano. -->

# Edificios y barrios

## Tipos de edificio (36)

| Edificio | id | Categoría | Plantas | Viviendas | Empleos | Visitantes | Horario |
|---|---|---|---|---|---|---:|---|
| Casa | `house` | home | 1–2 | 1–1 | — | 0 | — |
| Adosado | `townhouse` | home | 2–3 | 2–4 | — | 0 | — |
| Bloque de pisos | `apartment` | home | 4–8 | 10–32 | — | 0 | — |
| Torre residencial | `tower` | home | 12–30 | 40–110 | — | 0 | — |
| Oficinas | `office_small` | office | 2–5 | — | 15–60 | 10 | L 08:00–19:00; S cerrado |
| Torre de oficinas | `office_tower` | office | 12–40 | — | 200–900 | 60 | L 07:30–20:00; S cerrado |
| Ayuntamiento | `city_hall` | city_hall | 3–5 | — | 60–140 | 120 | L 08:30–15:00; S cerrado |
| Banco | `bank` | bank | 1–3 | — | 6–20 | 30 | L 08:30–14:00; S cerrado |
| Fábrica | `factory` | factory | 1–2 | — | 60–300 | 5 | L 06:00–22:00; S 06:00–14:00; D cerrado |
| Almacén | `warehouse` | warehouse | 1–1 | — | 20–80 | 5 | 24 h |
| Tienda | `shop_small` | shop_small | 1–1 | — | 1–4 | 15 | L 09:30–20:30; S 10:00–14:00; D cerrado |
| Supermercado | `supermarket` | supermarket | 1–1 | — | 15–45 | 160 | L 09:00–21:30; D cerrado |
| Mercado | `market` | market | 1–1 | — | 20–50 | 250 | L 08:00–14:30; D cerrado |
| Centro comercial | `mall` | mall | 2–4 | — | 150–500 | 2500 | L 10:00–22:00; D 11:00–21:00 |
| Cafetería | `cafe` | cafe | 1–1 | — | 2–6 | 35 | L 07:00–20:30; S 08:00–21:00; D 09:00–20:00 |
| Restaurante | `restaurant` | restaurant | 1–1 | — | 5–14 | 80 | L 12:30–23:30; S 12:30–00:30; D 12:30–23:00 |
| Bar | `bar` | bar | 1–1 | — | 2–6 | 60 | L 12:00–01:30; S 12:00–03:00; D 12:00–00:30 |
| Discoteca | `nightclub` | nightclub | 1–2 | — | 10–25 | 600 | L cerrado; V 23:30–06:00; S 23:30–06:30; D cerrado |
| Colegio | `school` | school | 2–3 | — | 30–70 | 20 | L 08:00–17:00; S cerrado |
| Universidad | `university` | university | 3–6 | — | 150–400 | 100 | L 08:00–21:30; S 09:00–14:00; D cerrado |
| Biblioteca | `library` | library | 2–3 | — | 6–15 | 150 | L 09:00–21:00; S 10:00–14:00; D cerrado |
| Centro de salud | `clinic` | clinic | 2–3 | — | 15–40 | 80 | L 08:00–21:00; S 09:00–14:00; D cerrado |
| Hospital | `hospital` | hospital | 5–10 | — | 400–1200 | 900 | 24 h |
| Farmacia | `pharmacy` | pharmacy | 1–1 | — | 2–5 | 15 | L 09:00–21:30; S 09:30–14:00; D cerrado |
| Gimnasio | `gym` | gym | 1–2 | — | 4–12 | 150 | L 06:30–23:00; S 08:00–21:00; D 08:00–15:00 |
| Polideportivo | `sports_center` | sports | 1–2 | — | 8–25 | 400 | L 08:00–23:00; S 09:00–22:00; D 09:00–21:00 |
| Parque | `park` | park | 0–0 | — | 2–8 | 1500 | 24 h |
| Plaza | `plaza` | plaza | 0–0 | — | 0–2 | 800 | 24 h |
| Cine | `cinema` | cinema | 1–2 | — | 8–20 | 500 | L 16:00–00:30; S 12:00–01:30; D 12:00–23:30 |
| Museo | `museum` | museum | 2–3 | — | 15–40 | 600 | L 10:00–20:00; D 10:00–15:00 |
| Teatro | `theater` | theater | 2–3 | — | 15–40 | 700 | L 18:00–23:30; S 17:00–00:30; D 17:00–22:00 |
| Comisaría | `police` | police | 2–4 | — | 40–120 | 30 | 24 h |
| Parque de bomberos | `fire_station` | fire_station | 1–2 | — | 20–50 | 5 | 24 h |
| Estación de autobuses | `bus_terminal` | transit | 1–1 | — | 20–60 | 400 | L 05:30–01:00; S 06:00–01:00; D 07:00–00:30 |
| Hotel | `hotel` | hotel | 5–14 | — | 30–90 | 300 | 24 h |
| Templo | `worship` | worship | 1–2 | — | 1–4 | 300 | L 08:00–20:00; D 08:00–14:00 |

## Tipos de barrio (11)

| Barrio | id | Densidad | Edificios más probables |
|---|---|---:|---|
| Residencial unifamiliar | `residential_low` | 0.45 | Casa, Adosado, Parque, Tienda, Colegio |
| Residencial | `residential_mid` | 0.6 | Bloque de pisos, Adosado, Tienda, Cafetería, Bar |
| Residencial en altura | `residential_high` | 0.7 | Torre residencial, Bloque de pisos, Tienda, Cafetería, Supermercado |
| Casco antiguo | `old_town` | 0.8 | Adosado, Bloque de pisos, Tienda, Bar, Restaurante |
| Comercial | `commercial` | 0.75 | Tienda, Cafetería, Restaurante, Bloque de pisos, Bar |
| Negocios | `office` | 0.75 | Oficinas, Torre de oficinas, Cafetería, Restaurante, Banco |
| Industrial | `industrial` | 0.55 | Fábrica, Almacén, Oficinas, Cafetería, Estación de autobuses |
| Campus | `campus` | 0.5 | Bloque de pisos, Universidad, Cafetería, Biblioteca, Parque |
| Centro cívico | `civic` | 0.6 | Oficinas, Cafetería, Plaza, Ayuntamiento, Banco |
| Zona verde | `green` | 0.25 | Parque, Polideportivo, Cafetería |
| Frente marítimo | `waterfront` | 0.55 | Bloque de pisos, Restaurante, Bar, Hotel, Cafetería |

## Plantillas de mapa (6)

| Plantilla | id | Separación (m) | Deformación | Avenidas | Agua |
|---|---|---|---|---|---|
| Cuadrícula | `grid` | 80×70 | jitter 0, warp 0 | cada 4 | none |
| Casco antiguo | `old_town` | 62×58 | jitter 0.28, warp 22 | no + diagonal | none |
| Ciudad con río | `river` | 78×68 | jitter 0.08, warp 10 | cada 4 | river |
| Afueras | `suburban` | 120×105 | jitter 0.12, warp 30 | cada 5 | none |
| Centro financiero | `financial` | 70×60 | jitter 0, warp 0 | cada 3 + diagonal | none |
| Ciudad costera | `coastal` | 76×66 | jitter 0.1, warp 14 | cada 4 | coast |

| Tamaño | id | Dimensiones | Población sugerida |
|---|---|---|---:|
| Pequeño | `small` | 900 × 650 m | 2500 |
| Mediano | `medium` | 1600 × 1150 m | 8000 |
| Grande | `large` | 2600 × 1800 m | 20000 |
