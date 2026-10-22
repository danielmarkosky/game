#include "Dialogue.h"
#include "Config.h"
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string trim(const std::string& s)
    {
        const size_t first = s.find_first_not_of(" \t\r");
        if (first == std::string::npos) return "";
        return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
    }

    // "Wren: Hello there" names who's speaking: a capitalised name of letters and spaces, up to
    // 20 characters, then a colon. Anything else is the same speaker as the line before.
    bool splitSpeaker(const std::string& line, std::string& speaker, std::string& said)
    {
        const size_t colon = line.find(": ");
        if (colon == std::string::npos || colon == 0 || colon > 20 || !std::isupper(static_cast<unsigned char>(line[0]))) return false;
        for (size_t i = 0; i < colon; ++i) {
            const unsigned char c = static_cast<unsigned char>(line[i]);
            if (!std::isalpha(c) && c != ' ' && c != '\'') return false;
        }
        speaker = line.substr(0, colon);
        said = trim(line.substr(colon + 2));
        return true;
    }
}

Dialogue Dialogue::load(const std::string& id)
{
    const std::string path = config::assetPath("dialogue/" + id + ".txt");
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Can't open dialogue: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    Dialogue dialogue = parse(buffer.str());
    if (dialogue.m_conversations.empty()) throw std::runtime_error("Nothing to say in " + path);
    return dialogue;
}

Dialogue Dialogue::parse(const std::string& text)
{
    Dialogue dialogue;
    Conversation current;
    std::string speaker;
    auto finish = [&] {
        if (!current.lines.empty()) dialogue.m_conversations.push_back(current);
        current = {};
        speaker.clear();
    };

    std::istringstream stream(text);
    for (std::string raw; std::getline(stream, raw);) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("---", 0) == 0) {
            finish();
            current.needs = trim(line.substr(3));
            continue;
        }
        std::string said = line;
        splitSpeaker(line, speaker, said);
        current.lines.push_back({speaker, said});
    }
    finish();
    return dialogue;
}

const std::vector<Conversation>& Dialogue::conversations() const
{
    return m_conversations;
}

std::optional<size_t> Dialogue::next(const std::function<bool(size_t)>& had, const std::function<bool(const std::string&)>& has) const
{
    std::optional<size_t> lastAvailable;
    for (size_t i = 0; i < m_conversations.size(); ++i) {
        const std::string& needs = m_conversations[i].needs;
        const bool allowed = needs.empty() || (needs[0] == '!' ? !has(needs.substr(1)) : has(needs));
        if (!allowed) continue;
        if (!had(i)) return i;
        lastAvailable = i;
    }
    return lastAvailable;
}
