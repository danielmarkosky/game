#include "Catalog.h"
#include "src/Config.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace catalog
{
    namespace
    {
        std::string trim(const std::string& s)
        {
            const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
            return a == std::string::npos ? "" : s.substr(a, b - a + 1);
        }

        // "kind a | b | c" lines, split at the bars, of the given kind.
        std::vector<std::vector<std::string>> lines(const std::string& file, const std::string& kind)
        {
            std::vector<std::vector<std::string>> found;
            std::ifstream in(config::assetPath(file));
            for (std::string line; std::getline(in, line);) {
                line = trim(line);
                if (line.rfind(kind + " ", 0) != 0) continue;
                std::vector<std::string> parts;
                std::istringstream rest(line.substr(kind.size() + 1));
                for (std::string part; std::getline(rest, part, '|');) parts.push_back(trim(part));
                found.push_back(parts);
            }
            return found;
        }

        struct All
        {
            std::vector<Item> items;
            std::vector<Quest> quests;
            std::vector<Race> races;
            std::vector<Outfit> outfits;
        };

        const All& all()
        {
            static const All loaded = [] {
                All a;
                for (const auto& p : lines("quests.txt", "item")) {
                    if (p.size() >= 4) a.items.push_back({p[0], p[1], p[2], p[3]});
                }
                for (const auto& p : lines("quests.txt", "quest")) {
                    if (p.size() >= 5) a.quests.push_back({p[0], p[1], p[2], p[3], p[4]});
                }
                for (const auto& p : lines("quests.txt", "race")) {
                    if (p.size() >= 2) a.races.push_back({p[0], std::strtof(p[1].c_str(), nullptr)});
                }
                for (const auto& p : lines("outfits.txt", "outfit")) {
                    if (p.size() >= 3) a.outfits.push_back({p[0], std::atoi(p[1].c_str()), p[2]});
                }
                return a;
            }();
            return loaded;
        }
    }

    const std::vector<Item>& items() { return all().items; }
    const std::vector<Quest>& quests() { return all().quests; }
    const std::vector<Race>& races() { return all().races; }
    const std::vector<Outfit>& outfits() { return all().outfits; }

    const Item* item(const std::string& id)
    {
        for (const Item& i : items()) {
            if (i.id == id) return &i;
        }
        return nullptr;
    }

    const Race* race(const std::string& id)
    {
        for (const Race& r : races()) {
            if (r.id == id) return &r;
        }
        return nullptr;
    }

    const Outfit* outfit(const std::string& look)
    {
        for (const Outfit& o : outfits()) {
            if (o.look == look) return &o;
        }
        return nullptr;
    }
}
