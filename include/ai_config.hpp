#ifndef AI_CONFIG_HPP
#define AI_CONFIG_HPP

#include "cpu_bomberman_difficulty.hpp"

namespace CpuBomberman {

namespace {
    constexpr bool kAiDebugEnabled = true;
}

inline float clampFloat(float value, float minValue, float maxValue)
{
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

struct AiProfile {
    float aggressiveness = 0.0f;       // Frecuencia/intención de plantar bombas.
    float dangerAversion = 0.0f;       // Anticipación del peligro por explosiones.
    float itemClairvoyance = 0.0f;     // Probabilidad de "ver" ítems ocultos.
    float pathfindingPrecision = 0.0f; // Probabilidad de seguir BFS óptimo.

    AiProfile() = default;
    AiProfile(float inAggressiveness,
              float inDangerAversion,
              float inItemClairvoyance,
              float inPathfindingPrecision)
        : aggressiveness(inAggressiveness),
          dangerAversion(inDangerAversion),
          itemClairvoyance(inItemClairvoyance),
          pathfindingPrecision(inPathfindingPrecision) {}
};

struct AiEvolutionCaps {
    float minAggressiveness = 0.0f;
    float maxAggressiveness = 1.0f;

    float minDangerAversion = 0.0f;
    float maxDangerAversion = 1.0f;

    float minItemClairvoyance = 0.0f;
    float maxItemClairvoyance = 1.0f;

    float minPathfindingPrecision = 0.0f;
    float maxPathfindingPrecision = 1.0f;

        AiEvolutionCaps() = default;
        AiEvolutionCaps(float minAgg,
                                        float maxAgg,
                                        float minDanger,
                                        float maxDanger,
                                        float minClairvoyance,
                                        float maxClairvoyance,
                                        float minPrecision,
                                        float maxPrecision)
                : minAggressiveness(minAgg),
                    maxAggressiveness(maxAgg),
                    minDangerAversion(minDanger),
                    maxDangerAversion(maxDanger),
                    minItemClairvoyance(minClairvoyance),
                    maxItemClairvoyance(maxClairvoyance),
                    minPathfindingPrecision(minPrecision),
                    maxPathfindingPrecision(maxPrecision) {}
};

struct AiDifficultyConfig {
    AiProfile initialProfile;
    AiEvolutionCaps caps;
};

inline AiProfile clampProfile(const AiProfile& profile, const AiEvolutionCaps& caps)
{
    AiProfile out = profile;
    out.aggressiveness = clampFloat(out.aggressiveness, caps.minAggressiveness, caps.maxAggressiveness);
    out.dangerAversion = clampFloat(out.dangerAversion, caps.minDangerAversion, caps.maxDangerAversion);
    out.itemClairvoyance = clampFloat(out.itemClairvoyance, caps.minItemClairvoyance, caps.maxItemClairvoyance);
    out.pathfindingPrecision = clampFloat(out.pathfindingPrecision, caps.minPathfindingPrecision, caps.maxPathfindingPrecision);
    return out;
}

inline const AiDifficultyConfig& configForDifficulty(Difficulty difficulty)
{
    static const AiDifficultyConfig easy{
        /*initialProfile=*/AiProfile(0.16f, 0.18f, 0.0f, 0.28f),
        /*caps=*/AiEvolutionCaps(
            0.08f, 0.35f,
            0.10f, 0.45f,
            0.0f, 0.0f,   // Fácil nunca detecta ocultos.
            0.15f, 0.45f
        )
    };

    static const AiDifficultyConfig medium{
        /*initialProfile=*/AiProfile(0.30f, 0.42f, 0.18f, 0.62f),
        /*caps=*/AiEvolutionCaps(
            0.20f, 0.60f,
            0.25f, 0.78f,
            0.0f, 0.55f,
            0.45f, 0.82f
        )
    };

    static const AiDifficultyConfig hard{
        /*initialProfile=*/AiProfile(0.48f, 0.62f, 0.78f, 0.82f),
        /*caps=*/AiEvolutionCaps(
            0.35f, 0.85f,
            0.45f, 0.95f,
            0.35f, 1.0f,
            0.65f, 0.97f
        )
    };

    switch (difficulty) {
        case Difficulty::Easy: return easy;
        case Difficulty::Medium: return medium;
        case Difficulty::Hard: return hard;
        default: return medium;
    }
}

} // namespace CpuBomberman

#endif // AI_CONFIG_HPP