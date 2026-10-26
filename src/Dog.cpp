#include "Dog.h"
#include "src/utils.h"
#include <cmath>

void Dog::reset(sf::Vector2f feet)
{
    path.clear();
    path.push_back(feet - sf::Vector2f(BEHIND * 0.7f, 0.f));
    path.push_back(feet);
    at = path.front();
    moved = {};
    still = 0.f;
}

void Dog::shift(sf::Vector2f by)
{
    for (sf::Vector2f& point : path) point += by;
    at += by;
}

void Dog::follow(sf::Vector2f feet, float dt)
{
    if (path.empty()) reset(feet);
    if (distance(path.back(), feet) >= 0.5f) path.push_back(feet);
    // As far behind you as the path is long, but never further than BEHIND.
    float length = 0.f;
    for (size_t ii = path.size() - 1; ii > 0; --ii) length += distance(path[ii], path[ii - 1]);
    while (path.size() > 2 && length - distance(path[0], path[1]) >= BEHIND) {
        length -= distance(path[0], path[1]);
        path.pop_front();
    }
    const sf::Vector2f before = at;
    at = path.front();
    moved = at - before;
    if (std::abs(moved.x) > 0.05f) facingLeft = moved.x < 0.f;
    still = magnitude(moved) < 0.05f ? still + dt : 0.f;
    pointing = std::max(0.f, pointing - dt);
}

void Dog::point(sf::Vector2f target)
{
    pointing = 1.2f;
    facingLeft = target.x < at.x;
}

bool Dog::isPointing() const
{
    return pointing > 0.f;
}

sf::Vector2f Dog::feet() const
{
    return at;
}

void Dog::draw(sf::RenderTarget& target, const Player& looks, sf::Vector2f centr, float time) const
{
    using Pose = Player::Pose;
    Pose pose = Pose::Idle;
    if (pointing > 0.f) pose = pointing > 0.6f ? Pose::Climb : Pose::Swing; // barking, then pointing
    else if (moved.y > 0.3f) pose = Pose::Jump;
    else if (moved.y < -0.3f) pose = Pose::Fall;
    else if (std::abs(moved.x) > 0.05f) pose = Pose::Run;
    else if (still > 1.5f) pose = Pose::Sit;
    const sf::Vector2f p = viewCoordinates(at, centr);
    looks.drawPose(target, Player::lookNamed("pip"), pose, {std::round(p.x), std::round(p.y)}, facingLeft, time);
}
