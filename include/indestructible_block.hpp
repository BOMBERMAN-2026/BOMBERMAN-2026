#ifndef INDESTRUCTIBLE_BLOCK_H
#define INDESTRUCTIBLE_BLOCK_H

#include "entity.hpp"

/*
 * Bloque indestructible: representa los muros sólidos del mapa de Bomberman.
 * No puede ser destruido por ninguna explosión.
 * Actúa como obstáculo permanente para el jugador, enemigos y bombas.
 */
class IndestructibleBlock : public Entity {
    public:
        // Constructor: posición y tamaño del bloque (velocidad 0 porque no se mueve)
        IndestructibleBlock(glm::vec2 pos, glm::vec2 size);

        ~IndestructibleBlock() override;

        // Actualiza el estado del bloque (no hace nada, es estático)
        void Update() override;

        // Renderiza el bloque en pantalla
        void Draw() override;
};

#endif // INDESTRUCTIBLE_BLOCK_H
