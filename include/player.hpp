#ifndef PLAYER_H
#define PLAYER_H

#include "entity.hpp"
#include "power_up.hpp"
#include <string>

/*
 * player.hpp
 * ----------
 * Jugador controlable (posición + movimiento + colisión con el mapa).
 *
 * - El render del jugador actualmente se gestiona desde `bomberman.cpp` usando un sprite atlas.
 * - Esta clase se centra en movimiento/colisiones (y deja el dibujo para el sistema de render actual).
 */

class GameMap;

// Estado de vida del jugador.
enum class PlayerLifeState {
    Alive,           // Se mueve y responde a inputs.
    DyingByEnemy,    // Muerte por enemigo: "jugador(color).muerto.N".
    DyingByExplosion,// Muerte por explosión: "jugador.muerto.quemado.N".
    Winning          // Animación de victoria y salida de nivel.
};

enum Move {
    MOVE_UP,    // Arriba
    MOVE_LEFT,  // Izquierda
    MOVE_DOWN,  // Abajo
    MOVE_RIGHT, // Derecha
    MOVE_NONE   // Sin movimiento
};

class Player : public Entity {
    public:
        // Animación
        std::string spritePrefix;      // Prefijo base (p.ej. "jugadorblanco")
        std::string currentSpriteName; // Sprite actual en el atlas
        float flipX;                   // 0.0 normal, 1.0 espejo horizontal
        GLint facingDirKey;            // Última dirección (tecla) para orientar sprite
        bool isWalking;                // true si está caminando (animación de andar)
        float walkTimer;               // Acumulador para alternar frames de andar
        int walkPhase;                 // Fase 0..3 (se traduce a frame 0/1/2)

        float deltaTime;               // Tiempo entre frames (segundos)

        int playerId = 0;               // Identificador estable (0=P1, 1=P2)

        // === Power-Up Stats (Arcade 1991) ===
        int maxBombs = 1;                // Bombas simultáneas permitidas (cap: 8)
        int activeBombs = 0;             // Bombas activas en el mapa (contador O(1))
        int explosionPower = 2;          // Radio de explosión (cap: 10)
        float baseSpeed;                 // Velocidad base actual (cap: 0.8f)
        int lives = 3;                   // Vidas

        // Invulnerabilidad ("Armadura" o spawn/respawn)
        bool invincible = false;                 // Si true, el jugador no puede morir por contacto/explosión
        float invincibilityTimer = 0.0f;         // Tiempo restante (segundos)
        float invincibilityTotalSeconds = 0.0f;  // Duración total activada (segundos). Útil para VFX/UI.
        bool invincibilityFromPowerUp = false;   // true si viene del power-up "Armadura" (16s)

        // Remote Control
        bool hasRemoteControl = false; // true si el jugador puede detonar bombas manualmente

        // Vida / respawn
        PlayerLifeState lifeState = PlayerLifeState::Alive; // Estado de vida
        glm::vec2 spawnPosition;                          // Punto de respawn (por ahora: el spawn inicial)
        float deathTimer = 0.0f;                          // Acumulador para frames de muerte
        int deathFrame = 0;                               // Frame actual de muerte
        bool pendingRespawn = false;                      // true si terminó animación y toca respawnear

        // Vida y animaciones de victoria
        bool hasFinishedWinning = false; // true cuando terminó la animación de victoria
        float winTimer = 0.0f; // Temporizador interno de la secuencia de victoria
        int winPhase = 0; // 0 = Creciendo/Pose V, 1 = Girando y saliendo
        float winScale = 1.8f; // Escala dinámica durante la victoria
        glm::vec2 winVelocity = glm::vec2(0.0f); // Dirección aleatoria de salida al ganar
        glm::vec2 winStartPosition = glm::vec2(0.0f); // Posición inicial al ganar para fijar sprites
        // Activa la animación de victoria y prepara la salida de nivel.
        void startWinning();

        // Avanza la animación de victoria y marca el final cuando termina.
        void updateWinningAnimation();

        Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity, int playerId = 0, const std::string& prefix = "jugadorblanco");
        ~Player() override;

        // Tick de lógica
        void Update() override;
        
        // Render del jugador (llamado desde `bomberman.cpp`).
        void Draw() override;

        // Intenta mover el jugador un paso en la dirección indicada.
        // Aplica colisión con el mapa (`GameMap`) y actualiza `position` si se puede mover.
        void UpdateSprite(Move mov, const GameMap* map, float deltaTime);

        // Devuelve true si el jugador está vivo y puede recibir input.
        bool isAlive() const { return lifeState == PlayerLifeState::Alive; }

        // Devuelve true si aún puede colocar otra bomba activa.
        bool canPlaceBomb() const { return activeBombs < maxBombs; }

        // Devuelve true si ya no le quedan vidas.
        bool isGameOver() const { return lives <= 0; }

        // Devuelve true cuando la animación de muerte ya terminó.
        bool isDeathAnimationFinished() const;

        // Aplica un power-up (respeta ArcadeCaps).
        void applyPowerUp(PowerUpType type);

        // Mata al jugador por contacto con enemigo (usa "jugador(color).muerto.N").
        void killByEnemy();

        // Mata al jugador por explosión (usa "jugador.muerto.quemado.N").
        void killByExplosion();

        // Vuelve al spawn y restaura estado de movimiento/animación.
        void respawn();

    private:
        void updateAnimation();
        void updateDeathAnimation();
        void setSpriteFromDirAndFrame(GLint dirKey, int frameIndex);
};

#endif // PLAYER_H
