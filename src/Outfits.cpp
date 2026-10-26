#include "Outfits.h"
#include "src/Tables.h"
#include <cstdlib>

namespace outfits
{
    const std::vector<Outfit>& all()
    {
        // "outfit <id> | <price> | <name> | <text>" lines.
        static const std::vector<Outfit> loaded = [] {
            std::vector<Outfit> found;
            for (const auto& row : readTable("outfits.txt", "outfit")) {
                if (row.size() >= 4) found.push_back({row[0], std::atoi(row[1].c_str()), row[2], row[3]});
            }
            return found;
        }();
        return loaded;
    }

    const Outfit* find(const std::string& id)
    {
        for (const Outfit& outfit : all()) {
            if (outfit.id == id) return &outfit;
        }
        return nullptr;
    }
}
