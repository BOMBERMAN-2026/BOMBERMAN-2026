#include "custom_game_mode.hpp"

#include "game_map.hpp"
#include "enemy.hpp"
#include "cpu_bomberman.hpp"
#include "enemies/leon.hpp"
#include "enemies/babosa.hpp"
#include "enemies/bebe_lloron.hpp"
#include "enemies/fantasma_mortal.hpp"
#include "enemies/sol_pervertido.hpp"
#include "enemies/dragon_joven.hpp"
#include "enemies/dron_bombardero.hpp"
#include "enemies/king_bomber.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

extern std::string resolveAssetPath(const std::string& assetPath);

namespace {
constexpr int kMaxEnemySlots = 12;

const std::array<std::string, 5> kCustomLevelPaths = {
    "levels/level_cg_01.txt",
    "levels/level_cg_02.txt",
    "levels/level_cg_03.txt",
    "levels/level_cg_04.txt",
    "levels/level_cg_05.txt"
};

const std::array<int, 5> kCustomStages = {
    1, 2, 2, 3, 4
};

const std::array<std::string, 5> kCustomHudLabels = {
    "1-2", "3-2", "3-6", "4-2", "6-6"
};

int clampMapIndex(int index) {
    if (index < 0) return 0;
    if (index >= static_cast<int>(kCustomLevelPaths.size())) {
        return static_cast<int>(kCustomLevelPaths.size()) - 1;
    }
    return index;
}
} // namespace

CustomGameMode::CustomGameMode()
    : active(false),
      settings(),
      enemyCounts({0, 0, 0, 0, 0, 0, 0, 0, 0}),
      playerCount(1),
      mapIndex(0),
      stageNumber(1),
      initialTimeSeconds(180.0f),
      infiniteTime(false),
      levelPath(kCustomLevelPaths[0]),
      hudLevelLabel(kCustomHudLabels[0]) {
}

void CustomGameMode::activate(const CustomGameSettings& inSettings,
                              const std::array<int, 9>& inEnemyCounts) {
    settings = inSettings;
    enemyCounts = inEnemyCounts;

    mapIndex = clampMapIndex(settings.mapIndex);
    stageNumber = kCustomStages[mapIndex];
    levelPath = kCustomLevelPaths[mapIndex];
    hudLevelLabel = kCustomHudLabels[mapIndex];

    // 1P+Comp mantiene 1 jugador humano; el compañero CPU se crea como Agent en spawns de enemigos.
    playerCount = (settings.players == CustomPlayersOption::TwoPlayers) ? 2 : 1;

    infiniteTime = false;
    switch (settings.timeLimit) {
        case CustomTimeLimitOption::OneMinute:
            initialTimeSeconds = 60.0f;
            break;
        case CustomTimeLimitOption::TwoMinutes:
            initialTimeSeconds = 120.0f;
            break;
        case CustomTimeLimitOption::ThreeMinutes:
            initialTimeSeconds = 180.0f;
            break;
        case CustomTimeLimitOption::Infinite:
            // Se mostrará 99:59 y no decrecerá en update.
            initialTimeSeconds = 5999.0f;
            infiniteTime = true;
            break;
    }

    active = true;
}

void CustomGameMode::deactivate() {
    active = false;
}

bool CustomGameMode::isAllDigits(const std::string& value) {
    if (value.empty()) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

std::vector<CustomGameMode::EnemySlot> CustomGameMode::readEnemySlotsFromLevel() const {
    std::vector<EnemySlot> slots;
    if (!active) {
        return slots;
    }

    const std::string resolvedPath = resolveAssetPath(levelPath);
    std::ifstream file(resolvedPath.c_str());
    if (!file.is_open()) {
        std::cerr << "CustomGameMode: no se pudo abrir " << resolvedPath << "\n";
        return slots;
    }

    std::map<int, EnemySlot> slotById;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const size_t firstNotSpace = line.find_first_not_of(" \t");
        if (firstNotSpace == std::string::npos) {
            continue;
        }

        const std::string trimmed = line.substr(firstNotSpace);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream iss(trimmed);
        std::string keyword;
        iss >> keyword;
        if (keyword != "enemy") {
            continue;
        }

        std::string slotToken;
        int x = 0;
        int y = 0;
        if (!(iss >> slotToken >> x >> y)) {
            continue;
        }

        if (!isAllDigits(slotToken)) {
            continue;
        }

        const int slotId = std::atoi(slotToken.c_str());
        if (slotId < 1 || slotId > kMaxEnemySlots) {
            continue;
        }

        EnemySlot slot;
        slot.slotId = slotId;
        slot.row = y;
        slot.col = x;
        slotById[slotId] = slot;
    }

    slots.reserve(slotById.size());
    for (std::map<int, EnemySlot>::const_iterator it = slotById.begin(); it != slotById.end(); ++it) {
        slots.push_back(it->second);
    }
    return slots;
}

