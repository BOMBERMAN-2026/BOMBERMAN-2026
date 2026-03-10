#ifndef PLAYER_H
#define PLAYER_H

#include "entity.hpp"
#include <string>

/*
 * player.hpp
 * ----------
 * Jugador controlable (posición + movimiento + colisión con el mapa).
 *
 * - El render del jugador actualmente se gestiona desde `bomberman.cpp` usando un sprite atlas.
 * - Esta clase se centra en movimiento/colisiones (y deja el dibujo para el sistema de render actual).
 */

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

        // Tick de lógica (placeholder en este proyecto).
        void Update() override;
        // Render (placeholder: el render real se hace en bomberman.cpp).
        void Draw()   override;

        // Intenta mover el jugador un paso en la dirección indicada.
        // Aplica colisión con el mapa (`GameMap`) y actualiza `position` si se puede mover.
        void UpdateSprite(Move mov, const GameMap* map, float deltaTime);
};

#endif // PLAYER_H
