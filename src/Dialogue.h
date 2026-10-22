#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

// What a character says, from a text file: assets/dialogue/<id>.txt. See
// assets/dialogue/README.md for how they're written.
struct DialogueLine
{
    std::string speaker; // "You" is the player
    std::string text;
};

// One talk with a character: what's said, one line after the other.
struct Conversation
{
    std::string needs; // an ability the player has to have for this one ("!ability": not have yet), or empty
    std::vector<DialogueLine> lines;
};

class Dialogue
{
public:
    // Throws std::runtime_error if the file can't be read or has nothing to say.
    static Dialogue load(const std::string& id);
    static Dialogue parse(const std::string& text);

    const std::vector<Conversation>& conversations() const;

    // Which conversation to have now: the first one not had yet that the player's abilities allow.
    // Once those have all been had, the last of them, again.
    std::optional<size_t> next(const std::function<bool(size_t)>& had, const std::function<bool(const std::string&)>& has) const;

private:
    std::vector<Conversation> m_conversations;
};
