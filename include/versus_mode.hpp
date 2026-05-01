#ifndef VERSUS_MODE_HPP
#define VERSUS_MODE_HPP

#include <string>
#include <vector>

#include "bomberman.hpp" // GameMode

class Player;

namespace VersusMode {

// Devuelve true si el modo actual es Versus.
bool isVersusMode(GameMode mode);

// Configuración de VS: secuencia de rondas y mapping ronda->stage.
const std::vector<std::string>& levelSequence();
const std::vector<int>& levelToStage();

// Cuenta cuántos jugadores siguen en pie en la ronda (estado Alive).
int countPlayersStillInMatch(const std::vector<Player*>& players);

// Devuelve el índice del último jugador vivo en la ronda. Si no hay, -1.
int findLastPlayerStillInMatchIndex(const std::vector<Player*>& players);

// Devuelve true si todos los jugadores humanos están fuera del encuentro.
// - VsOnePlayer: humano = player[0]
// - VsTwoPlayers: humanos = player[0] y player[1]
bool humansOut(GameMode mode, const std::vector<Player*>& players);

// Draw (empate):
// - si no queda nadie en pie (muerte simultánea)
// - o si se agota el tiempo y quedan varios con vida
bool isDraw(int remainingPlayers, float levelTimeRemaining);

// VS infinito: siguiente ronda ciclando sobre la secuencia.
int nextLevelIndex(int currentLevelIndex);

} // namespace VersusMode

#endif // VERSUS_MODE_HPP
