#include "../include/glb_loader.hpp"
#include "../include/external/stb_image.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace {

struct BufferViewInfo {
    int buffer = 0;
    int byteOffset = 0;
    int byteLength = 0;
    int byteStride = 0;
};

struct AccessorInfo {
    int bufferView = -1;
    int byteOffset = 0;
    int componentType = 0;
    int count = 0;
    std::string type;
};

static void setError(std::string* errorMsg, const std::string& message)
{
    if (errorMsg) {
        *errorMsg = message;
    }
}

static bool readBinaryFile(const std::string& path, std::vector<unsigned char>& outBytes)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        outBytes.clear();
        return true;
    }

    outBytes.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(outBytes.data()), size);
    return file.good();
}

static bool readU32LE(const std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t& out)
{
    if (offset + 4 > bytes.size()) {
        return false;
    }

    out = static_cast<std::uint32_t>(bytes[offset]) |
          (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
          (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
          (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    return true;
}

static std::size_t skipWs(const std::string& text, std::size_t pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return pos;
}

static std::size_t findMatching(const std::string& text,
                                std::size_t openPos,
                                char openChar,
                                char closeChar)
{
    if (openPos >= text.size() || text[openPos] != openChar) {
        return std::string::npos;
    }

    int depth = 0;
    bool inString = false;
    bool escape = false;

    for (std::size_t i = openPos; i < text.size(); ++i) {
        const char c = text[i];
        if (inString) {
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
            continue;
        }
        if (c == openChar) {
            ++depth;
            continue;
        }
        if (c == closeChar) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }

    return std::string::npos;
}

static bool extractNamedArray(const std::string& container,
                              const std::string& key,
                              std::string& outArray)
{
    const std::string token = "\"" + key + "\"";
    const std::size_t keyPos = container.find(token);
    if (keyPos == std::string::npos) {
        return false;
    }

    const std::size_t colonPos = container.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const std::size_t arrayStart = skipWs(container, colonPos + 1);
    if (arrayStart >= container.size() || container[arrayStart] != '[') {
        return false;
    }

    const std::size_t arrayEnd = findMatching(container, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos) {
        return false;
    }

    outArray = container.substr(arrayStart, arrayEnd - arrayStart + 1);
    return true;
}

static bool extractNamedObject(const std::string& container,
                               const std::string& key,
                               std::string& outObject)
{
    const std::string token = "\"" + key + "\"";
    const std::size_t keyPos = container.find(token);
    if (keyPos == std::string::npos) {
        return false;
    }

    const std::size_t colonPos = container.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    const std::size_t objectStart = skipWs(container, colonPos + 1);
    if (objectStart >= container.size() || container[objectStart] != '{') {
        return false;
    }

    const std::size_t objectEnd = findMatching(container, objectStart, '{', '}');
    if (objectEnd == std::string::npos) {
        return false;
    }

    outObject = container.substr(objectStart, objectEnd - objectStart + 1);
    return true;
}

static bool extractIntValue(const std::string& object,
                            const std::string& key,
                            int& outValue)
{
    const std::string token = "\"" + key + "\"";
    const std::size_t keyPos = object.find(token);
    if (keyPos == std::string::npos) {
        return false;
    }

    const std::size_t colonPos = object.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    std::size_t pos = skipWs(object, colonPos + 1);
    bool negative = false;
    if (pos < object.size() && object[pos] == '-') {
        negative = true;
        ++pos;
    }

    if (pos >= object.size() || !std::isdigit(static_cast<unsigned char>(object[pos]))) {
        return false;
    }

    long long value = 0;
    while (pos < object.size() && std::isdigit(static_cast<unsigned char>(object[pos]))) {
        value = value * 10 + static_cast<long long>(object[pos] - '0');
        if (value > std::numeric_limits<int>::max()) {
            return false;
        }
        ++pos;
    }

    outValue = negative ? -static_cast<int>(value) : static_cast<int>(value);
    return true;
}

static bool extractStringValue(const std::string& object,
                               const std::string& key,
                               std::string& outValue)
{
    const std::string token = "\"" + key + "\"";
    const std::size_t keyPos = object.find(token);
    if (keyPos == std::string::npos) {
        return false;
    }

    const std::size_t colonPos = object.find(':', keyPos + token.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    std::size_t pos = skipWs(object, colonPos + 1);
    if (pos >= object.size() || object[pos] != '"') {
        return false;
    }

    ++pos;
    std::string result;
    bool escape = false;
    while (pos < object.size()) {
        const char c = object[pos++];
        if (escape) {
            result.push_back(c);
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"') {
            outValue = result;
            return true;
        }
        result.push_back(c);
    }

    return false;
}

static std::vector<std::string> splitObjectsFromArray(const std::string& arrayText)
{
    std::vector<std::string> result;
    if (arrayText.empty() || arrayText.front() != '[') {
        return result;
    }

    std::size_t pos = 1;
    while (pos < arrayText.size()) {
        pos = skipWs(arrayText, pos);
        if (pos >= arrayText.size() || arrayText[pos] == ']') {
            break;
        }
        if (arrayText[pos] == ',') {
            ++pos;
            continue;
        }

        if (arrayText[pos] != '{') {
            ++pos;
            continue;
        }

        const std::size_t endPos = findMatching(arrayText, pos, '{', '}');
        if (endPos == std::string::npos) {
            break;
        }

        result.push_back(arrayText.substr(pos, endPos - pos + 1));
        pos = endPos + 1;
    }

    return result;
}

static bool parseBufferViews(const std::string& jsonText,
                             std::vector<BufferViewInfo>& outViews,
                             std::string* errorMsg)
{
    std::string arrayText;
    if (!extractNamedArray(jsonText, "bufferViews", arrayText)) {
        setError(errorMsg, "GLB JSON sin 'bufferViews'.");
        return false;
    }

    const std::vector<std::string> objects = splitObjectsFromArray(arrayText);
    if (objects.empty()) {
        setError(errorMsg, "'bufferViews' vacio.");
        return false;
    }

    outViews.clear();
    outViews.reserve(objects.size());

    for (const std::string& obj : objects) {
        BufferViewInfo view;
        (void)extractIntValue(obj, "buffer", view.buffer);
        (void)extractIntValue(obj, "byteOffset", view.byteOffset);
        (void)extractIntValue(obj, "byteStride", view.byteStride);
        if (!extractIntValue(obj, "byteLength", view.byteLength)) {
            setError(errorMsg, "bufferView sin byteLength.");
            return false;
        }
        if (view.byteOffset < 0 || view.byteLength <= 0 || view.byteStride < 0) {
            setError(errorMsg, "bufferView con offsets/tamanos invalidos.");
            return false;
        }
        outViews.push_back(view);
    }

    return true;
}

static bool parseAccessors(const std::string& jsonText,
                           std::vector<AccessorInfo>& outAccessors,
                           std::string* errorMsg)
{
    std::string arrayText;
    if (!extractNamedArray(jsonText, "accessors", arrayText)) {
        setError(errorMsg, "GLB JSON sin 'accessors'.");
        return false;
    }

    const std::vector<std::string> objects = splitObjectsFromArray(arrayText);
    if (objects.empty()) {
        setError(errorMsg, "'accessors' vacio.");
        return false;
    }

    outAccessors.clear();
    outAccessors.reserve(objects.size());

    for (const std::string& obj : objects) {
        AccessorInfo accessor;
        if (!extractIntValue(obj, "bufferView", accessor.bufferView)) {
            setError(errorMsg, "accessor sin bufferView (sparse no soportado). ");
            return false;
        }
        (void)extractIntValue(obj, "byteOffset", accessor.byteOffset);
        if (!extractIntValue(obj, "componentType", accessor.componentType) ||
            !extractIntValue(obj, "count", accessor.count) ||
            !extractStringValue(obj, "type", accessor.type)) {
            setError(errorMsg, "accessor incompleto.");
            return false;
        }

        if (accessor.bufferView < 0 || accessor.byteOffset < 0 || accessor.count <= 0) {
            setError(errorMsg, "accessor con valores invalidos.");
            return false;
        }

        outAccessors.push_back(accessor);
    }

    return true;
}

static bool parseFirstPrimitiveAccessorIndices(const std::string& jsonText,
                                               int& outPositionAccessor,
                                               int& outIndicesAccessor,
                                               std::string* errorMsg)
{
    outPositionAccessor = -1;
    outIndicesAccessor = -1;

    std::string meshesArray;
    if (!extractNamedArray(jsonText, "meshes", meshesArray)) {
        setError(errorMsg, "GLB JSON sin 'meshes'.");
        return false;
    }

    const std::vector<std::string> meshObjects = splitObjectsFromArray(meshesArray);
    if (meshObjects.empty()) {
        setError(errorMsg, "No hay meshes en el GLB.");
        return false;
    }

    for (const std::string& meshObj : meshObjects) {
        std::string primitivesArray;
        if (!extractNamedArray(meshObj, "primitives", primitivesArray)) {
            continue;
        }

        const std::vector<std::string> primitiveObjects = splitObjectsFromArray(primitivesArray);
        for (const std::string& primitiveObj : primitiveObjects) {
            std::string attributesObj;
            int positionAccessor = -1;
            int indicesAccessor = -1;

            if (!extractNamedObject(primitiveObj, "attributes", attributesObj)) {
                continue;
            }
            if (!extractIntValue(attributesObj, "POSITION", positionAccessor)) {
                continue;
            }
            (void)extractIntValue(primitiveObj, "indices", indicesAccessor);

            outPositionAccessor = positionAccessor;
            outIndicesAccessor = indicesAccessor;
            return true;
        }
    }

    setError(errorMsg, "No se encontro primitive con atributo POSITION.");
    return false;
}

static bool parseFirstPrimitiveDetails(const std::string& jsonText,
                                       int& outPositionAccessor,
                                       int& outNormalAccessor,
                                       int& outTexCoordAccessor,
                                       int& outIndicesAccessor,
                                       int& outMaterialIndex,
                                       std::string* errorMsg)
{
    outPositionAccessor = -1;
    outNormalAccessor = -1;
    outTexCoordAccessor = -1;
    outIndicesAccessor = -1;
    outMaterialIndex = -1;

    std::string meshesArray;
    if (!extractNamedArray(jsonText, "meshes", meshesArray)) {
        setError(errorMsg, "GLB JSON sin 'meshes'.");
        return false;
    }

    const std::vector<std::string> meshObjects = splitObjectsFromArray(meshesArray);
    for (const std::string& meshObj : meshObjects) {
        std::string primitivesArray;
        if (!extractNamedArray(meshObj, "primitives", primitivesArray)) {
            continue;
        }

        const std::vector<std::string> primitiveObjects = splitObjectsFromArray(primitivesArray);
        for (const std::string& primitiveObj : primitiveObjects) {
            std::string attributesObj;
            int positionAccessor = -1;
            int normalAccessor = -1;
            int texcoordAccessor = -1;
            int indicesAccessor = -1;
            int materialIndex = -1;

            if (!extractNamedObject(primitiveObj, "attributes", attributesObj)) {
                continue;
            }
            if (!extractIntValue(attributesObj, "POSITION", positionAccessor)) {
                continue;
            }

            (void)extractIntValue(attributesObj, "NORMAL", normalAccessor);
            (void)extractIntValue(attributesObj, "TEXCOORD_0", texcoordAccessor);
            (void)extractIntValue(primitiveObj, "indices", indicesAccessor);
            (void)extractIntValue(primitiveObj, "material", materialIndex);

            outPositionAccessor = positionAccessor;
            outNormalAccessor = normalAccessor;
            outTexCoordAccessor = texcoordAccessor;
            outIndicesAccessor = indicesAccessor;
            outMaterialIndex = materialIndex;
            return true;
        }
    }

    setError(errorMsg, "No se encontro primitive con POSITION para malla texturizada.");
    return false;
}

static std::size_t componentByteSize(int componentType)
{
    switch (componentType) {
        case 5120: return 1; // BYTE
        case 5121: return 1; // UNSIGNED_BYTE
        case 5122: return 2; // SHORT
        case 5123: return 2; // UNSIGNED_SHORT
        case 5125: return 4; // UNSIGNED_INT
        case 5126: return 4; // FLOAT
        default: return 0;
    }
}

static std::size_t componentCountForType(const std::string& type)
{
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    return 0;
}

static bool readAccessorFloatComponents(const std::vector<unsigned char>& binChunk,
                                        const std::vector<BufferViewInfo>& bufferViews,
                                        const std::vector<AccessorInfo>& accessors,
                                        int accessorIndex,
                                        const std::string& expectedType,
                                        std::size_t expectedComponents,
                                        std::vector<GLfloat>& outData,
                                        std::string* errorMsg)
{
    outData.clear();

    if (accessorIndex < 0 || static_cast<std::size_t>(accessorIndex) >= accessors.size()) {
        setError(errorMsg, "Indice de accessor float invalido.");
        return false;
    }

    const AccessorInfo& accessor = accessors[accessorIndex];
    if (accessor.componentType != 5126) {
        setError(errorMsg, "Accessor esperado como FLOAT.");
        return false;
    }

    const std::size_t actualComponents = componentCountForType(accessor.type);
    if (accessor.type != expectedType || actualComponents != expectedComponents) {
        setError(errorMsg, "Accessor float con tipo inesperado.");
        return false;
    }

    if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= bufferViews.size()) {
        setError(errorMsg, "bufferView de accessor float invalido.");
        return false;
    }

    const BufferViewInfo& view = bufferViews[accessor.bufferView];
    const std::size_t elementSize = expectedComponents * sizeof(float);
    const std::size_t stride = (view.byteStride > 0) ? static_cast<std::size_t>(view.byteStride) : elementSize;
    if (stride < elementSize) {
        setError(errorMsg, "byteStride invalido para accessor float.");
        return false;
    }

    const std::size_t viewOffset = static_cast<std::size_t>(view.byteOffset);
    const std::size_t accessorOffset = static_cast<std::size_t>(accessor.byteOffset);
    const std::size_t dataStart = viewOffset + accessorOffset;
    const std::size_t viewEnd = viewOffset + static_cast<std::size_t>(view.byteLength);

    outData.assign(static_cast<std::size_t>(accessor.count) * expectedComponents, 0.0f);
    for (int i = 0; i < accessor.count; ++i) {
        const std::size_t elementOffset = dataStart + static_cast<std::size_t>(i) * stride;
        if (elementOffset + elementSize > binChunk.size() || elementOffset + elementSize > viewEnd) {
            setError(errorMsg, "Lectura fuera de rango en accessor float.");
            return false;
        }

        std::memcpy(&outData[static_cast<std::size_t>(i) * expectedComponents],
                    &binChunk[elementOffset],
                    elementSize);
    }

    return true;
}

static bool readBufferViewRawBytes(const std::vector<unsigned char>& binChunk,
                                   const std::vector<BufferViewInfo>& bufferViews,
                                   int bufferViewIndex,
                                   std::vector<unsigned char>& outBytes,
                                   std::string* errorMsg)
{
    outBytes.clear();

    if (bufferViewIndex < 0 || static_cast<std::size_t>(bufferViewIndex) >= bufferViews.size()) {
        setError(errorMsg, "bufferView de imagen invalido.");
        return false;
    }

    const BufferViewInfo& view = bufferViews[bufferViewIndex];
    const std::size_t offset = static_cast<std::size_t>(view.byteOffset);
    const std::size_t length = static_cast<std::size_t>(view.byteLength);

    if (offset + length > binChunk.size()) {
        setError(errorMsg, "bufferView de imagen fuera de rango en BIN chunk.");
        return false;
    }

    outBytes.assign(binChunk.begin() + static_cast<std::ptrdiff_t>(offset),
                    binChunk.begin() + static_cast<std::ptrdiff_t>(offset + length));
    return true;
}

static bool extractBaseColorImageBytes(const std::string& jsonText,
                                       const std::vector<unsigned char>& binChunk,
                                       const std::vector<BufferViewInfo>& bufferViews,
                                       int materialIndex,
                                       std::vector<unsigned char>& outImageBytes)
{
    outImageBytes.clear();
    if (materialIndex < 0) {
        return false;
    }

    std::string materialsArray;
    if (!extractNamedArray(jsonText, "materials", materialsArray)) {
        return false;
    }
    const std::vector<std::string> materials = splitObjectsFromArray(materialsArray);
    if (static_cast<std::size_t>(materialIndex) >= materials.size()) {
        return false;
    }

    std::string pbrObj;
    if (!extractNamedObject(materials[static_cast<std::size_t>(materialIndex)], "pbrMetallicRoughness", pbrObj)) {
        return false;
    }

    std::string baseColorTextureObj;
    int textureIndex = -1;
    if (!extractNamedObject(pbrObj, "baseColorTexture", baseColorTextureObj) ||
        !extractIntValue(baseColorTextureObj, "index", textureIndex) ||
        textureIndex < 0) {
        return false;
    }

    std::string texturesArray;
    if (!extractNamedArray(jsonText, "textures", texturesArray)) {
        return false;
    }
    const std::vector<std::string> textures = splitObjectsFromArray(texturesArray);
    if (static_cast<std::size_t>(textureIndex) >= textures.size()) {
        return false;
    }

    int sourceImageIndex = -1;
    if (!extractIntValue(textures[static_cast<std::size_t>(textureIndex)], "source", sourceImageIndex) ||
        sourceImageIndex < 0) {
        return false;
    }

    std::string imagesArray;
    if (!extractNamedArray(jsonText, "images", imagesArray)) {
        return false;
    }
    const std::vector<std::string> images = splitObjectsFromArray(imagesArray);
    if (static_cast<std::size_t>(sourceImageIndex) >= images.size()) {
        return false;
    }

    int imageBufferView = -1;
    if (!extractIntValue(images[static_cast<std::size_t>(sourceImageIndex)], "bufferView", imageBufferView) ||
        imageBufferView < 0) {
        return false;
    }

    std::string ignoredError;
    return readBufferViewRawBytes(binChunk, bufferViews, imageBufferView, outImageBytes, &ignoredError);
}

static bool readAccessorPositions(const std::vector<unsigned char>& binChunk,
                                  const std::vector<BufferViewInfo>& bufferViews,
                                  const std::vector<AccessorInfo>& accessors,
                                  int accessorIndex,
                                  std::vector<GLfloat>& outPositions,
                                  std::string* errorMsg)
{
    if (accessorIndex < 0 || static_cast<std::size_t>(accessorIndex) >= accessors.size()) {
        setError(errorMsg, "Indice de accessor POSITION invalido.");
        return false;
    }

    const AccessorInfo& accessor = accessors[accessorIndex];
    if (accessor.componentType != 5126 || accessor.type != "VEC3") {
        setError(errorMsg, "POSITION debe ser FLOAT VEC3.");
        return false;
    }

    if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= bufferViews.size()) {
        setError(errorMsg, "bufferView de POSITION invalido.");
        return false;
    }

    const BufferViewInfo& view = bufferViews[accessor.bufferView];
    const std::size_t elemSize = sizeof(float) * 3;
    const std::size_t stride = (view.byteStride > 0) ? static_cast<std::size_t>(view.byteStride) : elemSize;
    if (stride < elemSize) {
        setError(errorMsg, "byteStride invalido para POSITION.");
        return false;
    }

    const std::size_t viewOffset = static_cast<std::size_t>(view.byteOffset);
    const std::size_t accessorOffset = static_cast<std::size_t>(accessor.byteOffset);
    const std::size_t dataStart = viewOffset + accessorOffset;
    const std::size_t viewEnd = viewOffset + static_cast<std::size_t>(view.byteLength);

    outPositions.assign(static_cast<std::size_t>(accessor.count) * 3, 0.0f);

    for (int i = 0; i < accessor.count; ++i) {
        const std::size_t elemOffset = dataStart + static_cast<std::size_t>(i) * stride;
        if (elemOffset + elemSize > binChunk.size() || elemOffset + elemSize > viewEnd) {
            setError(errorMsg, "Lectura fuera de rango en POSITION.");
            return false;
        }

        float values[3] = {0.0f, 0.0f, 0.0f};
        std::memcpy(values, &binChunk[elemOffset], elemSize);

        outPositions[static_cast<std::size_t>(i) * 3 + 0] = static_cast<GLfloat>(values[0]);
        outPositions[static_cast<std::size_t>(i) * 3 + 1] = static_cast<GLfloat>(values[1]);
        outPositions[static_cast<std::size_t>(i) * 3 + 2] = static_cast<GLfloat>(values[2]);
    }

    return true;
}

static bool readAccessorIndices(const std::vector<unsigned char>& binChunk,
                                const std::vector<BufferViewInfo>& bufferViews,
                                const std::vector<AccessorInfo>& accessors,
                                int accessorIndex,
                                std::vector<GLuint>& outIndices,
                                std::string* errorMsg)
{
    outIndices.clear();

    if (accessorIndex < 0) {
        return true;
    }

    if (static_cast<std::size_t>(accessorIndex) >= accessors.size()) {
        setError(errorMsg, "Indice de accessor de indices invalido.");
        return false;
    }

    const AccessorInfo& accessor = accessors[accessorIndex];
    if (accessor.type != "SCALAR") {
        setError(errorMsg, "Indices deben ser SCALAR.");
        return false;
    }

    if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= bufferViews.size()) {
        setError(errorMsg, "bufferView de indices invalido.");
        return false;
    }

    const std::size_t componentSize = componentByteSize(accessor.componentType);
    if (componentSize == 0) {
        setError(errorMsg, "componentType de indices no soportado.");
        return false;
    }

    const BufferViewInfo& view = bufferViews[accessor.bufferView];
    const std::size_t stride = (view.byteStride > 0) ? static_cast<std::size_t>(view.byteStride) : componentSize;
    if (stride < componentSize) {
        setError(errorMsg, "byteStride invalido para indices.");
        return false;
    }

    const std::size_t viewOffset = static_cast<std::size_t>(view.byteOffset);
    const std::size_t accessorOffset = static_cast<std::size_t>(accessor.byteOffset);
    const std::size_t dataStart = viewOffset + accessorOffset;
    const std::size_t viewEnd = viewOffset + static_cast<std::size_t>(view.byteLength);

    outIndices.resize(static_cast<std::size_t>(accessor.count));

    for (int i = 0; i < accessor.count; ++i) {
        const std::size_t elemOffset = dataStart + static_cast<std::size_t>(i) * stride;
        if (elemOffset + componentSize > binChunk.size() || elemOffset + componentSize > viewEnd) {
            setError(errorMsg, "Lectura fuera de rango en indices.");
            return false;
        }

        GLuint indexValue = 0;
        switch (accessor.componentType) {
            case 5121: {
                indexValue = static_cast<GLuint>(binChunk[elemOffset]);
                break;
            }
            case 5123: {
                std::uint16_t v = 0;
                std::memcpy(&v, &binChunk[elemOffset], sizeof(v));
                indexValue = static_cast<GLuint>(v);
                break;
            }
            case 5125: {
                std::uint32_t v = 0;
                std::memcpy(&v, &binChunk[elemOffset], sizeof(v));
                indexValue = static_cast<GLuint>(v);
                break;
            }
            default: {
                setError(errorMsg, "componentType de indices no soportado en runtime.");
                return false;
            }
        }

        outIndices[static_cast<std::size_t>(i)] = indexValue;
    }

    return true;
}

static void normalizeMesh(SimpleMeshData& mesh)
{
    if (mesh.positions.empty()) {
        return;
    }

    const std::size_t vertexCount = mesh.positions.size() / 3;
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (std::size_t i = 0; i < vertexCount; ++i) {
        const float x = mesh.positions[i * 3 + 0];
        const float y = mesh.positions[i * 3 + 1];
        const float z = mesh.positions[i * 3 + 2];
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        minZ = std::min(minZ, z);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        maxZ = std::max(maxZ, z);
    }

    const float centerX = (minX + maxX) * 0.5f;
    const float centerY = (minY + maxY) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;

    const float extentX = maxX - minX;
    const float extentY = maxY - minY;
    const float extentZ = maxZ - minZ;
    const float maxExtent = std::max(0.0001f, std::max(extentX, std::max(extentY, extentZ)));

    for (std::size_t i = 0; i < vertexCount; ++i) {
        mesh.positions[i * 3 + 0] = (mesh.positions[i * 3 + 0] - centerX) / maxExtent;
        mesh.positions[i * 3 + 1] = (mesh.positions[i * 3 + 1] - centerY) / maxExtent;
        mesh.positions[i * 3 + 2] = (mesh.positions[i * 3 + 2] - centerZ) / maxExtent;
    }

    float normalizedMinY = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < vertexCount; ++i) {
        normalizedMinY = std::min(normalizedMinY, mesh.positions[i * 3 + 1]);
    }

    for (std::size_t i = 0; i < vertexCount; ++i) {
        mesh.positions[i * 3 + 1] -= normalizedMinY;
    }
}

} // namespace

