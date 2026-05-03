#ifndef DRON_BOMBARDERO_HPP
#define DRON_BOMBARDERO_HPP

#include "enemy.hpp"

enum class DronState {
    Normal,
    TransformingToFire,
    FireMode,
    TransformingToNormal
};

class DronBombardero : public Enemy {
public:
    static constexpr float kSpeed    = 0.17f;
    static constexpr float kFireSpeed = 0.22f;

    DronBombardero(glm::vec2 pos, glm::vec2 size,
                   const std::string& skinBase = "dronrosa");
    ~DronBombardero() override;

    void Update() override;
    void Draw()   override;
    bool takeDamage(const SpriteAtlas& atlas, int amount = 1) override;

    bool isOnFire() const { return state == DronState::FireMode || state == DronState::TransformingToFire || state == DronState::TransformingToNormal; }
    const std::string& getSkinBase() const { return spriteSkinBase; }

private:
    std::string spriteSkinBase;
    
    DronState state;

    float stateTimer;         
    float fireCycleCooldown;  
    float fireCycleTimer;     

    int transformFrame;
    float transformTimer;
    bool fireBurstShotThisTransform;

    void updateNormal(float dist, float step);
    void updateTransformingToFire();
    void updateFireMode(float dist, float step);
    void updateTransformingToNormal();
    
    void shootSlowFireball(EnemyDirection dir);
    void shootBurstFireballs();
    void syncSkinFromCurrentSprite();
    void updateWalkAnimation(bool isFireMode);
};

#endif // DRON_BOMBARDERO_HPP