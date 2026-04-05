#include "sprite_atlas.hpp"

/*
¿Qué hace este archivo?
----------------------

sprite_atlas.cpp carga un "sprite atlas" desde un JSON
y lo deja listo para que el render pinte un sprite concreto recortando una textura.

En concreto:
- `loadSpriteAtlasMinimal(...)` lee el JSON y rellena: ruta de la imagen, tamaño
    de la imagen, y un mapa de sprites con sus rectángulos en píxeles (x,y,w,h).
- `getUvRectForSprite(...)` convierte ese rectángulo de píxeles a UV (0..1) para
    pasarlo al shader como `uvRect`.

Importante:
- El parser es intencionadamente "mínimo": NO es un JSON parser genérico.
    Solo entiende este formato. Si cambias el JSON, puede que haya que tocar esto.
- Se asume que el JSON usa (x,y) medido desde arriba-izquierda (como suele venir en
    editores/atlases). Esto encaja con los texcoords del quad actual (v=0 arriba).
*/

#include <fstream>
#include <sstream>
#include <cctype>

static bool fileExists(const std::string& path)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    return f.good();
}

// Si el exe corre desde build/, prueba también con "../" + path.
std::string resolveAssetPath(const std::string& path)
{
    if (fileExists(path)) return path;
    std::string prefixed = "../" + path;
    if (fileExists(prefixed)) return prefixed;
    return path;
}

// Lee un fichero completo a string (binario, sin transformar saltos de línea).
static bool readFileToString(const std::string& path, std::string& out)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

static void skipWs(const std::string& s, size_t& i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
}

// Parsea un string entre comillas (manejo mínimo de escapes).
static bool parseQuotedString(const std::string& s, size_t& i, std::string& out)
{
    skipWs(s, i);
    if (i >= s.size() || s[i] != '"') return false;
    i++; // opening quote

    std::string result;
    while (i < s.size())
    {
        char c = s[i++];
        if (c == '"') {
            out = result;
            return true;
        }
        if (c == '\\' && i < s.size()) {
            // Minimal escape handling
            result.push_back(s[i++]);
            continue;
        }
        result.push_back(c);
    }
    return false;
}

// Parsea un entero con signo desde la posición actual.
static bool parseIntValue(const std::string& s, size_t& i, int& out)
{
    skipWs(s, i);
    bool neg = false;
    if (i < s.size() && s[i] == '-') { neg = true; i++; }
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;

    int val = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        val = val * 10 + (s[i] - '0');
        i++;
    }
    out = neg ? -val : val;
    return true;
}

