#include "q_learning.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <random>

namespace CpuBomberman {

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

static float clampValue(float value, float minValue, float maxValue)
{
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

static float relu(float value)
{
    return value > 0.0f ? value : 0.0f;
}

uint32_t QState::encode() const
{
    uint32_t id = 0;
    id = id * 3u + (uint32_t)(dangerLevel      % 3u);
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

QLearningConfig qlConfigForDifficulty(int difficultyIndex)
{
    QLearningConfig cfg;

    switch (difficultyIndex) {
        case 0:
            cfg.alpha            = 0.15f;
            cfg.gamma            = 0.85f;
            cfg.epsilon          = 0.25f;
            cfg.epsilonDecay     = 0.97f;
            cfg.epsilonMin       = 0.08f;
            cfg.decisionInterval = 0.32f;
            cfg.heuristicSeed    = 0.35f;
            break;

        case 1:
            cfg.alpha            = 0.15f;
            cfg.gamma            = 0.95f;
            cfg.epsilon          = 0.15f;
            cfg.epsilonDecay     = 0.98f;
            cfg.epsilonMin       = 0.05f;
            cfg.decisionInterval = 0.22f;
            cfg.heuristicSeed    = 0.65f;
            break;

        default:
            cfg.alpha            = 0.12f;
            cfg.gamma            = 0.95f;
            cfg.epsilon          = 0.06f;
            cfg.epsilonDecay     = 0.95f;
            cfg.epsilonMin       = 0.02f;
            cfg.decisionInterval = 0.12f;
            cfg.heuristicSeed    = 0.95f;
            break;
    }

    return cfg;
}

QTable::QTable()
{
    resetWeights(0.18f);
}

void QTable::resetWeights(float scale)
{
    w1.assign((size_t)kHidden1 * (size_t)kInputCount, 0.0f);
    b1.assign((size_t)kHidden1, 0.0f);
    w2.assign((size_t)kHidden2 * (size_t)kHidden1, 0.0f);
    b2.assign((size_t)kHidden2, 0.0f);
    w3.assign((size_t)kQActionCount * (size_t)kHidden2, 0.0f);
    b3.assign((size_t)kQActionCount, 0.0f);

    auto fillWeights = [&](std::vector<float>& weights, int fanIn) {
        const float limit = scale / std::sqrt((float)std::max(1, fanIn));
        std::uniform_real_distribution<float> dist(-limit, limit);
        for (float& v : weights) {
            v = dist(qlRng());
        }
    };

    fillWeights(w1, kInputCount);
    fillWeights(w2, kHidden1);
    fillWeights(w3, kHidden2);
}

QState QTable::decodeState(uint32_t stateId)
{
    QState s;
    uint32_t tmp = stateId;
    s.hpAdvantage      = (uint8_t)(tmp % 2u); tmp /= 2u;
    s.ownBombActive    = (uint8_t)(tmp % 2u); tmp /= 2u;
    s.hasEscape        = (uint8_t)(tmp % 2u); tmp /= 2u;
    s.canBomb          = (uint8_t)(tmp % 2u); tmp /= 2u;
    s.visiblePowerUp   = (uint8_t)(tmp % 2u); tmp /= 2u;
    s.adjDestructibles = (uint8_t)(tmp % 3u); tmp /= 3u;
    s.opponentDist     = (uint8_t)(tmp % 4u); tmp /= 4u;
    s.opponentDir      = (uint8_t)(tmp % 5u); tmp /= 5u;
    s.dangerLevel      = (uint8_t)(tmp % 3u);
    return s;
}

void QTable::stateToInputs(uint32_t stateId, float out[kInputCount])
{
    const QState s = decodeState(stateId);

    out[0] = (float)s.dangerLevel / 2.0f;
    out[1] = (float)s.opponentDist / 3.0f;
    out[2] = (float)s.adjDestructibles / 2.0f;
    out[3] = (float)s.visiblePowerUp;
    out[4] = (float)s.canBomb;
    out[5] = (float)s.hasEscape;
    out[6] = (float)s.ownBombActive;
    out[7] = (float)s.hpAdvantage;

    for (int i = 0; i < 5; ++i) {
        out[8 + i] = (s.opponentDir == (uint8_t)i) ? 1.0f : 0.0f;
    }
}

void QTable::predict(uint32_t stateId, std::vector<float>& out) const
{
    float x[kInputCount] = {0.0f};
    stateToInputs(stateId, x);

    std::vector<float> h1((size_t)kHidden1, 0.0f);
    std::vector<float> h2((size_t)kHidden2, 0.0f);
    out.assign((size_t)kQActionCount, 0.0f);

    for (int h = 0; h < kHidden1; ++h) {
        float sum = b1[(size_t)h];
        for (int i = 0; i < kInputCount; ++i) {
            sum += w1[(size_t)h * (size_t)kInputCount + (size_t)i] * x[i];
        }
        h1[(size_t)h] = relu(sum);
    }

    for (int h = 0; h < kHidden2; ++h) {
        float sum = b2[(size_t)h];
        for (int i = 0; i < kHidden1; ++i) {
            sum += w2[(size_t)h * (size_t)kHidden1 + (size_t)i] * h1[(size_t)i];
        }
        h2[(size_t)h] = relu(sum);
    }

    for (int a = 0; a < kQActionCount; ++a) {
        float sum = b3[(size_t)a];
        for (int h = 0; h < kHidden2; ++h) {
            sum += w3[(size_t)a * (size_t)kHidden2 + (size_t)h] * h2[(size_t)h];
        }
        out[(size_t)a] = sum;
    }
}

void QTable::trainTargets(uint32_t stateId,
                          const std::vector<float>& target,
                          float learningRate)
{
    if (target.size() < (size_t)kQActionCount) return;

    float x[kInputCount] = {0.0f};
    stateToInputs(stateId, x);

    std::vector<float> h1((size_t)kHidden1, 0.0f);
    std::vector<float> h2((size_t)kHidden2, 0.0f);
    std::vector<float> out((size_t)kQActionCount, 0.0f);

    for (int h = 0; h < kHidden1; ++h) {
        float sum = b1[(size_t)h];
        for (int i = 0; i < kInputCount; ++i) {
            sum += w1[(size_t)h * (size_t)kInputCount + (size_t)i] * x[i];
        }
        h1[(size_t)h] = relu(sum);
    }

    for (int h = 0; h < kHidden2; ++h) {
        float sum = b2[(size_t)h];
        for (int i = 0; i < kHidden1; ++i) {
            sum += w2[(size_t)h * (size_t)kHidden1 + (size_t)i] * h1[(size_t)i];
        }
        h2[(size_t)h] = relu(sum);
    }

    for (int a = 0; a < kQActionCount; ++a) {
        float sum = b3[(size_t)a];
        for (int h = 0; h < kHidden2; ++h) {
            sum += w3[(size_t)a * (size_t)kHidden2 + (size_t)h] * h2[(size_t)h];
        }
        out[(size_t)a] = sum;
    }

    std::vector<float> delta3((size_t)kQActionCount, 0.0f);
    std::vector<float> delta2((size_t)kHidden2, 0.0f);
    std::vector<float> delta1((size_t)kHidden1, 0.0f);

    for (int a = 0; a < kQActionCount; ++a) {
        delta3[(size_t)a] = clampValue(out[(size_t)a] - target[(size_t)a], -30.0f, 30.0f);
    }

    for (int h = 0; h < kHidden2; ++h) {
        float sum = 0.0f;
        for (int a = 0; a < kQActionCount; ++a) {
            sum += delta3[(size_t)a] * w3[(size_t)a * (size_t)kHidden2 + (size_t)h];
        }
        delta2[(size_t)h] = (h2[(size_t)h] > 0.0f) ? sum : 0.0f;
    }

    for (int h = 0; h < kHidden1; ++h) {
        float sum = 0.0f;
        for (int j = 0; j < kHidden2; ++j) {
            sum += delta2[(size_t)j] * w2[(size_t)j * (size_t)kHidden1 + (size_t)h];
        }
        delta1[(size_t)h] = (h1[(size_t)h] > 0.0f) ? sum : 0.0f;
    }

    const float lr = clampValue(learningRate, 0.0005f, 0.08f);

    for (int a = 0; a < kQActionCount; ++a) {
        for (int h = 0; h < kHidden2; ++h) {
            const size_t idx = (size_t)a * (size_t)kHidden2 + (size_t)h;
            w3[idx] -= lr * delta3[(size_t)a] * h2[(size_t)h];
        }
        b3[(size_t)a] -= lr * delta3[(size_t)a];
    }

    for (int h = 0; h < kHidden2; ++h) {
        for (int i = 0; i < kHidden1; ++i) {
            const size_t idx = (size_t)h * (size_t)kHidden1 + (size_t)i;
            w2[idx] -= lr * delta2[(size_t)h] * h1[(size_t)i];
        }
        b2[(size_t)h] -= lr * delta2[(size_t)h];
    }

    for (int h = 0; h < kHidden1; ++h) {
        for (int i = 0; i < kInputCount; ++i) {
            const size_t idx = (size_t)h * (size_t)kInputCount + (size_t)i;
            w1[idx] -= lr * delta1[(size_t)h] * x[i];
        }
        b1[(size_t)h] -= lr * delta1[(size_t)h];
    }
}

float QTable::getValue(uint32_t stateId, int actionId) const
{
    if (stateId >= QState::kMaxStates || actionId < 0 || actionId >= kQActionCount)
        return 0.0f;

    std::vector<float> values;
    predict(stateId, values);
    return values[(size_t)actionId];
}

void QTable::setValue(uint32_t stateId, int actionId, float value)
{
    if (stateId >= QState::kMaxStates || actionId < 0 || actionId >= kQActionCount)
        return;

    std::vector<float> target;
    predict(stateId, target);
    target[(size_t)actionId] = value;
    trainTargets(stateId, target, 0.04f);
}

int QTable::bestAction(uint32_t stateId) const
{
    if (stateId >= QState::kMaxStates) return 0;

    std::vector<float> values;
    predict(stateId, values);

    int best = 0;
    float bestValue = values[0];
    for (int a = 1; a < kQActionCount - 1; ++a) {
        if (values[(size_t)a] > bestValue) {
            bestValue = values[(size_t)a];
            best = a;
        }
    }
    return best;
}

int QTable::chooseAction(uint32_t stateId, float epsilon) const
{
    if (qlRand01() < epsilon) {
        std::uniform_int_distribution<int> dist(0, kQActionCount - 2);
        return dist(qlRng());
    }
    return bestAction(stateId);
}

void QTable::update(uint32_t state, int action, float reward,
                    uint32_t nextState, float alpha, float gamma)
{
    if (state >= QState::kMaxStates || nextState >= QState::kMaxStates)
        return;
    if (action < 0 || action >= kQActionCount - 1)
        return;

    std::vector<float> current;
    std::vector<float> next;
    predict(state, current);
    predict(nextState, next);

    float maxNextQ = next[0];
    for (int a = 1; a < kQActionCount - 1; ++a) {
        if (next[(size_t)a] > maxNextQ) {
            maxNextQ = next[(size_t)a];
        }
    }

    const float oldQ = current[(size_t)action];
    const float tdTarget = clampValue(reward + gamma * maxNextQ, -160.0f, 160.0f);
    current[(size_t)action] = oldQ + alpha * (tdTarget - oldQ);

    trainTargets(state, current, clampValue(alpha * 0.35f, 0.005f, 0.06f));
}

static bool writeVector(std::ofstream& out, const std::vector<float>& data)
{
    const uint32_t count = (uint32_t)data.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (count > 0) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  (std::streamsize)(data.size() * sizeof(float)));
    }
    return out.good();
}

static bool readVector(std::ifstream& in, std::vector<float>& data, size_t expected)
{
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in.good() || (size_t)count != expected) return false;

