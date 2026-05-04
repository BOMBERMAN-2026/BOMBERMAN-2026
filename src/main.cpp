// GL and GLFW
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <algorithm>
#include <cstdint>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "bomberman.hpp"
#include "audio_manager.hpp"

/*
 * main.cpp
 * -------
 * Punto de entrada.
 *
 * Responsabilidades:
 * - Inicializar GLFW/GLEW y crear la ventana.
 * - Construir `Game` y ejecutar el loop principal (input/update/render).
 * - Registrar el callback de teclado (ver README para controles).
 */

Game* bomberman;

extern int menuSelection;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void window_focus_callback(GLFWwindow* window, int focused);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

#ifdef _WIN32
static WNDPROC gPreviousWindowProc = nullptr;
static HWND gRawInputHwnd = nullptr;

LRESULT CALLBACK raw_input_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool setup_raw_input_for_mice(GLFWwindow* window);
void shutdown_raw_input_hook();
#endif

#include <ctime>

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    // ============================== Ventana / OpenGL ==============================

    // Initialize GLFW
    if (!glfwInit())
    {
        printf("GLFW Initialization failed!\n");
        return 1;
    }

    // Setup GLFW window properties
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Screen dimensions (Fullscreen)
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    
    // Configurar GLFW para usar la resolución actual del monitor en fullscreen
    const int SCREEN_WIDTH = mode->width;
    const int SCREEN_HEIGHT = mode->height;

    // Crear la ventana en modo fullscreen pasando el monitor
    GLFWwindow* mainWindow = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Bomberman", primaryMonitor, nullptr);
    if (!mainWindow)
    {
        printf("GLFW window creation failed!\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(mainWindow);
    glfwSetFramebufferSizeCallback(mainWindow, framebuffer_size_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        printf("GLEW Initialization failed!\n");
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(mainWindow, key_callback);
    glfwSetWindowFocusCallback(mainWindow, window_focus_callback);
    glfwSetScrollCallback(mainWindow, scroll_callback);

#ifdef _WIN32
    setup_raw_input_for_mice(mainWindow);
#endif

    // OpenGL configuration
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClearDepth(1.0f);

    // ============================== Juego ==============================
    bomberman = new Game(mainWindow, SCREEN_WIDTH, SCREEN_HEIGHT);
    // Cambia a GameMode::HistoryOnePlayer si quieres modo 1 jugador.
    bomberman->setMode(GameMode::HistoryTwoPlayers);
    bomberman->init();

    // Initialize blend to blend transparent to the background
    //glEnable(GL_BLEND);
    //glEnable(GL_DEPTH_TEST);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // inicializar juego, ticks?

    // ============================== Loop principal ==============================
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(mainWindow))
    {
        double currentTime = glfwGetTime();
        const float frameDelta = static_cast<float>(currentTime - lastTime);
        bomberman->deltaTime = std::max(0.0f, std::min(frameDelta, 0.05f));
        lastTime = currentTime;

        // Check and call events
        glfwPollEvents();

        // Pollear gamepads y traducir a teclas equivalentes.
        pollGamepads(bomberman->keys, bomberman->lastDirKey, bomberman->lastDirKeyP2);

        // Deberia estar dentro de un bucle de ticks?
        bomberman->processInput();
        // Deberia estar dentro de un bucle de ticks?
        bomberman->update();

        bomberman->render();
        
        // Swap buffers
		glfwSwapBuffers(mainWindow);
    }

    delete bomberman;
    bomberman = nullptr;

#ifdef _WIN32
    shutdown_raw_input_hook();
#endif

    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    return 0;
}

// Callback de teclado (GLFW). Guarda estado y recuerda “última dirección” por jugador.
// Controles completos: ver README.
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (bomberman == nullptr) {
        return;
    }

    if (key >= 0 && key < 1024) {
        bomberman->keys[key] = action;
        bomberman->lastKeyPressed = key;

        if (action == GLFW_PRESS) {
            // ========== INTRO: pasar al menú ==========
            if (bomberman->state == GAME_INTRO && key == GLFW_KEY_SPACE) {
                bomberman->state = GAME_MENU;
                bomberman->init(); // Cargar textura del menú
                return;
            }

            // ========== JUEGO NORMAL ==========
           // if (bomberman->state == GAME_PLAYING) {
           //     if (key == GLFW_KEY_4 || key == GLFW_KEY_KP_4) {
           //         AudioManager::get().registerPlaceBombSpecialTap();
           //     }
           // }

            if (key == bomberman->inGameMenu.controlsMenu.upKey_P1 || key == bomberman->inGameMenu.controlsMenu.downKey_P1 || key == bomberman->inGameMenu.controlsMenu.leftKey_P1 || key == bomberman->inGameMenu.controlsMenu.rightKey_P1) {
                bomberman->lastDirKey = key;
            }

            if (key == bomberman->inGameMenu.controlsMenu.upKey_P2 || key == bomberman->inGameMenu.controlsMenu.downKey_P2 || key == bomberman->inGameMenu.controlsMenu.leftKey_P2 || key == bomberman->inGameMenu.controlsMenu.rightKey_P2) {
                bomberman->lastDirKeyP2 = key;
            }
        }
    }
}

