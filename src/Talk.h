#pragma once

#include "src/Dialogue.h"
#include "src/Level.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

class Player;

// A conversation on screen: a box at the top with the speaker's face and name, the words typing
// themselves out, and the choices when there are some. At the top like Celeste's: the talkers are
// usually lower down.
class Talk
{
public:
    // Has conversation number `index` of `dialogue` with `npc`. `setFlag` is called for the story
    // flags it sets as it goes, `act` for the commands (see Dialogue::isCommand).
    Talk(const Npc& npc, Dialogue dialogue, size_t index, std::function<void(const std::string&)> setFlag,
         std::function<void(const std::string&)> act = {});
    Talk(const Talk&) = delete;
    Talk& operator=(const Talk&) = delete;

    // What to remember it by, so it isn't had twice: "<dialogue> <number>".
    std::string key() const;
    const Npc& npc() const;
    bool finished() const;

    void update(float dt);
    // How many letters appeared in the last update: for the talking sound.
    int lettersTyped() const;
    const std::string& speaker() const;

    // Space, Enter, a click: show the rest of the line, or go on, or pick the highlighted choice.
    void advance();
    // Up and down through the choices.
    void moveChoice(int step);
    // The mouse highlights the choice it's over, and a click picks it.
    void pointAt(sf::Vector2f mouse, bool clicked);

    // `faces` draws the speakers, `playerSkin` is the look of "You".
    void draw(sf::RenderTarget& target, const Player& faces, size_t playerSkin, float time) const;

    static constexpr float TYPING_SPEED = 45.f; // letters a second

private:
    void startStep(); // wraps the current line into pages, or sets up a choice
    sf::FloatRect choiceRect(size_t option) const;

    Npc m_npc;
    Dialogue dialogue;
    size_t index;
    DialogueRunner runner;
    std::vector<std::vector<std::string>> pages; // of the current line, each a few wrapped lines
    size_t page = 0;
    float typed = 0.f;
    int newLetters = 0;
    size_t choice = 0;
};
