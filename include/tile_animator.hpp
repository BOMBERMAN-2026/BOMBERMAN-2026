#ifndef TILE_ANIMATOR_HPP
#define TILE_ANIMATOR_HPP

#include <unordered_map>
#include "sprite_atlas.hpp"

// Clase independiente que gestiona animaciones de sprites en el mapa.
// Recibe la configuracion desde el SpriteAtlas y mantiene el estado.
class TileAnimator {
public:
    TileAnimator();
    ~TileAnimator();

    // Configura la animacion leyendo los pares de intercambio y el intervalo desde el atlas
    void setup(const SpriteAtlas& atlas);

    // Actualiza el temporizador interno. Devuelve true si ha habido un cambio de estado este frame.
    bool update(float deltaTime);

    // Dado el Id original del tile, devuelve el Id que se deberia renderizar ahora.
    // Maneja automaticamente la bidireccionalidad (A <-> B).
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
