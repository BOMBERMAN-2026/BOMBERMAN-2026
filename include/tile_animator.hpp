#ifndef TILE_ANIMATOR_HPP
#define TILE_ANIMATOR_HPP

#include <unordered_map>
#include "sprite_atlas.hpp"

/*
 * tile_animator.hpp
 * -----------------
 * Animación simple de tiles del mapa por “swap” de IDs.
 *
 * - El atlas puede definir un intervalo y pares (A<->B).
 * - El `TileAnimator` alterna el estado cada `interval` y traduce IDs al render.
 */

// Clase independiente que gestiona animaciones de sprites en el mapa.
// Recibe la configuracion desde el SpriteAtlas y mantiene el estado.
class TileAnimator {
public:
    TileAnimator();
    ~TileAnimator();

    // Setup
    void setup(const SpriteAtlas& atlas);

    // Tick
    bool update(float deltaTime); // Devuelve true si alterna (toggle) en este frame.

    // Render
    int getDisplayId(int originalId) const; // Traduce ID original -> ID a renderizar.

private:
    // Estado
    float interval = 0.0f;      // Segundos entre toggles (0 = sin animación)
    float timer = 0.0f;         // Acumulador
    bool  isToggled = false;    // false = original, true = alternativo

    // Mappings (A <-> B)
    std::unordered_map<int, int> toAlternative; // original -> alternativo
    std::unordered_map<int, int> toOriginal;    // alternativo -> original
};

#endif // TILE_ANIMATOR_HPP
