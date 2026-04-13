#ifndef GAMEPAD_INPUT_HPP
#define GAMEPAD_INPUT_HPP

#include <GLFW/glfw3.h>
#include <map>

/*
 * gamepad_input.hpp
 * -----------------
 * Soporte de gamepad (DualShock 4 / cualquier mando SDL-compatible) para Bomberman.
 *
 * Diseño: "inyección transparente".
 * Este módulo lee el estado de los gamepads conectados cada frame y traduce
 * sus ejes y botones a las mismas teclas de teclado que ya usa el juego.
 * El resto del código (processInput, Player, menús) no necesita modificarse.
 *
 * Mapeo por defecto (estándar SDL / GLFW_GAMEPAD_*):
 *   Gamepad 1 → Jugador 1 (flechas + RCTRL/RALT)
 *   Gamepad 2 → Jugador 2 (WASD + X/Z)
 */

/// Deadzone para el stick analógico (evita drift del joystick).
static constexpr float GAMEPAD_DEADZONE = 0.30f;

/// Número máximo de gamepads gestionados (P1 + P2).
static constexpr int MAX_GAMEPADS = 2;

/**
 * Pollea todos los gamepads conectados, traduce su estado a teclas
 * equivalentes y las inyecta en el mapa `keys`.
 *
 * Debe llamarse UNA VEZ por frame, después de `glfwPollEvents()` y
 * antes de `Game::processInput()`.
 *
 * @param keys          Mapa de estado de teclas del Game (modificado in-place).
 * @param lastDirKey    Última dirección de P1 (flechas). Se actualiza si el gamepad 1 envía input.
 * @param lastDirKeyP2  Última dirección de P2 (WASD). Se actualiza si el gamepad 2 envía input.
 */
void pollGamepads(std::map<GLint, GLint>& keys, GLint& lastDirKey, GLint& lastDirKeyP2);

#endif // GAMEPAD_INPUT_HPP
