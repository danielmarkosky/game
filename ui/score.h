#pragma once
#include <SFML/Graphics.hpp>

class Score {
public:
    Score();
    void add(int value);
    int get() const;
    void draw(sf::RenderWindow& window);
private:
    int score;
    sf::Font font;
    sf::Text scoreText;
};