#include "Talk.h"
#include "Hud.h"
#include "Player.h"
#include "Text.h"
#include <algorithm>
#include <cmath>

namespace
{
    const sf::FloatRect BOX(8.f, 6.f, 304.f, 52.f);
    constexpr float TEXT_LEFT = 46.f; // from the left of the box, after the face
    constexpr size_t LINES_PER_PAGE = 3;

    size_t letters(const std::vector<std::string>& lines)
    {
        size_t n = 0;
        for (const auto& line : lines) n += line.size();
        return n;
    }
}

Talk::Talk(const Npc& npc, Dialogue d, size_t i, std::function<void(const std::string&)> setFlag)
    : m_npc(npc), dialogue(std::move(d)), index(i), runner(dialogue.conversations()[i], std::move(setFlag))
{
    startStep();
}

std::string Talk::key() const
{
    return m_npc.dialogue + " " + std::to_string(index);
}

const Npc& Talk::npc() const
{
    return m_npc;
}

bool Talk::finished() const
{
    return runner.finished();
}

const std::string& Talk::speaker() const
{
    static const std::string none;
    return finished() ? none : runner.current().speaker;
}

void Talk::startStep()
{
    pages.clear();
    page = 0;
    typed = 0.f;
    choice = 0;
    if (finished() || runner.current().kind != DialogueStep::Kind::Line) return;
    const std::vector<std::string> wrapped = text::wrap(runner.current().text, BOX.width - TEXT_LEFT - 8.f);
    for (size_t first = 0; first < wrapped.size(); first += LINES_PER_PAGE) {
        pages.emplace_back(wrapped.begin() + static_cast<long>(first), wrapped.begin() + static_cast<long>(std::min(first + LINES_PER_PAGE, wrapped.size())));
    }
    if (pages.empty()) pages.push_back({""});
}

void Talk::update(float dt)
{
    newLetters = 0;
    if (finished() || pages.empty()) return;
    const float before = typed;
    typed = std::min(typed + dt * TYPING_SPEED, static_cast<float>(letters(pages[page])));
    newLetters = static_cast<int>(typed) - static_cast<int>(before);
}

int Talk::lettersTyped() const
{
    return newLetters;
}

void Talk::advance()
{
    if (finished()) return;
    if (runner.current().kind == DialogueStep::Kind::Choice) {
        runner.choose(choice);
        startStep();
        return;
    }
    // First the rest of the page, then the next page, then the next line.
    const float length = static_cast<float>(letters(pages[page]));
    if (typed < length) {
        typed = length;
    } else if (page + 1 < pages.size()) {
        ++page;
        typed = 0.f;
    } else {
        runner.next();
        startStep();
    }
}

void Talk::moveChoice(int step)
{
    if (finished() || runner.current().kind != DialogueStep::Kind::Choice) return;
    const int count = static_cast<int>(runner.current().options.size());
    choice = static_cast<size_t>((static_cast<int>(choice) + step + count) % count);
}

sf::FloatRect Talk::choiceRect(size_t option) const
{
    return {BOX.left + TEXT_LEFT - 2.f, BOX.top + 6.f + text::LINE_HEIGHT * static_cast<float>(option) * 1.4f, BOX.width - TEXT_LEFT - 4.f, 12.f};
}

void Talk::pointAt(sf::Vector2f mouse, bool clicked)
{
    if (finished()) return;
    if (runner.current().kind == DialogueStep::Kind::Choice) {
        for (size_t i = 0; i < runner.current().options.size(); ++i) {
            if (!choiceRect(i).contains(mouse)) continue;
            choice = i;
            if (clicked) advance();
            return;
        }
        return;
    }
    if (clicked) advance();
}

void Talk::draw(sf::RenderTarget& target, const Player& faces, size_t playerSkin, float time) const
{
    if (finished()) return;
    const DialogueStep& step = runner.current();
    hud::fillRect(target, BOX, sf::Color(14, 14, 22, 235));
    hud::frame(target, BOX, sf::Color(150, 150, 170, 180));

    // The face of whoever's speaking: you, or the one you're talking to. For a choice, it's you.
    const bool you = step.kind == DialogueStep::Kind::Choice || step.speaker == "You";
    const sf::FloatRect face(BOX.left + 6.f, BOX.top + 8.f, 34.f, 36.f);
    hud::fillRect(target, face, sf::Color(34, 38, 58));
    hud::frame(target, face, sf::Color(150, 150, 170, 120));
    faces.drawFigure(target, you ? playerSkin : Player::lookNamed(m_npc.look), {face.left + 17.f, face.top + 33.f}, false, time, 2.f);

    const float left = BOX.left + TEXT_LEFT;
    if (step.kind == DialogueStep::Kind::Choice) {
        // What you could say, the one you'd pick marked.
        for (size_t i = 0; i < step.options.size(); ++i) {
            const sf::FloatRect r = choiceRect(i);
            const bool picked = i == choice;
            if (picked) hud::fillRect(target, r, sf::Color(255, 210, 60, 40));
            text::draw(target, (picked ? "> " : "  ") + step.options[i].first, {r.left + 2.f, r.top + 2.f}, picked ? hud::GOLD : hud::WHITE);
        }
        return;
    }

    if (!step.speaker.empty()) text::draw(target, step.speaker, {left, BOX.top + 4.f}, hud::GOLD);
    size_t shown = static_cast<size_t>(typed);
    for (size_t i = 0; i < pages[page].size(); ++i) {
        text::draw(target, pages[page][i], {left, BOX.top + 16.f + text::LINE_HEIGHT * static_cast<float>(i)}, hud::WHITE, shown);
        shown -= std::min(shown, pages[page][i].size());
    }
    // All shown: a blinking arrow says there's more, or that it's the end.
    if (typed >= static_cast<float>(letters(pages[page])) && std::fmod(time, 0.8f) < 0.5f) {
        const sf::Vector2f arrow(BOX.left + BOX.width - 10.f, BOX.top + BOX.height - 7.f);
        for (float row = 0.f; row < 3.f; ++row) hud::fillRect(target, {arrow.x + row, arrow.y + row, 5.f - 2.f * row, 1.f}, hud::GOLD);
    }
}
