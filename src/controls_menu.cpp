#include "controls_menu.hpp"
#include "in_game_menu.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <cctype>

static constexpr float scaleUsualHud = 0.0010f;
static constexpr float scaleMiniTitle = 0.000675f;

static constexpr glm::vec2 bomberman_1_Pos(-0.525f, 0.40f);
static constexpr glm::vec2 bomberman_1_ControlsPos(-0.225f, 0.40f);
static constexpr glm::vec2 bomberman_2_Pos(0.03275f, 0.40f);
static constexpr glm::vec2 bomberman_2_ControlsPos(0.33275f, 0.40f);
static constexpr glm::vec2 globalKeyPos(-0.525f, -0.20f);
static constexpr glm::vec2 globalKeyControlsPos(-0.225f, -0.20f);
static constexpr glm::vec2 saveResetExitPos(0.33275f, -0.275);

const std::vector<std::string> bombermanKeyStrings = {
    "BOMB",
    "DETONATE",
    "UP", 
    "LEFT",
    "DOWN",
    "RIGHT"
};

const std::vector<std::string> generalKeyStrings = {
    "2D-3D",
    "CAMERA",
    "WINDOW",
    "SELECT"
};

const std::vector<std::string> saveResetExitStrings = {
    "SAVE",
    "RESET",
    "EXIT"
};

// ============================== Input Helper ==============================

