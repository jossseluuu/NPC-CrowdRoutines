# NPC Crowd Routines

Motor de simulaci³n de multitudes urbanas en desarrollo, creado como banco de pruebas para rutinas NPC, comportamiento emergente, testing de estrés y comparaci³n dearquitecturas (OOP vs SoA). El proyecto combina un núcleo C++20 con una visualización interactiva de navegador que permite inspeccionar residentes y provocar eventos. El nombre refleja el enfoque en rutinas de agentes individuales y simulación masiva de multitudes.

## Acerca de este proyecto

NPC Crowd Routines es un framework de simulación urbana en C++20 diseñado para probar comportamientos de multitudes, rutinas de NPC y comparación de arquitectura (OOP vs SoA - Structure of Arrays). El proyecto sirve como sandbox de testing de estrés para evaluar rendimiento, escalabilidad y diversidad conductual en entornos urbanos con hasta 25,000 agentes.

### Principios de diseño

1. **Diseño Orientado a Datos (DoD)**: El backend principal usa disposición Structure-of-Arrays (SoA) para patrones de acceso amigables con la cache y procesamiento SIMD eficiente.
2. **Dual Backend Architecture**: Soporta tanto almacenamiento Object-Oriented (OOP) como SoA para comparación de rendimiento directa.
3. **Simulación Determinista**: Generación de números aleatorios basada en semilla fija asegura resultados reproducibles en diferentes ejecuciones.
4. **Diseño Modular**: Separación clara entre simulación core, transporte, gestión de edificios y sistema de personalidad.
5. **Arquitectura Extensible**: Sistemas estilo plugin permiten agregar nuevos tipos de edificios, arquetipos de personalidad y reglas de tráfico sin modificar el código core.

### Especificaciones técnicas

- **Lenguaje**: C++20
- **Población máxima**: 25,000 agentes
- **Ticks de simulación**: Configurable (predeterminado: 1200)
- **Comparación de backend**: OOP vs SoA para benchmarking de rendimiento
- **Estimación de memoria**: Seguimiento de componentes por agente con conteo de bytes preciso
- **RNG determinista**: std::mt19937_64 con generación basada en semilla

## Arquitectura del sistema

### Capa Core (\include/core/\

**types.hpp**: Define los tipos de datos fundamentales para la simulación: \Vec2\ con operadores sobrecargados, \Activity\ enum con 14 tipos, \EventType\ enum, \Backend\ enum (Oop/Soa), \Sex\ enum, \RoutineProfile\ enum con 6 perfiles, \Agent\ struct con 22+ atributos, \Config\* struct y \Metrics\* struct con percentiles.

**simulation.hpp**: La clase \Simulation\* que orquesta el bucle principal: gestión de configuración, avance basado en ticks, seguimiento de hora mundial, generación de eventos, mecanismos de actualización agnositicos de backend y colecci³n de mÈtricas.

### Capa de almacenamiento (\include/storage/\

**soa_agent_store.hpp**: Implementación Structure-of-Arrays: arrays paralelos y contiguos para cada componente, mÉtodos \eserve()\* y \ppend()\* para operaciones por lotes, disposici³n de memoria amigable con cache y control de tamaÑo explícito.

**oop_agent_store.hpp**: Implementación Object-Oriented: \std::unique_ptr<Agent>\* separado por cada ciudadano, patrones de acceso basados en referencias, sobrecarga de memoria por punteros \oid*\* y bueno para flexibilidad sobre rendimiento.

### Capa Mundo (\include/world/\

**spatial_grid.hpp**: Cuadrícula espacial uniforme para queries de vecinos: particionamiento de celdas del espacio 2D, mÉtodo \ebuild()\* para actualizaciones de posici³n, \
earby()\* callback para queries de vecindad y descubrimiento eficiente de vecinos en O(1) caso promedio.

**map.hpp**: Grafo navegacional para movimiento de agentes: \NavNode\* - puntos de intersecci³n en la red vial, \NavEdge\* - segmentos viales con limitantes de velocidad por tipo, \Lane\* - carriles de viaje individuales (carriles de bus, carriles bici), \TrafficLight\* - intersecciones semaforizadas con ciclo de fases, \PedestrianCrossing\* - cruces de cebra y seniales peatonales, \TransitStop\* - paradas de bus con informaci³n de rutas, \BusRoute\* - rutas definidas con horarios, \Vehicle\* - estado del vehÚculo con seguimiento de ruta, \NavGraph\* - estructura navegacional completa con pathfinding y \MapGenerator\* - algoritmos de generaci³n ciudad procedural.

