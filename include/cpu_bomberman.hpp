#ifndef CPU_BOMBERMAN_HPP
#define CPU_BOMBERMAN_HPP

#include <array>
#include <vector>

#include "bomberman.hpp" // GameMode
#include "player.hpp"    // Player, Move

class GameMap;

namespace CpuBomberman {

// Dificultades (hardcodeadas por ahora).
// - RandomAndBombs: se mueve aleatorio y pone bombas aleatorio.
// - RandomNoSuicide: aleatorio pero intenta no auto-explotarse.
// - PowerUpHunter: prioriza power-ups (y rompe ladrillos si procede).
// - Omniscient: persigue rivales con pathfinding simple y evita peligros inminentes.
enum class Difficulty {
    RandomAndBombs,
    RandomNoSuicide,
    PowerUpHunter,
    Omniscient
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
        Difficulty::RandomAndBombs,   // P1 (humano normalmente)
        Difficulty::RandomAndBombs,   // CPU #1
        Difficulty::RandomNoSuicide,  // CPU #2
        Difficulty::Omniscient        // CPU #3
    };
};

// Actualiza los Bomberman controlados por CPU (movimiento + bombas).
void updateCpuPlayers(GameMode mode,
                      const GameMap* map,
                      std::vector<Player*>& players,
                      float deltaTime,
                      const Context& context,
                      const Settings& settings);

} // namespace CpuBomberman

#endif // CPU_BOMBERMAN_HPP
