# Consideraciones y limitaciones

Esta página explica qué representa el simulador, qué no representa y qué conviene tener en cuenta al usarlo o al
interpretar sus resultados.

## Qué es el modelo

NPC Crowd Routines es un **modelo de agentes sintético**. Cada residente decide de forma individual a partir de reglas y
parámetros que están en `data/`, y el comportamiento colectivo emerge de esas decisiones. Los parámetros se han
ajustado para que la ciudad resulte **plausible**: la mayoría duerme de madrugada, hay horas punta por la mañana y por
la tarde, los parques se llenan los fines de semana y la gente huye de un incendio y se acerca a un concierto.

## Qué no es

- **No está calibrado con datos reales.** Los horarios, pesos y probabilidades no proceden de encuestas de movilidad,
  censos ni estudios de evacuación. Los resultados sirven para **comparar escenarios entre sí** (por ejemplo, la misma
  ciudad con y sin un puente cortado), no para predecir lo que ocurriría en una ciudad concreta.
- **No es un simulador físico de multitudes.** Los peatones siguen la red de aceras y calles peatonales a la velocidad
  que les corresponde, pero no se evitan entre sí ni modelan empujones o densidades críticas. Tampoco hay edificios por
  dentro: un residente está dentro de un edificio o en la calle.
- **No es un simulador de tráfico microscópico.** Los coches avanzan por los tramos de calzada con una velocidad que
  depende de la ocupación de cada tramo y de los semáforos, sin carriles, adelantamientos ni aparcamiento.
- **No modela la economía.** Hay empleos, plazas escolares y aforos, pero no dinero, precios ni consumo.

## Interpretar los resultados

- Cada ejecución es **una muestra**. Para sacar conclusiones, repite el experimento con varias semillas
  (`run --seed N`) y compara medias o distribuciones, no una sola ejecución.
- Los primeros minutos tras el arranque no son representativos: todos empiezan en casa con necesidades parecidas.
  Descarta un periodo de calentamiento (el benchmark descarta 30 minutos simulados).
- Las cifras agregadas (por ejemplo, el estrés medio) ocultan la variabilidad: usa los filtros y el inspector para ver
  qué grupos de residentes explican un cambio.

## Determinismo

Con la misma configuración, las mismas órdenes y **el mismo binario**, el resultado es idéntico. Entre compiladores o
sistemas operativos distintos no está garantizado, porque las funciones matemáticas de la biblioteca estándar pueden
diferir en el último bit. Detalles en [Escenarios y reproducibilidad](conceptos/reproducibilidad.md).

## Rendimiento y escala

- La simulación usa **un solo hilo**, una decisión deliberada para mantener el determinismo. El resto de núcleos
  quedan libres para el servidor y el navegador.
- El límite configurado es de **60.000 residentes**. Con poblaciones muy grandes, además del motor influyen el tamaño de
  los frames que envía el servidor y el dibujo en el navegador.
- Las cifras de rendimiento dependen del hardware y varían entre ejecuciones; ver [Rendimiento](rendimiento/index.md).

## Seguridad

- El servidor HTTP es **local y no tiene autenticación**. Por defecto escucha solo en `127.0.0.1`.
- Con `--host 0.0.0.0`, cualquier equipo de la red puede controlar la simulación, provocar eventos, subir mapas y
  guardar archivos en `runs/`. Úsalo solo en redes de confianza y **nunca lo expongas a Internet**.
- Los mapas subidos se validan antes de guardarse y los nombres de archivo se limpian, pero el servidor no está pensado
  para recibir datos de usuarios desconocidos.

## Privacidad

Los nombres de residentes, calles y barrios se generan al azar a partir de listas de nombres comunes
(`data/names.json` y `data/maps/names.json`). No representan a personas ni lugares reales, y la simulación no recoge ni
envía datos de quien la usa.

## Plataformas

- **Linux y WSL** son las plataformas de referencia: allí se ejecutan las pruebas, los sanitizers y el benchmark.
- **Windows con MSVC** compila sin avisos, y el flujo de CI está configurado para ejecutar las pruebas también en
  Windows. En equipos con *Smart App Control*
  activado, Windows puede bloquear los ejecutables compilados localmente que no están firmados; ver
  [ADR-0003](decisiones/ADR-0003-wsl-smart-app-control.md).
- La interfaz está probada en Chrome. Usa solo APIs estándar (canvas 2D, módulos ES, `fetch`), así que debería funcionar
  en cualquier navegador actual.
