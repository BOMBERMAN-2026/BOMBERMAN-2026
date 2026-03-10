/*
 * atlas_preview_sheet.cpp
 *
 * Crea una imagen “contact sheet” para comprobar recortes de un atlas.
 *
 * Entrada:
 *   atlas_preview_sheet <atlas.json>
 *
 * Lee del JSON:
 *   - ruta_imagen (ruta al spritesheet)
 *   - sprites (objeto: nombre -> {x,y,w,h})
 *
 * Salida (en la misma carpeta del JSON):
 *   <atlas>_preview.png
 *
 * Dependencias:
 *   - include/external/stb_image.h
 *   - include/external/stb_image_write.h
 */

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <cctype>

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image_write.h"

namespace fs = std::filesystem;

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct SpriteEntry {
    std::string name;
    Rect r;
};

struct ImageRGBA {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> px; // RGBA

    std::uint8_t* at(int x, int y) {
        return &px[(y * w + x) * 4];
    }

    const std::uint8_t* at(int x, int y) const {
        return &px[(y * w + x) * 4];
    }
};

static void fail(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

static std::string readAllText(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    fail(f.is_open(), "No se pudo abrir el JSON: " + path.string());
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static void skipWs(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

static std::size_t findQuotedKey(const std::string& s, const std::string& key, std::size_t start = 0) {
    const std::string needle = "\"" + key + "\"";
    return s.find(needle, start);
}

static std::size_t posAfterColon(const std::string& s, std::size_t keyPos, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    fail(keyPos != std::string::npos, "No se encontró la clave JSON: " + key);
    std::size_t i = keyPos + needle.size();
    std::size_t colon = s.find(':', i);
    fail(colon != std::string::npos, "JSON mal formado: falta ':' tras la clave " + key);
    i = colon + 1;
    skipWs(s, i);
    fail(i < s.size(), "JSON mal formado: valor vacío para la clave " + key);
    return i;
}

static std::string sliceBalanced(const std::string& s, std::size_t startPos, char openCh, char closeCh, const std::string& what) {
    fail(startPos < s.size() && s[startPos] == openCh,
         "JSON mal formado: se esperaba '" + std::string(1, openCh) + "' para " + what);

    bool inString = false;
    int depth = 0;

    for (std::size_t i = startPos; i < s.size(); ++i) {
        const char ch = s[i];

        if (inString) {
            if (ch == '\\') { ++i; continue; }
            if (ch == '"') inString = false;
            continue;
        }

        if (ch == '"') { inString = true; continue; }

        if (ch == openCh) {
            ++depth;
        } else if (ch == closeCh) {
            --depth;
            if (depth == 0) {
                return s.substr(startPos, i - startPos + 1);
            }
        }
    }

    throw std::runtime_error("JSON mal formado: no se cerró correctamente " + what);
}

static int parseIntAt(const std::string& s, std::size_t i, const std::string& contextKey) {
    skipWs(s, i);
    fail(i < s.size(), "JSON mal formado: número vacío en " + contextKey);

    bool neg = false;
    if (s[i] == '-') { neg = true; ++i; }

    fail(i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])),
         "JSON mal formado: se esperaba entero en " + contextKey);

    long long value = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        value = value * 10 + (s[i] - '0');
        ++i;
    }
    if (neg) value = -value;

    fail(value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max(),
         "JSON mal formado: entero fuera de rango en " + contextKey);

    return static_cast<int>(value);
}

static int extractIntKey(const std::string& obj, const std::string& key) {
    std::size_t pos = findQuotedKey(obj, key);
    fail(pos != std::string::npos, "Falta la clave obligatoria: " + key);
    std::size_t vpos = posAfterColon(obj, pos, key);
    return parseIntAt(obj, vpos, key);
}

static std::string parseJsonStringAt(const std::string& s, std::size_t start, const std::string& what) {
    std::size_t i = start;
    skipWs(s, i);
    fail(i < s.size() && s[i] == '"', "JSON mal formado: se esperaba string para " + what);
    ++i;

    std::string out;
    out.reserve(64);

    while (i < s.size()) {
        char ch = s[i++];
        if (ch == '"') break;
        if (ch == '\\') {
            fail(i < s.size(), "JSON mal formado: escape incompleto en " + what);
            char esc = s[i++];
            switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            default:
                // Para este tool no necesitamos \uXXXX.
                throw std::runtime_error("JSON: escape no soportado en " + what);
            }
            continue;
        }
        out.push_back(ch);
    }

    return out;
}

static std::string extractStringKey(const std::string& text, const std::string& key) {
    std::size_t keyPos = findQuotedKey(text, key);
    fail(keyPos != std::string::npos, "El JSON debe contener la clave: " + key);
    std::size_t vpos = posAfterColon(text, keyPos, key);
    return parseJsonStringAt(text, vpos, key);
}