std::vector<CustomGameMode::CustomEnemyKind> CustomGameMode::buildEnemyQueue() const {
    std::vector<CustomEnemyKind> queue;
    queue.reserve(kMaxEnemySlots);

    // Orden solicitado en Menu2:
    // 1) Bomberman enemigo
    // 2) Leon
    // 3) Bebe lloron
    // 4) Babosa
    // 5) Fantasma
    // 6) Sol
    // 7) Dragon
    // 8) Drones
    // 9) KingBomberman
    const std::array<CustomEnemyKind, 9> indexToKind = {
        CustomEnemyKind::BombermanEnemy,
        CustomEnemyKind::Leon,
        CustomEnemyKind::BebeLloron,
        CustomEnemyKind::Babosa,
        CustomEnemyKind::Fantasma,
        CustomEnemyKind::Sol,
        CustomEnemyKind::Dragon,
        CustomEnemyKind::Drones,
        CustomEnemyKind::KingBomberman
    };

    for (int i = 0; i < static_cast<int>(enemyCounts.size()); ++i) {
        const int amount = std::max(0, enemyCounts[i]);
        for (int j = 0; j < amount; ++j) {
            if (static_cast<int>(queue.size()) >= kMaxEnemySlots) {
                return queue;
            }
            queue.push_back(indexToKind[i]);
        }
    }

    return queue;
}

Enemy* CustomGameMode::createEnemyFromKind(CustomEnemyKind kind,
                                           const glm::vec2& position,
                                           const glm::vec2& enemySize,
                                           float defaultPlayerSpeed,
                                           int& bomberDifficultyCursor,
                                           int& droneColorCursor) const {
    switch (kind) {
        case CustomEnemyKind::BombermanEnemy: {
            const std::string prefix = "jugadoramarillo";

            const size_t difficultyIndex = std::min<size_t>(settings.enemyBombermanDifficulties.size() - 1,
                                                            static_cast<size_t>(std::max(0, bomberDifficultyCursor)));
            const CpuBomberman::Difficulty difficulty = settings.enemyBombermanDifficulties[difficultyIndex];
            bomberDifficultyCursor += 1;

            const float bomberSpeed = std::max(0.18f, defaultPlayerSpeed);
            CpuBomberman::Agent* enemy = new CpuBomberman::Agent(position,
                                                                  enemySize,
                                                                  bomberSpeed,
                                                                  CpuBomberman::TeamAffiliation::Enemy,
                                                                  difficulty,
                                                                  prefix);
            enemy->currentSpriteName = prefix + ".abajo.0";
            return enemy;
        }
        case CustomEnemyKind::Leon: {
            Leon* enemy = new Leon(position, enemySize, 0.10f);
            enemy->currentSpriteName = "leon.abajo.0";
            return enemy;
        }
        case CustomEnemyKind::BebeLloron: {
            BebeLloron* enemy = new BebeLloron(position, enemySize, 0.08f);
            enemy->currentSpriteName = "bebe.derecha.0";
            return enemy;
        }
        case CustomEnemyKind::Babosa: {
            Babosa* enemy = new Babosa(position, enemySize, 0.06f);
            enemy->currentSpriteName = "babosa.abajo.0";
            return enemy;
        }
        case CustomEnemyKind::Fantasma: {
            FantasmaMortal* enemy = new FantasmaMortal(position, enemySize, 0.11f);
            enemy->currentSpriteName = "fantasma.derecha.0";
            return enemy;
        }
        case CustomEnemyKind::Sol: {
            SolPervertido* enemy = new SolPervertido(position, enemySize, 0.07f);
            enemy->currentSpriteName = "sol.grande.0";
            return enemy;
        }
        case CustomEnemyKind::Dragon: {
            DragonJoven* enemy = new DragonJoven(position, enemySize, 0.07f);
            enemy->currentSpriteName = "dragon.abajo.0";
            return enemy;
        }
        case CustomEnemyKind::Drones: {
            static const char* kDroneSprites[4] = {
                "dronrosa.abajo.0",
                "dronverde.abajo.0",
                "dronamarillo.abajo.0",
                "dronazul.abajo.0"
            };

            DronBombardero* enemy = new DronBombardero(position, enemySize, 0.09f);
            enemy->currentSpriteName = kDroneSprites[droneColorCursor % 4];
            droneColorCursor += 1;
            return enemy;
        }
        case CustomEnemyKind::KingBomberman: {
            KingBomber* enemy = new KingBomber(position, enemySize, defaultPlayerSpeed);
            enemy->currentSpriteName = "kingbomber1.abajo.0";
            return enemy;
        }
    }

    return NULL;
}

