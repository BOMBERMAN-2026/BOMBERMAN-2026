#include "versus_mode.hpp"

#include "player.hpp"

namespace VersusMode {

// Devuelve true si el modo actual es Versus.
bool isVersusMode(GameMode mode)
{
    return mode == GameMode::VsOnePlayer || mode == GameMode::VsTwoPlayers;
}

// Configuración de VS: secuencia de rondas y mapping ronda->stage.
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

// Primero que llegue a este número de wins gana el encuentro VS.
int winsToEndMatch()
{
    return 3;
}

// Cuenta cuántos jugadores siguen en ronda (no están en Game Over).
int countPlayersStillInMatch(const std::vector<Player*>& players)
{
    int count = 0;
    for (auto* p : players) {
        if (!p) continue;
        if (p->isAlive()) count++;
    }
    return count;
}

// Devuelve el índice del último jugador que sigue en la ronda. Si no hay, -1.
int findLastPlayerStillInMatchIndex(const std::vector<Player*>& players)
{
    for (int i = 0; i < (int)players.size(); ++i) {
        Player* p = players[i];
        if (!p) continue;
        if (p->isAlive()) return i;
    }
    return -1;
}

// Devuelve true si todos los jugadores humanos están fuera del encuentro.
// - VsOnePlayer: humano = player[0]
// - VsTwoPlayers: humanos = player[0] y player[1]
bool humansOut(GameMode mode, const std::vector<Player*>& players)
{
    const bool p1Out = (players.size() > 0 && players[0] != nullptr && players[0]->isGameOver());
    const bool p2Out = (players.size() > 1 && players[1] != nullptr && players[1]->isGameOver());

    return (mode == GameMode::VsOnePlayer) ? p1Out : (p1Out && p2Out);
}

// Draw (empate):
// - si no queda nadie en pie (muerte simultánea)
// - o si se agota el tiempo y quedan varios con vida
bool isDraw(int remainingPlayers, float levelTimeRemaining)
{
    const bool timeUp = (levelTimeRemaining <= 0.0f);
    return (remainingPlayers == 0) || (timeUp && remainingPlayers > 1);
}

// VS infinito: siguiente ronda ciclando sobre la secuencia.
int nextLevelIndex(int currentLevelIndex)
{
    const int n = (int)levelSequence().size();
    if (n <= 0) return 0;
    return (currentLevelIndex + 1) % n;
}

// Devuelve true si algún jugador ya llegó al límite de wins.
bool hasMatchWinner(const std::vector<int>& wins)
{
    const int limit = winsToEndMatch();
    for (int w : wins) {
        if (w >= limit) return true;
    }
    return false;
}

} // namespace VersusMode
