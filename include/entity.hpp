#ifndef ENTITY_H
#define ENTITY_H

#include <GL/glew.h>
#include <glm/glm.hpp>

/*
 * entity.hpp
 * -----------
 * Entidades base del juego.
 *
 * - `Entity` define el “mínimo común” para objetos del mundo (jugadores, enemigos, etc.).
 * - No implementa render ni lógica: solo expone estado común y una interfaz virtual.
 *
 * Uso:
 * - Hereda de `Entity` y sobreescribe `Update()` y `Draw()`.
 */

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

                // Crea una entidad con posición, tamaño (NDC) y velocidad base.
                Entity(glm::vec2 pos, glm::vec2 size, float speed)
            : position(pos), size(size), speed(speed),
              direction(glm::vec2(0.0f)), tintColor(glm::vec4(1.0f)) {}

        // Destructor virtual: necesario para delete a través de puntero base
        virtual ~Entity() {}

        // Tick de lógica por frame.
        virtual void Update() = 0;
        // Render (si se usa el patrón Entity::Draw en el futuro).
        virtual void Draw()   = 0;
};

#endif // ENTITY_H
