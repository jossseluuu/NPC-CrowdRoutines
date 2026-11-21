# Comportamiento

## Quién es cada residente

Al crear la ciudad, cada vivienda recibe un hogar (1–5 personas). Cada residente tiene:

- un **perfil de rutina** (oficinista, sanitario a turnos, escolar, jubilado, autónomo... 16 en total) que fija su edad,
  dónde trabaja o estudia, qué días y a qué hora;
- una **personalidad** de 12 rasgos (sociabilidad, responsabilidad, curiosidad, prudencia, paciencia, agresividad,
  empatía, actividad física, ahorro, rutina, nocturnidad, neuroticismo). Se construye mezclando **dos arquetipos**
  (70 % + 30 %) de los 48 disponibles y añadiendo ruido individual, así que no hay dos residentes iguales;
- un **horario personal**: hora de despertarse y acostarse (según trabajo, edad y nocturnidad), días laborables
  (fijos, de lunes a sábado, rotativos o aleatorios) y transporte habitual (a pie, bici, autobús o coche);
- una **red social**: familia y pareja, vecinos del edificio, compañeros de trabajo o clase, amistades elegidas
  por afinidad de edad, personalidad y cercanía, y algún rival.

Detalle de perfiles y arquetipos: [Población (generado)](../referencia/generado/poblacion.md).

## Necesidades

Ocho necesidades entre 0 (satisfecha) y 1 (desesperada): cansancio, hambre, higiene, soledad, aburrimiento,
malestar, estrés y miedo. Crecen con el tiempo según la personalidad (los sociables sienten antes la soledad) y cada
actividad las alivia o empeora a su ritmo (dormir quita cansancio; trabajar sube el estrés). Los eventos también las
modifican. Si una necesidad llega a un nivel crítico, el residente interrumpe lo que hace (si no es una obligación).

## La agenda del día

Cada día se genera una agenda por residente con:

- **obligaciones**: trabajo, clase o cuidados en su bloque horario (los turnos de noche continúan tras la medianoche);
- **planes**: deporte, compras, comer fuera, quedadas, ocio o recados, según la personalidad y si el día es libre.

Las obligaciones son casi seguras; los planes solo dan un empujón a esa actividad.

## Decisión por utilidad

Cuando termina lo que hacía (o algo le interrumpe), el residente puntúa **cada actividad elegible**:

```
puntuación = base
           + Σ peso(actividad, necesidad) × necesidad^1.5
           + bonificación por hora del día y fin de semana
           + agenda (obligación activa ≫ plan)
           (× personalidad: rasgos y preferencias de sus dos arquetipos)
           − penalización por mal tiempo si es al aire libre
           − tiempo de desplazamiento × (impaciencia)
           + inercia si ya la está haciendo + un poco de ruido (menos en los rutinarios)
```

Gana la más alta. Todos los pesos están en `data/activities.json` y `data/population.json`. La pestaña **Decisión** del
inspector muestra esta cuenta en vivo para cualquier residente.

### Dónde

Para cada actividad el motor busca un **lugar abierto** de la categoría adecuada: tiene en cuenta el horario de
apertura, si un evento lo ha cerrado, el aforo y la distancia. Cada residente tiene además **lugares favoritos**
estables (una preferencia personal que pesa como unos cientos de metros, más en los curiosos), y los eventos con
público (conciertos, mercadillos, fiestas) atraen a la gente cercana con tiempo libre.

### Cómo llega

El residente planifica la ruta (A* sobre la red de calles) según su transporte: a pie o en bici por aceras y calles
peatonales, en coche por calzadas (con congestión y semáforos) o en autobús (camina a la parada, espera, sube si hay
plazas y el autobús va en su sentido, se baja y camina; si espera más de 25 minutos, desiste y va andando).
Los trayectos cortos se hacen a pie y los menores no conducen. Los peatones caminan por la acera derecha, esperan
en los semáforos en rojo y van más despacio con lluvia o en zonas afectadas por eventos.

## Grupos y encuentros

- **Salir juntos**: al salir de casa hacia una actividad social, de ocio, deporte, compras o comer fuera, puede invitar
  a familiares o amigos que estén libres en el mismo edificio. Caminan juntos (*caminando y hablando*) al mismo ritmo.
- **Ir al colegio**: los niños pequeños van acompañados por un adulto de la familia, que hace *llevar al colegio* y vuelve.
- **Encuentros en la calle**: dos conocidos que se cruzan pueden pararse a **charlar** unos minutos (alivia la soledad);
  dos rivales agresivos pueden **discutir**, y a veces la discusión escala a una **pelea callejera**, un evento que
  el resto percibe y al que reacciona.