static std::string getKeyName(GLint key){
    std::string str;
    switch(key) {
        case GLFW_KEY_UNKNOWN: str = "UNK";
        break;
        case GLFW_KEY_SPACE: str = "SPACE";
        break;
        case GLFW_KEY_APOSTROPHE: str = "APOS";
        break;
        case GLFW_KEY_COMMA: str = ",";
        break;
        case GLFW_KEY_MINUS: str = "-";
        break;
        case GLFW_KEY_PERIOD: str = "PER";
        break;
        case GLFW_KEY_SLASH: str = "/";
        break;
        case GLFW_KEY_0: str = "0";
        break;
        case GLFW_KEY_1: str = "1";
        break;
        case GLFW_KEY_2: str = "2";
        break;
        case GLFW_KEY_3: str = "3";
        break;
        case GLFW_KEY_4: str = "4";
        break;
        case GLFW_KEY_5: str = "5";
        break;
        case GLFW_KEY_6: str = "6";
        break;
        case GLFW_KEY_7: str = "7";
        break;
        case GLFW_KEY_8: str = "8";
        break;
        case GLFW_KEY_9: str = "9";
        break;
        case GLFW_KEY_SEMICOLON: str = "'";
        break;
        case GLFW_KEY_EQUAL: str = "=";
        break;
        case GLFW_KEY_A: str = "A";
        break;
        case GLFW_KEY_B: str = "B";
        break;
        case GLFW_KEY_C: str = "C";
        break;
        case GLFW_KEY_D: str = "D";
        break;
        case GLFW_KEY_E: str = "E";
        break;
        case GLFW_KEY_F: str = "F";
        break;
        case GLFW_KEY_G: str = "G";
        break;
        case GLFW_KEY_H: str = "H";
        break;
        case GLFW_KEY_I: str = "I";
        break;
        case GLFW_KEY_J: str = "J";
        break;
        case GLFW_KEY_K: str = "K";
        break;
        case GLFW_KEY_L: str = "L";
        break;
        case GLFW_KEY_M: str = "M";
        break;
        case GLFW_KEY_N: str = "N";
        break;
        case GLFW_KEY_O: str = "O";
        break;
        case GLFW_KEY_P: str = "P";
        break;
        case GLFW_KEY_Q: str = "Q";
        break;
        case GLFW_KEY_R: str = "R";
        break;
        case GLFW_KEY_S: str = "S";
        break;
        case GLFW_KEY_T: str = "T";
        break;
        case GLFW_KEY_U: str = "U";
        break;
        case GLFW_KEY_V: str = "V";
        break;
        case GLFW_KEY_W: str = "W";
        break;
        case GLFW_KEY_X: str = "X";
        break;
        case GLFW_KEY_Y: str = "Y";
        break;
        case GLFW_KEY_Z: str = "Z";
        break;
        case GLFW_KEY_LEFT_BRACKET: str = "{";
        break;
        case GLFW_KEY_BACKSLASH: str = "\\";
        break;
        case GLFW_KEY_RIGHT_BRACKET: str = "}";
        break;
        case GLFW_KEY_GRAVE_ACCENT: str = "`";
        break;
        case GLFW_KEY_WORLD_1: str = "W1";
        break;
        case GLFW_KEY_WORLD_2: str = "W2";
        break;
        case GLFW_KEY_ESCAPE: str = "ESC";
        break;
        case GLFW_KEY_ENTER: str = "ENTER";
        break;
        case GLFW_KEY_TAB: str = "TAB";
        break;
        case GLFW_KEY_BACKSPACE: str = "BS";
        break;
        case GLFW_KEY_INSERT: str = "INS";
        break;
        case GLFW_KEY_DELETE: str = "DEL";
        break;
        case GLFW_KEY_RIGHT: str = "RIGHT";
        break;
        case GLFW_KEY_LEFT: str = "LEFT";
        break;
        case GLFW_KEY_DOWN: str = "DOWN";
        break;
        case GLFW_KEY_UP: str = "UP";
        break;
        case GLFW_KEY_PAGE_UP: str = "PAG.UP";
        break;
        case GLFW_KEY_PAGE_DOWN: str = "PAG.DOWN";
        break;
        case GLFW_KEY_HOME: str = "HOME";
        break;
        case GLFW_KEY_END: str = "END";
        break;
        case GLFW_KEY_CAPS_LOCK: str = "C.LOCK";
        break;
        case GLFW_KEY_SCROLL_LOCK: str = "S.LOCK";
        break;
        case GLFW_KEY_NUM_LOCK: str = "N.LOCK";
        break;
        case GLFW_KEY_PRINT_SCREEN: str = "IMPR";
        break;
        case GLFW_KEY_PAUSE: str = "PAUSE";
        break;
        case GLFW_KEY_F1: str = "F1";
        break;
        case GLFW_KEY_F2: str = "F2";
        break;
        case GLFW_KEY_F3: str = "F3";
        break;
        case GLFW_KEY_F4: str = "F4";
        break;
        case GLFW_KEY_F5: str = "F5";
        break;
        case GLFW_KEY_F6: str = "F6";
        break;
        case GLFW_KEY_F7: str = "F7";
        break;
        case GLFW_KEY_F8: str = "F8";
        break;
        case GLFW_KEY_F9: str = "F9";
        break;
        case GLFW_KEY_F10: str = "F10";
        break;
        case GLFW_KEY_F11: str = "F11";
        break;
        case GLFW_KEY_F12: str = "F12";
        break;
        case GLFW_KEY_F13: str = "F13";
        break;
        case GLFW_KEY_F14: str = "F14";
        break;
        case GLFW_KEY_F15: str = "F15";
        break;
        case GLFW_KEY_F16: str = "F16";
        break;
        case GLFW_KEY_F17: str = "F17";
        break;
        case GLFW_KEY_F18: str = "F18";
        break;
        case GLFW_KEY_F19: str = "F19";
        break;
        case GLFW_KEY_F20: str = "F20";
        break;
        case GLFW_KEY_F21: str = "F21";
        break;
        case GLFW_KEY_F22: str = "F22";
        break;
        case GLFW_KEY_F23: str = "F23";
        break;
        case GLFW_KEY_F24: str = "F24";
        break;
        case GLFW_KEY_F25: str = "F25";
        break;
        case GLFW_KEY_KP_0: str = "KP0";
        break;
        case GLFW_KEY_KP_1: str = "KP1";
        break;
        case GLFW_KEY_KP_2: str = "KP2";
        break;
        case GLFW_KEY_KP_3: str = "KP3";
        break;
        case GLFW_KEY_KP_4: str = "KP4";
        break;
        case GLFW_KEY_KP_5: str = "KP5";
        break;
        case GLFW_KEY_KP_6: str = "KP6";
        break;
        case GLFW_KEY_KP_7: str = "KP7";
        break;
        case GLFW_KEY_KP_8: str = "KP8";
        break;
        case GLFW_KEY_KP_9: str = "KP9";
        break;
        case GLFW_KEY_KP_DECIMAL: str = "KPDEC";
        break;
        case GLFW_KEY_KP_DIVIDE: str = "KPDIV";
        break;
        case GLFW_KEY_KP_MULTIPLY: str = "KPMUL";
        break;
        case GLFW_KEY_KP_SUBTRACT: str = "KPSUB";
        break;
        case GLFW_KEY_KP_ADD: str = "KPADD";
        break;
        case GLFW_KEY_KP_ENTER: str = "KPENTER";
        break;
        case GLFW_KEY_KP_EQUAL: str = "KPEQ";
        break;
        case GLFW_KEY_LEFT_SHIFT: str = "SHIFT";
        break;
        case GLFW_KEY_LEFT_CONTROL: str = "CTRL";
        break;
        case GLFW_KEY_LEFT_ALT: str = "ALT";
        break;
        case GLFW_KEY_LEFT_SUPER: str = "SUPER";
        break;
        case GLFW_KEY_RIGHT_SHIFT: str = "RSHIFT";
        break;
        case GLFW_KEY_RIGHT_CONTROL: str = "RCTRL";
        break;
        case GLFW_KEY_RIGHT_ALT: str = "RALT";
        break;
        case GLFW_KEY_RIGHT_SUPER: str = "RSUPER";
        break;
        case GLFW_KEY_MENU: str = "MENU";
        break;
    }
    return str;
}

