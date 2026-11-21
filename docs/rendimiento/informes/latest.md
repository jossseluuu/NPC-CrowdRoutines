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
| 2500 | 0.121 | 0.374 | 0.789 | 2.99 | ×6117 | 5.3 MB |
| 5000 | 0.254 | 0.702 | 1.313 | 2.798 | ×3070 | 10.7 MB |
| 10000 | 0.559 | 1.703 | 2.375 | 3.831 | ×1388 | 21.5 MB |
| 25000 | 1.688 | 3.611 | 4.509 | 6.328 | ×520 | 53.9 MB |

*Velocidad máx.*: segundos simulados por segundo real que el motor puede sostener en esta máquina.
