#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

// What a character says, from a text file: assets/dialogue/<id>.txt. See
// assets/dialogue/README.md for how they're written: lines, choices, jumps to labels, flags, and
// commands: giving you an ability.

// One step of a conversation.
struct DialogueStep
{
    enum class Kind { Line, Choice, Set, Do, Jump, Label };
    Kind kind = Kind::Line;
    std::string speaker; // Line: who says it ("You" is the player)
    std::string text;    // Line: what's said. Set: the flag. Do: the command. Jump, Label: the label
    std::vector<std::pair<std::string, std::string>> options; // Choice: what you can say, and the label it leads to
};

// One talk with a character.
struct Conversation
{
    std::vector<std::string> needs; // facts that must hold for this one ("!fact": must not), all of them
    std::vector<DialogueStep> steps;
};

class Dialogue
{
public:
    using Facts = std::function<bool(const std::string&)>;

    // Throws std::runtime_error if the file can't be read, has nothing to say, or jumps to a
    // label that isn't there.
    static Dialogue load(const std::string& id);
    static Dialogue parse(const std::string& text);
    // The commands a conversation can give: "give ability rope", "open shop" (Mags's stall, once
    // the talking's done). True if `command` is one.
    static bool isCommand(const std::string& command);

    const std::vector<Conversation>& conversations() const;

    // Whether the facts (abilities and story flags) allow a conversation.
    static bool allowed(const Conversation& conversation, const Facts& has);
    // Which conversation to have now: the first one not had yet that the facts allow. Once those
    // have all been had, the last of them, again.
    std::optional<size_t> next(const std::function<bool(size_t)>& had, const Facts& has) const;

private:
    std::vector<Conversation> m_conversations;
};

// Steps through a conversation: says its lines, stops at choices, follows jumps and sets flags.
class DialogueRunner
{
public:
    // `act` is told every command ("give ability rope") as the conversation reaches it.
    DialogueRunner(const Conversation& conversation, std::function<void(const std::string&)> setFlag,
                   std::function<void(const std::string&)> act = {});

    bool finished() const;
    // The line being said, or the choice being made.
    const DialogueStep& current() const;
    // On to what comes after the current line.
    void next();
    // Picks one of the current choice's options.
    void choose(size_t option);

private:
    void run(size_t from); // to the next line or choice
    void jump(const std::string& label);

    const Conversation* conversation;
    std::function<void(const std::string&)> setFlag;
    std::function<void(const std::string&)> act;
    size_t at = 0;
};
