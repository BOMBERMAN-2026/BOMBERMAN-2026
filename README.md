
-------------------------------------------------------------
							BOMBERMAN
-------------------------------------------------------------

Bomberman es un juego de acción y puzzle de estilo arcade en el que el jugador se enfrenta a enemigos controlados por la CPU dentro de un laberinto con bloques destructibles e indestructibles. Cada fase presenta un escenario cerrado donde la colocación estratégica de bombas y el control del espacio son esenciales para sobrevivir y completar el nivel.

Para nuestra version del juego, además de poder enfrentarte al modo historia con 1 o 2 players, se puede acceder a un modo VERSUS característico de la versión japonesa del juego, donde solo podían jugar 2 personas, a diferencia de la versión occidental de 4 jugadores fijos.

Como añadido, se ha implementado un modo CUSTOM, donde puedes escoger las reglas del juego por ti mismo, escogiendo donde jugar, cuanto tiempo y hasta qué enemigos y su cantidad a superar en él.

-------------------------------------------------------------
							DISTRIBUCÓN DE CARPETAS
-------------------------------------------------------------

El desarrollo del juego se ha llevado a cabo en las diferentes carpetas que se nombran mas abajo.

- **CMakeLists.txt** => compilación del proyecto (CMake). Es conveniente crearse una carpeta build en la raiz del directorio y ejecutar desde ahí "cmake .." para realizar una compilación más limpia y no ensuciar la raiz del proyecto
- **build/** => carpeta generada por CMake con artefactos de compilación (“clean build” para borrar en caso de querer compilar de 0).
- **dlls/** => carpeta donde se encuentran todas las librerias para poder ejecutar el juego en cualquier dispositivo windows de manera 100% portable (para linux o bien instalas dependencias o mediante wine tambíen se podria ejecutar)
- **docs** => documentos relacionados con entregas de la asignatura (GDD, memoria, video promocional, etc)
- **include/** => cabeceras del juego (.hpp).
  - **include/enemies/** => cabeceras de los enemigos.
  - **include/external/** => librerías header-only (por ejemplo stb).
- **levels/** => niveles en texto (`level_XX.txt`, `level_cg_XX.txt`, `level_vs_XX.txt`).
- **lib/** => librerías externas del proyecto (headers de OpenGL/GLFW/GLM, etc.).
- **models/** => modelos 3d para el juego
- **resources/** => assets del juego.
  - **resources/keyBindings/** => controles guardados para la siguiente carga de juego (se pueden modificar desde el menu inGame)
  - **resources/rankings/** => listado de las últimas 8 puntuaciones más altas
  - **resources/sounds/** => música y efectos.
  - **resources/sprites/** => sprites/atlases.
  - **resources/video/** => videos para las diferentes cinematicas y paso entre niveles
- **shaders/** => shaders GLSL.
- **src/** => código fuente (.cpp). Aqui se encuentra todo el código fuente del proyecto
  - **src/enemies/** => código fuente de los enemigos
- **tools/** => utilidades/herramientas (programas auxiliares).

-------------------------------------------------------------
							CONTROLES
-------------------------------------------------------------

Esta es la distribución inicial de los controles, los que estan marcados con el signo `%%` se pueden modificar en el menu inGame para una mayor personalización

- **Intro**
  - `ESPACIO`: saltarse la intro e ir al menú.

- **Menú**
  - %% `↑/↓`: mover selección.
  - %% `ENTER`: confirmar (1P/2P).
  - `ESC`: salir del juego.

- **Menú Custom game 1**
  - `↑/↓`: mover selección.
  - `ENTER`: alternar la opción y pasar a siguiente pantalla en `Next`.
  - `ESC`: Volver a Menú.

- **Menú Custom game 2**
  - `<-/->`: cambiar de enemigo.
  - `↑/↓`: aumentar/disminuir número de enemigos.
  - `ENTER`: Confirmar.
  - `ESC`: Volver a Menú Custom Game 1.

- **Cinematica**
  - `ESPACIO`: Saltar.

- **Ranking**
  - `ESC`: Saltar.
  - `↑/↓`: aumentar/disminuir caracter a escribir.
  - %% `ENTER`: Seleccionar caracter.

- **Juego**
  - **Jugador 1**: %% `↑/↓/←/→` mover.
    - **Jugador 1**: %% `CTRL derecho` poner bomba.
    - **Jugador 1**: %% `ALT derecho` detonar (requiere Remote Control).
  - **Jugador 2**: %% `W/A/S/D` mover.
    - **Jugador 2**: %% `X` poner bomba.
    - **Jugador 2**: %% `Z` detonar (requiere Remote Control).
  - %% `TAB`: alternar ventana/pantalla completa.
  - %% `1`: alternar vista 2D/3D.
  - %% `2`: cambiar tipo de cámara 3D.
  - En modo 3D, pulsar `3` dos veces rápido: revelar/cargar fondo sorpresa.

  - **Modo 3D**
    - **Ratón**: `holc click izq` mover la direccion en la que apunta la camara
    - `I/J/K/L` mover la camara en el espacio

- **Debug/Testing**
  - `F3`: forzar avance al siguiente nivel (sin limpiar enemigos).