    data.assign(expected, 0.0f);
    if (count > 0) {
        in.read(reinterpret_cast<char*>(data.data()),
                (std::streamsize)(data.size() * sizeof(float)));
    }
    return in.good();
}

bool QTable::save(const std::string& filepath) const
{
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    const uint32_t magic = kMagic;
    const uint32_t version = kVersion;
    const uint32_t inputCount = (uint32_t)kInputCount;
    const uint32_t hidden1 = (uint32_t)kHidden1;
    const uint32_t hidden2 = (uint32_t)kHidden2;
    const uint32_t actionCount = (uint32_t)kQActionCount;

    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&inputCount), sizeof(inputCount));
    out.write(reinterpret_cast<const char*>(&hidden1), sizeof(hidden1));
    out.write(reinterpret_cast<const char*>(&hidden2), sizeof(hidden2));
    out.write(reinterpret_cast<const char*>(&actionCount), sizeof(actionCount));

    const bool ok =
        writeVector(out, w1) &&
        writeVector(out, b1) &&
        writeVector(out, w2) &&
        writeVector(out, b2) &&
        writeVector(out, w3) &&
        writeVector(out, b3);

    if (ok) {
        std::cout << "[CPU-AI][MLP] Modelo guardado: " << filepath
                  << " (" << countNonZero() << " pesos no-cero)" << std::endl;
    }
    return ok;
}

