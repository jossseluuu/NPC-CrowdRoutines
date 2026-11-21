# Glosario

| Término | Significado |
|---|---|
| **Actividad** | Lo que hace un residente a grandes rasgos (*Durmiendo*, *Trabajando*, *Ocio*...). Hay 14. Ver [Estados](conceptos/estados.md). |
| **Agenda** | Bloques del día de un residente: obligaciones (trabajo, clase, cuidados) y planes (deporte, compras, quedadas). |
| **Arquetipo** | Patrón de personalidad (*Madrugador*, *Alma de la fiesta*, *Casero*...) que ajusta rasgos y preferencias. Cada residente mezcla dos de los 48. |
| **Caos** | Multiplicador de la frecuencia de los eventos aleatorios. 0 los desactiva; 1 es el valor normal. |
| ***Checksum*** | Huella numérica del estado completo de la simulación. Si dos ejecuciones terminan con el mismo *checksum*, han producido el mismo resultado. |
| **CSR** | *Compressed sparse row*: formato que guarda las relaciones de todos los residentes en dos vectores contiguos, sin un objeto por relación. |
| **Encuentro** | Interacción espontánea entre dos residentes que se cruzan en la calle: una charla o, entre rivales, una discusión. |
| **Escenario** | Configuración (plantilla, tamaño, semilla, población, hora...) más una lista de eventos programados. Ver [Escenarios](datos/escenarios.md). |
| **Frame** | Instantánea binaria compacta con la posición y el estado de cada residente que el servidor envía al navegador. |
| **Grupo** | Residentes que viajan juntos: un líder y los familiares o amigos que se le unen. |
| **Inspector** | Panel de la interfaz con toda la información de un residente o edificio. |
| **Necesidad** | Valor de 0 a 1 que crece o baja con el tiempo y las actividades: cansancio, hambre, higiene, soledad, aburrimiento, malestar, estrés y miedo. |
| **p50 / p95 / p99** | Percentiles del tiempo por tick: el p95 es el tiempo que no supera el 95 % de los ticks. |
| **Partida guardada** | Configuración más todas las órdenes aplicadas, cada una con su tick, y el *checksum* final. Permite repetir una ejecución exacta. |
| **Perfil de rutina** | Tipo de vida que marca la agenda: oficinista, turnos, estudiante, jubilado, teletrabajo... Hay 16. |
| **Plantilla** | Receta del generador de mapas: forma de las calles, barrios y elementos singulares. Hay 6. |
| **Rasgo** | Dimensión de personalidad de 0 a 1 (sociabilidad, curiosidad, paciencia, prudencia, neuroticismo...). Hay 12. |
| **Semilla** | Número que fija todos los resultados aleatorios. La misma semilla genera la misma ciudad y la misma población. |
| **SoA** | *Structure of arrays*: un vector por campo (todas las posiciones juntas, todas las edades juntas...) en lugar de un objeto por residente. Aprovecha mejor la caché de la CPU. |
| **Subestado** | Detalle de la actividad (*Ocio › Paseando al perro*). Cada residente está siempre en exactamente uno. Hay 85. |
| **Telemetría** | Serie temporal con una muestra por minuto simulado: actividades, necesidades medias, tráfico, movimiento y eventos. |
| **Tick** | Paso fijo de la simulación; por defecto, 1 segundo simulado. |
| **Utilidad** | Puntuación que da un residente a cada actividad posible; elige la más alta. Ver [Comportamiento](conceptos/comportamiento.md). |
