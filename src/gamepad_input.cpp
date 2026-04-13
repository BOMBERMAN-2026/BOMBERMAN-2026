
#include "gamepad_input.hpp"
#include <cmath>

/*
 * gamepad_input.cpp
 * -----------------
 * Implementación del polling de gamepads.
 *
 * Estrategia:
 * 1. Cada frame se consulta el estado de GLFW_JOYSTICK_1..N.
 * 2. Se usa glfwGetGamepadState() (GLFW 3.3+, mapeo estándar SDL).
 *    Si el joystick no tiene mapeo de gamepad (caso raro), se intenta
 *    leer ejes/botones crudos como fallback.
 * 3. Se traduce D-Pad + stick izquierdo → teclas de dirección.
 *    Se traduce botones → teclas de acción.
 * 4. Para generar flancos correctos (PRESS vs REPEAT), se mantiene el
 *    estado previo de cada botón por gamepad.
 *
 * Nota: este módulo es sin estado externo; todo el estado vive en
 * variables estáticas locales al TU.
 */

// =============================================================================
// Estado previo por gamepad (para detectar PRESS vs REPEAT / RELEASE).
// =============================================================================

/// Estado booleano de cada "botón virtual" por gamepad en el frame anterior.
/// Índices: [gamepadIndex][virtualButton].
/// Los "botones virtuales" incluyen tanto botones reales como direcciones
/// derivadas del stick/D-Pad.
enum VirtualButton {
    VB_DPAD_UP = 0,
    VB_DPAD_DOWN,
    VB_DPAD_LEFT,
    VB_DPAD_RIGHT,
    VB_CROSS,       // × — poner bomba
    VB_CIRCLE,      // ○ — detonar (Remote Control)
    VB_TRIANGLE,    // △ — saltar intro (SPACE)
    VB_START,       // START/Options — confirmar (ENTER)
    VB_BACK,        // SHARE/Back — fullscreen (TAB)
    VB_COUNT
};

static bool prevState[MAX_GAMEPADS][VB_COUNT] = {};

// =============================================================================
// Helpers
// =============================================================================

/// Inyecta una tecla en el mapa `keys` respetando flancos:
///  - Si `pressed` y no estaba pulsada antes → GLFW_PRESS
///  - Si `pressed` y ya estaba pulsada       → no toca (deja REPEAT o lo que haya)
///  - Si no `pressed`                        → no toca (el teclado podría tenerla pulsada)
static void injectKey(std::map<GLint, GLint>& keys, int glfwKey, bool pressed, bool wasPressedPrev)
{
    if (pressed) {
        // Solo inyectar PRESS si la tecla ya NO está pulsada por teclado ni por
        // un PRESS previo del gamepad (evitar sobrescribir un PRESS de teclado).
        const GLint currentVal = keys[glfwKey];
        if (!wasPressedPrev && currentVal != GLFW_PRESS && currentVal != GLFW_REPEAT) {
            keys[glfwKey] = GLFW_PRESS;
        }
        // Si ya estaba pulsada (por gamepad anterior o teclado), no tocamos.
        // processInput ya convierte PRESS → REPEAT tras consumir el evento.
    }
    // Si no está pulsada en el gamepad, NO forzamos RELEASE: el teclado podría
    // tener esa tecla pulsada. Solo el callback de teclado gestiona RELEASE.
}

// =============================================================================
// Polling principal
// =============================================================================

