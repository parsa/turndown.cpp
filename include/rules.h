// turndown.cpp/include/rules.h
#ifndef RULES_H
#define RULES_H

#include "gumbo_adapter.h"

#include <functional>
#include <string>
#include <vector>

namespace turndown_cpp {

struct TurndownOptions;

struct Rule {
    std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter;
    std::function<std::string(std::string const&, gumbo::NodeView, TurndownOptions const&)> replacement;
    std::function<std::string(TurndownOptions const&)> append;
    std::string key;
};

class Rules {
public:
    explicit Rules(TurndownOptions const& options);
    ~Rules();

    void addRule(std::string const& key, Rule rule);
    void keep(std::string const& filter);
    void keep(std::vector<std::string> const& filters);
    void keep(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter);
    void remove(std::string const& filter);
    void remove(std::vector<std::string> const& filters);
    void remove(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter);
    Rule const& forNode(gumbo::NodeView node) const;
    void forEach(std::function<void(Rule const&)> fn) const;

private:
    std::function<bool(gumbo::NodeView, TurndownOptions const&)> makeTagFilter(std::vector<std::string> const& filters) const;
    void addKeepRule(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter, std::string const& keySuffix);
    void addRemoveRule(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter, std::string const& keySuffix);
    Rule const* findRule(std::vector<Rule> const& candidates, gumbo::NodeView node) const;

    TurndownOptions const& options;
    std::vector<Rule> rulesArray;
    std::vector<Rule> keepRules;
    std::vector<Rule> removeRules;

    Rule blankRule;
    Rule keepReplacementRule;
    Rule defaultRule;
};

} // namespace turndown_cpp

#endif // RULES_H