void CustomGameMode::spawnConfiguredEnemies(const GameMap* gameMap,
                                            const std::vector<Player*>* players,
                                            std::vector<Enemy*>& outEnemies,
                                            const glm::vec2& enemySize,
                                            float defaultPlayerSpeed) const {
    if (!active || gameMap == NULL) {
        return;
    }

    // 1P+Comp: crea Bomberman rojo CPU como segundo jugador no humano.
    if (settings.players == CustomPlayersOption::OnePlayerPlusCpu) {
        const bool cooperative = (settings.teamMode == CustomTeamModeOption::Cooperative);
        const CpuBomberman::TeamAffiliation affiliation = cooperative
            ? CpuBomberman::TeamAffiliation::Ally
            : CpuBomberman::TeamAffiliation::Enemy;

        const glm::vec2 cpuSpawnPos = gameMap->getSpawnPosition(1);
        const float bomberSpeed = std::max(0.18f, defaultPlayerSpeed);
        CpuBomberman::Agent* companion = new CpuBomberman::Agent(cpuSpawnPos,
                                                                  enemySize,
                                                                  bomberSpeed,
                                                                  affiliation,
                                                                  settings.allyCpuDifficulty,
                                                                  "jugadorazul");
        companion->currentSpriteName = "jugadorazul.abajo.0";
        companion->setContext(gameMap, players);
        outEnemies.push_back(companion);
    }

    const std::vector<CustomEnemyKind> queue = buildEnemyQueue();
    if (queue.empty()) {
        return;
    }

    const std::vector<EnemySlot> slots = readEnemySlotsFromLevel();
    if (slots.empty()) {
        std::cerr << "CustomGameMode: no se encontraron spawns enemy <slot> <x> <y> en " << levelPath << "\n";
        return;
    }

    const int spawnCount = std::min(static_cast<int>(slots.size()), static_cast<int>(queue.size()));
    int bomberDifficultyCursor = 0;
    int droneColorCursor = 0;

    for (int i = 0; i < spawnCount; ++i) {
        const EnemySlot& slot = slots[i];

        if (!gameMap->isWalkable(slot.row, slot.col)) {
            std::cerr << "CustomGameMode: spawn no caminable (slot=" << slot.slotId
                      << ", row=" << slot.row << ", col=" << slot.col << ")\n";
            continue;
        }

        const glm::vec2 pos = gameMap->gridToNDC(slot.row, slot.col);
        Enemy* enemy = createEnemyFromKind(queue[i],
                                           pos,
                                           enemySize,
                                           defaultPlayerSpeed,
                                           bomberDifficultyCursor,
                                           droneColorCursor);
        if (!enemy) {
            continue;
        }

        enemy->setContext(gameMap, players);
        outEnemies.push_back(enemy);
    }
}
