# Informe de rendimiento

| Parámetro | Valor |
|---|---|
| CPU | AMD Ryzen 7 250 w/ Radeon 780M Graphics |
| Compilador | gcc 15.2 (release) |
| Hilos disponibles | 16 (la simulación usa 1) |
| Mapa | grid / large |
| Ticks medidos | 1800 (tras 1800 de calentamiento), 1 tick = 1 s simulado |

| Residentes | p50 (ms) | p95 (ms) | p99 (ms) | máx (ms) | Velocidad máx. | Memoria aprox. |
|---:|---:|---:|---:|---:|---:|---:|
| 2500 | 0.079 | 0.145 | 0.199 | 0.414 | ×11608 | 5.3 MB |
| 5000 | 0.181 | 0.444 | 0.789 | 1.386 | ×4564 | 10.7 MB |
| 10000 | 0.358 | 0.633 | 0.976 | 3.844 | ×2508 | 21.5 MB |
| 25000 | 1.29 | 2.404 | 3.013 | 4.516 | ×710 | 53.9 MB |

*Velocidad máx.*: segundos simulados por segundo real que el motor puede sostener en esta máquina.
