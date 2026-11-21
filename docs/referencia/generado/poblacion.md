<!-- Generado por `npc_crowd_routines export-docs` a partir de data/. No editar a mano. -->

# Perfiles y arquetipos

## Perfiles de rutina (16)

| Perfil | id | Peso | Edad | Lugar de trabajo/estudio | Días | Inicio | Horas |
|---|---|---:|---|---|---|---|---|
| Oficinista | `office_worker` | 17 | 23–64 | office, bank, city_hall | lun–vie | 08:36 ± 42 min | 8.30 |
| Dependiente/a | `retail_worker` | 7 | 18–62 | shop_small, supermarket, mall, market, pharmacy | lun–sáb | 09:30 ± 84 min | 7.00 |
| Hostelería | `hospitality_worker` | 6 | 18–55 | cafe, restaurant, bar, hotel, cinema, theater, gym, museum | rotativos | 13:00 ± 210 min | 7.50 |
| Operario/a | `factory_worker` | 6 | 19–63 | factory, warehouse | lun–vie | turnos 06:00/14:00/22:00 | 8.00 |
| Sanitario/a a turnos | `healthcare_shift` | 5 | 23–64 | hospital, clinic | rotativos | turnos 07:00/15:00/23:00 | 8.00 |
| Trabajo nocturno | `night_worker` | 2 | 20–60 | warehouse, hospital, nightclub, hotel, transit | rotativos | 22:00 ± 48 min | 8.00 |
| Docente | `teacher` | 3 | 25–64 | school, university, library | lun–vie | 08:06 ± 18 min | 7.00 |
| Servicios de emergencia | `emergency_services` | 1.5 | 22–60 | police, fire_station | rotativos | turnos 07:00/19:00 | 12.0 |
| Transporte público | `transit_worker` | 1 | 24–63 | transit | rotativos | turnos 05:30/13:30 | 8.00 |
| Escolar | `student_school` | 12 | 6–17 | school | lun–vie | 08:18 ± 15 min | 6.20 |
| Universitario/a | `student_university` | 6 | 18–26 | university | lun–vie | 10:00 ± 96 min | 5.00 |
| Jubilado/a | `retired` | 15 | 65–94 | — | — | — | — |
| Cuidador/a familiar | `caregiver` | 5 | 24–70 | — | lun–vie | 08:12 ± 18 min | 1.20 |
| Teletrabajo | `remote_worker` | 5 | 23–62 | home | lun–vie | 09:00 ± 60 min | 7.80 |
| Autónomo/a | `freelancer` | 3 | 21–60 | home, cafe, library, office | aleatorios | 10:30 ± 120 min | 6.00 |
| En paro | `unemployed` | 4.5 | 18–64 | — | — | — | — |

## Arquetipos de personalidad (48)

Cada residente combina un arquetipo principal (70 %) y uno secundario (30 %); sus rasgos se muestrean con una normal de desviación 0.11 alrededor de esa mezcla. Valores 0–100.

