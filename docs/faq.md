# Preguntas frecuentes

## Instalación y ejecución

??? question "Windows bloquea el ejecutable al abrirlo. ¿Es un virus?"
    No. Si tienes **Smart App Control** activado, Windows bloquea los programas recién compilados que no están firmados
    digitalmente, aunque sean tuyos. No hace falta desactivar esa protección: compila y ejecuta el proyecto en WSL
    (ver [Primeros pasos](primeros-pasos.md) y [ADR-0003](decisiones/ADR-0003-wsl-smart-app-control.md)).

??? question "Abro http://localhost:8080 y no carga nada."
    Comprueba que el servidor sigue en marcha en la terminal y que lo lanzaste **desde la raíz del repositorio**: si no
    encuentra `./web`, no puede servir la interfaz. También puedes indicar las rutas con `--web` y `--data`. Si el puerto
    está ocupado, usa otro con `--port 8081`.

??? question "¿Puedo abrir la interfaz desde otro equipo de mi red?"
    Sí, lanzando el servidor con `--host 0.0.0.0`. Ten en cuenta que no hay autenticación: cualquiera en la red podrá
    controlar la simulación. Ver [Consideraciones](consideraciones.md#seguridad).

## Simulación

??? question "¿Cuántos residentes hay por defecto y cómo lo cambio?"
    Depende del tamaño del mapa: 2.500 en el pequeño, 8.000 en el mediano y 20.000 en el grande. Cámbialo con
    `--population N` (hasta 60.000) o en la pestaña **Escenarios › Nueva ciudad**.

??? question "¿Por qué a veces hay residentes durmiendo la siesta o en casa a media mañana?"
    Porque no todos trabajan o estudian a la misma hora: hay jubilados, personas en paro, turnos de noche, teletrabajo y
    cuidadores. La pestaña **Decisión** del inspector muestra por qué cada residente hace lo que hace.

??? question "¿Por qué el resultado no cambia si subo la velocidad?"
    Porque la simulación es de paso fijo: la velocidad solo decide cuántos pasos se calculan por segundo real. Una hora
    simulada a 1× y a 1 h/s produce exactamente el mismo resultado.

??? question "¿Cómo provoco un evento en un lugar concreto?"
    En la pestaña **Eventos**, marca **Elegir lugar en el mapa**, pulsa el tipo de evento y haz clic en el mapa. Puedes
    fijar la gravedad, el radio y la duración, o dejarlos en automático.

## Experimentos

??? question "¿Cómo repito un experimento exactamente?"
    Guarda la partida (pestaña **Escenarios › Partidas guardadas**, o `run --out carpeta`) y repítela con
    `npc_crowd_routines replay archivo.json`. El programa vuelve a simular desde cero y comprueba que el *checksum* final
    coincide. Ver [Escenarios y reproducibilidad](conceptos/reproducibilidad.md).

??? question "¿Dónde se guardan los escenarios, partidas y mapas?"
    Los escenarios en `data/scenarios/`, los mapas personalizados en `data/maps/custom/` y las partidas y resultados de
    `run` en `runs/` (o en las carpetas que indiques con `--data` y `--runs`).

??? question "¿Cómo exporto los datos para analizarlos?"
    Hay tres vías: **Descargar CSV de telemetría** en la pestaña Gráficas (una fila por minuto simulado), **Exportar CSV**
    en la pestaña Filtros (los residentes que cumplen las condiciones) y la orden `run`, que escribe `telemetry.csv` y
    `summary.json` sin interfaz.

## Personalización

??? question "¿Necesito recompilar para cambiar actividades, edificios o eventos?"
    No. Todo está en los JSON de `data/` y se carga al arrancar o al generar una ciudad nueva. Si hay un error, el
    programa indica el archivo y el campo. Ver [Formatos de datos](datos/index.md).

??? question "¿Cómo creo mi propio mapa?"
    Lo más cómodo es partir de uno generado: `npc_crowd_routines generate-map --template grid --out mi_mapa.json`, o el
    botón **Exportar mapa actual**. Edítalo, compruébalo con `validate-map mi_mapa.json` y súbelo desde la interfaz.
    Ver [Mapas personalizados](datos/mapas-personalizados.md).

??? question "¿Cómo añado otro idioma?"
    Copia `data/i18n/en.json` con el código del idioma (por ejemplo `fr.json`) y traduce sus textos. El selector de idioma
    lo mostrará automáticamente.
