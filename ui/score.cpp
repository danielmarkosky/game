#include "score.h"

Score::Score() : score(0) {
    font.loadFromFile("assets/BBHSansBogle-Regular.ttf");
    scoreText.setFont(font);
    scoreText.setCharacterSize(16);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(8.f, 8.f);
    scoreText.setString("Score: 0");
}

void Score::add(int value) {
    score += value;
    scoreText.setString("Score: " + std::to_string(score));
}

int Score::get() const {
    return score;
}

void Score::draw(sf::RenderWindow& window) {
    window.draw(scoreText);
}