// Encuentra la '}' que cierra la '{' en openPos (sin entender strings/escapes complejos).
static size_t findMatchingBrace(const std::string& s, size_t openPos)
{
    if (openPos == std::string::npos || openPos >= s.size() || s[openPos] != '{') return std::string::npos;
    int depth = 0;
    for (size_t i = openPos; i < s.size(); i++)
    {
        if (s[i] == '{') depth++;
        else if (s[i] == '}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

// Loader mínimo del JSON (rellena imagePath/imageWidth/imageHeight + sprites).
bool loadSpriteAtlasMinimal(const std::string& jsonPath, SpriteAtlas& out)
{
    out = SpriteAtlas{};

    std::string contents;
    if (!readFileToString(jsonPath, contents)) return false;

    // ruta_imagen
    {
        size_t pos = contents.find("\"ruta_imagen\"");
        if (pos == std::string::npos) return false;
        pos = contents.find(':', pos);
        if (pos == std::string::npos) return false;
        pos++;
        if (!parseQuotedString(contents, pos, out.imagePath)) return false;
    }

    // tamano_imagen.ancho / tamano_imagen.alto
    {
        size_t pos = contents.find("\"tamano_imagen\"");
        if (pos == std::string::npos) return false;
        size_t open = contents.find('{', pos);
        if (open == std::string::npos) return false;
        size_t close = findMatchingBrace(contents, open);
        if (close == std::string::npos) return false;

        auto parseIntAfterKeyInRange = [&](const char* key, int& dst) -> bool {
            std::string k = std::string("\"") + key + "\"";
            size_t kpos = contents.find(k, open);
            if (kpos == std::string::npos || kpos > close) return false;
            size_t cpos = contents.find(':', kpos);
            if (cpos == std::string::npos || cpos > close) return false;
            cpos++;
            size_t ii = cpos;
            return parseIntValue(contents, ii, dst);
        };

        if (!parseIntAfterKeyInRange("ancho", out.imageWidth)) return false;
        if (!parseIntAfterKeyInRange("alto", out.imageHeight)) return false;
    }

    // sprites
    {
        size_t pos = contents.find("\"sprites\"");
        if (pos == std::string::npos) return false;
        size_t open = contents.find('{', pos);
        if (open == std::string::npos) return false;
        size_t close = findMatchingBrace(contents, open);
        if (close == std::string::npos) return false;

        size_t i = open + 1;
        while (i < close)
        {
            skipWs(contents, i);
            if (i < close && contents[i] == ',') { i++; continue; }
            skipWs(contents, i);
            if (i >= close || contents[i] == '}') break;

            std::string spriteName;
            if (!parseQuotedString(contents, i, spriteName)) break;

            skipWs(contents, i);
            if (i >= close || contents[i] != ':') break;
            i++;

            skipWs(contents, i);
            if (i >= close || contents[i] != '{') break;

            size_t frameOpen = i;
            size_t frameClose = findMatchingBrace(contents, frameOpen);
            if (frameClose == std::string::npos || frameClose > close) break;

            SpriteFrame frame;
            auto parseIntInFrame = [&](const char* key, int& dst) -> bool {
                std::string k = std::string("\"") + key + "\"";
                size_t kpos = contents.find(k, frameOpen);
                if (kpos == std::string::npos || kpos > frameClose) return false;
                size_t cpos = contents.find(':', kpos);
                if (cpos == std::string::npos || cpos > frameClose) return false;
                cpos++;
                size_t ii = cpos;
                return parseIntValue(contents, ii, dst);
            };

            if (parseIntInFrame("x", frame.x) &&
                parseIntInFrame("y", frame.y) &&
                parseIntInFrame("w", frame.w) &&
                parseIntInFrame("h", frame.h))
            {
                // Leer campo "walkable" (true si no existe)
                std::string wKey = "\"walkable\"";
                size_t wpos = contents.find(wKey, frameOpen);
                if (wpos != std::string::npos && wpos < frameClose) {
                    size_t cpos = contents.find(':', wpos);
                    if (cpos != std::string::npos && cpos < frameClose) {
                        size_t vpos = cpos + 1;
                        skipWs(contents, vpos);
                        // "true" o "false"
                        frame.walkable = (contents.compare(vpos, 4, "true") == 0);
                    }
                }

                // Leer campo "type" ("floor" si no existe)
                std::string tKey = "\"type\"";
                size_t tpos = contents.find(tKey, frameOpen);
                if (tpos != std::string::npos && tpos < frameClose) {
                    size_t cpos = contents.find(':', tpos);
                    if (cpos != std::string::npos && cpos < frameClose) {
                        size_t vpos = cpos + 1;
                        if (parseQuotedString(contents, vpos, frame.type)) {
                            // OK
                        }
                    }
                }
                // Leer campo "align" ("center" si no existe)
                std::string aKey = "\"align\"";
                size_t apos = contents.find(aKey, frameOpen);
                if (apos != std::string::npos && apos < frameClose) {
                    size_t cpos = contents.find(':', apos);
                    if (cpos != std::string::npos && cpos < frameClose) {
                        size_t vpos = cpos + 1;
                        if (parseQuotedString(contents, vpos, frame.align)) {
                            // OK
                        }
                    }
                }

                out.sprites[spriteName] = frame;
            }

            i = frameClose + 1;
        }
    }

    // animations (opcional)
    {
        size_t pos = contents.find("\"animations\"");
        if (pos != std::string::npos) {
            size_t open = contents.find('{', pos);
            if (open != std::string::npos) {
                size_t close = findMatchingBrace(contents, open);
                if (close != std::string::npos) {
                    
                    // Parse interval (float)
                    std::string iKey = "\"interval\"";
                    size_t ipos = contents.find(iKey, open);
                    if (ipos != std::string::npos && ipos < close) {
                        size_t cpos = contents.find(':', ipos);
                        if (cpos != std::string::npos && cpos < close) {
                            size_t vpos = cpos + 1;
                            skipWs(contents, vpos);
                            // Simple float string to float
                            std::string fStr;
                            while(vpos < close && (std::isdigit(contents[vpos]) || contents[vpos] == '.' || contents[vpos] == '-')) {
                                fStr += contents[vpos];
                                vpos++;
                            }
                            if (!fStr.empty()) {
                                out.animInterval = std::stof(fStr);
                            }
                        }
                    }

                    // Parse swaps
                    size_t swapsPos = contents.find("\"swaps\"", open);
                    if (swapsPos != std::string::npos && swapsPos < close) {
                        size_t sOpen = contents.find('{', swapsPos);
                        if (sOpen != std::string::npos && sOpen < close) {
                            size_t sClose = findMatchingBrace(contents, sOpen);
                            if (sClose != std::string::npos && sClose <= close) {
                                size_t si = sOpen + 1;
                                while (si < sClose) {
                                    skipWs(contents, si);
                                    if (si < sClose && contents[si] == ',') { si++; continue; }
                                    if (si >= sClose || contents[si] == '}') break;

                                    std::string keyStr;
                                    if (!parseQuotedString(contents, si, keyStr)) break;
                                    
                                    skipWs(contents, si);
                                    if (si >= sClose || contents[si] != ':') break;
                                    si++;
                                    
                                    int valInt;
                                    if (!parseIntValue(contents, si, valInt)) break;

                                    try {
                                        int keyInt = std::stoi(keyStr);
                                        out.animSwaps[keyInt] = valInt;
                                    } catch (...) {}
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return !out.imagePath.empty() && out.imageWidth > 0 && out.imageHeight > 0;
}

// (u0,v0,u1,v1) en 0..1; devuelve false si no existe.
bool getUvRectForSprite(const SpriteAtlas& atlas, const std::string& spriteName, glm::vec4& uvRect)
{
    auto it = atlas.sprites.find(spriteName);
    if (it == atlas.sprites.end()) return false;
    const SpriteFrame& f = it->second;
    if (atlas.imageWidth <= 0 || atlas.imageHeight <= 0) return false;

    // Media texel de inset para evitar muestrear fuera del sprite con GL_NEAREST
    float u0 = (static_cast<float>(f.x) + 0.5f) / static_cast<float>(atlas.imageWidth);
    float v0 = (static_cast<float>(f.y) + 0.5f) / static_cast<float>(atlas.imageHeight);
    float u1 = (static_cast<float>(f.x + f.w) - 0.5f) / static_cast<float>(atlas.imageWidth);
    float v1 = (static_cast<float>(f.y + f.h) - 0.5f) / static_cast<float>(atlas.imageHeight);

    uvRect = glm::vec4(u0, v0, u1, v1);
    return true;
}
