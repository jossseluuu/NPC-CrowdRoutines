# Rendimiento

## Metodología

`npc_crowd_routines benchmark` construye una ciudad grande para cada tamaño de población, simula un periodo de
calentamiento (para que la ciudad esté en marcha: desplazamientos, trabajo, eventos) y mide el tiempo de cada tick
(1 s simulado) con un reloj monotónico. Informa p50, p95, p99 y máximo, la velocidad máxima sostenible
(segundos simulados por segundo real) y una estimación de memoria de los residentes.

- Un solo hilo de simulación (el motor es determinista y secuencial).
- Compilación Release (`-O3`).
- Eventos aleatorios activos con caos 1.

La CI regenera este informe en cada cambio de `main` en una máquina de GitHub (las cifras varían según el hardware;
compara siempre en la misma máquina).

## Último informe (máquina de referencia)

--8<-- "rendimiento/informes/latest.md"

## Lectura de las cifras

Con 25.000 residentes, un tick cuesta ~1,2 ms de mediana: el motor puede simular unas 700 veces más rápido que el
tiempo real, así que la interfaz va fluida incluso a 15 minutos simulados por segundo. Las decisiones son lo más caro;
por eso se escalonan y cada residente se compromete con su actividad un tiempo mínimo.

## Micro-benchmark de almacenamiento

`storage_benchmark` es el experimento original que compara la disposición de memoria OOP (un objeto por residente) con
SoA (un vector por campo) con un modelo simplificado. El motor actual usa SoA. Ver
[ADR-0001](../decisiones/ADR-0001-storage-backends.md).
