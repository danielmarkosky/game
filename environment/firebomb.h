#pragma once
#include <SFML/Graphics.hpp>
#include "src/projectiles.h"

class Firebomb : public Projectile {
public:
    Firebomb(float startX, float startY);
private:
    float speed;
};
