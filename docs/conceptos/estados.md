# Estados y actividades

Cada residente está **siempre** en exactamente una **actividad principal** y uno de **sus subestados**.
La lista completa, generada desde los datos, está en [Actividades (generado)](../referencia/generado/actividades.md).

| Actividad | Ejemplos de subestados |
|---|---|
| Durmiendo | sueño nocturno, siesta, sueño inquieto |
| En casa | descansando, cocinando, comiendo, viendo la tele, tareas, aseo, leyendo, videojuegos, llamada |
| Desplazándose | caminando, **caminando y hablando**, **paseando**, con prisa, en bici, esperando el autobús, en autobús, conduciendo, atascado, esperando para cruzar |
| Trabajando | en su puesto, reunión, turno, atendiendo al público, dando clase, teletrabajo, pausa café, pausa comida |
| Estudiando | en clase, recreo, biblioteca, deberes, examen |
| Compras | supermercado, mercado, centro comercial, tienda, farmacia, haciendo cola, escaparates |
| Comiendo fuera | desayuno en cafetería, café, restaurante, tomando algo, tapas |
| Ocio | parque, pasear al perro, cine, museo, teatro, fiesta, biblioteca, turismo, espectáculo callejero |
| Deporte | gimnasio, deporte en equipo, correr, yoga en el parque, ruta en bici |
| Vida social | charlando, quedada en grupo, visitando a un amigo, tiempo en familia, discutiendo |
| Cuidados | llevar a los niños al colegio, cuidar a un familiar, cuidar niños, acompañar a una cita |
| Salud | médico, hospital, enfermo en casa |
| Recados | banco, trámites, oficio religioso, hotel |
| Emergencia | observando, curioseando, huyendo, evacuando, refugiándose, ayudando, persiguiendo |

## Cómo se elige el subestado

Los subestados se describen en `data/activities.json` y el motor los elige así:

1. **Ligados a un lugar** (`"venue"`): si la actividad ocurre en una cafetería, el subestado será *café* o *desayuno*;
   en una oficina, *en su puesto* o *reunión*.
2. **Genéricos**: se eligen con un peso que depende de la hora (`"hours"`), de las necesidades (`"needs"`: con hambre
   gana *comiendo en casa*) y de la personalidad (`"traits"`: los curiosos leen más).
3. **Automáticos** (`"auto": true`): los asigna el motor por contexto. Ejemplos:
    - *caminando y hablando*: el residente va en **grupo** (familia o amigos que salen juntos, o un adulto llevando a un niño al colegio);
    - *paseando*: va a una actividad de ocio o social sin prisa;
    - *con prisa*: llega tarde a una obligación de su agenda;
    - *esperando para cruzar*: semáforo en rojo; *atascado*: el tramo está congestionado;
    - *charlando* / *discutiendo*: se ha encontrado en la calle con un conocido / con un rival;
    - subestados de emergencia: reacción a un evento.

Dentro de una misma actividad el subestado cambia con el tiempo (por ejemplo, *cocinando → comiendo → viendo la tele*).
Cada cambio queda en el **historial** del residente.

## Añadir actividades o subestados

Se editan en `data/activities.json` sin tocar C++ (ver [Formatos de datos](../datos/index.md)). Solo hay que añadir
el nombre en `data/i18n/es.json` y `en.json`; una prueba automática avisa si falta alguna traducción.
