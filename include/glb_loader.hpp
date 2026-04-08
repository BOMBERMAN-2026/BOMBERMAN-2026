#ifndef GLB_LOADER_HPP
#define GLB_LOADER_HPP

#include <GL/glew.h>

#include <string>
#include <vector>

struct SimpleMeshData {
    std::vector<GLfloat> positions;
    std::vector<GLuint> indices;
};

struct TexturedMeshData {
    // Interleaved: position(3), normal(3), uv(2)
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    // RGBA8 data for baseColor texture extracted from GLB
    std::vector<unsigned char> baseColorRgba;
    int textureWidth = 0;
    int textureHeight = 0;
};

// Carga un .glb (glTF 2.0 binario) y extrae el primer primitive POSITION+indices.
// Devuelve false y rellena errorMsg en caso de fallo.
bool loadGlbMeshPositions(const std::string& path, SimpleMeshData& outMesh, std::string* errorMsg = nullptr);

// Carga un .glb y extrae malla con POSITION/NORMAL/TEXCOORD_0 + textura baseColor del material.
bool loadGlbTexturedMesh(const std::string& path, TexturedMeshData& outMesh, std::string* errorMsg = nullptr);

#endif // GLB_LOADER_HPP
