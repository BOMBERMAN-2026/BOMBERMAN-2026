#ifndef CUSTOM_GAME_MODE_HPP
#define CUSTOM_GAME_MODE_HPP

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

#include "custom_game_menu.hpp"

class Enemy;
class GameMap;
class Player;

class CustomGameMode {
public:
    CustomGameMode();

    void activate(const CustomGameSettings& settings,
                  const std::array<int, 9>& enemyCounts);
    void deactivate();

    bool isActive() const { return active; }
    int getPlayerCount() const { return playerCount; }
    int getStageNumber() const { return stageNumber; }
    int getMapIndex() const { return mapIndex; }
    const std::string& getLevelPath() const { return levelPath; }
    const std::string& getHudLevelLabel() const { return hudLevelLabel; }
    float getInitialTimeSeconds() const { return initialTimeSeconds; }
    bool isInfiniteTime() const { return infiniteTime; }

    void spawnConfiguredEnemies(const GameMap* gameMap,
                                const std::vector<Player*>* players,
                                std::vector<Enemy*>& outEnemies,
                                const glm::vec2& enemySize,
                                float defaultPlayerSpeed) const;

private:
    enum class CustomEnemyKind {
        BombermanEnemy,
        Leon,
        BebeLloron,
        Babosa,
        Fantasma,
        Sol,
        Dragon,
        Drones,
        KingBomberman
    };

    struct EnemySlot {
        int slotId = 0;
        int row = 0;
        int col = 0;
    };

    bool active;
    CustomGameSettings settings;
    std::array<int, 9> enemyCounts;
    int playerCount;
    int mapIndex;
    int stageNumber;
    float initialTimeSeconds;
    bool infiniteTime;
    std::string levelPath;
    std::string hudLevelLabel;

    std::vector<EnemySlot> readEnemySlotsFromLevel() const;
    std::vector<CustomEnemyKind> buildEnemyQueue() const;

    Enemy* createEnemyFromKind(CustomEnemyKind kind,
                               const glm::vec2& position,
                               const glm::vec2& enemySize,
                               float defaultPlayerSpeed,
                               int& bomberColorCursor,
                               int& droneColorCursor) const;

    static bool isAllDigits(const std::string& value);
};

#endif // CUSTOM_GAME_MODE_HPP