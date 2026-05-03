#include "q_learning.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

namespace CpuBomberman {

// ─── RNG propio del módulo QL ──────────────────────────────────────────────

static std::mt19937& qlRng()
{
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

static float qlRand01()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(qlRng());
}

// ─── QState ────────────────────────────────────────────────────────────────

uint32_t QState::encode() const
{
    // Empaqueta los 9 features en un entero único con aritmética mixta.
    // Orden: danger(3) · oppDir(5) · oppDist(4) · adjDestr(3) ·
    //        visPow(2) · canBomb(2) · hasEsc(2) · ownBomb(2) · hpAdv(2)
    uint32_t id = 0;
    id = id * 3u + (uint32_t)(dangerLevel     % 3u);
    id = id * 5u + (uint32_t)(opponentDir      % 5u);
    id = id * 4u + (uint32_t)(opponentDist     % 4u);
    id = id * 3u + (uint32_t)(adjDestructibles % 3u);
    id = id * 2u + (uint32_t)(visiblePowerUp   % 2u);
    id = id * 2u + (uint32_t)(canBomb          % 2u);
    id = id * 2u + (uint32_t)(hasEscape        % 2u);
    id = id * 2u + (uint32_t)(ownBombActive    % 2u);
    id = id * 2u + (uint32_t)(hpAdvantage      % 2u);
    return id;
}

// ─── QLearningConfig por dificultad ────────────────────────────────────────

QLearningConfig qlConfigForDifficulty(int difficultyIndex)
{
    QLearningConfig cfg;

    switch (difficultyIndex) {
        case 0: // Easy — aprende desde cero, explora mucho, reacciona lento
            cfg.alpha            = 0.15f;
            cfg.gamma            = 0.85f;
            cfg.epsilon          = 0.45f;
            cfg.epsilonDecay     = 0.97f;
            cfg.epsilonMin       = 0.18f;
            cfg.decisionInterval = 0.45f;
            cfg.heuristicSeed    = 0.0f;  // Sin pre-entrenamiento
            break;

        case 1: // Medium
            cfg.alpha            = 0.15f;
            cfg.gamma            = 0.95f;
            cfg.epsilon          = 0.60f; // 60% de acciones aleatorias
            cfg.epsilonDecay     = 0.98f;
            cfg.epsilonMin       = 0.10f;
            cfg.decisionInterval = 0.30f;
            cfg.heuristicSeed    = 0.0f;  // ¡AQUÍ! Ponlo a 0.0f para que empiece tonta y aprenda.
            break;

        default: // Hard — pre-entrenada, explota conocimiento
            cfg.alpha            = 0.08f;
            cfg.gamma            = 0.97f;
            cfg.epsilon          = 0.08f;
            cfg.epsilonDecay     = 0.92f;
            cfg.epsilonMin       = 0.02f;
            cfg.decisionInterval = 0.18f;
            cfg.heuristicSeed    = 1.0f;  // Completamente pre-entrenada
            break;
    }

    return cfg;
}

// ─── QTable ────────────────────────────────────────────────────────────────

QTable::QTable()
    : table(QState::kMaxStates * (size_t)kQActionCount, 0.0f)
{
}

float QTable::getValue(uint32_t stateId, int actionId) const
{
    if (stateId >= QState::kMaxStates || actionId < 0 || actionId >= kQActionCount)
        return 0.0f;
    return table[(size_t)stateId * (size_t)kQActionCount + (size_t)actionId];
}

void QTable::setValue(uint32_t stateId, int actionId, float value)
{
    if (stateId >= QState::kMaxStates || actionId < 0 || actionId >= kQActionCount)
        return;
    table[(size_t)stateId * (size_t)kQActionCount + (size_t)actionId] = value;
}

int QTable::bestAction(uint32_t stateId) const
{
    if (stateId >= QState::kMaxStates) return 0;

    const size_t base = (size_t)stateId * (size_t)kQActionCount;
    int best = 0;
    float bestVal = table[base];
    for (int a = 1; a < kQActionCount; ++a) {
        if (table[base + (size_t)a] > bestVal) {
            bestVal = table[base + (size_t)a];
            best = a;
        }
    }
    return best;
}

int QTable::chooseAction(uint32_t stateId, float epsilon) const
{
    if (qlRand01() < epsilon) {
        // Exploración: acción aleatoria.
        std::uniform_int_distribution<int> dist(0, kQActionCount - 1);
        return dist(qlRng());
    }
    // Explotación: mejor acción conocida.
    return bestAction(stateId);
}

void QTable::update(uint32_t state, int action, float reward,
                    uint32_t nextState, float alpha, float gamma)
{
    if (state >= QState::kMaxStates || action < 0 || action >= kQActionCount)
        return;

    const float oldQ = getValue(state, action);

    // max_a'(Q(s', a'))
    float maxNextQ = getValue(nextState, 0);
    for (int a = 1; a < kQActionCount; ++a) {
        const float q = getValue(nextState, a);
        if (q > maxNextQ) maxNextQ = q;
    }

    // Ecuación de Bellman: Q(s,a) ← Q(s,a) + α·[r + γ·maxQ' − Q(s,a)]
    const float newQ = oldQ + alpha * (reward + gamma * maxNextQ - oldQ);
    setValue(state, action, newQ);
}

// ─── Persistencia binaria ──────────────────────────────────────────────────

bool QTable::save(const std::string& filepath) const
{
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    const uint32_t magic   = 0x514C5442u;
    const uint32_t version = 1u;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    const uint32_t stateCount  = QState::kMaxStates;
    const uint32_t actionCount = (uint32_t)kQActionCount;
    out.write(reinterpret_cast<const char*>(&stateCount), sizeof(stateCount));
    out.write(reinterpret_cast<const char*>(&actionCount), sizeof(actionCount));

    out.write(reinterpret_cast<const char*>(table.data()),
              (std::streamsize)(table.size() * sizeof(float)));

    if (out.good()) {
        std::cout << "[CPU-AI][QL] Q-table guardada: " << filepath
                  << " (" << countNonZero() << " entradas no-cero)" << std::endl;
    }
    return out.good();
}

bool QTable::load(const std::string& filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    const uint32_t expectedMagic   = 0x514C5442u;
    const uint32_t expectedVersion = 1u;

    uint32_t magic = 0, version = 0, stateCount = 0, actionCount = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&stateCount), sizeof(stateCount));
    in.read(reinterpret_cast<char*>(&actionCount), sizeof(actionCount));

    if (magic != expectedMagic || version != expectedVersion) return false;
    if (stateCount != QState::kMaxStates || actionCount != (uint32_t)kQActionCount) return false;

    in.read(reinterpret_cast<char*>(table.data()),
            (std::streamsize)(table.size() * sizeof(float)));

    if (in.good()) {
        std::cout << "[CPU-AI][QL] Q-table cargada: " << filepath
                  << " (" << countNonZero() << " entradas no-cero)" << std::endl;
    }
    return in.good();
}

