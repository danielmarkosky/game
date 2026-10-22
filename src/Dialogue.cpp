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

    bool startsWith(const std::string& s, const std::string& prefix)
    {
        return s.rfind(prefix, 0) == 0;
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

    // Checks every jump and option leads to a label in the same conversation.
    void checkLabels(const Conversation& c)
    {
        auto exists = [&](const std::string& label) {
            if (label == "end") return true;
            for (const auto& step : c.steps) {
                if (step.kind == DialogueStep::Kind::Label && step.text == label) return true;
            }
            return false;
        };
        for (const auto& step : c.steps) {
            if (step.kind == DialogueStep::Kind::Jump && !exists(step.text)) throw std::runtime_error("No label '" + step.text + "' to jump to");
            for (const auto& [text, label] : step.options) {
                if (!exists(label)) throw std::runtime_error("No label '" + label + "' for the choice '" + text + "'");
            }
        }
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
    try {
        for (const auto& c : dialogue.m_conversations) checkLabels(c);
    } catch (const std::runtime_error& e) {
        throw std::runtime_error(std::string(e.what()) + " in " + path);
    }
    return dialogue;
}

bool Dialogue::isCommand(const std::string& command)
{
    std::istringstream words(command);
    std::string verb, what, name;
    words >> verb >> what >> name;
    if (verb == "shop") return what.empty();
    if (verb == "race") return !what.empty() && name.empty();
    if (verb == "give") return (what == "gems" || what == "outfit" || what == "item") && !name.empty();
    if (verb == "take") return what == "item" && !name.empty();
    return false;
}

Dialogue Dialogue::parse(const std::string& text)
{
    Dialogue dialogue;
    Conversation current;
    std::string speaker;
    auto finish = [&] {
        if (!current.steps.empty()) dialogue.m_conversations.push_back(current);
        current = {};
        speaker.clear();
    };

    std::istringstream stream(text);
    for (std::string raw; std::getline(stream, raw);) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (startsWith(line, "---")) {
            finish();
            std::istringstream needs(line.substr(3));
            for (std::string fact; needs >> fact;) current.needs.push_back(fact);
            continue;
        }
        DialogueStep step;
        if (startsWith(line, "= ")) {
            step.kind = DialogueStep::Kind::Label;
            step.text = trim(line.substr(2));
            speaker.clear();
        } else if (startsWith(line, "-> ")) {
            step.kind = DialogueStep::Kind::Jump;
            step.text = trim(line.substr(3));
        } else if (startsWith(line, "set ")) {
            step.kind = DialogueStep::Kind::Set;
            step.text = trim(line.substr(4));
        } else if (isCommand(line)) {
            step.kind = DialogueStep::Kind::Do;
            step.text = line;
        } else if (startsWith(line, "? ")) {
            // Choices next to each other are one choice.
            const size_t arrow = line.find("->");
            const std::string said = trim(line.substr(2, arrow == std::string::npos ? std::string::npos : arrow - 2));
            const std::string label = arrow == std::string::npos ? "end" : trim(line.substr(arrow + 2));
            if (!current.steps.empty() && current.steps.back().kind == DialogueStep::Kind::Choice) {
                current.steps.back().options.push_back({said, label});
                continue;
            }
            step.kind = DialogueStep::Kind::Choice;
            step.options.push_back({said, label});
        } else {
            step.kind = DialogueStep::Kind::Line;
            step.text = line;
            splitSpeaker(line, speaker, step.text);
            step.speaker = speaker;
        }
        current.steps.push_back(step);
    }
    finish();
    return dialogue;
}

const std::vector<Conversation>& Dialogue::conversations() const
{
    return m_conversations;
}

bool Dialogue::allowed(const Conversation& conversation, const Facts& has)
{
    for (const std::string& need : conversation.needs) {
        const bool negated = need[0] == '!';
        if (has(negated ? need.substr(1) : need) == negated) return false;
    }
    return true;
}

std::optional<size_t> Dialogue::next(const std::function<bool(size_t)>& had, const Facts& has) const
{
    std::optional<size_t> lastAllowed;
    for (size_t i = 0; i < m_conversations.size(); ++i) {
        if (!allowed(m_conversations[i], has)) continue;
        if (!had(i)) return i;
        lastAllowed = i;
    }
    return lastAllowed;
}

DialogueRunner::DialogueRunner(const Conversation& c, std::function<void(const std::string&)> set,
                               std::function<void(const std::string&)> command)
    : conversation(&c), setFlag(std::move(set)), act(std::move(command))
{
    run(0);
}

bool DialogueRunner::finished() const
{
    return at >= conversation->steps.size();
}

const DialogueStep& DialogueRunner::current() const
{
    return conversation->steps[at];
}

void DialogueRunner::next()
{
    if (!finished()) run(at + 1);
}

void DialogueRunner::choose(size_t option)
{
    if (finished() || current().kind != DialogueStep::Kind::Choice || option >= current().options.size()) return;
    jump(current().options[option].second);
}

void DialogueRunner::run(size_t from)
{
    const auto& steps = conversation->steps;
    for (at = from; at < steps.size(); ++at) {
        switch (steps[at].kind) {
            case DialogueStep::Kind::Line:
            case DialogueStep::Kind::Choice: return;
            case DialogueStep::Kind::Set: setFlag(steps[at].text); break;
            case DialogueStep::Kind::Do:
                if (act) act(steps[at].text);
                break;
            case DialogueStep::Kind::Jump: jump(steps[at].text); return;
            // Reaching the next label is the end of a branch.
            case DialogueStep::Kind::Label: at = steps.size(); return;
        }
    }
}

void DialogueRunner::jump(const std::string& label)
{
    const auto& steps = conversation->steps;
    for (size_t i = 0; i < steps.size(); ++i) {
        if (steps[i].kind == DialogueStep::Kind::Label && steps[i].text == label) {
            run(i + 1);
            return;
        }
    }
    at = steps.size(); // "end", or nowhere
}