bool loadGlbMeshPositions(const std::string& path, SimpleMeshData& outMesh, std::string* errorMsg)
{
    outMesh.positions.clear();
    outMesh.indices.clear();

    std::vector<unsigned char> fileBytes;
    if (!readBinaryFile(path, fileBytes)) {
        setError(errorMsg, "No se pudo abrir el archivo GLB: " + path);
        return false;
    }

    if (fileBytes.size() < 20) {
        setError(errorMsg, "Archivo GLB demasiado pequeno.");
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t declaredLength = 0;
    if (!readU32LE(fileBytes, 0, magic) ||
        !readU32LE(fileBytes, 4, version) ||
        !readU32LE(fileBytes, 8, declaredLength)) {
        setError(errorMsg, "No se pudo leer header GLB.");
        return false;
    }

    const std::uint32_t kGlbMagic = 0x46546C67; // 'glTF'
    if (magic != kGlbMagic) {
        setError(errorMsg, "Magic GLB invalido.");
        return false;
    }
    if (version != 2) {
        setError(errorMsg, "Solo se soporta GLB version 2.");
        return false;
    }
    if (declaredLength > fileBytes.size()) {
        setError(errorMsg, "Longitud declarada del GLB fuera de rango.");
        return false;
    }

    std::string jsonChunk;
    std::vector<unsigned char> binChunk;

    std::size_t offset = 12;
    while (offset + 8 <= fileBytes.size()) {
        std::uint32_t chunkLength = 0;
        std::uint32_t chunkType = 0;
        if (!readU32LE(fileBytes, offset, chunkLength) || !readU32LE(fileBytes, offset + 4, chunkType)) {
            setError(errorMsg, "No se pudo leer encabezado de chunk GLB.");
            return false;
        }
        offset += 8;

        if (offset + static_cast<std::size_t>(chunkLength) > fileBytes.size()) {
            setError(errorMsg, "Chunk GLB fuera de rango.");
            return false;
        }

        const std::uint32_t kJsonChunkType = 0x4E4F534A; // 'JSON'
        const std::uint32_t kBinChunkType = 0x004E4942;  // 'BIN\0'

        if (chunkType == kJsonChunkType) {
            jsonChunk.assign(reinterpret_cast<const char*>(&fileBytes[offset]),
                             reinterpret_cast<const char*>(&fileBytes[offset + chunkLength]));
        } else if (chunkType == kBinChunkType) {
            binChunk.assign(fileBytes.begin() + static_cast<std::ptrdiff_t>(offset),
                            fileBytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }

        offset += static_cast<std::size_t>(chunkLength);
    }

    if (jsonChunk.empty() || binChunk.empty()) {
        setError(errorMsg, "GLB incompleto: falta chunk JSON o BIN.");
        return false;
    }

    while (!jsonChunk.empty() &&
           (jsonChunk.back() == '\0' || std::isspace(static_cast<unsigned char>(jsonChunk.back())))) {
        jsonChunk.pop_back();
    }

    std::vector<BufferViewInfo> bufferViews;
    std::vector<AccessorInfo> accessors;
    if (!parseBufferViews(jsonChunk, bufferViews, errorMsg) ||
        !parseAccessors(jsonChunk, accessors, errorMsg)) {
        return false;
    }

    int positionAccessor = -1;
    int indicesAccessor = -1;
    if (!parseFirstPrimitiveAccessorIndices(jsonChunk, positionAccessor, indicesAccessor, errorMsg)) {
        return false;
    }

    if (!readAccessorPositions(binChunk, bufferViews, accessors, positionAccessor, outMesh.positions, errorMsg)) {
        return false;
    }

    if (!readAccessorIndices(binChunk, bufferViews, accessors, indicesAccessor, outMesh.indices, errorMsg)) {
        return false;
    }

    if (outMesh.positions.empty()) {
        setError(errorMsg, "El GLB no contiene vertices POSITION.");
        return false;
    }

    if (outMesh.indices.empty()) {
        const std::size_t vertexCount = outMesh.positions.size() / 3;
        outMesh.indices.resize(vertexCount);
        for (std::size_t i = 0; i < vertexCount; ++i) {
            outMesh.indices[i] = static_cast<GLuint>(i);
        }
    }

    normalizeMesh(outMesh);
    return true;
}

bool loadGlbTexturedMesh(const std::string& path, TexturedMeshData& outMesh, std::string* errorMsg)
{
    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.baseColorRgba.clear();
    outMesh.textureWidth = 0;
    outMesh.textureHeight = 0;

    std::vector<unsigned char> fileBytes;
    if (!readBinaryFile(path, fileBytes)) {
        setError(errorMsg, "No se pudo abrir el archivo GLB: " + path);
        return false;
    }

    if (fileBytes.size() < 20) {
        setError(errorMsg, "Archivo GLB demasiado pequeno.");
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t declaredLength = 0;
    if (!readU32LE(fileBytes, 0, magic) ||
        !readU32LE(fileBytes, 4, version) ||
        !readU32LE(fileBytes, 8, declaredLength)) {
        setError(errorMsg, "No se pudo leer header GLB.");
        return false;
    }

    const std::uint32_t kGlbMagic = 0x46546C67; // 'glTF'
    if (magic != kGlbMagic) {
        setError(errorMsg, "Magic GLB invalido.");
        return false;
    }
    if (version != 2) {
        setError(errorMsg, "Solo se soporta GLB version 2.");
        return false;
    }
    if (declaredLength > fileBytes.size()) {
        setError(errorMsg, "Longitud declarada del GLB fuera de rango.");
        return false;
    }

    std::string jsonChunk;
    std::vector<unsigned char> binChunk;

    std::size_t offset = 12;
    while (offset + 8 <= fileBytes.size()) {
        std::uint32_t chunkLength = 0;
        std::uint32_t chunkType = 0;
        if (!readU32LE(fileBytes, offset, chunkLength) || !readU32LE(fileBytes, offset + 4, chunkType)) {
            setError(errorMsg, "No se pudo leer encabezado de chunk GLB.");
            return false;
        }
        offset += 8;

        if (offset + static_cast<std::size_t>(chunkLength) > fileBytes.size()) {
            setError(errorMsg, "Chunk GLB fuera de rango.");
            return false;
        }

        const std::uint32_t kJsonChunkType = 0x4E4F534A; // 'JSON'
        const std::uint32_t kBinChunkType = 0x004E4942;  // 'BIN\0'

        if (chunkType == kJsonChunkType) {
            jsonChunk.assign(reinterpret_cast<const char*>(&fileBytes[offset]),
                             reinterpret_cast<const char*>(&fileBytes[offset + chunkLength]));
        } else if (chunkType == kBinChunkType) {
            binChunk.assign(fileBytes.begin() + static_cast<std::ptrdiff_t>(offset),
                            fileBytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }

        offset += static_cast<std::size_t>(chunkLength);
    }

    if (jsonChunk.empty() || binChunk.empty()) {
        setError(errorMsg, "GLB incompleto: falta chunk JSON o BIN.");
        return false;
    }

    while (!jsonChunk.empty() &&
           (jsonChunk.back() == '\0' || std::isspace(static_cast<unsigned char>(jsonChunk.back())))) {
        jsonChunk.pop_back();
    }

    std::vector<BufferViewInfo> bufferViews;
    std::vector<AccessorInfo> accessors;
    if (!parseBufferViews(jsonChunk, bufferViews, errorMsg) ||
        !parseAccessors(jsonChunk, accessors, errorMsg)) {
        return false;
    }

    int positionAccessor = -1;
    int normalAccessor = -1;
    int texcoordAccessor = -1;
    int indicesAccessor = -1;
    int materialIndex = -1;
    if (!parseFirstPrimitiveDetails(jsonChunk,
                                    positionAccessor,
                                    normalAccessor,
                                    texcoordAccessor,
                                    indicesAccessor,
                                    materialIndex,
                                    errorMsg)) {
        return false;
    }

    SimpleMeshData normalizedBase;
    if (!readAccessorPositions(binChunk, bufferViews, accessors, positionAccessor, normalizedBase.positions, errorMsg)) {
        return false;
    }
    if (!readAccessorIndices(binChunk, bufferViews, accessors, indicesAccessor, normalizedBase.indices, errorMsg)) {
        return false;
    }
    if (normalizedBase.positions.empty()) {
        setError(errorMsg, "El GLB no contiene vertices POSITION.");
        return false;
    }
    if (normalizedBase.indices.empty()) {
        const std::size_t vertexCount = normalizedBase.positions.size() / 3;
        normalizedBase.indices.resize(vertexCount);
        for (std::size_t i = 0; i < vertexCount; ++i) {
            normalizedBase.indices[i] = static_cast<GLuint>(i);
        }
    }
    normalizeMesh(normalizedBase);

    std::vector<GLfloat> normals;
    std::vector<GLfloat> texcoords;
    std::string ignoredError;
    const bool hasNormals = (normalAccessor >= 0)
        ? readAccessorFloatComponents(binChunk, bufferViews, accessors, normalAccessor, "VEC3", 3, normals, &ignoredError)
        : false;
    const bool hasTexcoords = (texcoordAccessor >= 0)
        ? readAccessorFloatComponents(binChunk, bufferViews, accessors, texcoordAccessor, "VEC2", 2, texcoords, &ignoredError)
        : false;

    const std::size_t vertexCount = normalizedBase.positions.size() / 3;
    const bool normalsMatch = hasNormals && normals.size() == vertexCount * 3;
    const bool texcoordsMatch = hasTexcoords && texcoords.size() == vertexCount * 2;

    outMesh.vertices.resize(vertexCount * 8, 0.0f);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        const std::size_t dst = i * 8;
        outMesh.vertices[dst + 0] = normalizedBase.positions[i * 3 + 0];
        outMesh.vertices[dst + 1] = normalizedBase.positions[i * 3 + 1];
        outMesh.vertices[dst + 2] = normalizedBase.positions[i * 3 + 2];

        outMesh.vertices[dst + 3] = normalsMatch ? normals[i * 3 + 0] : 0.0f;
        outMesh.vertices[dst + 4] = normalsMatch ? normals[i * 3 + 1] : 1.0f;
        outMesh.vertices[dst + 5] = normalsMatch ? normals[i * 3 + 2] : 0.0f;

        const float u = texcoordsMatch ? texcoords[i * 2 + 0] : 0.0f;
        const float v = texcoordsMatch ? texcoords[i * 2 + 1] : 0.0f;
        outMesh.vertices[dst + 6] = u;
        outMesh.vertices[dst + 7] = v;
    }

    outMesh.indices = normalizedBase.indices;

    std::vector<unsigned char> imageBytes;
    if (extractBaseColorImageBytes(jsonChunk, binChunk, bufferViews, materialIndex, imageBytes) && !imageBytes.empty()) {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* rgba = stbi_load_from_memory(imageBytes.data(),
                                              static_cast<int>(imageBytes.size()),
                                              &width,
                                              &height,
                                              &channels,
                                              STBI_rgb_alpha);
        if (rgba != nullptr && width > 0 && height > 0) {
            outMesh.textureWidth = width;
            outMesh.textureHeight = height;
            outMesh.baseColorRgba.assign(rgba, rgba + (width * height * 4));
        }
        if (rgba != nullptr) {
            stbi_image_free(rgba);
        }
    }

    if (outMesh.baseColorRgba.empty()) {
        outMesh.textureWidth = 1;
        outMesh.textureHeight = 1;
        outMesh.baseColorRgba = {255, 255, 255, 255};
    }

    return true;
}
