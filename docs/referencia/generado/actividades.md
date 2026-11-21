<!-- Generado por `npc_crowd_routines export-docs` a partir de data/. No editar a mano. -->

# Actividades y subestados

Cada residente está siempre en **una actividad principal** y **uno de sus subestados**. 14 actividades y 85 subestados en total.

Columnas: *elegible* = la IA por utilidad puede escogerla; *agenda* = solo dentro de su bloque de agenda; subestados *auto* los asigna el motor según el contexto (grupo, transporte, eventos...).

| Actividad | id | Elegible | Lugar | Duración (min) | Horas preferidas |
|---|---|---|---|---|---|
| ☾ Durmiendo | `sleeping` | sí | casa | 60–660 | 22:00–07:00 (1.4), 13:30–15:30 (0.25) |
| ⌂ En casa | `home` | sí | casa | 20–240 | 06:00–09:00 (0.2), 19:00–23:30 (0.35) |
| ➜ Desplazándose | `travelling` | no | en tránsito | — | — |
| ▣ Trabajando | `working` | agenda | trabajo | 45–600 | — |
| ✎ Estudiando | `studying` | agenda | centro de estudios | 45–480 | — |
| ◍ Compras | `shopping` | sí | supermarket, market, mall, shop_small, pharmacy | 10–90 | 10:00–13:30 (0.2), 17:00–20:30 (0.25) |
| ☕ Comiendo fuera | `eating_out` | sí | cafe, restaurant, bar | 15–120 | 07:00–10:00 (0.3), 13:00–15:30 (0.55), 20:30–23:00 (0.45) |
| ✦ Ocio | `leisure` | sí | park, cinema, museum, theater, nightclub, library, plaza | 30–210 | 17:00–23:00 (0.25), 10:00–13:00 (0.15) |
| ⚑ Deporte | `exercise` | sí | gym, sports, park | 30–120 | 06:30–09:00 (0.35), 18:00–21:30 (0.45) |
| ☺ Vida social | `social` | sí | park, plaza, bar, cafe, casa de una amistad | 20–180 | 18:00–23:00 (0.4) |
| ♥ Cuidados | `care` | agenda | school, clinic, casa de un familiar | 20–240 | — |
| ✚ Salud | `health` | sí | clinic, hospital, casa | 20–300 | 08:30–19:00 (0.15) |
| ✉ Recados | `errands` | sí | bank, city_hall, worship, hotel | 10–75 | 09:00–14:00 (0.3) |
| ⚠ Emergencia | `emergency` | no | cualquiera | — | — |

## ☾ Durmiendo (`sleeping`)

| Subestado | id | Cuándo |
|---|---|---|
| Sueño nocturno | `night_sleep` | genérico · 21:00–09:00 |
| Siesta | `nap` | genérico · 09:00–21:00 |
| Sueño inquieto | `restless_sleep` | genérico |

## ⌂ En casa (`home`)

| Subestado | id | Cuándo |
|---|---|---|
| Descansando | `relaxing` | genérico |
| Cocinando la comida | `cooking` | genérico · 12:00–14:30 |
| Preparando la cena | `cooking_dinner` | genérico · 19:00–22:00 |
| Comiendo en casa | `eating_home` | genérico |
| Viendo la tele | `watching_tv` | genérico · 18:00–01:00 |
| Tareas domésticas | `chores` | genérico |
| Aseo personal | `hygiene` | genérico |
| Leyendo | `reading` | genérico |
| Jugando a videojuegos | `gaming` | genérico |
| Llamada telefónica | `phone_call` | genérico |

## ➜ Desplazándose (`travelling`)

| Subestado | id | Cuándo |
|---|---|---|
| Caminando | `walking` | auto (contexto) |
| Caminando y hablando | `walking_talking` | auto (contexto) |
| Paseando | `strolling` | auto (contexto) |
| Caminando con prisa | `hurrying` | auto (contexto) |
| En bicicleta | `cycling` | auto (contexto) |
| Esperando el autobús | `waiting_bus` | auto (contexto) |
| En autobús | `on_bus` | auto (contexto) |
| Conduciendo | `driving` | auto (contexto) |
| Atascado en el tráfico | `stuck_in_traffic` | auto (contexto) |
| Esperando para cruzar | `waiting_crossing` | auto (contexto) |

## ▣ Trabajando (`working`)

