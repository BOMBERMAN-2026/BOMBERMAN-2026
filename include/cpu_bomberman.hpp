#ifndef CPU_BOMBERMAN_HPP
#define CPU_BOMBERMAN_HPP

#include <array>
#include <string>
#include <vector>

#include "ai_config.hpp"
#include "bomberman.hpp" // GameMode
#include "cpu_bomberman_difficulty.hpp"
#include "q_learning.hpp"
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
    int versusRoundNumber = 1; // Ronda actual dentro del encuentro VS.
};

struct Settings {
    // Dificultad por playerId (0..3). Por defecto: CPU1 fácil, CPU2 media, CPU3 “hard”.
    // Nota: en VS 1P la CPU empieza en playerId=1; en VS 2P empieza en playerId=2.
    std::array<Difficulty, 4> difficultyByPlayerId = {
        Difficulty::Easy,    // P1 (Humano - Blanco)
        Difficulty::Easy,    // CPU #1 (P2 - Rojo)
        Difficulty::Medium,  // CPU #2 (P3 - Azul)
        Difficulty::Hard     // CPU #3 (P4 - Amarillo)
    };
};

enum class DeathReason {
    Unknown,
    EnemyContact,
    ExplosionOther,
    ExplosionSelfBomb
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
    TeamAffiliation affiliation; // Lado del agente: aliado o rival.
    Difficulty difficulty; // Dificultad base de comportamiento.
    std::string botSpritePrefix; // Prefijo del atlas para este agente.

    Move currentMove = MOVE_NONE; // Dirección actual que intenta mantener.
    float moveLockSeconds = 0.0f; // Tiempo restante antes de recalcular movimiento.
    float bombCooldownSeconds = 0.0f; // Enfriamiento entre colocaciones de bomba.

    int bombPower = 2; // Potencia de explosión de las bombas propias.
    int maxOwnedBombs = 2; // Límite de bombas que puede controlar a la vez.
    std::vector<glm::ivec2> ownedBombTiles; // Casillas ocupadas por bombas remotas activas.

    // ── Q-Learning (aprendizaje por refuerzo) ──
    QState  lastQState;
    int     lastQAction = -1;
    float   accumulatedReward = 0.0f;
    float   qlDecisionTimer = 0.0f;
    bool    qlInitialized = false;
};

// Devuelve true si el puntero apunta a un agente CPU.
bool isAgent(const Enemy* enemy);

// Devuelve true si el agente pertenece al bando aliado.
bool isAllyAgent(const Enemy* enemy);

// Devuelve true si el agente pertenece al bando enemigo.
bool isEnemyAgent(const Enemy* enemy);

// Actualiza los Bomberman controlados por CPU (movimiento + bombas).
void updateCpuPlayers(GameMode mode,
                      const GameMap* map,
                      std::vector<Player*>& players,
                      float deltaTime,
                      const Context& context,
                      const Settings& settings);

// Reinicia el estado evolutivo de la IA entre partidas completas.
void resetEvolutionState();

// Limpia el tracking de muertes de la ronda actual.
void resetRoundDeathTracking();

// Registra la causa de muerte de una CPU concreta.
void recordCpuDeath(int playerId, DeathReason reason);

// Consume las causas de muerte acumuladas durante la ronda.
std::vector<DeathReason> consumeRoundDeathReasons();

// Ajusta los perfiles de CPU al cierre de cada ronda.
void evolveCpuPlayers(bool playerWon, const std::vector<DeathReason>& deaths);

// ── Q-Learning: interfaz pública ───────────────────────────────────────

// Inyecta una recompensa al CPU indicado (se acumula hasta la siguiente decisión).
void rewardCpu(int playerId, float reward);

// Carga las Q-tables compartidas desde disco (llamar al inicio de partida VS/Custom).
void loadQLearning();

// Guarda las Q-tables compartidas a disco (llamar al final de ronda).
void saveQLearning();

} // namespace CpuBomberman

#endif // CPU_BOMBERMAN_HPP
