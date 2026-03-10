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

    // Configura la animación leyendo el intervalo y los pares de intercambio desde el atlas.
    void setup(const SpriteAtlas& atlas);

    // Actualiza el temporizador interno.
    // Devuelve true si cambió el estado (toggle) en este frame.
    bool update(float deltaTime);

    // Dado el ID del tile (el que viene del .txt), devuelve el ID a renderizar ahora.
    // Maneja automáticamente la bidireccionalidad (A <-> B).
    int getDisplayId(int originalId) const;

private:
    float interval = 0.0f;
    float timer = 0.0f;
    bool  isToggled = false; // false = estado original, true = estado alternativo

    // Guarda los mappings en ambos sentidos para poder resolver facilmente
    std::unordered_map<int, int> toAlternative;
    std::unordered_map<int, int> toOriginal;
};

#endif // TILE_ANIMATOR_HPP
