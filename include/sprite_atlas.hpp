#ifndef SPRITE_ATLAS_HPP
#define SPRITE_ATLAS_HPP

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

struct SpriteFrame {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool walkable = true; // leido del campo "walkable" del JSON
};

struct SpriteAtlas {
    std::string imagePath;
    int imageWidth = 0;
    int imageHeight = 0;
    std::unordered_map<std::string, SpriteFrame> sprites;
};

// Intenta resolver rutas cuando el exe se ejecuta desde build/.
// Si existe tal cual, devuelve path. Si no, prueba con "../" + path.
std::string resolveAssetPath(const std::string& path);

// Loader mínimo para el formato que generas en SpriteAtlasPlayer.json.
// Devuelve true si pudo leer el JSON y llenar imagePath/imageWidth/imageHeight y sprites.
bool loadSpriteAtlasMinimal(const std::string& jsonPath, SpriteAtlas& out);

// Devuelve (u0, v0, u1, v1) para el sprite, en coordenadas 0..1.
// Convención: (x,y) del JSON se asume desde arriba-izquierda.
bool getUvRectForSprite(const SpriteAtlas& atlas, const std::string& spriteName, glm::vec4& uvRect);

#endif // SPRITE_ATLAS_HPP
