#ifndef POWER_UP_HPP
#define POWER_UP_HPP

/*
 * power_up.hpp
 * ------------
 * Tipos de power-up y constantes de balance del Arcade original (1991).
 *
 * Este header define únicamente el enum y los topes (caps).
 * La lógica de aplicación reside en Player::applyPowerUp().
 * La colocación en el mapa se hará en una fase posterior.
 */

enum class PowerUpType {
    ExtraLife,       // 1-UP: +1 vida
    BombUp,          // Bomb Up: +1 bomba máxima simultánea
    FireUp,          // Fire Up: +1 radio de explosión
    SpeedUp,         // Speed Up: incrementa velocidad
    Invincibility,   // Invincibility: 16 segundos de invulnerabilidad
    RemoteControl    // Remote Control: detona bombas a voluntad (se pierde al morir)
};

// Topes y balance fieles al Arcade original.
namespace ArcadeCaps {
    constexpr int   MAX_BOMBS          = 8;
    constexpr int   MAX_FIRE_POWER     = 10;
    constexpr float MAX_SPEED          = 0.8f;
    constexpr float SPEED_INCREMENT    = 0.05f;   // Incremento por cada Speed Up
    constexpr float INVINCIBILITY_TIME = 16.0f;   // Segundos de invulnerabilidad
}

#endif // POWER_UP_HPP