// ============================== INITS / DESTROYERS ==============================

// TODO, leer de fichero las teclas e introducirlas en las variables
ControlsMenu::ControlsMenu() : showControlsMenu(false), modifyingControls(false), actualIndexBomberman_1_Controls(0), actualIndexBomberman_2_Controls(-1), actualIndexGlobalControls(-1), actualIndexSaveResetExit(-1) {
    readFromFile();
    flushTemps();
}

// TODO, limpiar 
ControlsMenu::~ControlsMenu() {}

void ControlsMenu::readFromFile() {

    std::string path = "resources/keyBindings/controls.txt";
    std::ifstream file(resolveAssetPath(path));
    std::string line;

    std::cout << "########### READING FROM FILE ###########\n\n";

    if (file.is_open()) {
        while (std::getline(file, line)) {
            // Buscamos la posición del '='
            size_t delimiterPos = line.find('=');
            if (delimiterPos == std::string::npos) continue; // Línea inválida

            std::string key = line.substr(0, delimiterPos);
            int value = std::stoi(line.substr(delimiterPos + 1));

            // Asignamos el valor a la variable correspondiente
            if (key == "bombKey_P1") bombKey_P1 = value;
            else if (key == "detonateBombKey_P1") detonateBombKey_P1 = value;
            else if (key == "upKey_P1") upKey_P1 = value;
            else if (key == "leftKey_P1") leftKey_P1 = value;
            else if (key == "downKey_P1") downKey_P1 = value;
            else if (key == "rightKey_P1") rightKey_P1 = value;

            else if (key == "bombKey_P2") bombKey_P2 = value;
            else if (key == "detonateBombKey_P2") detonateBombKey_P2 = value;
            else if (key == "upKey_P2") upKey_P2 = value;
            else if (key == "leftKey_P2") leftKey_P2 = value;
            else if (key == "downKey_P2") downKey_P2 = value;
            else if (key == "rightKey_P2") rightKey_P2 = value;
            
            else if (key == "swap2D_3DKey") swap2D_3DKey = value;
            else if (key == "swap3DCamenraKey") swap3DCameraKey = value;
            else if (key == "swapWindowModeKey") swapWindowModeKey = value;
            else if (key == "selectKey") selectKey = value;
        }
        file.close();
        std::cout << "Controles cargados correctamente." << std::endl;
    } else {
        std::cout << "No se encontró archivo de config. Se usarán valores por defecto." << std::endl;
    }

    std::cout << "bombKey_P1=" << bombKey_P1 << "\n";
    std::cout << "detonateBombKey_P1=" << detonateBombKey_P1 << "\n";
    std::cout << "upKey_P1=" << upKey_P1 << "\n";
    std::cout << "leftKey_P1=" << leftKey_P1 << "\n";
    std::cout << "downKey_P1=" << downKey_P1 << "\n";
    std::cout << "rightKey_P1=" << rightKey_P1 << "\n";
    
    std::cout << "bombKey_P2=" << bombKey_P2 << "\n";
    std::cout << "detonateBombKey_P2=" << detonateBombKey_P2 << "\n";
    std::cout << "upKey_P2=" << upKey_P2 << "\n";
    std::cout << "leftKey_P2=" << leftKey_P2 << "\n";
    std::cout << "downKey_P2=" << downKey_P2 << "\n";
    std::cout << "rightKey_P2=" << rightKey_P2 << "\n";


    std::cout << "swap2D_3DKey=" << swap2D_3DKey << "\n";
    std::cout << "swap3DCamenraKey=" << swap3DCameraKey << "\n";
    std::cout << "swapWindowModeKey=" << swapWindowModeKey << "\n";
    std::cout << "selectKey=" << selectKey << "\n";
}

void ControlsMenu::saveToNonTemps() {
    bombKey_P1 = bombKey_P1_temp;
    detonateBombKey_P1 = detonateBombKey_P1_temp;
    upKey_P1 = upKey_P1_temp;
    leftKey_P1 = leftKey_P1_temp;
    downKey_P1 = downKey_P1_temp;
    rightKey_P1 = rightKey_P1_temp;

    // Bomberman 2
    bombKey_P2 = bombKey_P2_temp;
    detonateBombKey_P2 = detonateBombKey_P2_temp;
    upKey_P2 = upKey_P2_temp;
    leftKey_P2 = leftKey_P2_temp;
    downKey_P2 = downKey_P2_temp;
    rightKey_P2 = rightKey_P2_temp;

    // Generales
    swap2D_3DKey = swap2D_3DKey_temp;
    swap3DCameraKey = swap3DCameraKey_temp;
    swapWindowModeKey = swapWindowModeKey_temp;
    selectKey = selectKey_temp;
}

