#include "destructible_block.hpp"

DestructibleBlock::DestructibleBlock(glm::vec2 pos, glm::vec2 size, bool hasPowerUp)
    : Entity(pos, size, 0.0f), // velocidad 0: el bloque no se mueve
      destroyed(false),
      hasPowerUp(hasPowerUp)
{}

DestructibleBlock::~DestructibleBlock() {}

void DestructibleBlock::Update()
{
    // Lógica de actualización: por ejemplo, avanzar la animación de destrucción
    // (a implementar cuando haya sistema de animaciones)
}

void DestructibleBlock::Draw()
{
    // Si el bloque ya fue destruido no se dibuja
    if (destroyed) return;

    // Lógica de renderizado del bloque destructible (a implementar)
}

void DestructibleBlock::OnExplosionHit()
{
    // Marca el bloque como destruido al recibir el impacto de una bomba
    if (!destroyed)
    {
        destroyed = true;

        // Si tenía un power-up oculto, aquí se notifica al sistema de juego
        // para que lo coloque en el mapa (a implementar)
        if (hasPowerUp)
        {
            // TODO: notificar al Game/Level para spawnear el power-up en this->position
        }
    }
}
