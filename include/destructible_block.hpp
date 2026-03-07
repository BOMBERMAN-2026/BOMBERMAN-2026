#ifndef DESTRUCTIBLE_BLOCK_H
#define DESTRUCTIBLE_BLOCK_H

#include "entity.hpp"

/*
 * Bloque destructible: representa los bloques de madera/ladrillo del mapa de Bomberman.
 * Puede ser destruido por la explosión de una bomba.
 * Al ser destruido puede revelar un power-up oculto debajo.
 */
class DestructibleBlock : public Entity {
    public:
        // Indica si el bloque ha sido destruido
        bool destroyed;

        // Indica si el bloque tiene un power-up oculto que se revela al destruirlo
        bool hasPowerUp;

        // Constructor: posición y tamaño del bloque
        DestructibleBlock(glm::vec2 pos, glm::vec2 size, bool hasPowerUp = false);

        ~DestructibleBlock() override;

        // Actualiza el estado del bloque (animación de destrucción, etc.)
        void Update() override;

        // Renderiza el bloque en pantalla (o su animación de destrucción)
        void Draw() override;

        // Recibe el impacto de una explosión y marca el bloque como destruido
        void OnExplosionHit();
};

#endif // DESTRUCTIBLE_BLOCK_H
