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

void DronBombardero::enterFireMode() {
    onFire = true;
    fireTimer = fireDuration;
    // Al prenderse, lanza bolas de fuego en las 4 direcciones
    shootFireballAllDirections();
}

void DronBombardero::updateFireMode() {
    fireTimer -= deltaTime;
    if (fireTimer <= 0.0f) {
        onFire = false;
        fireTimer = 0.0f;
    }
}

void DronBombardero::dropBomb() {
    // TODO: Instanciar una bomba en la posición actual con explosionRange = bombExplosionRange
    bombCooldown = bombCooldownMax;
}

void DronBombardero::shootFireball(EnemyDirection dir) {
    // TODO: Instanciar un proyectil de bola de fuego en la dirección dada con rango infinito
    (void)dir;
}

void DronBombardero::shootFireballAllDirections() {
    shootFireball(EnemyDirection::UP);
    shootFireball(EnemyDirection::DOWN);
    shootFireball(EnemyDirection::LEFT);
    shootFireball(EnemyDirection::RIGHT);
}

void DronBombardero::Update() {
    if (!alive) return;

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

void DronBombardero::Draw() {
    // TODO: Renderizar sprite del Dron bombardero (con efecto de fuego si onFire)
}
