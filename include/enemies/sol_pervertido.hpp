#ifndef SOL_PERVERTIDO_HPP
#define SOL_PERVERTIDO_HPP

#include "enemy.hpp"
#include <vector>
#include <memory>

/*
 * Sol pervertido – 5000 pts x8, 3 HP (JEFE)
 * Rebota en las paredes como un pinball.
 * Al ser bombardeado se divide:
 *   Fase 1: 1 entero  (3 HP)
 *   Fase 2: 2 mitades  (cada una con comportamiento propio)
 *   Fase 3: 4 cuartos  (se subdividen de cada mitad)
 *   → 8 piezas pequeñas en total, más rápidas.
 */
class SolPervertido : public Enemy {
public:
    enum class Phase {
        FULL,    // Forma completa
        HALF,    // Media forma
        QUARTER  // Cuarto de forma
    };

    SolPervertido(glm::vec2 pos, glm::vec2 size, float speed, Phase phase = Phase::FULL);
    ~SolPervertido() override;

    void Update() override;
    void Draw()   override;

    // Al recibir daño, genera las subdivisiones.
    // Devuelve una lista de nuevos enemigos hijos (vacía si muere definitivamente).
    std::vector<std::unique_ptr<SolPervertido>> split();

    Phase getPhase() const { return currentPhase; }

private:
    Phase currentPhase;
    glm::vec2 velocity;  // Dirección + magnitud del rebote estilo pinball

    void bounce();       // Lógica de rebote contra paredes/bloques
};

#endif // SOL_PERVERTIDO_HPP
