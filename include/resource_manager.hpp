#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#include <GL/glew.h>
#include <functional>
#include <map>
#include <string>

struct MeshResource {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};

class ResourceManager {
public:
    static GLuint loadShader(const std::string& name,
                             const std::string& vertexPath,
                             const std::string& fragmentPath);
    static GLuint getShader(const std::string& name);

    static GLuint loadTexture(const std::string& name,
                              const std::string& texturePath,
                              const std::function<GLuint(const char*)>& loader);
    static GLuint getTexture(const std::string& name);

    static MeshResource* createMesh(const std::string& name,
                                    const std::function<MeshResource(void)>& builder);
    static MeshResource* getMesh(const std::string& name);

    static void clear();

private:
    static std::map<std::string, GLuint> shaders;
    static std::map<std::string, GLuint> textures;
    static std::map<std::string, MeshResource> meshes;
};

#endif