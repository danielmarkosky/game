#pragma once

#include <string>
#include <vector>

// The lists in assets/ that are tables: regions.txt, outfits.txt. A line "<keyword> a | b | c" is a
// row, its cells trimmed; anything else (notes, blank lines) isn't.
std::vector<std::vector<std::string>> readTable(const std::string& asset, const std::string& keyword);
