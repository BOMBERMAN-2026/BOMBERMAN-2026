
#ifndef IN_GAME_MENU_HPP
#define IN_GAME_MENU_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <map>

#include "sprite_atlas.hpp"
#include "controls_menu.hpp" 
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
class InGameMenu {

    private:
        // Texturas y atlases
        GLuint menuArrowTexture;
        GLuint vocabTexture; 
        GLuint blackTexture; // Textura negra para el menú in-game
        SpriteAtlas menuBombAtlas;
        SpriteAtlas vocabAtlas;

        // Tamaños del menu
        float inGameMenuHeight;
        float inGameMenuWidth;

        // Logica de selección
        int posSeleccion; // Indice del vector actualmente seleccionado para cambiarlo de color (y mas cosas)

    public:
        // Menu de controles
        ControlsMenu controlsMenu;

        // True = mostrar el menu
        // False = no mostrar
        bool showInGameMenu;

        InGameMenu();
        ~InGameMenu();

        void renderTextString(const std::string& text, glm::vec2 startPos, float scale,
                              const SpriteAtlas& atlas, GLuint atlasTexture, GLuint vao,
                              GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect, int colorUse);

        void renderInGameMenu(GLuint VAO, GLuint shader, GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect,
                              SpriteAtlas gVocabAmarilloAtlas, GLuint vocabAmarilloTexture, 
                              SpriteAtlas gVocabNaranjaAtlas, GLuint vocabNaranjaTexture);

        int processInputInGameMenu(std::map<int, int>& keys);
};

#endif // IN_GAME_MENU_HPP