// ─── Seeding heurístico ────────────────────────────────────────────────────

void QTable::seedWithHeuristics(float strength)
{
    if (strength <= 0.0f) return;

    // Para cada estado posible, asigna Q-values basados en conocimiento experto.
    // El Hard arranca con estos valores = ya sabe jugar.
    // El Medium arranca con valores parciales = sabe algo pero no todo.

    for (uint32_t sid = 0; sid < QState::kMaxStates; ++sid) {
        // Decodificar estado (inverso de encode).
        uint32_t tmp = sid;
        const uint8_t hpAdv    = tmp % 2u; tmp /= 2u;
        const uint8_t ownBomb  = tmp % 2u; tmp /= 2u;
        const uint8_t hasEsc   = tmp % 2u; tmp /= 2u;
        const uint8_t canBmb   = tmp % 2u; tmp /= 2u;
        const uint8_t visPow   = tmp % 2u; tmp /= 2u;
        const uint8_t adjDes   = tmp % 3u; tmp /= 3u;
        const uint8_t oppDist  = tmp % 4u; tmp /= 4u;
        const uint8_t oppDir   = tmp % 5u; tmp /= 5u;
        const uint8_t danger   = tmp % 3u;

        float q[kQActionCount] = {0.0f};

        // ── FLEE_DANGER (0): Prioridad alta pero no eclipsa todo ──
        if (danger == 2)      q[0] = 30.0f; 
        else if (danger == 1) q[0] = 8.0f;  
        else                  q[0] = -5.0f;

        // ── CHASE_OPPONENT (1) ──
        if (danger <= 1 && oppDir > 0) {
            if (oppDist == 3)      q[1] = 12.0f;
            else if (oppDist == 2) q[1] = 10.0f;
            else                   q[1] = 8.0f;
        } else if (danger > 1) {
            q[1] = -10.0f; 
        }

        // ── COLLECT_POWERUP (2) ──
        if (visPow && danger <= 1) q[2] = 20.0f;
        else                       q[2] = -2.0f;

        // ── DESTROY_BLOCK (3): muy importante al inicio ──
        if (adjDes > 0 && canBmb && danger <= 1) {
            q[3] = 18.0f + (float)adjDes * 4.0f; 
        } else {
            q[3] = -2.0f;
        }

        // ── PLACE_BOMB_COMBAT (4): agresividad máxima ──
        if (oppDir > 0 && oppDist >= 2 && canBmb && danger <= 1) {
            q[4] = 40.0f + (oppDist == 3 ? 15.0f : 0.0f); 
            if (hpAdv) q[4] += 10.0f;
        } else {
            q[4] = -5.0f;
        }

        // ── WANDER (5) ──
        q[5] = 2.0f;
        if (danger == 0 && oppDir == 0 && !visPow && adjDes == 0) {
            q[5] = 8.0f;
        }

        // ── HOLD_POSITION (6) ──
        if (danger == 0 && ownBomb && !canBmb) q[6] = 2.0f;
        else                                    q[6] = -20.0f; 


        // Aplicar fuerza del seeding.
        for (int a = 0; a < kQActionCount; ++a) {
            setValue(sid, a, q[a] * strength);
        }
    }

    std::cout << "[CPU-AI][QL] Heuristic seed applied (strength=" << strength
              << ", nonZero=" << countNonZero() << ")" << std::endl;
}

void QTable::clear()
{
    std::fill(table.begin(), table.end(), 0.0f);
}

int QTable::countNonZero() const
{
    int count = 0;
    for (float v : table) {
        if (std::abs(v) > 1e-6f) ++count;
    }
    return count;
}

} // namespace CpuBomberman
