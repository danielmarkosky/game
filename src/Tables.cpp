#include "Tables.h"
#include "src/Config.h"
#include <fstream>

namespace
{
    std::string trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }
}

std::vector<std::vector<std::string>> readTable(const std::string& asset, const std::string& keyword)
{
    std::vector<std::vector<std::string>> rows;
    std::ifstream in(config::assetPath(asset));
    for (std::string line; std::getline(in, line);) {
        line = trim(line);
        if (line.rfind(keyword + " ", 0) != 0) continue;
        std::vector<std::string> cells;
        std::string rest = line.substr(keyword.size() + 1);
        for (size_t bar; (bar = rest.find('|')) != std::string::npos; rest = rest.substr(bar + 1)) cells.push_back(trim(rest.substr(0, bar)));
        cells.push_back(trim(rest));
        rows.push_back(std::move(cells));
    }
    return rows;
}
