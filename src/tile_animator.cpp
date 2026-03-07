#include "tile_animator.hpp"

TileAnimator::TileAnimator() 
    : interval(0.0f), timer(0.0f), isToggled(false) {
}

TileAnimator::~TileAnimator() {}

void TileAnimator::setup(const SpriteAtlas& atlas) {
    this->interval = atlas.animInterval;
    this->timer = 0.0f;
    this->isToggled = false;
    
    this->toAlternative.clear();
    this->toOriginal.clear();

    // Copiar los mappings del atlas
    for (const auto& pair : atlas.animSwaps) {
        this->toAlternative[pair.first] = pair.second;
        this->toOriginal[pair.second] = pair.first;
    }
}

bool TileAnimator::update(float deltaTime) {
    if (interval <= 0.0f) {
        return false; // No hay animacion configurada
    }

    timer += deltaTime;
    if (timer >= interval) {
        timer -= interval;
        isToggled = !isToggled;
        return true; // Hubo cambio
    }
    return false;
}

int TileAnimator::getDisplayId(int originalId) const {
    if (interval <= 0.0f) {
        return originalId;
    }

    if (isToggled) {
        // En estado alternativo, buscamos si el original tiene una version alternativa
        auto it = toAlternative.find(originalId);
        if (it != toAlternative.end()) {
            return it->second;
        }
        
        // Si el tile YA ES un alternativo en el mapa, devolvemos a si mismo de momento. 
        // Lo habitual es que el mapa .txt use los IDs originales.
    } else {
        // En estado original, si en el .txt han puesto un ID alternativo, 
        // lo forzamos al original.
        auto it = toOriginal.find(originalId);
        if (it != toOriginal.end()) {
            return it->second;
        }
    }
    
    return originalId;
}
