# ADR-0002: Un único motor de simulación en C++ y un cliente web fino

- **Estado:** Aceptada (2026-11-21)
- **Contexto:** Existían dos simulaciones distintas: un modelo en JavaScript dentro del visor web (con rutinas, relaciones,
  filtros y eventos) y un benchmark nativo en C++ con un modelo mucho más simple. Cada mejora había que hacerla dos veces,
  los resultados no eran comparables y el proyecto quiere estar escrito en C++ en su gran mayoría.
- **Decisión:** Toda la lógica (mapa, población, comportamiento, movimiento, eventos, filtros, estadísticas, escenarios)
  vive en el motor C++. El navegador es un cliente fino que dibuja instantáneas y envía órdenes a través de un servidor
  HTTP propio incluido en el ejecutable. Se elimina el modelo JavaScript.
- **Consecuencias:**
    - Una sola fuente de verdad; las pruebas del motor cubren lo que se ve.
    - Determinismo y repetición posibles (las órdenes de la interfaz se registran con su tick).
    - El visor necesita que el motor esté en marcha (`npc_crowd_routines serve`); no hay demo estática.
    - El servidor HTTP se escribió sin dependencias (sockets POSIX/Winsock) y está pensado para uso local.
- **Revisar si:** se necesita una demo pública sin servidor (opción: compilar el motor a WebAssembly) o varios clientes
  remotos simultáneos.
