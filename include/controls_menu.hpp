#ifndef CONTROLS_MENU_HPP
#define CONTROLS_MENU_HPP

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <map>

#include "sprite_atlas.hpp"
#include <vector>

class ControlsMenu {
    private:

    public:
        bool showControlsMenu;

        // Bomberman 1
        GLint upKey_P1;
        GLint downKey_P1;
        GLint leftKey_P1;
        GLint rightKey_P1;
        GLint bombKey_P1;
        GLint detonateBombKey_P1;

        // Bomberman 2
        GLint upKey_P2;
        GLint downKey_P2;
        GLint leftKey_P2;
        GLint rightKey_P2;
        GLint bombKey_P2;
        GLint detonateBombKey_P2;

        // Generales
        GLint swap2D_3DKey;
        GLint swap3DCamenraKey;
        GLint swapWindowModeKey;
        GLint selectKey;

        // Temporales (copias de lo anterior como salvaguarda)

            // Bomberman 1
        GLint upKey_P1_temp;
        GLint downKey_P1_temp;
        GLint leftKey_P1_temp;
        GLint rightKey_P1_temp;
        GLint bombKey_P1_temp;
        GLint detonateBombKey_P1_temp;

            // Bomberman 2
        GLint upKey_P2_temp;
        GLint downKey_P2_temp;
        GLint leftKey_P2_temp;
        GLint rightKey_P2_temp;
        GLint bombKey_P2_temp;
        GLint detonateBombKey_P2_temp;

            // Generales
        GLint swap2D_3DKey_temp;
        GLint swap3DCamenraKey_temp;
        GLint swapWindowModeKey_temp;
        GLint selectKey_temp;

        ControlsMenu();
        ~ControlsMenu();

        void readFromFile();
        void saveToFile();

        void loadDefaultControls();

        void renderControlsMenu(const SpriteAtlas& atlas, GLuint atlasTexture, 
                                const SpriteAtlas& atlas2, GLuint atlasTexture2,  
                                GLuint vao, GLuint uniformModel, GLuint uniformUvRect);

};

#endif // CONTROLS_MENU_HPP