| Subestado | id | Cuándo |
|---|---|---|
| En su puesto | `at_desk` | en office, city_hall, bank |
| En una reunión | `meeting` | en office, city_hall |
| En su turno | `on_shift` | en factory, warehouse, hospital, police, fire_station, transit |
| Atendiendo al público | `serving_customers` | en shop_small, supermarket, market, mall, cafe, restaurant, bar, nightclub, pharmacy, gym, cinema, museum, theater, hotel, clinic, library |
| Dando clase | `teaching` | en school, university |
| Teletrabajando | `remote_work` | en home |
| Pausa para el café | `coffee_break` | genérico · 10:00–11:30 |
| Pausa para comer | `lunch_break` | genérico · 13:00–15:00 |

## ✎ Estudiando (`studying`)

| Subestado | id | Cuándo |
|---|---|---|
| En clase | `in_class` | en school, university |
| En el recreo | `recess` | en school · 10:30–12:00 |
| Estudiando en la biblioteca | `library_study` | en library, university |
| Haciendo deberes | `homework` | en home |
| En un examen | `exam` | en school, university |

## ◍ Compras (`shopping`)

| Subestado | id | Cuándo |
|---|---|---|
| Haciendo la compra | `groceries` | en supermarket |
| En el mercado | `market_stalls` | en market |
| En el centro comercial | `mall_browsing` | en mall |
| En una tienda | `small_shop` | en shop_small |
| Comprando en la farmacia | `pharmacy_purchase` | en pharmacy |
| Haciendo cola | `queueing` | auto (contexto) |
| Mirando escaparates | `window_shopping` | genérico |

## ☕ Comiendo fuera (`eating_out`)

| Subestado | id | Cuándo |
|---|---|---|
| Desayunando en una cafetería | `breakfast_cafe` | en cafe · 06:00–11:00 |
| Tomando un café | `coffee` | en cafe · 11:00–20:00 |
| Comiendo en un restaurante | `restaurant_meal` | en restaurant |
| Tomando algo | `drinks` | en bar |
| De tapas | `tapas` | en bar, restaurant · 12:00–16:00 |

## ✦ Ocio (`leisure`)

| Subestado | id | Cuándo |
|---|---|---|
| Descansando en el parque | `park_relax` | en park |
| Paseando al perro | `walking_dog` | en park, plaza · 06:00–23:00 |
| En el cine | `cinema_film` | en cinema |
| En un museo | `museum_visit` | en museum |
| En el teatro | `theater_show` | en theater |
| De fiesta | `nightlife` | en nightclub |
| Visitando la biblioteca | `library_visit` | en library |
| Haciendo turismo | `sightseeing` | en plaza |
| Viendo un espectáculo callejero | `street_performance` | en plaza, park |

## ⚑ Deporte (`exercise`)

| Subestado | id | Cuándo |
|---|---|---|
| En el gimnasio | `gym_workout` | en gym |
| Deporte en equipo | `team_sport` | en sports |
| Corriendo | `jogging` | en park |
| Yoga en el parque | `yoga_park` | en park |
| Ruta en bici | `outdoor_cycling` | en park |

## ☺ Vida social (`social`)

| Subestado | id | Cuándo |
|---|---|---|
| Charlando | `chatting` | auto (contexto) |
| Quedada en grupo | `group_hangout` | en park, plaza, bar, cafe |
| Visitando a un amigo | `visiting_friend` | en home_friend |
| Tiempo en familia | `family_time` | en home |
| Discutiendo | `arguing` | auto (contexto) |

## ♥ Cuidados (`care`)

| Subestado | id | Cuándo |
|---|---|---|
| Llevando a los niños al colegio | `school_run` | en school |
| Cuidando a un familiar | `caring_relative` | en home_family |
| Cuidando de los niños | `childcare` | en home |
| Acompañando a una cita | `accompanying_appointment` | en clinic |

## ✚ Salud (`health`)

| Subestado | id | Cuándo |
|---|---|---|
| En el médico | `doctor_visit` | en clinic |
| En el hospital | `hospital_visit` | en hospital |
| Enfermo en casa | `resting_ill` | en home |

## ✉ Recados (`errands`)

| Subestado | id | Cuándo |
|---|---|---|
| En el banco | `bank_visit` | en bank |
| Haciendo trámites | `paperwork` | en city_hall |
| En un oficio religioso | `religious_service` | en worship |
| En un hotel | `hotel_visit` | en hotel |

## ⚠ Emergencia (`emergency`)

| Subestado | id | Cuándo |
|---|---|---|
| Observando | `watching` | auto (contexto) |
| Curioseando | `investigating` | auto (contexto) |
| Huyendo | `fleeing` | auto (contexto) |
| Evacuando | `evacuating` | auto (contexto) |
| Refugiándose | `sheltering` | auto (contexto) |
| Ayudando | `helping` | auto (contexto) |
| Persiguiendo | `pursuing` | auto (contexto) |