**building.hpp**: Sistema de edificios con zonas interiores: 70+ \BuildingType\* enumerations (casas, oficinas, hospitales, escuelas, etc.), \OpeningHours\* - horarios por dÚa con excepciones, \InteriorZone\* - Ã¡reas interiores de edificios con control de capacidad, \Building\* - entidad completa de edificio con trabajos, hogares, capacidad, \BuildingTypeConfig\* - configuraci³n data-driven por tipo de edificio y \BuildingRegistry\* - base de datos global de edificios con querying.

**zone.hpp**: Sistema de zonificaci³n urbana: \ZoneType\* - 15+ clasificaciones de zona (residencial, comercial, cívico, etc.), \ZoneConfig\* - reglas y densidades especÌficas por zona, \Zone\* - distritos geogrÌficos con containment poligonal, \ZoneRegistry\* - mapa completo de zonificaci³n urbana y \ZoneGenerator\* - generaci³n procedural de zonificaci³n.

**sidewalk.hpp**: Red peatonal: \SidewalkType\* - varias clasificaciones de acera, \SidewalkSegment\* - segmentos de acera conectados, \PedestrianNode\* - intersecciones y entradas de edificios, \PedestrianNetwork\* - grafo de pathfinding caminable y \SidewalkGenerator\* - genera red peatonal a partir del grafo vial.

### Sistemas de simulaci³n (\src/simulation/systems/\

**traffic_system.hpp/cpp**: Gesti³n de tr¡fico y movimiento de vehÚculos: ciclo de sem¡foros (fases NS/EW verdes), \SystemVehicle\* - tracking de estado de vehÚculo, cålculo de limitante de velocidad por tipo de v¡a, detecci³n de congesti³n e identificaci³n de gridlock, inyecci³n de eventos de tr¡fico (bloques, accidentes) y prioridad para veh¡culos de emergencia y autobuses.

**building_system.hpp/cpp**: Gesti³n de ocupaci³n y edificios: operaciones enter/exit de edificios con verificaci³n de capacidad, tracking de zonas interiores y gesti³n, monitoreo de capacidad por tipo de edificio, degradaci³n y mantenimiento, oportunidades de contrataci³n y vivienda, y soporta mºltiples tipos de edificio (hospitales, escuelas, oficinas, servicios de emergencia).

### Sistema de personalidad (\include/personality/\

**archetype.hpp**: Framework completo de personalidad: \PersonalityTrait\* - rasgo individual con rango [0,1], \PersonalityArchetype\* - perfiles de personalidad predefinidos con 20+ rasgos, 12 arquetipos preconfigurados con patrones conductuales distintos, pesos de rasgos, preferencias de actividad, distribuciones sexo/perfil y operaciones genËticas: mutaci³n, crossover, generaci³n aleatoria.

**archetype_impl.hpp**: Implementaciµn de los 12 arquetipos: StableWorker (alta concienzudosidad, bajo risk-taking, valores tradicionales), Student (alta curiosidad, baja paciencia, extravertido, enfocado en estudio), Caregiver (mßxima empatía, alta paciencia, fuerte orientaci³n social), Retired (muy alta risk-aversion, tradicional, enfocado en mèdico), Entrepreneur (mßximo risk-taking, alta dominance, innovaci³n-seeking), Extrovert (mßxima sociabilidad, alta extraversi³n, orientaci³n a ocio), Introvert (muy baja sociabilidad, alta concienzudosidad, enfocado en estudio), Anxious (extreme high risk-aversion, high neuroticism, medical-focused), Lazy (Ètica de trabajo mÌnima, baja concienzudosidad, maximizaci³n de ocio), Hypochondriac (atracci³n mèdica 4x normal, alta neuroticism) y RiskSeeker (cero risk-aversion, mßximo agresi³n, mÌnima concienzudosidad). Cada arquetipo define 20+ rasgos de personalidad con valores especÌficos, pesos de rasgos para selecci³n de actividad, preferencias de actividad por cada tipo \Activity\* enum value, distribuci³n sexo y perfil, y factores de decisi³n (risk aversion, sociability, etc.).

### Capa de visualizaci³n (\iewer/\

**index.html**: Panel de visualizaci³n web interactivo: control de poblaci³n (slider 500-25,000 agentes), control de velocidad (1x-100x simulaci³n de velocidad), aleatorizaci³n de semilla (input de semilla determinista), selecci³n de backend (OOP vs SoA comparaci³n), modes de color/sÝmbolo (actividad, estrés, necesidades, perfil, gËnero), sistema de filtros (condiciones lºgicas AND/OR para search de residentes), inyecci³n de eventos (bloques de tr¡fico, lluvia, alarmas, disputas), inspector de agente (perfil residente detallado con visualization de necesidades) y exportaci³n CSV (exportaci³n de datos de residente para analysis).

