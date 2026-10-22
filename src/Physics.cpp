#include "Physics.h"
#include <fstream>
#include <sstream>

namespace
{
    struct Field
    {
        const char* name;
        float Physics::* value;
    };
    const Field FIELDS[] = {
        {"run_speed", &Physics::runSpeed},
        {"run_acceleration", &Physics::runAcceleration},
        {"run_deceleration", &Physics::runDeceleration},
        {"air_acceleration", &Physics::airAcceleration},
        {"air_deceleration", &Physics::airDeceleration},
        {"fling_deceleration", &Physics::flingDeceleration},
        {"gravity", &Physics::gravity},
        {"jump_speed", &Physics::jumpSpeed},
        {"jump_release_gravity", &Physics::jumpReleaseGravity},
        {"max_fall_speed", &Physics::maxFallSpeed},
        {"coyote_time", &Physics::coyoteTime},
        {"jump_buffer_time", &Physics::jumpBufferTime},
        {"air_jump_speed", &Physics::airJumpSpeed},
        {"rope_range", &Physics::ropeRange},
        {"long_rope_range", &Physics::longRopeRange},
        {"air_throw_range", &Physics::airThrowRange},
        {"throw_windup", &Physics::throwWindup},
        {"throw_speed", &Physics::throwSpeed},
        {"rope_return_speed", &Physics::ropeReturnSpeed},
        {"swing_push", &Physics::swingPush},
        {"max_swing_speed", &Physics::maxSwingSpeed},
        {"reel_speed", &Physics::reelSpeed},
        {"rope_jump_speed", &Physics::ropeJumpSpeed},
        {"climb_speed", &Physics::climbSpeed},
        {"climb_down_speed", &Physics::climbDownSpeed},
        {"climb_stamina", &Physics::climbStamina},
        {"wall_slide_speed", &Physics::wallSlideSpeed},
        {"wall_jump_push", &Physics::wallJumpPush},
        {"climb_over_speed", &Physics::climbOverSpeed},
    };

    Physics current;
}

Physics Physics::parse(const std::string& text, std::vector<std::string>* problems)
{
    Physics p;
    std::istringstream lines(text);
    int number = 0;
    for (std::string line; std::getline(lines, line);) {
        ++number;
        if (const size_t comment = line.find('#'); comment != std::string::npos) line.resize(comment);
        std::istringstream words(line);
        std::string name;
        if (!(words >> name)) continue;
        const Field* field = nullptr;
        for (const Field& f : FIELDS) {
            if (name == f.name) field = &f;
        }
        float value;
        if (!field) {
            if (problems) problems->push_back("line " + std::to_string(number) + ": no number called " + name);
        } else if (!(words >> value) || value < 0.f) {
            if (problems) problems->push_back("line " + std::to_string(number) + ": " + name + " needs a number, 0 or more");
        } else {
            p.*(field->value) = value;
        }
    }
    return p;
}

std::string Physics::serialize() const
{
    std::ostringstream out;
    for (const Field& f : FIELDS) out << f.name << " " << this->*(f.value) << "\n";
    return out.str();
}

const Physics& physics()
{
    return current;
}

void setPhysics(const Physics& numbers)
{
    current = numbers;
}

std::vector<std::string> loadPhysics(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        current = {};
        return {"can't read " + path + ", so the usual numbers"};
    }
    std::stringstream text;
    text << file.rdbuf();
    std::vector<std::string> problems;
    current = Physics::parse(text.str(), &problems);
    return problems;
}