void ControlsMenu::flushTemps() {
    bombKey_P1_temp = bombKey_P1;
    detonateBombKey_P1_temp = detonateBombKey_P1;
    upKey_P1_temp = upKey_P1;
    leftKey_P1_temp = leftKey_P1;
    downKey_P1_temp = downKey_P1;
    rightKey_P1_temp = rightKey_P1;

    // Bomberman 2
    bombKey_P2_temp = bombKey_P2;
    detonateBombKey_P2_temp = detonateBombKey_P2;
    upKey_P2_temp = upKey_P2;
    leftKey_P2_temp = leftKey_P2;
    downKey_P2_temp = downKey_P2;
    rightKey_P2_temp = rightKey_P2;

    // Generales
    swap2D_3DKey_temp = swap2D_3DKey;
    swap3DCameraKey_temp = swap3DCameraKey;
    swapWindowModeKey_temp = swapWindowModeKey;
    selectKey_temp = selectKey;
}

void ControlsMenu::saveToFile() {

    std::string path = "resources/keyBindings/controls.txt";
    std::ofstream file(resolveAssetPath(path)); // Crea o abre el archivo en la ruta

    if (file.is_open()) {
        file << "bombKey_P1=" << bombKey_P1 << "\n";
        file << "detonateBombKey_P1=" << detonateBombKey_P1 << "\n";
        file << "upKey_P1=" << upKey_P1 << "\n";
        file << "leftKey_P1=" << leftKey_P1 << "\n";
        file << "downKey_P1=" << downKey_P1 << "\n";
        file << "rightKey_P1=" << rightKey_P1 << "\n";
        
        file << "bombKey_P2=" << bombKey_P2 << "\n";
        file << "detonateBombKey_P2=" << detonateBombKey_P2 << "\n";
        file << "upKey_P2=" << upKey_P2 << "\n";
        file << "leftKey_P2=" << leftKey_P2 << "\n";
        file << "downKey_P2=" << downKey_P2 << "\n";
        file << "rightKey_P2=" << rightKey_P2 << "\n";


        file << "swap2D_3DKey=" << swap2D_3DKey << "\n";
        file << "swap3DCamenraKey=" << swap3DCameraKey << "\n";
        file << "swapWindowModeKey=" << swapWindowModeKey << "\n";
        file << "selectKey=" << selectKey << "\n";

        std::cout << "############# SAVING TO FILE ##############\n\n";

        std::cout << "bombKey_P1=" << bombKey_P1 << "\n";
        std::cout << "detonateBombKey_P1=" << detonateBombKey_P1 << "\n";
        std::cout << "upKey_P1=" << upKey_P1 << "\n";
        std::cout << "leftKey_P1=" << leftKey_P1 << "\n";
        std::cout << "downKey_P1=" << downKey_P1 << "\n";
        std::cout << "rightKey_P1=" << rightKey_P1 << "\n";
        
        std::cout << "bombKey_P2=" << bombKey_P2 << "\n";
        std::cout << "detonateBombKey_P2=" << detonateBombKey_P2 << "\n";
        std::cout << "upKey_P2=" << upKey_P2 << "\n";
        std::cout << "leftKey_P2=" << leftKey_P2 << "\n";
        std::cout << "downKey_P2=" << downKey_P2 << "\n";
        std::cout << "rightKey_P2=" << rightKey_P2 << "\n";


        std::cout << "swap2D_3DKey=" << swap2D_3DKey << "\n";
        std::cout << "swap3DCamenraKey=" << swap3DCameraKey << "\n";
        std::cout << "swapWindowModeKey=" << swapWindowModeKey << "\n";
        std::cout << "selectKey=" << selectKey << "\n";

        file.close();
        std::cout << "Cambios de controles registrados con exito" << std::endl;
    } else {
        std::cerr << "Error al abrir el archivo " << path << " para escribir." << std::endl;
    }
}

// TODO, cargar los controles default del fichero, usar readFromFile
void ControlsMenu::loadDefaultControls() {
    bombKey_P1_temp = GLFW_KEY_RIGHT_CONTROL;
    detonateBombKey_P1_temp = GLFW_KEY_RIGHT_ALT;
    upKey_P1_temp = GLFW_KEY_UP;
    leftKey_P1_temp = GLFW_KEY_LEFT;
    downKey_P1_temp = GLFW_KEY_DOWN;
    rightKey_P1_temp = GLFW_KEY_RIGHT;

    // Bomberman 2
    bombKey_P2_temp = GLFW_KEY_X;
    detonateBombKey_P2_temp = GLFW_KEY_Z;
    upKey_P2_temp = GLFW_KEY_W;
    leftKey_P2_temp = GLFW_KEY_A;
    downKey_P2_temp = GLFW_KEY_S;
    rightKey_P2_temp = GLFW_KEY_D;

    // Generales
    swap2D_3DKey_temp = GLFW_KEY_1;
    swap3DCameraKey_temp = GLFW_KEY_2;
    swapWindowModeKey_temp = GLFW_KEY_TAB;
    selectKey_temp = GLFW_KEY_ENTER;
}