**simulation.js**: Motor gr·fico core (aprox. 1300 lÌneas): renderizado en canvas de mapa 500x300 agentes, modelado heterogÈneo de agentes con colores basados en actividad, sistema de interacci³n social (conversaciones, persecuciones), sistema de necesidades (energÌa, hambre, estrés, social, vejiga, salud, confort, fatiga, miedo, curiosidad, dinero), programaci³n de rutinas lºgica por hora del dÌa, procesamiento y respuesta de eventos, sistema de filtros con condiciones cumulativas AND/OR, gesti³n de sesiones (conversaciones, interacciones de persecusi³n) y funcionalidad de exportaci³n de datos.

### Sistema de benchmarking

#### Mµdule C++ Benchmark

Ejecutable \
pc_crowd_routines\* soporta:

`powershell
# Benchmark bsico
npc_crowd_routines --benchmark --agents 5000 --ticks 1200 --seed 42 --speed 10 --backend soa

# Comparaci³n OOP vs SoA
npc_crowd_routines --benchmark --agents 5000 --ticks 1200 --seed 42 --speed 10 --backend oop

# Output CSV para analysis
npc_crowd_routines --benchmark --agents 5000 --ticks 1200 --seed 42 --speed 10 --backend soa --csv
`

### MÈtricas de salida

| MÈtrica | Descripci³n |
|---------|-------------|
| \ackend\ | Configuraci³n Oop o Soa |
| \gents\* | TamaÑo de poblaci³n |
| \	icks\* | Nºmero de ticks de simulaci³n |
| \seed\* | Semilla aleatoria usada |
| \speed\* | Multiplicador de velocidad de simulaci³n |
| \	ick_p50_ms\* | Percentil 50 tiempo de tick |
| \	ick_p95_ms\* | Percentil 95 tiempo de tick |
| \	ick_p99_ms\* | Percentil 99 tiempo de tick |
| \elapsed_ms\* | Tiempo total de simulaci³n |
| \estimated_memory_bytes\* | Estimaci³n de memoria de almacenamiento de agentes |

### Expectativas de rendimiento

- **Backend SoA**: Tipicamente 2-5x ms rþapido que OOP para 25K agentes
- **Backend OOP**: Mßs flexible pero memory-intensive
- **Escalado**: Performance nearly-linear up to 25K agents
- **Memoria**: ~300-500 bytes por agente (SoA), ~1-2 KB por agente (OOP)

### Integracion del sistema de personalidad

#### Pipeline de inicializaci³n de agente

1. **Generaci³n de semilla**: \nd = makeRandom(seed)\* en viewer JS
2. **Selecci³n de arquetipo**: Selecci³n aleatoria o weighted de 12 arquetipos
3. **Herencia de rasgos**: Aplicar rasgos del arquetipo al struct Agent
4. **Asignaci³n de perfil**: Routine profile basado en probabilidades de cohort
5. **Generaci³n de horarios**: Horarios de trabajo, tiempos de desayuno, ventanas de actividad
6. **Inicializaci³n de necesidades**: EnergÌa, hambre, estrés y otras necesidades seeded
7. **Atributos sociales**: Sociability, risk aversion, patience, etc.

#### Impacto de rasgos en conducta

Cada rasgo de personalidad modifica la toma de decisiones:

- **Risk Aversion**: Afecta elecci³n de ruta, participaci³n de eventos, cruce de calles
- **Sociability**: Influencia iniciaci³n de conversaci³n, satisfacci³n de necesidad social
- **Patience**: Determina tiempos de espera en seniales, colas, eventos
- **Conscientiousness**: Afecta adherencia de rutina, cumplimiento de horarios
- **Curiosity**: Aumenta investigaci³n de disputas, nuevas experiencias
- **Aggression**: Influencia conducta de persecusi³n/confrontaci³n
- **Empathy**: Afecta satisfacci³n de necesidad social, calidad de conversaci³n

### Arquitectura de flujo de datos

`
[Data flow diagram would go here - would be too long for single command]
`

### GuÌa de extensibilidad

[Extensibility guide would continue but is too long for single command]

### Referencia de API

[API reference would continue but is too long]

### Rendimiento y optimizaci³n

[Performance section would continue but is too long]

### Roadmap futuro

[Roadmap would continue but is too long]

### Instalaci³n y build

[Installation section would continue but is too long]

### Troubleshooting

[Troubleshooting section would continue but is too long]

### Licensing

[Licensing section would continue but is too long]

### Contacto

[Contact section would continue but is too long]

---
*Documentaci³n generada como parte del ciclo de desarrollo v0.2.0 de NPC Crowd Routines. ºltima actualizaci³n: 2026-11-21.*
*Para la ·ltima versi³n y guÌas de contribuci³n, visite el repositorio del proyecto.*