bool QTable::load(const std::string& filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t inputCount = 0;
    uint32_t hidden1 = 0;
    uint32_t hidden2 = 0;
    uint32_t actionCount = 0;

    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&inputCount), sizeof(inputCount));
    in.read(reinterpret_cast<char*>(&hidden1), sizeof(hidden1));
    in.read(reinterpret_cast<char*>(&hidden2), sizeof(hidden2));
    in.read(reinterpret_cast<char*>(&actionCount), sizeof(actionCount));

    if (magic != kMagic || version != kVersion) return false;
    if (inputCount != (uint32_t)kInputCount ||
        hidden1 != (uint32_t)kHidden1 ||
        hidden2 != (uint32_t)kHidden2 ||
        actionCount != (uint32_t)kQActionCount)
    {
        return false;
    }

    const bool ok =
        readVector(in, w1, (size_t)kHidden1 * (size_t)kInputCount) &&
        readVector(in, b1, (size_t)kHidden1) &&
        readVector(in, w2, (size_t)kHidden2 * (size_t)kHidden1) &&
        readVector(in, b2, (size_t)kHidden2) &&
        readVector(in, w3, (size_t)kQActionCount * (size_t)kHidden2) &&
        readVector(in, b3, (size_t)kQActionCount);

    if (ok) {
        std::cout << "[CPU-AI][MLP] Modelo cargado: " << filepath
                  << " (" << countNonZero() << " pesos no-cero)" << std::endl;
    }
    return ok;
}