// Limpia el estado de teclas al perder foco para evitar entradas "pegadas".
void window_focus_callback(GLFWwindow* window, int focused) {
    if (focused || bomberman == nullptr) {
        return;
    }

    for (std::map<GLint, GLint>::iterator it = bomberman->keys.begin(); it != bomberman->keys.end(); ++it) {
        it->second = GLFW_RELEASE;
    }

    bomberman->lastDirKey = GLFW_KEY_UNKNOWN;
    bomberman->lastDirKeyP2 = GLFW_KEY_UNKNOWN;
    bomberman->resetRawMouseInputState(/*clearDeviceAssignments=*/false);
}

// Ajusta el viewport cuando cambia el tamaño de la ventana.
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    if (bomberman != nullptr) {
        bomberman->onResize(width, height);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;

    if (bomberman != nullptr) {
        bomberman->onMouseScroll(yoffset);
    }
}

#ifdef _WIN32
LRESULT CALLBACK raw_input_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT && bomberman != nullptr) {
        UINT dataSize = 0;
        const UINT headerSize = sizeof(RAWINPUTHEADER);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dataSize, headerSize) == 0 && dataSize > 0) {
            std::vector<BYTE> rawBuffer(dataSize);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawBuffer.data(), &dataSize, headerSize) == dataSize) {
                const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(rawBuffer.data());
                if (raw != nullptr && raw->header.dwType == RIM_TYPEMOUSE) {
                    const RAWMOUSE& mouse = raw->data.mouse;
                    const int deltaX = (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) ? 0 : static_cast<int>(mouse.lLastX);
                    const int deltaY = (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) ? 0 : static_cast<int>(mouse.lLastY);

                    const bool leftDown = (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0;
                    const bool leftUp = (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) != 0;
                    const bool rightDown = (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0;
                    const bool rightUp = (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) != 0;

                    if (deltaX != 0 || deltaY != 0 || leftDown || leftUp || rightDown || rightUp) {
                        const std::uint64_t deviceId = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(raw->header.hDevice));
                        bomberman->onRawMouseInput(deviceId,
                                                   deltaX,
                                                   deltaY,
                                                   leftDown,
                                                   leftUp,
                                                   rightDown,
                                                   rightUp);
                    }
                }
            }
        }
    }

    if (gPreviousWindowProc != nullptr) {
        return CallWindowProc(gPreviousWindowProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool setup_raw_input_for_mice(GLFWwindow* window) {
    if (window == nullptr) {
        return false;
    }

    gRawInputHwnd = glfwGetWin32Window(window);
    if (gRawInputHwnd == nullptr) {
        return false;
    }

    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01; // Generic desktop controls
    rid.usUsage = 0x02;     // Mouse
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = gRawInputHwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    gPreviousWindowProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(gRawInputHwnd,
                         GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(raw_input_wnd_proc)));

    return gPreviousWindowProc != nullptr;
}

void shutdown_raw_input_hook() {
    if (gRawInputHwnd != nullptr && gPreviousWindowProc != nullptr) {
        SetWindowLongPtr(gRawInputHwnd,
                         GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(gPreviousWindowProc));
    }

    gPreviousWindowProc = nullptr;
    gRawInputHwnd = nullptr;
}
#endif
