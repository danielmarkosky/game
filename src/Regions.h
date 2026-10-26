#pragma once

#include <string>
#include <vector>

// The regions of the world, as the map names them: read from assets/regions.txt (see the note at
// its top). Each room's `region` field is one of their ids.
namespace regions
{
    struct Region
    {
        std::string id;    // a room's `region`
        std::string name;  // "THE HIDDEN LAKE"
    };

    const std::vector<Region>& all();
    // The name of a region, or "" if there's no such region.
    std::string name(const std::string& id);
}
