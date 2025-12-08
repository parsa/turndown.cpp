// turndown.cpp/src/rules.cpp
#include "rules.h"
#include "gumbo_adapter.h"
#include "node.h"
#include "turndown.h"
#include "utilities.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace turndown_cpp {

namespace {

// Lowercases a vector of tag names in-place and returns it.
std::vector<std::string> normalizeTags(std::vector<std::string> tags) {
    for (auto& tag : tags) {
        std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    }
    return tags;
}

} // namespace

// Initializes rule set with built-in blank/keep/default rules.
Rules::Rules(TurndownOptions const& opts)
    : options(opts) {
    blankRule = Rule{
        [](gumbo::NodeView, TurndownOptions const&) { return true; },
        [this](std::string const& content, gumbo::NodeView node, TurndownOptions const&) {
            return options.blankReplacement(content, node);
        },
        nullptr,
        "blank"
    };

    keepReplacementRule = Rule{
        [](gumbo::NodeView, TurndownOptions const&) { return true; },
        [this](std::string const& content, gumbo::NodeView node, TurndownOptions const&) {
            return options.keepReplacement(content, node);
        },
        nullptr,
        "keep-replacement"
    };

    defaultRule = Rule{
        [](gumbo::NodeView, TurndownOptions const&) { return true; },
        [this](std::string const& content, gumbo::NodeView node, TurndownOptions const&) {
            return options.defaultReplacement(content, node);
        },
        nullptr,
        "default"
    };
}

// Defaulted destructor for Rules.
Rules::~Rules() = default;

// Adds a rule to the front of the main rule list.
void Rules::addRule(std::string const& key, Rule rule) {
    rule.key = key;
    rulesArray.insert(rulesArray.begin(), std::move(rule));
}

// Builds a filter that matches element tags against provided names.
std::function<bool(gumbo::NodeView, TurndownOptions const&)> Rules::makeTagFilter(std::vector<std::string> const& filters) const {
    auto normalized = normalizeTags(filters);
    return [normalized](gumbo::NodeView node, TurndownOptions const&) {
        if (!node || !node.is_element()) return false;
        std::string tag = node.tag_name();
        for (auto const& value : normalized) {
            if (value == tag) {
                return true;
            }
        }
        return false;
    };
}

// Adds a keep rule with a generated key suffix.
void Rules::addKeepRule(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter, std::string const& keySuffix) {
    Rule rule;
    rule.key = "keep-" + keySuffix;
    rule.filter = std::move(filter);
    rule.replacement = keepReplacementRule.replacement;
    keepRules.insert(keepRules.begin(), std::move(rule));
}

// Adds a remove rule with a generated key suffix.
void Rules::addRemoveRule(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter, std::string const& keySuffix) {
    Rule rule;
    rule.key = "remove-" + keySuffix;
    rule.filter = std::move(filter);
    rule.replacement = [](std::string const&, gumbo::NodeView, TurndownOptions const&) {
        return std::string();
    };
    removeRules.insert(removeRules.begin(), std::move(rule));
}

// Keep rule for a single tag.
void Rules::keep(std::string const& filter) {
    addKeepRule(makeTagFilter({filter}), filter);
}

// Keep rule for multiple tags.
void Rules::keep(std::vector<std::string> const& filters) {
    addKeepRule(makeTagFilter(filters), "multi");
}

// Keep rule using a custom predicate.
void Rules::keep(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter) {
    addKeepRule(std::move(filter), "custom");
}

// Remove rule for a single tag.
void Rules::remove(std::string const& filter) {
    addRemoveRule(makeTagFilter({filter}), filter);
}

// Remove rule for multiple tags.
void Rules::remove(std::vector<std::string> const& filters) {
    addRemoveRule(makeTagFilter(filters), "multi");
}

// Remove rule using a custom predicate.
void Rules::remove(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter) {
    addRemoveRule(std::move(filter), "custom");
}

// Returns the first rule whose filter matches the node, or nullptr.
Rule const* Rules::findRule(std::vector<Rule> const& candidates, gumbo::NodeView node) const {
    for (auto const& rule : candidates) {
        if (rule.filter(node, options)) {
            return &rule;
        }
    }
    return nullptr;
}

// Selects the applicable rule for a node, honoring blank/keep/remove/default order.
Rule const& Rules::forNode(gumbo::NodeView node) const {
    if (!isVoid(node) && isBlank(node)) {
        return blankRule;
    }

    if (auto* rule = findRule(rulesArray, node)) return *rule;
    if (auto* rule = findRule(keepRules, node)) return *rule;
    if (auto* rule = findRule(removeRules, node)) return *rule;

    return defaultRule;
}

// Iterates all main rules and applies the provided functor.
void Rules::forEach(std::function<void(Rule const&)> fn) const {
    for (auto const& rule : rulesArray) {
        fn(rule);
    }
}

} // namespace turndown_cpp