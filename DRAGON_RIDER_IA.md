# Dragon Rider: prototipo de IA táctica (UE 5.8)

**Modalidad elegida:** B, Escuadrón Táctico. Hay 6 dragones en la escena: 2 guardianes por nido × 3 especies.

## 1. GDD simplificado

El jugador entra a una **arena propia de 300 m × 300 m** (`Lvl_DragonRider`) con rocas de cobertura y **3 nidos gigantes**, uno por especie: el Volador en una plataforma elevada con rampa, el Saltarín rodeado de pilares-percha y el Impulsivo dentro de un círculo de rocas. Cada nido tiene 2 dragones guardianes. El jugador puede **robar un huevo** (se le queda pegado), lo que provoca a los guardianes. Con su ballesta de dardos los puede **aturdir o matar**. Si los dragones lo derrotan, reaparece en el inicio y el huevo vuelve al nido.

El proyecto **no usa nada del template Shooter**. El jugador, el GameMode, el HUD y el mapa son propios. Del template First Person solo se reutilizan el personaje base y las acciones de Enhanced Input.

La incubación, la crianza y la domesticación del GDD original **quedan fuera** de este prototipo. El robo del huevo se conserva solo porque dispara el comportamiento *"Buscar huevo robado"* del GDD.

| Especie (color) | Arquetipo táctico | Movilidad | Comportamientos exclusivos |
|---|---|---|---|
| **Volador** (rojo, con alas) | Tirador a distancia | Vuela (steering 3D, sin NavMesh) | Bola de fuego, rodear en el aire, ganar altura. **GOAP:** sin munición → vuela al nido a **recargar** |
| **Saltarín** (verde) | Flanqueador | Saltos y planeos (LaunchCharacter) | Embestida saltando, **flanquear** por la espalda, vigilar y retirarse a **perchas** (pilares) |
| **Impulsivo** (morado, grande) | Tanque / Berserker | Camina y carga | **Rugido** de advertencia (alerta a la manada y sube la furia), **embestida en carga**, **caída en picada** de área |

## 2. Cómo cumple cada punto de los lineamientos

| Requisito | Implementación |
|---|---|
| **3 StateTrees diferenciados** | `Content/DragonRider/AI/ST_DragonVolador`, `ST_DragonSaltarin`, `ST_DragonImpulsivo`. Cada árbol tiene ramas y acciones propias (no solo números distintos). |
| **Ciclo base compartido** | Todos tienen: `Idle` → `Patrullar` → `Vigilar nido`; `Buscar intruso`/`Combate` (detección y persecución vía AIPerception); `Aturdido (Reacción a daño)` con transición *Critical*. Del GDD también tienen `Regresar al nido` y `Buscar huevo robado`. |
| **Evaluators propios desacoplados** | `FDragonSensesEvaluator`: consulta AIPerception (vista, oído, daño) **cada 0.15 s con desfase aleatorio por dragón** (intervalos escalonados). Mide distancia, ángulo relativo, línea de visión, salud, munición/energía, furia y estado del objetivo (si lleva NUESTRO huevo). Sus salidas se ven en el StateTree Debugger. **No hay lógica de IA en Event Tick**: el Tick del actor solo gira el texto hacia la cámara. |
| **Cadenas de Markov ≥ 4×4** | `Content/DragonRider/Data/DT_Markov_*`: DataTables con filas `FDragonMarkovRow` (fila = estado origen, columnas = destino). **Cada fila suma 1.0**: el generador lo valida y en runtime se vuelve a validar y renormalizar con warning. |
| **Moduladores dinámicos** | La tarea `Dragon: Seleccion Markov` multiplica las probabilidades por *baja salud* (<35 %), *furia* (sube con el huevo robado, el daño y los rugidos) y *distancia* (objetivo lejos). Luego renormaliza a 1.0 y muestrea. |
| **Precondiciones GOAP** | Cada acción tiene reglas (`FDragonActionRule`): distancia mín./máx., energía/munición, línea de visión y furia máxima. Si una regla falla, p = 0. Además, el Volador con munición = 0 hace una **transición prioritaria** (High) hacia `Recargar fuego (GOAP)`. |
| **Claridad visual** | Color y silueta distintos por especie. Texto sobre cada dragón con la especie, el estado actual, HP, munición/estamina, furia y la última transición de Markov (`Origen->Destino p=0.45 [fila final]`). El color del texto indica el ánimo: blanco = tranquilo, amarillo = alerta, rojo = combate, azul = aturdido, naranja = GOAP/huevo. |
| **Log** | Cada decisión de Markov sale en el Output Log (`LogDragonAI`) con la fila final normalizada. |

