#ifndef PLAYER_H
#define PLAYER_H

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <string>

enum Move {
    MOVE_UP,
    MOVE_LEFT,
    MOVE_DOWN,
    MOVE_RIGHT,
    MOVE_NONE
};

class Player {
    public: 
        float posX, posY;
        glm::vec2 position; 
        glm::vec2 direction;
        glm::vec4 tintColor;
        float speed;
        glm::vec2 size;

        Player(glm::vec2 pos, glm::vec2 size, GLfloat velocity);
        ~Player();

        void UpdateSprite(Move mov);
};


#endif // PLAYER_H
