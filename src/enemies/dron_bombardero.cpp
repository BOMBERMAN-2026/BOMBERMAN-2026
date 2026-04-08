#include "enemies/dron_bombardero.hpp"
#include "game_map.hpp"
#include <cstdlib>
#include <cmath>

DronBombardero::DronBombardero(glm::vec2 pos, glm::vec2 size, float speed)
    : Enemy(pos, size, speed, /*hp=*/3, /*score=*/2000),
      preferredDistance(0.35f),
      bombDropRange(0.25f),
      bombExplosionRange(2),
      fireballCooldown(0.0f),
      fireballCooldownMax(4.0f),
      onFire(false),
      fireTimer(0.0f),
      fireDuration(5.0f),
      fireCycleCooldown(15.0f),
      fireCycleTimer(15.0f),
      bombCooldown(0.0f),
      bombCooldownMax(3.0f)
{
    facing = randomDirection();
}

DronBombardero::~DronBombardero() {}

// Entra en modo fuego (inmunidad temporal) y lanza una ráfaga inicial.
void DronBombardero::enterFireMode() {
    onFire = true;
    fireTimer = fireDuration;
    // Al prenderse, lanza bolas de fuego en las 4 direcciones
    shootFireballAllDirections();
}

// Mantiene el modo fuego y lo desactiva al terminar.
void DronBombardero::updateFireMode() {
    fireTimer -= deltaTime;
    if (fireTimer <= 0.0f) {
        onFire = false;
        fireTimer = 0.0f;
    }
}

// Coloca una bomba en la posición actual (pendiente).
// TODO: Instanciar una bomba con explosionRange = bombExplosionRange.
void DronBombardero::dropBomb() {
    bombCooldown = bombCooldownMax;
}

// Dispara una bola de fuego en la dirección dada (pendiente).
// TODO: Instanciar un proyectil con rango infinito.
void DronBombardero::shootFireball(EnemyDirection dir) {
    (void)dir;
}

// Dispara bolas de fuego en las 4 direcciones (pendiente).
void DronBombardero::shootFireballAllDirections() {
    shootFireball(EnemyDirection::UP);
    shootFireball(EnemyDirection::DOWN);
    shootFireball(EnemyDirection::LEFT);
    shootFireball(EnemyDirection::RIGHT);
}

// IA: mantiene distancia preferida, suelta bombas en rango y dispara fuego.
void DronBombardero::Update() {
    if (lifeState != EnemyLifeState::Alive) return;

    float step = speed * deltaTime;
    float dist = distanceToPlayer();

    // Actualizar cooldowns
    if (bombCooldown > 0.0f) bombCooldown -= deltaTime;
    if (fireballCooldown > 0.0f) fireballCooldown -= deltaTime;
    fireCycleTimer -= deltaTime;

    // Ciclo de fuego periódico
    if (fireCycleTimer <= 0.0f && !onFire) {
        enterFireMode();
        fireCycleTimer = fireCycleCooldown;
    }

    // Actualizar estado de fuego
    if (onFire) {
        updateFireMode();
        // Mientras está en fuego, sigue moviéndose pero es inmortal
    }

    // IA de persecución con distancia preferida
    if (dist > preferredDistance) {
        // Acercarse al jugador
        EnemyDirection toPlayer = directionTowardPlayer();
        if (!tryMove(toPlayer, step)) {
            // Intentar ruta alternativa
            EnemyDirection alt = randomDirection();
            tryMove(alt, step);
        }
    } else if (dist < preferredDistance * 0.5f) {
        // Demasiado cerca, retroceder
        EnemyDirection away = oppositeDirection(directionTowardPlayer());
        if (!tryMove(away, step)) {
            tryMove(randomDirection(), step);
        }
    }

    // Soltar bomba si está en rango
    if (dist <= bombDropRange && bombCooldown <= 0.0f) {
        dropBomb();
    }

    // Disparar bola de fuego ocasionalmente
    if (fireballCooldown <= 0.0f && dist < 0.6f) {
        EnemyDirection toPlayer = directionTowardPlayer();
        shootFireball(toPlayer);
        fireballCooldown = fireballCooldownMax;
    }
}

// Render del enemigo (pendiente).
// TODO: Dibujar sprite del dron (y efecto fuego si onFire).
void DronBombardero::Draw() {
}
