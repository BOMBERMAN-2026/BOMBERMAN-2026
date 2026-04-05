#ifndef SPRITE_ATLAS_HPP
#define SPRITE_ATLAS_HPP

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

/*
 * sprite_atlas.hpp
 * ---------------
 * Estructuras y utilidades para cargar un atlas de sprites desde JSON.
 *
 * Conceptos:
 * - `SpriteAtlas`: imagen + dimensiones + mapa de `SpriteFrame` por nombre/ID.
 * - `getUvRectForSprite(...)`: traduce (x,y,w,h) en píxeles a UV (0..1) para el shader.
 *
 * Nota:
 * - El loader es “mínimo” (solo entiende el formato actual de tus JSONs).
 */

struct SpriteFrame {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool walkable = true;          // Campo "walkable" del JSON
    std::string type = "floor";    // Campo "type" (floor, barrier, indestructible, destructible)
    std::string align = "center";  // Campo "align" (center, left, right)
};

struct SpriteAtlas {
    // Datos
    std::string imagePath;                              // Ruta a la sprite sheet
    int imageWidth = 0;                                 // Ancho de la imagen
    int imageHeight = 0;                                // Alto de la imagen
    std::unordered_map<std::string, SpriteFrame> sprites; // Nombre -> frame

    // Animación de tiles (opcional)
    float animInterval = 0.0f;                          // 0 = sin animación
    std::unordered_map<int, int> animSwaps;             // ID original -> ID alternativo
};

// Rutas
std::string resolveAssetPath(const std::string& path); // Si el exe corre desde build/, prueba también con "../" + path.

// Loader mínimo del JSON (rellena imagePath/imageWidth/imageHeight + sprites).
bool loadSpriteAtlasMinimal(const std::string& jsonPath, SpriteAtlas& out);

// UV
bool getUvRectForSprite(const SpriteAtlas& atlas, const std::string& spriteName, glm::vec4& uvRect); // (u0,v0,u1,v1) en 0..1.

#endif // SPRITE_ATLAS_HPP
