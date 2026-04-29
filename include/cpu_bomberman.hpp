#ifndef CPU_BOMBERMAN_HPP
#define CPU_BOMBERMAN_HPP

#include <array>
#include <string>
#include <vector>

#include "bomberman.hpp" // GameMode
#include "cpu_bomberman_difficulty.hpp"
#include "enemy.hpp"     // Enemy
#include "player.hpp"    // Player, Move

class GameMap;

namespace CpuBomberman {


// Relación del Bomberman CPU con el equipo de jugadores humanos.
enum class TeamAffiliation {
    Enemy,
    Ally
};

// Parámetros de contexto (lo que la CPU puede “saber” del match).
// Se pasan desde Game para poder usar ronda, stage, etc.
struct Context {
    int versusRoundNumber = 1;
};

struct Settings {
    // Dificultad por playerId (0..3). Por defecto: CPU1 fácil, CPU2 media, CPU3 “hard”.
    // Nota: en VS 1P la CPU empieza en playerId=1; en VS 2P empieza en playerId=2.
    std::array<Difficulty, 4> difficultyByPlayerId = {
        Difficulty::Easy,    // P1 (humano normalmente)
        Difficulty::Easy,    // CPU #1
        Difficulty::Medium,  // CPU #2
        Difficulty::Hard     // CPU #3
    };
};

// Bomberman controlado por IA pero gestionado como enemigo del mapa (gEnemies).
// Se usa en Custom Game con spawns `enemy <slot> <x> <y>`.
class Agent : public Enemy {
public:
    Agent(glm::vec2 pos,
          glm::vec2 size,
          float speed,
          TeamAffiliation affiliation,
          Difficulty difficulty = Difficulty::Medium,
          const std::string& spritePrefix = "jugadorrojo");
    ~Agent() override;

    void Update() override;
    void Draw() override;

    bool takeDamage(const SpriteAtlas& atlas, int amount = 1) override;
    void startDying(const SpriteAtlas& atlas) override;

    TeamAffiliation getAffiliation() const { return affiliation; }
    bool isAlly() const { return affiliation == TeamAffiliation::Ally; }
    bool isEnemy() const { return affiliation == TeamAffiliation::Enemy; }
    const std::string& getSpritePrefix() const { return botSpritePrefix; }

private:
    TeamAffiliation affiliation;
    Difficulty difficulty;
    std::string botSpritePrefix;

    Move currentMove = MOVE_NONE;
    float moveLockSeconds = 0.0f;
    float bombCooldownSeconds = 0.0f;

    int bombPower = 2;
    int maxOwnedBombs = 2;
    std::vector<glm::ivec2> ownedBombTiles;
};

bool isAgent(const Enemy* enemy);
bool isAllyAgent(const Enemy* enemy);
bool isEnemyAgent(const Enemy* enemy);

// Actualiza los Bomberman controlados por CPU (movimiento + bombas).
void updateCpuPlayers(GameMode mode,
                      const GameMap* map,
                      std::vector<Player*>& players,
                      float deltaTime,
                      const Context& context,
                      const Settings& settings);

} // namespace CpuBomberman

#endif // CPU_BOMBERMAN_HPP