// ============================== RENDER ==============================

/** Render del inGameMenu
 *  El valor de colorUse esta entre 0 (color naranja), 1 (color amarillo) y 2 (color amarillo blanquecino) 
 */

void ControlsMenu::renderTextString(const std::string& text, glm::vec2 startPos, float scale,
                      const SpriteAtlas& atlas, GLuint atlasTexture, GLuint vao,
                      GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect, int colorUse) {
    if (text.empty()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glBindVertexArray(vao);

    float currentX = startPos.x;
    float y = startPos.y;
    const float interSeparation = 0.015f * scale;

    for (char c : text) {
        std::string spriteName(1, c);
        if (spriteName == " ") {
            currentX += 0.054f;
            continue;
        }

        if (colorUse == 0)
            spriteName = spriteName + "_Nar";
        else if (colorUse == 1)
            spriteName = spriteName + "_Ama";
        else if (colorUse == 2)
            spriteName = spriteName + "_AmaBla";
        else 
            spriteName = spriteName + "_Roj";
        
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

        if (!getUvRectForSprite(atlas, spriteName, uvRect)) {
            std::cerr << "Advertencia: Sprite '" << spriteName << "' no encontrado en atlas de vocabulario amarillo\n";
            continue;
        }

        auto it = atlas.sprites.find(spriteName);
        float spriteWidth = (it != atlas.sprites.end()) ? static_cast<float>(it->second.w) * scale : 20.0f * scale;
        float spriteHeight = (it != atlas.sprites.end()) ? static_cast<float>(it->second.h) * scale : 23.0f * scale;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(currentX + spriteWidth * 0.5f, y, 0.1f)); // 0.1f en Z para que se pueda mostrar en el 3D
        model = glm::scale(model, glm::vec3(spriteWidth * 0.5f, spriteHeight * 0.5f, 1.0f));

        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(uniformUvRect, 1, glm::value_ptr(uvRect));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        currentX += spriteWidth + interSeparation;
    }
}

