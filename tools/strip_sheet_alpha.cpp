/*
 * strip_sheet_alpha.cpp
 *
 * Funcionalidad:
 *   - Carga una spritesheet PNG.
 *   - Lee del atlas JSON la lista de colores de croma y la tolerancia.
 *   - Convierte en transparentes los píxeles que coincidan con esos colores.
 *   - Guarda automáticamente una copia con sufijo _alpha.png en la misma ruta.
 *
 * Uso:
 *   strip_sheet_alpha <atlas.json> <imagen_origen.png>
 *
 * Ejemplo:
 *   strip_sheet_alpha SpriteAtlasEnemy.json resources/sprites/BombermanArcade-baddies.png
 *
 * Salida generada automáticamente:
 *   resources/sprites/BombermanArcade-baddies_alpha.png
 *
 * Dependencias:
 *   - stb_image.h
 *   - stb_image_write.h
 */

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cctype>

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image_write.h"

namespace fs = std::filesystem;

struct ColorRGB {
    int r, g, b;
};

struct Config {
    std::vector<ColorRGB> colores;
    int tolerancia = 0;
};

struct ImageRGBA {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> px;

    std::uint8_t* at(int x, int y) {
        return &px[(y * w + x) * 4];
    }

    const std::uint8_t* at(int x, int y) const {
        return &px[(y * w + x) * 4];
    }
};

/**
 * Lanza una excepción si la condición no se cumple.
 *
 * @param cond Condición que debe ser verdadera.
 * @param msg Mensaje de error a lanzar si cond es falsa.
 */
