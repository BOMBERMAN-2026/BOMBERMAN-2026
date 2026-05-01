#include "versus_mode.hpp"

#include "player.hpp"

namespace VersusMode {

// Devuelve true si el modo actual es Versus.
bool isVersusMode(GameMode mode)
{
    return mode == GameMode::VsOnePlayer || mode == GameMode::VsTwoPlayers;
}

// Configuracion de VS: secuencia de rondas y mapping ronda->stage.
const std::vector<std::string>& levelSequence()
{
    static const std::vector<std::string> kVsLevelSequence = {
        "levels/level_vs_01.txt",
        "levels/level_vs_02.txt",
        "levels/level_vs_03.txt",
        "levels/level_vs_04.txt"
    };
    return kVsLevelSequence;
}

const std::vector<int>& levelToStage()
{
    static const std::vector<int> kVsLevelToStage = {1, 2, 3, 4};
    return kVsLevelToStage;
}

// Cuenta cuantos jugadores siguen en ronda.
int countPlayersStillInMatch(const std::vector<Player*>& players)
{
    int count = 0;
    for (auto* p : players) {
        if (!p) continue;
        if (p->isAlive()) count++;
    }
    return count;
}

// Devuelve el indice del ultimo jugador que sigue en la ronda. Si no hay, -1.
int findLastPlayerStillInMatchIndex(const std::vector<Player*>& players)
{
    for (int i = 0; i < (int)players.size(); ++i) {
        Player* p = players[i];
        if (!p) continue;
        if (p->isAlive()) return i;
    }
    return -1;
}

// Devuelve true si todos los jugadores humanos estan fuera del encuentro.
bool humansOut(GameMode mode, const std::vector<Player*>& players)
{
    const bool p1Out = (players.size() > 0 && players[0] != nullptr && players[0]->isGameOver());
    const bool p2Out = (players.size() > 1 && players[1] != nullptr && players[1]->isGameOver());

    return (mode == GameMode::VsOnePlayer) ? p1Out : (p1Out && p2Out);
}

// Draw si no queda nadie en pie o si se agota el tiempo y quedan varios con vida.
bool isDraw(int remainingPlayers, float levelTimeRemaining)
{
    const bool timeUp = (levelTimeRemaining <= 0.0f);
    return (remainingPlayers == 0) || (timeUp && remainingPlayers > 1);
}

// Siguiente ronda ciclando sobre la secuencia VS.
int nextLevelIndex(int currentLevelIndex)
{
    const int n = (int)levelSequence().size();
    if (n <= 0) return 0;
    return (currentLevelIndex + 1) % n;
}

} // namespace VersusMode
