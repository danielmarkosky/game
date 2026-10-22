#include "Props.h"
#include "src/Config.h"
#include <fstream>
#include <sstream>

namespace props
{
    const std::vector<PropInfo>& all()
    {
        static const std::vector<PropInfo> list = [] {
            std::vector<PropInfo> found;
            std::ifstream file(config::assetPath("gfx/props.txt"));
            for (std::string line; std::getline(file, line);) {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream words(line);
                PropInfo p;
                int x, y, w, h, hangs, r, g, b;
                if (!(words >> p.name >> x >> y >> w >> h >> p.frames >> p.fps >> hangs >> r >> g >> b >> p.lightRadius)) continue;
                p.rect = {x, y, w, h};
                p.hangs = hangs != 0;
                p.light = sf::Color(static_cast<sf::Uint8>(r), static_cast<sf::Uint8>(g), static_cast<sf::Uint8>(b));
                found.push_back(p);
            }
            return found;
        }();
        return list;
    }

    const PropInfo* find(const std::string& name)
    {
        for (const PropInfo& p : all()) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }

    const sf::Texture& texture()
    {
        static const sf::Texture sheet = [] {
            sf::Texture t;
            t.loadFromFile(config::assetPath("gfx/props.png"));
            return t;
        }();
        return sheet;
    }
}
