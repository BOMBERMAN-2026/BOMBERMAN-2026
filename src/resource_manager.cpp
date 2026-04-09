#include "resource_manager.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

std::map<std::string, GLuint> ResourceManager::shaders;
std::map<std::string, GLuint> ResourceManager::textures;
std::map<std::string, MeshResource> ResourceManager::meshes;

static bool readFileText(const std::string& path, std::string& out)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

static bool compileAndLinkProgram(const std::string& vertexPath,
                                  const std::string& fragmentPath,
                                  GLuint& outProgram)
{
    std::string vertexCode;
    std::string fragmentCode;
    if (!readFileText(vertexPath, vertexCode)) {
        std::cerr << "No se pudo leer vertex shader: " << vertexPath << "\n";
        return false;
    }
    if (!readFileText(fragmentPath, fragmentCode)) {
        std::cerr << "No se pudo leer fragment shader: " << fragmentPath << "\n";
        return false;
    }

    auto compileSingle = [](const char* code, GLenum shaderType) -> GLuint {
        GLuint shaderObject = glCreateShader(shaderType);
        glShaderSource(shaderObject, 1, &code, nullptr);
        glCompileShader(shaderObject);

        GLint ok = 0;
        glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char errorLog[1024] = {0};
            glGetShaderInfoLog(shaderObject, sizeof(errorLog), nullptr, errorLog);
            std::cerr << "Error compilando shader " << shaderType << ": " << errorLog << "\n";
            glDeleteShader(shaderObject);
            return 0;
        }
        return shaderObject;
    };

    GLuint vs = compileSingle(vertexCode.c_str(), GL_VERTEX_SHADER);
    GLuint fs = compileSingle(fragmentCode.c_str(), GL_FRAGMENT_SHADER);
    if (vs == 0 || fs == 0) {
        if (vs != 0) glDeleteShader(vs);
        if (fs != 0) glDeleteShader(fs);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char errorLog[1024] = {0};
        glGetProgramInfoLog(program, sizeof(errorLog), nullptr, errorLog);
        std::cerr << "Error enlazando shader program: " << errorLog << "\n";
        glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

GLuint ResourceManager::loadShader(const std::string& name,
                                   const std::string& vertexPath,
                                   const std::string& fragmentPath)
{
    std::map<std::string, GLuint>::const_iterator it = shaders.find(name);
    if (it != shaders.end()) {
        return it->second;
    }

    GLuint program = 0;
    if (!compileAndLinkProgram(vertexPath, fragmentPath, program)) {
        return 0;
    }

    shaders[name] = program;
    return program;
}

GLuint ResourceManager::getShader(const std::string& name)
{
    std::map<std::string, GLuint>::const_iterator it = shaders.find(name);
    return (it != shaders.end()) ? it->second : 0;
}

GLuint ResourceManager::loadTexture(const std::string& name,
                                    const std::string& texturePath,
                                    const std::function<GLuint(const char*)>& loader)
{
    std::map<std::string, GLuint>::const_iterator it = textures.find(name);
    if (it != textures.end()) {
        return it->second;
    }

    const GLuint texture = loader(texturePath.c_str());
    if (texture != 0) {
        textures[name] = texture;
    }
    return texture;
}

GLuint ResourceManager::getTexture(const std::string& name)
{
    std::map<std::string, GLuint>::const_iterator it = textures.find(name);
    return (it != textures.end()) ? it->second : 0;
}

MeshResource* ResourceManager::createMesh(const std::string& name,
                                          const std::function<MeshResource(void)>& builder)
{
    std::map<std::string, MeshResource>::iterator it = meshes.find(name);
    if (it != meshes.end()) {
        return &it->second;
    }

    MeshResource mesh = builder();
    if (mesh.vao == 0) {
        return nullptr;
    }

    std::pair<std::map<std::string, MeshResource>::iterator, bool> inserted =
        meshes.insert(std::make_pair(name, mesh));
    return inserted.second ? &inserted.first->second : nullptr;
}

MeshResource* ResourceManager::getMesh(const std::string& name)
{
    std::map<std::string, MeshResource>::iterator it = meshes.find(name);
    return (it != meshes.end()) ? &it->second : nullptr;
}

void ResourceManager::clear()
{
    for (std::map<std::string, GLuint>::iterator it = shaders.begin(); it != shaders.end(); ++it) {
        if (it->second != 0) {
            glDeleteProgram(it->second);
        }
    }
    shaders.clear();

    for (std::map<std::string, GLuint>::iterator it = textures.begin(); it != textures.end(); ++it) {
        if (it->second != 0) {
            glDeleteTextures(1, &it->second);
        }
    }
    textures.clear();

    for (std::map<std::string, MeshResource>::iterator it = meshes.begin(); it != meshes.end(); ++it) {
        if (it->second.ebo != 0) glDeleteBuffers(1, &it->second.ebo);
        if (it->second.vbo != 0) glDeleteBuffers(1, &it->second.vbo);
        if (it->second.vao != 0) glDeleteVertexArrays(1, &it->second.vao);
    }
    meshes.clear();
}