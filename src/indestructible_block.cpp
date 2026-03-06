#include "indestructible_block.hpp"

IndestructibleBlock::IndestructibleBlock(glm::vec2 pos, glm::vec2 size)
    : Entity(pos, size, 0.0f) // velocidad 0: el bloque no se mueve
{}

IndestructibleBlock::~IndestructibleBlock() {}

void IndestructibleBlock::Update()
{
    // Los bloques indestructibles no tienen lógica de actualización
}

void IndestructibleBlock::Draw()
{
    // Lógica de renderizado del bloque indestructible (a implementar)
}