void pollGamepads(std::map<GLint, GLint>& keys, GLint& lastDirKey, GLint& lastDirKeyP2)
{
    // Teclas de dirección por slot de jugador.
    // Gamepad 0 → P1 (flechas), Gamepad 1 → P2 (WASD).
    static const GLint dirKeys[MAX_GAMEPADS][4] = {
        // UP, DOWN, LEFT, RIGHT
        { GLFW_KEY_UP,  GLFW_KEY_DOWN,  GLFW_KEY_LEFT,  GLFW_KEY_RIGHT },
        { GLFW_KEY_W,   GLFW_KEY_S,     GLFW_KEY_A,     GLFW_KEY_D     }
    };
    // Teclas de acción por slot.
    //   [0] = bomba,  [1] = detonar,  [2] = space (intro),  [3] = enter (menú),  [4] = tab (fullscreen)
    static const GLint actionKeys[MAX_GAMEPADS][5] = {
        { GLFW_KEY_RIGHT_CONTROL, GLFW_KEY_RIGHT_ALT, GLFW_KEY_SPACE,     GLFW_KEY_ENTER, GLFW_KEY_TAB },
        { GLFW_KEY_X,             GLFW_KEY_Z,          GLFW_KEY_SPACE,     GLFW_KEY_ENTER, GLFW_KEY_TAB }
    };

    for (int gp = 0; gp < MAX_GAMEPADS; ++gp) {
        const int jid = GLFW_JOYSTICK_1 + gp;

        if (!glfwJoystickPresent(jid)) {
            // Gamepad desconectado: resetear estado previo para evitar
            // un "PRESS fantasma" al reconectar.
            for (int i = 0; i < VB_COUNT; ++i) {
                prevState[gp][i] = false;
            }
            continue;
        }

        // ---- Leer estado actual del gamepad ----
        bool curState[VB_COUNT] = {};

        GLFWgamepadstate state;
        if (glfwJoystickIsGamepad(jid) && glfwGetGamepadState(jid, &state)) {
            // ===== Ejes: stick izquierdo =====
            const float lx = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
            const float ly = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

            curState[VB_DPAD_UP]    = (ly < -GAMEPAD_DEADZONE);
            curState[VB_DPAD_DOWN]  = (ly >  GAMEPAD_DEADZONE);
            curState[VB_DPAD_LEFT]  = (lx < -GAMEPAD_DEADZONE);
            curState[VB_DPAD_RIGHT] = (lx >  GAMEPAD_DEADZONE);

            // ===== D-Pad (OR con stick) =====
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]    == GLFW_PRESS) curState[VB_DPAD_UP]    = true;
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]  == GLFW_PRESS) curState[VB_DPAD_DOWN]  = true;
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]  == GLFW_PRESS) curState[VB_DPAD_LEFT]  = true;
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS) curState[VB_DPAD_RIGHT] = true;

            // ===== Botones de acción =====
            curState[VB_CROSS]    = (state.buttons[GLFW_GAMEPAD_BUTTON_A]     == GLFW_PRESS);  // × (Cross) en PS4 = A en SDL
            curState[VB_CIRCLE]   = (state.buttons[GLFW_GAMEPAD_BUTTON_B]     == GLFW_PRESS);  // ○ (Circle)  = B en SDL
            curState[VB_TRIANGLE] = (state.buttons[GLFW_GAMEPAD_BUTTON_Y]     == GLFW_PRESS);  // △ (Triangle) = Y en SDL
            curState[VB_START]    = (state.buttons[GLFW_GAMEPAD_BUTTON_START]  == GLFW_PRESS);  // OPTIONS/START
            curState[VB_BACK]     = (state.buttons[GLFW_GAMEPAD_BUTTON_BACK]  == GLFW_PRESS);  // SHARE/SELECT/BACK

        } else {
            // ===== Fallback: joystick sin mapeo SDL =====
            // Intentar leer ejes/botones crudos.
            int axisCount = 0;
            const float* axes = glfwGetJoystickAxes(jid, &axisCount);

            if (axes && axisCount >= 2) {
                curState[VB_DPAD_LEFT]  = (axes[0] < -GAMEPAD_DEADZONE);
                curState[VB_DPAD_RIGHT] = (axes[0] >  GAMEPAD_DEADZONE);
                curState[VB_DPAD_UP]    = (axes[1] < -GAMEPAD_DEADZONE);
                curState[VB_DPAD_DOWN]  = (axes[1] >  GAMEPAD_DEADZONE);
            }

            int btnCount = 0;
            const unsigned char* btns = glfwGetJoystickButtons(jid, &btnCount);
            if (btns) {
                if (btnCount > 0) curState[VB_CROSS]    = (btns[0] == GLFW_PRESS);
                if (btnCount > 1) curState[VB_CIRCLE]   = (btns[1] == GLFW_PRESS);
                if (btnCount > 2) curState[VB_TRIANGLE] = (btns[2] == GLFW_PRESS);
                if (btnCount > 7) curState[VB_START]    = (btns[7] == GLFW_PRESS);
                if (btnCount > 6) curState[VB_BACK]     = (btns[6] == GLFW_PRESS);

                // D-Pad como botones (algunos drivers PS4 lo reportan así).
                if (btnCount > 14) {
                    if (btns[11] == GLFW_PRESS) curState[VB_DPAD_UP]    = true;
                    if (btns[12] == GLFW_PRESS) curState[VB_DPAD_DOWN]  = true;
                    if (btns[13] == GLFW_PRESS) curState[VB_DPAD_LEFT]  = true;
                    if (btns[14] == GLFW_PRESS) curState[VB_DPAD_RIGHT] = true;
                }
            }
        }

        // ---- Inyectar direcciones ----
        injectKey(keys, dirKeys[gp][0], curState[VB_DPAD_UP],    prevState[gp][VB_DPAD_UP]);
        injectKey(keys, dirKeys[gp][1], curState[VB_DPAD_DOWN],  prevState[gp][VB_DPAD_DOWN]);
        injectKey(keys, dirKeys[gp][2], curState[VB_DPAD_LEFT],  prevState[gp][VB_DPAD_LEFT]);
        injectKey(keys, dirKeys[gp][3], curState[VB_DPAD_RIGHT], prevState[gp][VB_DPAD_RIGHT]);

        // Actualizar lastDirKey del jugador correspondiente.
        GLint* dirKeyPtr = (gp == 0) ? &lastDirKey : &lastDirKeyP2;
        if (curState[VB_DPAD_UP]   && !prevState[gp][VB_DPAD_UP])    *dirKeyPtr = dirKeys[gp][0];
        if (curState[VB_DPAD_DOWN] && !prevState[gp][VB_DPAD_DOWN])  *dirKeyPtr = dirKeys[gp][1];
        if (curState[VB_DPAD_LEFT] && !prevState[gp][VB_DPAD_LEFT])  *dirKeyPtr = dirKeys[gp][2];
        if (curState[VB_DPAD_RIGHT]&& !prevState[gp][VB_DPAD_RIGHT]) *dirKeyPtr = dirKeys[gp][3];

        // ---- Inyectar botones de acción ----
        injectKey(keys, actionKeys[gp][0], curState[VB_CROSS],    prevState[gp][VB_CROSS]);
        injectKey(keys, actionKeys[gp][1], curState[VB_CIRCLE],   prevState[gp][VB_CIRCLE]);
        injectKey(keys, actionKeys[gp][2], curState[VB_TRIANGLE], prevState[gp][VB_TRIANGLE]);
        injectKey(keys, actionKeys[gp][3], curState[VB_START],    prevState[gp][VB_START]);
        injectKey(keys, actionKeys[gp][4], curState[VB_BACK],     prevState[gp][VB_BACK]);

        // ---- Gestionar RELEASE: si un botón del gamepad se suelta Y el teclado
        //      no tiene esa tecla pulsada, forzar RELEASE. ----
        // Direcciones
        for (int d = 0; d < 4; ++d) {
            const int vb = VB_DPAD_UP + d;
            if (prevState[gp][vb] && !curState[vb]) {
                // El gamepad liberó esta dirección.
                // Solo liberar en keys[] si el valor actual parece venir del gamepad
                // (no hay forma perfecta de saberlo, pero si está en PRESS o REPEAT
                //  y el teclado no la mantiene, hay que limpiar).
                GLint& kv = keys[dirKeys[gp][d]];
                if (kv >= GLFW_PRESS) {
                    kv = GLFW_RELEASE;
                }
            }
        }
        // Botones de acción
        const int actionVBs[] = { VB_CROSS, VB_CIRCLE, VB_TRIANGLE, VB_START, VB_BACK };
        for (int a = 0; a < 5; ++a) {
            if (prevState[gp][actionVBs[a]] && !curState[actionVBs[a]]) {
                GLint& kv = keys[actionKeys[gp][a]];
                if (kv >= GLFW_PRESS) {
                    kv = GLFW_RELEASE;
                }
            }
        }

        // ---- Guardar estado actual como previo ----
        for (int i = 0; i < VB_COUNT; ++i) {
            prevState[gp][i] = curState[i];
        }
    }
}
