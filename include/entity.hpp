#ifndef ENTITY_H
#define ENTITY_H

#include <GL/glew.h>
#include <glm/glm.hpp>

/*
 * Clase base genérica para todas las entidades del juego (jugadores, enemigos, etc.).
 * Proporciona atributos comunes y una interfaz polimórfica.
 */
class Entity {
    public:
        glm::vec2 position;
        glm::vec2 direction;
        glm::vec2 size;
        glm::vec4 tintColor;
        float speed;

        Entity(glm::vec2 pos, glm::vec2 size, float speed)
            : position(pos), size(size), speed(speed),
              direction(glm::vec2(0.0f)), tintColor(glm::vec4(1.0f)) {}

        // Destructor virtual: necesario para delete a través de puntero base
        virtual ~Entity() {}

        // Interfaz polimórfica que cada subclase debe implementar
        virtual void Update() = 0;
        virtual void Draw()   = 0;
};

#endif // ENTITY_H
