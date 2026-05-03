#ifndef Q_LEARNING_HPP
#define Q_LEARNING_HPP

/*
 * q_learning.hpp
 * --------------
 * Motor de Q-Learning tabular para la IA del CPU Bomberman.
 *
 * Arquitectura en dos capas:
 *   - CAPA ALTA (este módulo): Q-Learning elige la ESTRATEGIA (qué hacer).
 *   - CAPA BAJA (BFS existente): ejecuta CÓMO hacerlo (pathfinding, escape).
 *
 * El estado del juego se discretiza en 9 features → 5,760 estados posibles.
 * El Q-Learner elige entre 7 acciones estratégicas de alto nivel.
 * Q-Table: 5,760 × 7 = 40,320 floats (~160 KB). Operaciones O(1).
 *
 * Persistencia: las tablas se guardan/cargan como binario en resources/ai/.
 * Hay una tabla compartida por nivel de dificultad (Easy/Medium/Hard).
 */

#include <cstdint>
#include <string>
#include <vector>

namespace CpuBomberman {

// ─── Acciones estratégicas de alto nivel ───────────────────────────────────

enum class QAction : int {
    FLEE_DANGER      = 0, // Escapar de explosiones (bfsNextStepToEscapeBlast)
    CHASE_OPPONENT   = 1, // Perseguir al rival más cercano (bfsNextStepToTargetCell)
    COLLECT_POWERUP  = 2, // Ir a por power-ups visibles (bfsNextStepToNearestPowerUp)
    DESTROY_BLOCK    = 3, // Romper bloques destructibles + plantar bomba
    PLACE_BOMB_COMBAT= 4, // Plantar bomba ofensiva contra rival cercano
    WANDER           = 5, // Explorar aleatoriamente (pickRandomMove)
    HOLD_POSITION    = 6, // Quedarse quieto
    COUNT            = 7
};

static constexpr int kQActionCount = static_cast<int>(QAction::COUNT);

// ─── Estado discretizado ───────────────────────────────────────────────────

struct QState {
    uint8_t dangerLevel     = 0; // 0=seguro, 1=peligro cercano, 2=peligro inmediato
    uint8_t opponentDir     = 0; // 0=ninguno, 1=arriba, 2=abajo, 3=izquierda, 4=derecha
    uint8_t opponentDist    = 0; // 0=lejos(>8), 1=medio(4-8), 2=cerca(2-3), 3=adyacente(≤1)
    uint8_t adjDestructibles= 0; // 0, 1, 2 (clamped)
    uint8_t visiblePowerUp  = 0; // 0=no, 1=sí
    uint8_t canBomb         = 0; // 0=no, 1=sí
    uint8_t hasEscape       = 0; // 0=no hay ruta de escape, 1=sí
    uint8_t ownBombActive   = 0; // 0=no tiene bomba activa, 1=sí
    uint8_t hpAdvantage     = 0; // 0=menos vidas, 1=igual o más

    // Codifica el estado en un entero único [0, kMaxStates).
    uint32_t encode() const;

    static constexpr uint32_t kMaxStates = 3u * 5u * 4u * 3u * 2u * 2u * 2u * 2u * 2u; // 5760
};

// ─── Hiperparámetros de Q-Learning ─────────────────────────────────────────

struct QLearningConfig {
    float alpha            = 0.10f;  // Tasa de aprendizaje
    float gamma            = 0.90f;  // Factor de descuento
    float epsilon          = 0.20f;  // Tasa de exploración (ε-greedy)
    float epsilonDecay     = 0.95f;  // Multiplicador de ε tras cada ronda
    float epsilonMin       = 0.05f;  // Mínimo de exploración
    float decisionInterval = 0.30f;  // Segundos entre decisiones estratégicas
    float heuristicSeed    = 0.0f;   // Fuerza del seeding heurístico [0=nada, 1=máximo]
};

// Devuelve la configuración QL para una dificultad.
QLearningConfig qlConfigForDifficulty(int difficultyIndex);

// ─── Q-Table ───────────────────────────────────────────────────────────────

class QTable {
public:
    QTable();

    // Lectura/escritura de valores Q.
    float getValue(uint32_t stateId, int actionId) const;
    void  setValue(uint32_t stateId, int actionId, float value);

    // Devuelve la acción con mayor Q-value para el estado dado.
    int bestAction(uint32_t stateId) const;

    // Selección ε-greedy: explora con probabilidad ε, explota con (1-ε).
    int chooseAction(uint32_t stateId, float epsilon) const;

    // Actualización Bellman: Q(s,a) += α·[r + γ·max_a'(Q(s',a')) − Q(s,a)]
    void update(uint32_t state, int action, float reward,
                uint32_t nextState, float alpha, float gamma);

    // Persistencia binaria.
    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

    // Siembra la tabla con valores heurísticos de experto.
    // strength ∈ [0,1]: 0 = sin seeding, 1 = conocimiento experto completo.
    void seedWithHeuristics(float strength);

    // Pone todos los valores a cero.
    void clear();

    // Cuenta entradas no-cero (debug).
    int countNonZero() const;

private:
    // Array plano: [stateId * kQActionCount + actionId]
    std::vector<float> table;

    static constexpr uint32_t kMagic   = 0x514C5442u; // "QLTB"
    static constexpr uint32_t kVersion = 1u;
};

} // namespace CpuBomberman

#endif // Q_LEARNING_HPP
