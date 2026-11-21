# Escenarios y reproducibilidad

El motor es **determinista**: con la misma configuración, las mismas órdenes y el mismo binario, el resultado es
idéntico bit a bit, sea cual sea la velocidad de la interfaz o la carga de la máquina.

!!! note "Alcance de la garantía"
    Está verificada entre ejecuciones del mismo binario: lo comprueban las pruebas y `replay` en Linux, tanto en local
    como en la CI. Entre compiladores o sistemas operativos distintos **no está garantizada**: el motor usa funciones
    como `std::log`, `std::sqrt` o `std::exp`, y las bibliotecas matemáticas pueden diferir en el último bit. Una
    diferencia mínima acaba cambiando alguna decisión y la simulación diverge. Para comparar experimentos, ejecútalos
    todos con la misma compilación.

## Cómo se consigue

- **Paso fijo**: un *tick* es siempre la misma cantidad de tiempo simulado (1 s por defecto). La velocidad de la interfaz
  solo cambia cuántos ticks se calculan por segundo real; nunca cambia el resultado.
- **Generador aleatorio propio y portable** (xoshiro256**). Las distribuciones de la biblioteca estándar de C++ no dan
  los mismos números en todos los compiladores, así que el motor usa las suyas.
- **Flujos aleatorios independientes** derivados de la semilla: mapa, población, comportamiento, eventos y vida social.
  Cambiar algo de un subsistema no altera los números de los demás.
- **Órdenes en el borde de un tick**: cada orden (provocar un evento, cambiar el caos...) se aplica al principio de un tick
  y se registra con ese número. Al repetir, se aplica exactamente en el mismo tick.
- **Un solo hilo de simulación** con orden de iteración fijo.

## Escenarios

Un escenario (`data/scenarios/*.json`, formato en [Escenarios y partidas](../datos/escenarios.md)) es una configuración
más una lista de eventos programados. Incluidos:

| Escenario | Qué prueba |
|---|---|
| Hora punta con accidente | atasco matinal, accidente en un puente y lluvia |
| Simulacro de evacuación | alarma en el centro del casco antiguo y fuga de gas, sin eventos aleatorios |
| Fin de semana de fiestas | mercadillo, fiestas, concierto y tormenta nocturna en la costa |
| Prueba de estrés: 25.000 residentes | rendimiento y comportamiento con mucha carga y caos alto |

Desde la interfaz puedes **guardar como escenario** lo que estás haciendo: la configuración y todos los eventos que
has provocado, con su hora.

## Partidas guardadas y verificación

*Guardar partida* escribe un archivo `npc-run` con la configuración, **todas las órdenes con su tick**, el tick final y
un ***checksum*** del estado (posiciones, actividades, subestados, lugares y necesidades de todos los residentes).

*Repetir y verificar* (o `npc_crowd_routines replay archivo.json`) reconstruye la ciudad desde cero, reaplica las órdenes,
simula hasta el tick final y compara el *checksum*. Si coincide, el experimento es exactamente reproducible.
Las pruebas automáticas comprueban esto en cada cambio.

## Experimentos sin interfaz

```sh
npc_crowd_routines run --scenario data/scenarios/rush_hour.json --hours 6 --out runs/hora_punta
```

genera `telemetry.csv` (una fila por minuto simulado con actividades, necesidades, movimiento, tráfico y eventos),
`summary.json` y `run.json` (repetible). Ideal para barridos de parámetros: cambia la semilla, la población o el caos
y compara los CSV.
