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

Con 25.000 residentes, un tick cuesta entre 1,3 y 1,9 ms de mediana según la ejecución: el motor puede simular entre
450 y 700 veces más rápido que el tiempo real, así que la interfaz va fluida incluso a 15 minutos simulados por segundo. Las decisiones son lo más caro;
por eso se escalonan y cada residente se compromete con su actividad un tiempo mínimo.

## Variabilidad

En un portátil las cifras varían entre ejecuciones (hasta un 15–20 % en la máquina de referencia) por la gestión
térmica y de energía, y por WSL. Para comparar dos versiones del motor:

- ejecuta ambas en la misma máquina, conectada a la corriente y sin otras cargas;
- repite cada medida varias veces y compara la mediana de los p50;
- usa la misma semilla, plantilla y tamaño (el benchmark las fija por defecto).

La versión 0.3.1 es algo más lenta que la 0.3.0 porque corrige dos fallos que reducían el trabajo real: los grupos
nunca llegaban a formarse y los autobuses se quedaban parados en la última parada.

## Micro-benchmark de almacenamiento

`storage_benchmark` es el experimento original que compara la disposición de memoria OOP (un objeto por residente) con
SoA (un vector por campo) con un modelo simplificado. El motor actual usa SoA. Ver
[ADR-0001](../decisiones/ADR-0001-storage-backends.md).