void QTable::heuristicTargets(uint32_t stateId, float strength, std::vector<float>& out)
{
    const QState s = decodeState(stateId);
    out.assign((size_t)kQActionCount, 0.0f);

    if (s.dangerLevel == 2)      out[(size_t)QAction::FLEE_DANGER] = 30.0f;
    else if (s.dangerLevel == 1) out[(size_t)QAction::FLEE_DANGER] = 8.0f;
    else                         out[(size_t)QAction::FLEE_DANGER] = -5.0f;

    if (s.dangerLevel <= 1 && s.opponentDir > 0) {
        if (s.opponentDist == 3)      out[(size_t)QAction::CHASE_OPPONENT] = 28.0f;
        else if (s.opponentDist == 2) out[(size_t)QAction::CHASE_OPPONENT] = 24.0f;
        else if (s.opponentDist == 1) out[(size_t)QAction::CHASE_OPPONENT] = 7.0f;
        else                          out[(size_t)QAction::CHASE_OPPONENT] = 2.0f;
    } else if (s.dangerLevel > 1) {
        out[(size_t)QAction::CHASE_OPPONENT] = -10.0f;
    }

    out[(size_t)QAction::COLLECT_POWERUP] =
        (s.visiblePowerUp && s.dangerLevel <= 1) ? 36.0f : 1.0f;

    if (s.adjDestructibles > 0 && s.canBomb && s.dangerLevel <= 1) {
        out[(size_t)QAction::DESTROY_BLOCK] = 32.0f + (float)s.adjDestructibles * 5.0f;
    } else {
        out[(size_t)QAction::DESTROY_BLOCK] = 4.0f;
    }

    if (s.opponentDir > 0 && s.opponentDist >= 2 && s.canBomb && s.dangerLevel <= 1) {
        out[(size_t)QAction::PLACE_BOMB_COMBAT] = 42.0f + (s.opponentDist == 3 ? 18.0f : 0.0f);
        if (s.hpAdvantage) out[(size_t)QAction::PLACE_BOMB_COMBAT] += 12.0f;
        if (!s.hasEscape) out[(size_t)QAction::PLACE_BOMB_COMBAT] -= 18.0f;
    } else {
        out[(size_t)QAction::PLACE_BOMB_COMBAT] = -4.0f;
    }

    out[(size_t)QAction::WANDER] = (s.dangerLevel == 0 && s.opponentDir == 0 &&
                                   !s.visiblePowerUp && s.adjDestructibles == 0) ? 4.0f : 0.0f;
    out[(size_t)QAction::HOLD_POSITION] = -20.0f;

    for (float& v : out) {
        v *= strength;
    }
}

void QTable::seedWithHeuristics(float strength)
{
    clear();
    if (strength <= 0.0f) return;

    std::vector<float> target;
    const int epochs = 5;
    const float lr = 0.018f;

    for (int epoch = 0; epoch < epochs; ++epoch) {
        for (uint32_t sid = 0; sid < QState::kMaxStates; ++sid) {
            heuristicTargets(sid, strength, target);
            trainTargets(sid, target, lr);
        }
    }

    std::cout << "[CPU-AI][MLP] Heuristic pretrain aplicado (strength=" << strength
              << ", weights=" << countNonZero() << ")" << std::endl;
}

void QTable::clear()
{
    resetWeights(0.18f);
}

int QTable::countNonZero() const
{
    int count = 0;
    auto addCount = [&](const std::vector<float>& data) {
        for (float v : data) {
            if (std::abs(v) > 1e-6f) ++count;
        }
    };

    addCount(w1);
    addCount(b1);
    addCount(w2);
    addCount(b2);
    addCount(w3);
    addCount(b3);
    return count;
}

} // namespace CpuBomberman
