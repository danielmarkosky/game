#include "Regions.h"
#include "src/Tables.h"

namespace regions
{
    const std::vector<Region>& all()
    {
        // "region <id> | <name>" lines.
        static const std::vector<Region> loaded = [] {
            std::vector<Region> found;
            for (const auto& row : readTable("regions.txt", "region")) {
                if (row.size() >= 2) found.push_back({row[0], row[1]});
            }
            return found;
        }();
        return loaded;
    }

    std::string name(const std::string& id)
    {
        for (const Region& r : all()) {
            if (r.id == id) return r.name;
        }
        return "";
    }
}