### Estructura de cada StateTree
```
Root
 ├─ Aturdido (Reaccion a dano)         [Condición: Aturdido]
 └─ Vivo                               (OnTick Aturdido → Aturdido, Critical)
     ├─ Recargar fuego (GOAP)          [Energia < 1]            ← solo Volador
     ├─ Combate                        [Tiene objetivo]
     │    ├─ Advertencia (rugido)                               ← solo Impulsivo
     │    ├─ Decidir (Cadena de Markov)
     │    └─ Ejecutar accion → 4 acciones del arquetipo [Accion Markov == X]
     ├─ Buscar huevo robado            [Huevo robado]
     ├─ Regresar al nido               [Lejos del nido]
     ├─ Buscar intruso                 [Debe investigar]
     └─ Tranquilo: Idle → Patrullar → Vigilar nido / percha
```

## 3. Código (C++)

| Archivo | Qué hace |
|---|---|
| `Source/AI_GAME/DragonRider/DragonTypes.h` | Enums, fila de Markov, reglas GOAP y la Blackboard del dragón |
| `DragonStateTreeNodes.h/.cpp` | Evaluator, Condición, tareas Markov, Mover, Atacar, Esperar/Rugir, Recargar y Aturdido |
| `DragonCharacter.h/.cpp` | Dragón base y sus 3 subclases (stats, greybox, texto, daño y aturdimiento) |
| `DragonAIController.h/.cpp` | AIPerception (vista, oído, daño) y arranque del StateTree |
| `DragonNest.h/.cpp` | Nidos (plataforma y rampa, pilares-percha, rocas) y huevos |
| `DragonArena.h/.cpp` | Director (colocado en el mapa) que crea los nidos grandes y los guardianes |
| `DragonRiderGame.h/.cpp` | Jugador (ballesta), dardo, HUD, PlayerController (comando `DRNido`) y GameMode |
| `DragonFireball.h/.cpp` | Proyectil del Volador |
| `Source/AI_GAMEEditor/DragonSetupCommandlet.cpp` | **Generador** de los 3 StateTrees, las 3 DataTables y el mapa `Lvl_DragonRider` |

Para regenerar los assets, escribe en la consola del editor (`~`) `DragonRider.GenerateAssets`. También puedes correr:
`UnrealEditor-Cmd.exe AI_GAME.uproject -run=DragonSetup`

## 4. Demo para la evaluación

1. Abre el proyecto. Ya abre `Lvl_DragonRider` y todo está compilado.
2. Pulsa **Play**. Apareces en el centro de la arena. Controles: WASD + ratón, espacio para saltar, clic izquierdo para disparar.
   - Atajo de demo: en la consola (`~`) escribe `DRNido 0` (Volador), `DRNido 1` (Impulsivo) o `DRNido 2` (Saltarín) para aparecer frente a ese nido.
3. Abre el StateTree Debugger: menú **Tools → Debug → StateTree Debugger**. Pulsa *Start Recording*, elige un `DragonAIController_X` y observa las transiciones y los valores del Evaluator.
4. Acércate a un nido y roba el huevo (tócalo) para ver "Buscar huevo robado" y la furia. Dispara a un dragón varias veces para provocar el estado Aturdido. Al Volador déjalo disparar 4 veces para ver la recarga GOAP.
5. Para enseñar los FPS, escribe `stat fps` en la consola.
