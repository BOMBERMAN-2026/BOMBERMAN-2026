
#ifndef IN_GAME_MENU_HPP
#define IN_GAME_MENU_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <map>

#include "sprite_atlas.hpp"
#include <vector>

/**
 * InGameMenu
 * ---------------
 * Clase para el menu in game. Muestra las opciones
 * - Modificar teclas (inputs) para cada jugador
 * - Modificar el volumen de la música
 * - Modificar el volumen de los efectos de sonido
 * - Cambier entre 2D y 3D
 *      - (Si esta en 3D) Cambiar entre camaras 
 * - Salir al menu principal
 * - Salir del juego
 */

const std::vector<std::string> menuOptions = {
    "CONTINUE",
    "CONTROLS",
    "MUSIC",
    "SOUNDS",
    "GRAPHICS",
    "CAMERA",
    "EXIT"
};

class InGameMenu {

    private:

        const uint numMenuOptions = 7; // Número total de opciones en el menú (ajustar según las opciones reales)
        glm::vec2 menuOptionPos;  // Posición base el menu

       // Texturas y atlases
        GLuint menuArrowTexture;
        GLuint vocabTexture; 
        GLuint blackTexture; // Textura negra para el menú in-game
        SpriteAtlas menuBombAtlas;
        SpriteAtlas vocabAtlas;

        // Tamaños del menu
        float inGameMenuHeight;
        float inGameMenuWidth;

        glm::vec2 arrowPos; // Posición de la flecha de selección

    public:

        bool showInGameMenu;

        InGameMenu();
        ~InGameMenu();

        void initInGameMenu();
        void updateInGameMenu(float deltaTime);
        void renderInGameMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect,
                              SpriteAtlas gVocabAmarilloAtlas, GLuint vocabAmarilloTexture, 
                              SpriteAtlas gVocabNaranjaAtlas, GLuint vocabNaranjaTexture);

        void processInputMenu(std::map<int, int>& keys);

        void renderTextString(const std::string& text, glm::vec2 startPos, float scale,
                              const SpriteAtlas& atlas, GLuint atlasTexture, GLuint vao,
                              GLuint uniformModel, GLuint uniformUvRect, bool useNaranja);
};

#endif // IN_GAME_MENU_HPP