static std::vector<SpriteEntry> extractSprites(const std::string& text) {
    const std::size_t spritesKeyPos = findQuotedKey(text, "sprites");
    fail(spritesKeyPos != std::string::npos, "El JSON debe contener la clave: sprites");

    std::size_t spritesValPos = posAfterColon(text, spritesKeyPos, "sprites");
    const std::string spritesObj = sliceBalanced(text, spritesValPos, '{', '}', "sprites");

    std::vector<SpriteEntry> out;

    // Parse: "name": {"x":...,"y":...,"w":...,"h":...}
    std::size_t i = 0;
    while (i < spritesObj.size()) {
        // buscar próxima clave
        std::size_t quote = spritesObj.find('"', i);
        if (quote == std::string::npos) break;
        std::string name = parseJsonStringAt(spritesObj, quote, "sprite name");

        // saltar hasta ':'
        std::size_t afterName = spritesObj.find('"', quote + 1);
        if (afterName == std::string::npos) break;
        // afterName apunta al cierre de string; buscamos ':' a partir de ahí
        std::size_t colon = spritesObj.find(':', afterName + 1);
        if (colon == std::string::npos) break;
        std::size_t valPos = colon + 1;
        skipWs(spritesObj, valPos);
        if (valPos >= spritesObj.size()) break;

        if (spritesObj[valPos] != '{') {
            i = valPos + 1;
            continue;
        }

        const std::string rectObj = sliceBalanced(spritesObj, valPos, '{', '}', name);
        Rect r;
        r.x = extractIntKey(rectObj, "x");
        r.y = extractIntKey(rectObj, "y");
        r.w = extractIntKey(rectObj, "w");
        r.h = extractIntKey(rectObj, "h");

        out.push_back({name, r});
        i = valPos + rectObj.size();
    }

    fail(!out.empty(), "No se encontraron sprites dentro de sprites{}.");
    return out;
}

static ImageRGBA loadImage(const fs::path& path) {
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &n, 4);
    fail(data != nullptr, "No se pudo cargar la imagen: " + path.string());

    ImageRGBA img;
    img.w = w;
    img.h = h;
    img.px.assign(data, data + (w * h * 4));
    stbi_image_free(data);
    return img;
}

static void savePng(const ImageRGBA& img, const fs::path& path) {
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    int ok = stbi_write_png(path.string().c_str(), img.w, img.h, 4, img.px.data(), img.w * 4);
    fail(ok != 0, "No se pudo guardar el PNG: " + path.string());
}

static void blitSprite(const ImageRGBA& src, const Rect& r, ImageRGBA& dst, int dx, int dy) {
    for (int y = 0; y < r.h; ++y) {
        for (int x = 0; x < r.w; ++x) {
            const std::uint8_t* sp = src.at(r.x + x, r.y + y);
            std::uint8_t* dp = dst.at(dx + x, dy + y);
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
}

static fs::path resolveImagePath(const fs::path& jsonPath, const std::string& rutaImagen) {
    fs::path p(rutaImagen);
    if (p.is_absolute()) return p;

    // 1) relativo al directorio del JSON
    fs::path candidate1 = jsonPath.parent_path() / p;
    if (fs::exists(candidate1)) return candidate1;

    // 2) relativo al cwd
    fs::path candidate2 = fs::current_path() / p;
    if (fs::exists(candidate2)) return candidate2;

    return candidate2; // para el mensaje de error
}

static int ceilDiv(int a, int b) {
    return (a + b - 1) / b;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cout << "Uso:\n"
                      << "  atlas_preview_sheet <atlas.json>\n\n"
                      << "Genera <atlas>_preview.png en la misma carpeta del JSON.\n";
            return 0;
        }

        const fs::path jsonPath = fs::path(argv[1]);
        fail(fs::exists(jsonPath), "No existe el JSON: " + jsonPath.string());

        const std::string text = readAllText(jsonPath);
        const std::string rutaImagen = extractStringKey(text, "ruta_imagen");
        const auto sprites = extractSprites(text);

        fs::path imagePath = resolveImagePath(jsonPath, rutaImagen);
        fail(fs::exists(imagePath), "No existe la imagen indicada por ruta_imagen: " + imagePath.string());

        ImageRGBA sheet = loadImage(imagePath);

        int maxW = 1;
        int maxH = 1;
        for (const auto& s : sprites) {
            fail(s.r.w > 0 && s.r.h > 0, "Sprite con tamaño inválido: " + s.name);
            fail(s.r.x >= 0 && s.r.y >= 0, "Sprite con coordenadas negativas: " + s.name);
            fail(s.r.x + s.r.w <= sheet.w && s.r.y + s.r.h <= sheet.h,
                 "Sprite fuera de límites: " + s.name);
            if (s.r.w > maxW) maxW = s.r.w;
            if (s.r.h > maxH) maxH = s.r.h;
        }

        const int n = static_cast<int>(sprites.size());
        int cols = 1;
        while (cols * cols < n) ++cols; // ceil(sqrt(n)) sin <cmath>
        const int rows = ceilDiv(n, cols);

        ImageRGBA out;
        out.w = cols * maxW;
        out.h = rows * maxH;
        out.px.assign(static_cast<std::size_t>(out.w) * out.h * 4, 0); // fondo transparente

        for (int idx = 0; idx < n; ++idx) {
            const int cx = idx % cols;
            const int cy = idx / cols;
            const int dx = cx * maxW;
            const int dy = cy * maxH;
            blitSprite(sheet, sprites[idx].r, out, dx, dy);
        }

        const fs::path outPath = jsonPath.parent_path() / (jsonPath.stem().string() + "_preview.png");
        savePng(out, outPath);

        std::cout << "Preview generada: " << outPath.string() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
