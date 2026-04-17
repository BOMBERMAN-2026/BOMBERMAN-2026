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

        // Valores para modificar el sprite y saber donde se mueve el jugador
        // -1 -> no se encuentra en la seccion correspondiente
        int actualIndexBomberman_1_Controls;
        int actualIndexBomberman_2_Controls;
        int actualIndexGlobalControls;
        int actualIndexSaveResetExit;

        void saveToNonTemps();
        void flushTemps();

        void checkAlreadyTakenControl(int control, int actualKey, int lastKeyPressed);

    public:
        bool showControlsMenu;

        bool modifyingControls;

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
        GLint swap3DCameraKey;
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
        GLint swap3DCameraKey_temp;
        GLint swapWindowModeKey_temp;
        GLint selectKey_temp;

        ControlsMenu();
        ~ControlsMenu();

        void readFromFile();
        void saveToFile();

        void loadDefaultControls();

        void renderTextString(const std::string& text, glm::vec2 startPos, float scale,
                              const SpriteAtlas& atlas, GLuint atlasTexture, GLuint vao,
                              GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect, int colorUse);

        void renderControlsMenu(const SpriteAtlas& atlas, GLuint atlasTexture, 
                                const SpriteAtlas& atlas2, GLuint atlasTexture2,  
                                GLuint vao, GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect);

        void processInputControlsMenu(std::map<int, int>& keys, int lastKeyPressed);

};

#endif // CONTROLS_MENU_HPP