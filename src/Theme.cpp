#include "Theme.h"
#include "Config.h"
#include <map>
#include <stdexcept>

std::shared_ptr<const Theme> Theme::get(const std::string& name)
{
    // Weak, so a theme's textures are freed once no level uses it.
    static std::map<std::string, std::weak_ptr<const Theme>> loaded;
    if (auto theme = loaded[name].lock()) return theme;

    auto theme = std::make_shared<Theme>();
    theme->name = name;
    const std::string folder = "gfx/themes/" + name + "/";
    auto load = [&](sf::Texture& texture, const std::string& file) {
        if (!texture.loadFromFile(config::assetPath(folder + file))) {
            throw std::runtime_error("Missing theme file: " + folder + file);
        }
    };
    load(theme->tiles, "tiles.png");
    const char* layerFiles[] = {"sky.png", "far.png", "near.png"};
    for (size_t i = 0; i < theme->layers.size(); ++i) {
        load(theme->layers[i], layerFiles[i]);
        theme->layers[i].setRepeated(true);
    }
    loaded[name] = theme;
    return theme;
}