void ControlsMenu::renderControlsMenu(const SpriteAtlas& atlas, GLuint atlasTexture, 
                                      const SpriteAtlas& atlas2, GLuint atlasTexture2,  
                                      GLuint vao, GLuint uniformModel, GLuint uniformProjection, GLuint uniformUvRect) {

    // Mini-title
    renderTextString("BOMBERMAN 1", bomberman_1_Pos + glm::vec2(0.0f, 0.1), scaleMiniTitle, atlas2, atlasTexture2, vao, uniformModel, uniformProjection, uniformUvRect, 0);
    renderTextString("BOMBERMAN 2", bomberman_2_Pos + glm::vec2(0.0f, 0.1), scaleMiniTitle, atlas2, atlasTexture2, vao, uniformModel, uniformProjection, uniformUvRect, 0);

    // Nombres de los controles para cada jugador
    for (int i=0; i < bombermanKeyStrings.size(); i++) {
        renderTextString(bombermanKeyStrings[i], bomberman_1_Pos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, 1);
        renderTextString(bombermanKeyStrings[i], bomberman_2_Pos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, 1);
    }

    // Mini-title
    renderTextString("GLOBAL", globalKeyPos + glm::vec2(0.0f, 0.1), scaleMiniTitle, atlas2, atlasTexture2, vao, uniformModel, uniformProjection, uniformUvRect, 0);
    
    // Nombres de los controles generales
    for (int i=0; i < generalKeyStrings.size(); i++) {
        float separacion = (i == 0 ? 0.1f : 0.075f);
        renderTextString(generalKeyStrings[i], globalKeyPos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, 1);
    }

    // Teclas usadas por el jugador 1
    renderTextString(getKeyName(bombKey_P1_temp), bomberman_1_ControlsPos, scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_1_Controls == 0 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(detonateBombKey_P1_temp), bomberman_1_ControlsPos + glm::vec2(0.0f, -0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_1_Controls == 1 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(upKey_P1_temp), bomberman_1_ControlsPos + glm::vec2(0.0f, -0.150f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_1_Controls == 2 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(leftKey_P1_temp), bomberman_1_ControlsPos + glm::vec2(0.0f, -0.225f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_1_Controls == 3 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(downKey_P1_temp), bomberman_1_ControlsPos + glm::vec2(0.0f, -0.300f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_1_Controls == 4 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(rightKey_P1_temp), bomberman_1_ControlsPos + glm::vec2(0.0f, -0.375f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_1_Controls == 5 ? (modifyingControls ? 3 : 2) : 1);

     // Teclas usadas por el jugador 2
    renderTextString(getKeyName(bombKey_P2_temp), bomberman_2_ControlsPos, scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_2_Controls == 0 ? 2 : 1);
    renderTextString(getKeyName(detonateBombKey_P2_temp), bomberman_2_ControlsPos + glm::vec2(0.0f, -0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_2_Controls == 1 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(upKey_P2_temp), bomberman_2_ControlsPos + glm::vec2(0.0f, -0.150f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_2_Controls == 2 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(leftKey_P2_temp), bomberman_2_ControlsPos + glm::vec2(0.0f, -0.225f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_2_Controls == 3 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(downKey_P2_temp), bomberman_2_ControlsPos + glm::vec2(0.0f, -0.300f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_2_Controls == 4 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(rightKey_P2_temp), bomberman_2_ControlsPos + glm::vec2(0.0f, -0.375f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexBomberman_2_Controls == 5 ? (modifyingControls ? 3 : 2) : 1);

    // Teclas usadas para los controles generales
    renderTextString(getKeyName(swap2D_3DKey_temp), globalKeyControlsPos, scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexGlobalControls == 0 ? 2 : 1);
    renderTextString(getKeyName(swap3DCameraKey_temp), globalKeyControlsPos + glm::vec2(0.0f, -0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexGlobalControls == 1 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(swapWindowModeKey_temp), globalKeyControlsPos + glm::vec2(0.0f, -0.150f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexGlobalControls == 2 ? (modifyingControls ? 3 : 2) : 1);
    renderTextString(getKeyName(selectKey_temp), globalKeyControlsPos + glm::vec2(0.0f, -0.225f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexGlobalControls == 3 ? (modifyingControls ? 3 : 2) : 1);

    // Teclas para guardar cambios, resetearlos o salir
    for (int i=0; i < saveResetExitStrings.size(); i++) {
        renderTextString(saveResetExitStrings[i], saveResetExitPos + glm::vec2(0.0f, -i * 0.075f), scaleUsualHud, atlas, atlasTexture, vao, uniformModel, uniformProjection, uniformUvRect, actualIndexSaveResetExit == i ? (modifyingControls ? 3 : 2) : 1);
    }

}

// ============================== INPUT ==============================

/**
 * Comprobamos si hay algun control con esta tecla ya asignada
 * Devolvemos -1 en caso de que este libre, en caso contrario devolvemos la tecla usada por el control
 */

void ControlsMenu::checkAlreadyTakenControl(int control, int actualKey, int lastKeyPressed) {

    if (control != 0 && bombKey_P1_temp == lastKeyPressed) bombKey_P1_temp = actualKey; 
    else if (control != 1 && detonateBombKey_P1_temp == lastKeyPressed) detonateBombKey_P1_temp = actualKey;
    else if (control != 2 && upKey_P1_temp == lastKeyPressed) upKey_P1_temp = actualKey;
    else if (control != 3 && leftKey_P1_temp == lastKeyPressed) leftKey_P1_temp = actualKey; 
    else if (control != 4 && downKey_P1_temp == lastKeyPressed) downKey_P1_temp = actualKey; 
    else if (control != 5 && rightKey_P1_temp == lastKeyPressed) rightKey_P1_temp = actualKey; 

    else if (control != 6 && bombKey_P2_temp == lastKeyPressed) bombKey_P2_temp = actualKey; 
    else if (control != 7 && detonateBombKey_P2_temp == lastKeyPressed) detonateBombKey_P2_temp = actualKey;
    else if (control != 8 && upKey_P2_temp == lastKeyPressed) upKey_P2_temp = actualKey;
    else if (control != 9 && leftKey_P2_temp == lastKeyPressed) leftKey_P2_temp = actualKey; 
    else if (control != 10 && downKey_P2_temp == lastKeyPressed) downKey_P2_temp = actualKey; 
    else if (control != 11 && rightKey_P2_temp == lastKeyPressed) rightKey_P2_temp = actualKey;

    else if (control != 12 && swap2D_3DKey_temp == lastKeyPressed) swap2D_3DKey_temp = actualKey; 
    else if (control != 13 && swap3DCameraKey_temp == lastKeyPressed) swap3DCameraKey_temp = actualKey; 
    else if (control != 14 && swapWindowModeKey_temp == lastKeyPressed) swapWindowModeKey_temp = actualKey; 
    else if (control != 15 && selectKey_temp == lastKeyPressed) selectKey_temp = actualKey; 
}

// Procesar el movimiento y cambio de teclas
void ControlsMenu::processInputControlsMenu(std::map<int, int>& keys, int lastKeyPressed) { 

    // Si >= 0 significa que estamos en el menu
    if (actualIndexBomberman_1_Controls >= 0) { // BOMBERMAN 1
        if (!modifyingControls && keys[downKey_P1] == GLFW_PRESS) {
            if (actualIndexBomberman_1_Controls >= bombermanKeyStrings.size() - 1) {
                actualIndexBomberman_1_Controls = -1;
                actualIndexGlobalControls = 0;
            }
            else actualIndexBomberman_1_Controls += 1;
            keys[downKey_P1] = GLFW_REPEAT;
        }

        if (!modifyingControls && keys[upKey_P1] == GLFW_PRESS) {
            if (actualIndexBomberman_1_Controls <= 0) {
                actualIndexBomberman_1_Controls = -1;
                actualIndexGlobalControls = generalKeyStrings.size() - 1;
            }
            else actualIndexBomberman_1_Controls -= 1;
            keys[upKey_P1] = GLFW_REPEAT;
        }

        if (!modifyingControls && ( keys[leftKey_P1] == GLFW_PRESS || keys[rightKey_P1] == GLFW_PRESS )) {
            actualIndexBomberman_2_Controls = actualIndexBomberman_1_Controls;
            actualIndexBomberman_1_Controls = -1;

            keys[leftKey_P1] = GLFW_REPEAT; keys[rightKey_P1] = GLFW_REPEAT;
        }

        if (keys[selectKey] == GLFW_PRESS && !modifyingControls) modifyingControls = true;
        else if (modifyingControls && lastKeyPressed != selectKey) {
            std::cout << "Entro a modificar controles bomb 1" << std::endl;
            switch (actualIndexBomberman_1_Controls) {
                //  BOMB
                case 0: checkAlreadyTakenControl(0, bombKey_P1_temp, lastKeyPressed); bombKey_P1_temp = lastKeyPressed; break;
                // DETONATE
                case 1: checkAlreadyTakenControl(1, detonateBombKey_P1_temp, lastKeyPressed); detonateBombKey_P1_temp = lastKeyPressed; break;
                // UP
                case 2: checkAlreadyTakenControl(2, upKey_P1_temp, lastKeyPressed); upKey_P1_temp = lastKeyPressed; break;
                // LEFT
                case 3: checkAlreadyTakenControl(3, leftKey_P1_temp, lastKeyPressed); leftKey_P1_temp = lastKeyPressed; break;
                // DOWN
                case 4: checkAlreadyTakenControl(4, downKey_P1_temp, lastKeyPressed); downKey_P1_temp = lastKeyPressed; break;
                // RIGHT
                case 5: checkAlreadyTakenControl(5, rightKey_P1_temp, lastKeyPressed); rightKey_P1_temp = lastKeyPressed; break;
                default: break;
            }
            keys[selectKey] = GLFW_REPEAT;
            keys[lastKeyPressed] = GLFW_REPEAT;
            modifyingControls = false;
        }
    }
    else if (actualIndexBomberman_2_Controls >= 0) { // BOMBERMAN 2
        if (!modifyingControls && keys[downKey_P1] == GLFW_PRESS) {
            if (actualIndexBomberman_2_Controls >= bombermanKeyStrings.size() - 1) {
                actualIndexBomberman_2_Controls = -1;
                actualIndexSaveResetExit = 0;
            }
            else actualIndexBomberman_2_Controls += 1;
            keys[downKey_P1] = GLFW_REPEAT;
        }

        if (!modifyingControls && keys[upKey_P1] == GLFW_PRESS) {
            if (actualIndexBomberman_2_Controls <= 0) {
                actualIndexBomberman_2_Controls = -1;
                actualIndexSaveResetExit = saveResetExitStrings.size() - 1;
            }
            else actualIndexBomberman_2_Controls -= 1;
            keys[upKey_P1] = GLFW_REPEAT;
        }

        if (!modifyingControls && ( keys[leftKey_P1] == GLFW_PRESS || keys[rightKey_P1] == GLFW_PRESS )) {
            actualIndexBomberman_1_Controls = actualIndexBomberman_2_Controls;
            actualIndexBomberman_2_Controls = -1;

            keys[leftKey_P1] = GLFW_REPEAT; keys[rightKey_P1] = GLFW_REPEAT;
        }

        if (keys[selectKey] == GLFW_PRESS && !modifyingControls) modifyingControls = true;
        else if (modifyingControls && lastKeyPressed != selectKey) {
            switch (actualIndexBomberman_2_Controls) {
                //  BOMB
                case 0: checkAlreadyTakenControl(6, bombKey_P2_temp, lastKeyPressed); bombKey_P2_temp = lastKeyPressed; break;
                // DETONATE
                case 1: checkAlreadyTakenControl(7, detonateBombKey_P2_temp, lastKeyPressed); detonateBombKey_P2_temp = lastKeyPressed; break;
                // UP
                case 2: checkAlreadyTakenControl(8, upKey_P2_temp, lastKeyPressed); upKey_P2_temp = lastKeyPressed; break;
                // LEFT
                case 3: checkAlreadyTakenControl(9, leftKey_P2_temp, lastKeyPressed); leftKey_P2_temp = lastKeyPressed; break;
                // DOWN
                case 4: checkAlreadyTakenControl(10, downKey_P2_temp, lastKeyPressed); downKey_P2_temp = lastKeyPressed; break;
                // RIGHT
                case 5: checkAlreadyTakenControl(11, rightKey_P2_temp, lastKeyPressed); rightKey_P2_temp = lastKeyPressed; break;
                default: break;
            }
            keys[selectKey] = GLFW_REPEAT;
            keys[lastKeyPressed] = GLFW_REPEAT;
            modifyingControls = false;
        }
    }
    else if (actualIndexGlobalControls >= 0) { // GENERAL
        if (!modifyingControls && keys[downKey_P1] == GLFW_PRESS) {
            if (actualIndexGlobalControls >= generalKeyStrings.size() - 1) {
                actualIndexGlobalControls = -1;
                actualIndexBomberman_1_Controls = 0;
            }
            else actualIndexGlobalControls += 1;
            keys[downKey_P1] = GLFW_REPEAT;
        }

        if (!modifyingControls && keys[upKey_P1] == GLFW_PRESS) {
            if (actualIndexGlobalControls <= 0) {
                actualIndexGlobalControls = -1;
                actualIndexBomberman_1_Controls = bombermanKeyStrings.size() - 1;
            }
            else actualIndexGlobalControls -= 1;
            keys[upKey_P1] = GLFW_REPEAT;
        }

        if (!modifyingControls && ( keys[leftKey_P1] == GLFW_PRESS ||  keys[rightKey_P1] == GLFW_PRESS )) {
            if (actualIndexGlobalControls > 0) actualIndexSaveResetExit = actualIndexGlobalControls - 1;
            else actualIndexSaveResetExit = actualIndexGlobalControls;
            actualIndexGlobalControls = -1;

            keys[leftKey_P1] = GLFW_REPEAT; keys[rightKey_P1] = GLFW_REPEAT;
        }

        if (keys[selectKey] == GLFW_PRESS && !modifyingControls) modifyingControls = true;
        else if (modifyingControls && lastKeyPressed != selectKey) {
            switch (actualIndexGlobalControls) {
                // 2D-3D
                case 0: checkAlreadyTakenControl(12, swap2D_3DKey_temp, lastKeyPressed); swap2D_3DKey_temp = lastKeyPressed; break;
                // CAMERA
                case 1: checkAlreadyTakenControl(13, swap3DCameraKey_temp, lastKeyPressed); swap3DCameraKey_temp = lastKeyPressed; break;
                // WINDOW
                case 2: checkAlreadyTakenControl(14, swapWindowModeKey_temp, lastKeyPressed); swapWindowModeKey_temp = lastKeyPressed; break;
                // SELECT
                case 3: checkAlreadyTakenControl(15, selectKey_temp, lastKeyPressed); selectKey_temp = lastKeyPressed; break;
                default: break;
            }
            keys[selectKey] = GLFW_REPEAT;
            keys[lastKeyPressed] = GLFW_REPEAT;
            modifyingControls = false;
        }
    }
    else {
        if (keys[downKey_P1] == GLFW_PRESS) {
            if (actualIndexSaveResetExit >= saveResetExitStrings.size() - 1) {
                actualIndexSaveResetExit = -1;
                actualIndexBomberman_2_Controls = 0;
            }
            else actualIndexSaveResetExit += 1;
            keys[downKey_P1] = GLFW_REPEAT;
        }

        if (keys[upKey_P1] == GLFW_PRESS) {
            if (actualIndexSaveResetExit <= 0) {
                actualIndexSaveResetExit = -1;
                actualIndexBomberman_2_Controls = bombermanKeyStrings.size() - 1;
            }
            else actualIndexSaveResetExit -= 1;
            keys[upKey_P1] = GLFW_REPEAT;
        }

        if (keys[leftKey_P1] == GLFW_PRESS || keys[rightKey_P1] == GLFW_PRESS) {

            actualIndexGlobalControls = actualIndexSaveResetExit + 1;
            actualIndexSaveResetExit = -1;

            keys[leftKey_P1] = GLFW_REPEAT; keys[rightKey_P1] = GLFW_REPEAT;
        }
        
        if (keys[selectKey] == GLFW_PRESS) {
            switch (actualIndexSaveResetExit) {
                // SAVE
                case 0: saveToNonTemps(); saveToFile(); break;
                // RESET
                case 1: loadDefaultControls(); break;
                 // EXIT
                case 2: showControlsMenu = false; flushTemps(); 
                    actualIndexBomberman_1_Controls = 0; actualIndexBomberman_2_Controls = -1; 
                    actualIndexGlobalControls = -1; actualIndexSaveResetExit = -1; 
                    readFromFile();
                    break;
                default: break;
            }
            keys[selectKey] = GLFW_REPEAT;
        }
    }
}
