#ifndef Q_LEARNING_HPP
#define Q_LEARNING_HPP

/*
 * q_learning.hpp
 * --------------
 * Motor de Q-Learning con aproximador MLP para la IA del CPU Bomberman.
 *
 * Arquitectura:
 *   - Capa alta: una Q-network MLP elige la estrategia.
 *   - Capa baja: BFS y reglas tacticas ejecutan el movimiento concreto.
 *
 * El estado se discretiza en 9 features y se transforma en 13 entradas
 * normalizadas para una red densa 13 -> 32 -> 24 -> 7.
 * La salida estima Q(s,a) para cada accion estrategica.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace CpuBomberman {

enum class QAction : int {
    FLEE_DANGER       = 0,
    CHASE_OPPONENT    = 1,
    COLLECT_POWERUP   = 2,
    DESTROY_BLOCK     = 3,
    PLACE_BOMB_COMBAT = 4,
    WANDER            = 5,
    HOLD_POSITION     = 6,
    COUNT             = 7
};

static constexpr int kQActionCount = static_cast<int>(QAction::COUNT);

struct QState {
    uint8_t dangerLevel      = 0; // 0=seguro, 1=peligro cercano, 2=peligro inmediato
    uint8_t opponentDir      = 0; // 0=ninguno, 1=arriba, 2=abajo, 3=izquierda, 4=derecha
    uint8_t opponentDist     = 0; // 0=lejos, 1=medio, 2=cerca, 3=adyacente
    uint8_t adjDestructibles = 0; // 0, 1, 2
    uint8_t visiblePowerUp   = 0; // 0=no, 1=si
    uint8_t canBomb          = 0; // 0=no, 1=si
    uint8_t hasEscape        = 0; // 0=no, 1=si
    uint8_t ownBombActive    = 0; // 0=no, 1=si
    uint8_t hpAdvantage      = 0; // 0=no, 1=si

    uint32_t encode() const;

    static constexpr uint32_t kMaxStates =
        3u * 5u * 4u * 3u * 2u * 2u * 2u * 2u * 2u;
};

struct QLearningConfig {
    float alpha            = 0.10f;
    float gamma            = 0.90f;
    float epsilon          = 0.20f;
    float epsilonDecay     = 0.95f;
    float epsilonMin       = 0.05f;
    float decisionInterval = 0.30f;
    float heuristicSeed    = 0.0f;
};

QLearningConfig qlConfigForDifficulty(int difficultyIndex);

// Mantiene el nombre QTable para no tocar todo el juego, pero por dentro ya es
// una red MLP que aproxima Q(s,a).
class QTable {
public:
    QTable();

    float getValue(uint32_t stateId, int actionId) const;
    void  setValue(uint32_t stateId, int actionId, float value);

    int bestAction(uint32_t stateId) const;
    int chooseAction(uint32_t stateId, float epsilon) const;

    void update(uint32_t state, int action, float reward,
                uint32_t nextState, float alpha, float gamma);

    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

    void seedWithHeuristics(float strength);
    void clear();
    int countNonZero() const;

private:
    static constexpr int kInputCount = 13;
    static constexpr int kHidden1 = 32;
    static constexpr int kHidden2 = 24;

    std::vector<float> w1;
    std::vector<float> b1;
    std::vector<float> w2;
    std::vector<float> b2;
    std::vector<float> w3;
    std::vector<float> b3;

    void resetWeights(float scale);
    void predict(uint32_t stateId, std::vector<float>& out) const;
    void trainTargets(uint32_t stateId, const std::vector<float>& target, float learningRate);

    static QState decodeState(uint32_t stateId);
    static void stateToInputs(uint32_t stateId, float out[kInputCount]);
    static void heuristicTargets(uint32_t stateId, float strength, std::vector<float>& out);

    static constexpr uint32_t kMagic   = 0x4D4C5051u; // "MLPQ"
    static constexpr uint32_t kVersion = 2u;
};

} // namespace CpuBomberman

#endif // Q_LEARNING_HPP
