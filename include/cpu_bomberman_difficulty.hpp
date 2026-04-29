#ifndef CPU_BOMBERMAN_DIFFICULTY_HPP
#define CPU_BOMBERMAN_DIFFICULTY_HPP

namespace CpuBomberman {

enum class Difficulty {
    Easy,
    Medium,
    Hard
};

inline Difficulty difficultyFromIndex(int index)
{
    switch (index) {
        case 0: return Difficulty::Easy;
        case 1: return Difficulty::Medium;
        default: return Difficulty::Hard;
    }
}

} // namespace CpuBomberman

#endif // CPU_BOMBERMAN_DIFFICULTY_HPP
