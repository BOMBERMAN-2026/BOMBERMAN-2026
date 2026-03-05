#ifndef PLAYER_H
#define PLAYER_H

#include "entity.hpp"
#include <string>

class GameMap;

enum Move {
    MOVE_UP,
    MOVE_LEFT,
    MOVE_DOWN,
    MOVE_RIGHT,
    MOVE_NONE
};

class Player : public Entity {
    public:
        Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity);
        ~Player() override;

        // Implementaciones de la interfaz Entity
        void Update() override;
        void Draw()   override;

        // Lógica específica del jugador
        void UpdateSprite(Move mov, const GameMap* map);
};

#endif // PLAYER_H
