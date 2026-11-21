<!-- Generado por `npc_crowd_routines export-docs` a partir de data/. No editar a mano. -->

# Eventos (18)

Tasa base = eventos por día simulado con caos 1 (se multiplica por el nivel de caos, la hora y el fin de semana).

| Evento | id | Categoría | Tasa/día | Lugar | Radio (m) | Duración (min) | Efectos | Reacciones |
|---|---|---|---:|---|---|---|---|---|
| ⚠ Accidente de tráfico | `traffic_accident` | traffic | 1.6 | calle | 25–60 | 20–90 | corta calles, coches ×0.20, peatones ×0.90, estrés +0.08/h | observar, curiosear, ayudar |
| ⛏ Obras en la calzada | `road_works` | traffic | 0.5 | calle | 30–80 | 240–900 | corta calles, coches ×0.35, peatones ×0.85, estrés +0.02/h | observar |
| ⛟ Avería de autobús | `bus_breakdown` | traffic | 0.6 | calle | 20–40 | 20–60 | coches ×0.50, estrés +0.05/h | observar |
| ☂ Lluvia intensa | `heavy_rain` | weather | 0.35 | toda la ciudad | — | 40–240 | tiempo: rain, coches ×0.80, peatones ×0.85, estrés +0.02/h | refugiarse |
| ⛈ Tormenta | `storm` | weather | 0.08 | toda la ciudad | — | 30–150 | tiempo: storm, coches ×0.65, peatones ×0.75, estrés +0.08/h | refugiarse |
| ☀ Ola de calor | `heatwave` | weather | 0.05 | toda la ciudad | — | 240–600 | tiempo: heat, peatones ×0.90, estrés +0.04/h | refugiarse |
| 🔥 Incendio | `fire` | hazard | 0.12 | edificio | 40–110 | 30–150 | corta calles, cierra edificios, evacuación, coches ×0.30, estrés +0.35/h | observar, huir, evacuar, ayudar |
| ☣ Fuga de gas | `gas_leak` | hazard | 0.05 | edificio | 60–140 | 40–180 | corta calles, cierra edificios, evacuación, estrés +0.25/h | huir, evacuar |
| ⚡ Apagón | `power_outage` | hazard | 0.1 | barrio | 180–420 | 30–240 | cierra edificios, coches ×0.70, estrés +0.06/h | observar |
| ⚠ Alarma de evacuación | `evacuation_alarm` | hazard | 0.04 | cualquiera | 70–160 | 20–90 | cierra edificios, evacuación, estrés +0.40/h | huir, evacuar |
| ✚ Emergencia médica | `medical_emergency` | hazard | 1.4 | cualquiera | 12–25 | 10–35 | estrés +0.12/h | observar, ayudar |
| ⚡ Pelea callejera | `street_dispute` | social | 1.2 | cualquiera | 18–40 | 5–30 | conflicto, estrés +0.20/h | observar, curiosear, huir, ayudar |
| ✖ Atraco | `robbery` | social | 0.25 | edificio | 25–50 | 5–20 | cierra edificios, conflicto, estrés +0.30/h | observar, huir, refugiarse |
| ♫ Concierto | `concert` | public | 0.15 | edificio | 60–120 | 90–240 | multitud 80–600, coches ×0.70, peatones ×0.80 | observar |
| ⛺ Mercadillo | `street_market` | public | 0.2 | edificio | 40–80 | 180–360 | multitud 60–300, peatones ×0.85 | observar |
| ✊ Manifestación | `protest` | public | 0.06 | edificio | 60–150 | 60–240 | corta calles, multitud 100–800, coches ×0.30, peatones ×0.70, estrés +0.08/h | observar, huir |
| ⚽ Partido | `sports_match` | public | 0.1 | edificio | 60–130 | 100–150 | multitud 100–900, coches ×0.50, peatones ×0.85 | observar |
| ✺ Fiestas del barrio | `festival` | public | 0.03 | barrio | 150–320 | 300–720 | multitud 300–1500, coches ×0.40, peatones ×0.75 | observar |