| Arquetipo | id | Sociabilidad | Responsabilidad | Curiosidad | Prudencia | Paciencia | Agresividad | Empatía | Actividad física | Ahorro | Rutina | Nocturnidad | Neuroticismo | Preferencias |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Trabajador constante | `steady_commuter` | 45 | 80 | 40 | 65 | 65 | 25 | 55 | 40 | 60 | 85 | 25 | 35 | — |
| Adicto al trabajo | `workaholic` | 35 | 95 | 45 | 55 | 40 | 40 | 40 | 35 | 55 | 75 | 55 | 60 | Ocio ×0.6, Vida social ×0.6 |
| Alma de la fiesta | `social_butterfly` | 95 | 45 | 70 | 40 | 50 | 30 | 70 | 55 | 30 | 30 | 65 | 35 | Comiendo fuera ×1.4, Vida social ×1.6 |
| Casero | `homebody` | 20 | 55 | 30 | 75 | 70 | 20 | 55 | 20 | 65 | 80 | 40 | 50 | En casa ×1.6, Ocio ×0.6, Vida social ×0.6 |
| Ave nocturna | `night_owl` | 60 | 40 | 65 | 40 | 45 | 35 | 50 | 45 | 35 | 25 | 95 | 40 | Ocio ×1.3 |
| Madrugador | `early_bird` | 50 | 75 | 45 | 60 | 65 | 20 | 55 | 65 | 55 | 80 | 5 | 30 | Deporte ×1.3 |
| Deportista | `fitness_fan` | 60 | 70 | 45 | 45 | 50 | 35 | 50 | 95 | 45 | 70 | 35 | 30 | Deporte ×2.2 |
| Sedentario | `couch_potato` | 40 | 30 | 30 | 60 | 60 | 30 | 45 | 5 | 50 | 55 | 65 | 45 | En casa ×1.5, Deporte ×0.2 |
| Gastrónomo | `foodie` | 70 | 45 | 75 | 40 | 45 | 25 | 55 | 45 | 15 | 40 | 55 | 30 | Compras ×1.2, Comiendo fuera ×2.0 |
| Ahorrador | `frugal_saver` | 40 | 75 | 35 | 75 | 70 | 20 | 50 | 40 | 95 | 80 | 30 | 40 | Compras ×0.7, Comiendo fuera ×0.4 |
| Comprador compulsivo | `shopaholic` | 70 | 30 | 60 | 40 | 30 | 30 | 50 | 45 | 5 | 35 | 50 | 55 | Compras ×2.2 |
| Amante de la cultura | `culture_lover` | 55 | 60 | 90 | 50 | 70 | 15 | 65 | 40 | 45 | 50 | 50 | 35 | Ocio ×1.6 |
| Ratón de biblioteca | `bookworm` | 25 | 70 | 90 | 65 | 80 | 10 | 55 | 25 | 65 | 65 | 55 | 45 | En casa ×1.2, Ocio ×1.1 |
| Fiestero | `party_animal` | 90 | 25 | 70 | 15 | 30 | 50 | 45 | 60 | 20 | 15 | 90 | 35 | Ocio ×1.5, Vida social ×1.5 |
| Ansioso | `anxious_worrier` | 35 | 60 | 35 | 90 | 35 | 20 | 60 | 30 | 60 | 75 | 40 | 95 | En casa ×1.3, Salud ×1.6 |
| Buscador de emociones | `thrill_seeker` | 65 | 35 | 85 | 5 | 25 | 55 | 40 | 80 | 25 | 15 | 65 | 25 | Ocio ×1.4, Deporte ×1.4 |
| Cuidador nato | `caregiver_soul` | 65 | 70 | 45 | 70 | 85 | 10 | 95 | 45 | 55 | 75 | 30 | 45 | Vida social ×1.2, Cuidados ×1.5 |
| Pilar del barrio | `community_pillar` | 80 | 70 | 50 | 55 | 70 | 15 | 85 | 45 | 50 | 70 | 30 | 25 | Vida social ×1.3, Recados ×1.4 |
| Solitario | `loner` | 5 | 55 | 45 | 60 | 60 | 35 | 30 | 40 | 60 | 70 | 60 | 50 | En casa ×1.3, Vida social ×0.3 |
| Vecino gruñón | `grumpy_neighbour` | 25 | 55 | 25 | 65 | 15 | 80 | 20 | 35 | 70 | 80 | 40 | 65 | — |
| Impulsivo | `hothead` | 55 | 30 | 45 | 20 | 10 | 95 | 20 | 60 | 40 | 30 | 60 | 70 | — |
| Conciliador | `peacemaker` | 65 | 60 | 50 | 60 | 90 | 5 | 90 | 45 | 50 | 60 | 35 | 25 | — |
| Explorador curioso | `curious_explorer` | 60 | 40 | 95 | 30 | 55 | 25 | 55 | 60 | 40 | 15 | 55 | 30 | Compras ×1.1, Ocio ×1.4 |
| Animal de costumbres | `creature_of_habit` | 40 | 70 | 15 | 75 | 70 | 20 | 50 | 40 | 65 | 98 | 30 | 45 | — |
| Espontáneo | `spontaneous` | 70 | 20 | 75 | 30 | 35 | 35 | 50 | 55 | 30 | 5 | 60 | 35 | — |
| Ambicioso | `ambitious_climber` | 60 | 90 | 55 | 35 | 40 | 50 | 35 | 55 | 40 | 65 | 45 | 45 | — |
| Tranquilo | `laid_back` | 55 | 25 | 45 | 45 | 85 | 15 | 55 | 30 | 45 | 35 | 60 | 15 | Ocio ×1.2 |
| Perfeccionista | `perfectionist` | 35 | 98 | 50 | 75 | 35 | 35 | 45 | 50 | 65 | 90 | 35 | 75 | — |
| Hipocondríaco | `hypochondriac` | 35 | 55 | 30 | 90 | 40 | 20 | 50 | 25 | 50 | 70 | 40 | 90 | Salud ×2.5 |
| Ciclista ecológico | `eco_cyclist` | 55 | 60 | 65 | 45 | 60 | 20 | 70 | 85 | 70 | 60 | 35 | 30 | Deporte ×1.3 |
| Dependiente del coche | `car_dependent` | 45 | 55 | 35 | 55 | 30 | 45 | 40 | 20 | 35 | 65 | 40 | 45 | — |
| Usuario del transporte público | `transit_regular` | 45 | 65 | 45 | 60 | 65 | 20 | 55 | 40 | 70 | 75 | 35 | 40 | — |
| Padre/madre primerizo | `young_parent` | 55 | 70 | 40 | 75 | 60 | 25 | 80 | 50 | 65 | 80 | 20 | 55 | Ocio ×0.7, Cuidados ×1.6 |
| Padre/madre ocupado | `busy_parent` | 45 | 80 | 35 | 65 | 35 | 40 | 65 | 45 | 55 | 80 | 25 | 70 | Compras ×1.2, Cuidados ×1.4 |
| Jubilado activo | `active_retiree` | 70 | 60 | 65 | 55 | 70 | 15 | 65 | 65 | 55 | 65 | 15 | 30 | Ocio ×1.3, Deporte ×1.3, Vida social ×1.3 |
| Jubilado tranquilo | `quiet_retiree` | 35 | 60 | 35 | 75 | 75 | 15 | 55 | 20 | 70 | 85 | 15 | 45 | En casa ×1.4, Recados ×1.3 |
| Habitual del club | `chess_club_regular` | 60 | 65 | 70 | 60 | 85 | 20 | 50 | 25 | 60 | 85 | 25 | 35 | Ocio ×1.2, Vida social ×1.4 |
| Jugón | `gamer` | 40 | 35 | 60 | 50 | 40 | 40 | 45 | 15 | 40 | 45 | 90 | 50 | En casa ×1.6 |
| Estudioso | `studious` | 40 | 85 | 80 | 65 | 65 | 15 | 55 | 40 | 60 | 80 | 40 | 55 | Estudiando ×1.4 |
| Adolescente rebelde | `rebellious_teen` | 70 | 15 | 70 | 15 | 20 | 65 | 35 | 65 | 40 | 10 | 75 | 55 | Vida social ×1.4 |
| Influencer | `influencer` | 95 | 50 | 75 | 30 | 30 | 35 | 40 | 60 | 10 | 30 | 70 | 55 | Comiendo fuera ×1.5, Ocio ×1.4 |
| Artista | `artist` | 55 | 30 | 95 | 35 | 55 | 25 | 70 | 40 | 45 | 10 | 80 | 60 | Ocio ×1.5 |
| Turista de corazón | `tourist_at_heart` | 70 | 40 | 90 | 35 | 55 | 20 | 60 | 65 | 30 | 20 | 50 | 25 | Comiendo fuera ×1.3, Ocio ×1.6 |
| Devoto | `devout` | 60 | 75 | 30 | 70 | 75 | 10 | 80 | 35 | 65 | 90 | 15 | 35 | Recados ×1.8 |
| Voluntario | `volunteer` | 75 | 65 | 55 | 50 | 75 | 10 | 95 | 55 | 55 | 60 | 30 | 30 | Vida social ×1.3, Cuidados ×1.4 |
| Superviviente de turnos | `shift_survivor` | 40 | 65 | 35 | 55 | 50 | 40 | 55 | 35 | 60 | 40 | 70 | 65 | — |
| Vividor | `bon_vivant` | 80 | 30 | 60 | 30 | 55 | 25 | 55 | 35 | 10 | 30 | 70 | 20 | Comiendo fuera ×1.8, Vida social ×1.4 |
| Minimalista | `minimalist` | 35 | 70 | 55 | 60 | 70 | 15 | 60 | 60 | 85 | 65 | 35 | 25 | Compras ×0.4 |