static void fail(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

static std::string readAllText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    fail(f.is_open(), "No se pudo abrir el JSON: " + path);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static void skipWs(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
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

static int parseIntAt(const std::string& s, std::size_t i, const std::string& contextKey) {
    skipWs(s, i);
    fail(i < s.size(), "JSON mal formado: número vacío en " + contextKey);

    bool neg = false;
    if (s[i] == '-') {
        neg = true;
        ++i;
    }
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

static std::string sliceBalanced(const std::string& s, std::size_t startPos, char openCh, char closeCh, const std::string& what) {
    fail(startPos < s.size() && s[startPos] == openCh, "JSON mal formado: se esperaba '" + std::string(1, openCh) + "' para " + what);

    bool inString = false;
    int depth = 0;
    for (std::size_t i = startPos; i < s.size(); ++i) {
        const char ch = s[i];

        if (inString) {
            if (ch == '\\') {
                ++i; // saltar el carácter escapado
                continue;
            }
            if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }

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

static bool tryExtractIntKey(const std::string& obj, const std::string& key, int& out) {
    std::size_t pos = findQuotedKey(obj, key);
    if (pos == std::string::npos) return false;
    std::size_t vpos = posAfterColon(obj, pos, key);
    out = parseIntAt(obj, vpos, key);
    return true;
}

static int extractIntKey(const std::string& obj, const std::string& key) {
    int v = 0;
    fail(tryExtractIntKey(obj, key, v), "Falta la clave obligatoria: " + key);
    return v;
}

/**
 * Carga del atlas JSON la configuración de transparencia.
 *
 * Espera encontrar:
 *   color_transparente.colores -> array de colores RGB
 *   color_transparente.tolerancia -> entero opcional
 *
 * @param jsonPath Ruta al fichero JSON.
 * @return Configuración con los colores a eliminar y la tolerancia.
 */
static Config loadConfig(const std::string& jsonPath) {
    const std::string text = readAllText(jsonPath);

    // Aislamos el objeto "color_transparente" para no confundirnos con otras claves.
    const std::size_t ctKeyPos = findQuotedKey(text, "color_transparente");
    fail(ctKeyPos != std::string::npos, "El JSON debe contener la clave: color_transparente");
    std::size_t ctValPos = posAfterColon(text, ctKeyPos, "color_transparente");
    skipWs(text, ctValPos);
    const std::string ctObj = sliceBalanced(text, ctValPos, '{', '}', "color_transparente");

    Config cfg;
    int tol = 0;
    if (tryExtractIntKey(ctObj, "tolerancia", tol)) {
        cfg.tolerancia = tol;
    }

    const std::size_t coloresPos = findQuotedKey(ctObj, "colores");
    fail(coloresPos != std::string::npos, "El JSON debe tener color_transparente.colores");
    std::size_t arrPos = posAfterColon(ctObj, coloresPos, "colores");
    const std::string coloresArr = sliceBalanced(ctObj, arrPos, '[', ']', "color_transparente.colores");

    // Recorremos objetos { ... } dentro del array.
    bool inString = false;
    for (std::size_t i = 0; i < coloresArr.size(); ++i) {
        const char ch = coloresArr[i];

        if (inString) {
            if (ch == '\\') { ++i; continue; }
            if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }

        if (ch == '{') {
            const std::string obj = sliceBalanced(coloresArr, i, '{', '}', "un color {r,g,b}");
            const int r = extractIntKey(obj, "r");
            const int g = extractIntKey(obj, "g");
            const int b = extractIntKey(obj, "b");
            cfg.colores.push_back({r, g, b});

            i += obj.size() - 1; // saltar el objeto completo
        }
    }

    fail(!cfg.colores.empty(), "No hay colores transparentes definidos en el JSON.");
    return cfg;
}

/**
 * Carga una imagen PNG y la convierte a RGBA.
 *
 * @param path Ruta de la imagen de entrada.
 * @return Imagen cargada en memoria con 4 canales por píxel.
 */
static ImageRGBA loadImage(const std::string& path) {
    int w, h, n;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    fail(data != nullptr, "No se pudo cargar la imagen: " + path);

    ImageRGBA img;
    img.w = w;
    img.h = h;
    img.px.assign(data, data + w * h * 4);
    stbi_image_free(data);
    return img;
}

/**
 * Guarda una imagen RGBA en formato PNG.
 *
 * Si la carpeta de destino no existe, la crea automáticamente.
 *
 * @param img Imagen a guardar.
 * @param path Ruta de salida.
 */
static void saveImage(const ImageRGBA& img, const std::string& path) {
    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    int ok = stbi_write_png(path.c_str(), img.w, img.h, 4, img.px.data(), img.w * 4);
    fail(ok != 0, "No se pudo guardar el PNG: " + path);
}

/**
 * Comprueba si un píxel coincide con un color dado dentro de una tolerancia.
 *
 * La comparación se hace mediante distancia Manhattan en RGB:
 *   |r-cr| + |g-cg| + |b-cb|
 *
 * @param p Puntero al píxel RGBA.
 * @param c Color objetivo.
 * @param tol Tolerancia máxima permitida.
 * @return true si el píxel se considera equivalente al color objetivo.
 */
static bool matchColor(const std::uint8_t* p, const ColorRGB& c, int tol) {
    int d = std::abs((int)p[0] - c.r)
          + std::abs((int)p[1] - c.g)
          + std::abs((int)p[2] - c.b);
    return d <= tol;
}

/**
 * Indica si un píxel debe hacerse transparente según la configuración del atlas.
 *
 * @param p Puntero al píxel RGBA.
 * @param cfg Configuración cargada desde JSON.
 * @return true si coincide con alguno de los colores transparentes.
 */
static bool isTransparentColor(const std::uint8_t* p, const Config& cfg) {
    for (const auto& c : cfg.colores) {
        if (matchColor(p, c, cfg.tolerancia)) {
            return true;
        }
    }
    return false;
}

/**
 * Recorre la imagen completa y pone alpha = 0 en los píxeles de croma.
 *
 * No modifica los valores RGB del píxel, solo el canal alpha.
 *
 * @param img Imagen a procesar.
 * @param cfg Configuración con colores transparentes y tolerancia.
 */
static void removeChroma(ImageRGBA& img, const Config& cfg) {
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            std::uint8_t* p = img.at(x, y);
            if (isTransparentColor(p, cfg)) {
                p[3] = 0;
            }
        }
    }
}

/**
 * Construye la ruta de salida a partir de la imagen original.
 *
 * Ejemplo:
 *   input : resources/sprites/sheet.png
 *   output: resources/sprites/sheet_alpha.png
 *
 * @param inputPath Ruta de la imagen original.
 * @return Ruta de salida con sufijo _alpha.
 */
static std::string buildOutputPath(const std::string& inputPath) {
    fs::path p(inputPath);
    fs::path dir = p.parent_path();
    std::string stem = p.stem().string();
    fs::path out = dir / (stem + "_alpha.png");
    return out.string();
}

/**
 * Punto de entrada principal.
 *
 * Argumentos esperados:
 *   argv[1] -> atlas JSON
 *   argv[2] -> imagen origen PNG
 *
 * @param argc Número de argumentos.
 * @param argv Vector de argumentos.
 * @return 0 si el proceso termina correctamente, 1 si hay error.
 */
int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cout << "Uso:\n"
                      << "  strip_sheet_alpha <atlas.json> <imagen_origen.png>\n\n"
                      << "Ejemplo:\n"
                      << "  strip_sheet_alpha SpriteAtlasEnemy.json resources/sprites/BombermanArcade-baddies.png\n";
            return 0;
        }

        const std::string jsonPath = argv[1];
        const std::string inputPath = argv[2];
        const std::string outPath = buildOutputPath(inputPath);

        Config cfg = loadConfig(jsonPath);
        ImageRGBA img = loadImage(inputPath);

        removeChroma(img, cfg);
        saveImage(img, outPath);

        std::cout << "Imagen generada: " << outPath << "\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
