// turndown.cpp/include/turndown.h
#ifndef TURNDOWN_H
#define TURNDOWN_H

#include "dom_source.h"
#include "gumbo_adapter.h"
#include "rules.h"
#include "utilities.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace turndown_cpp {

struct TurndownOptions {
    // Existing basic options:
    std::string headingStyle; // "setext" or "atx"
    std::string hr;
    std::string bulletListMarker;      // *, -, +
    std::string codeBlockStyle; // "indented" or "fenced"
    std::string fence;               // ```, ~~~
    std::string emDelimiter;           // _, *
    std::string strongDelimiter;      // **, __

    // Link/reference style:
    std::string linkStyle;       // "inlined" or "referenced"
    std::string linkReferenceStyle; // "full", "collapsed", "shortcut"

    // Line break
    std::string br;

    // Preformatted code option
    bool preformattedCode;

    // Customizable escaping logic.
    std::function<std::string(std::string const&)> escapeFunction;

    // Keep tags
    std::vector<std::string> keepTags;

    // Customizable replacement functions:
    std::function<std::string(std::string const&, gumbo::NodeView)> blankReplacement;
    std::function<std::string(std::string const&, gumbo::NodeView)> keepReplacement;
    std::function<std::string(std::string const&, gumbo::NodeView)> defaultReplacement;

    TurndownOptions(); // Constructor to set defaults
};

class DomSource;

class TurndownService {
public:
    using Plugin = std::function<void(TurndownService&)>;
    using RuleFactory = std::function<void(Rules&)>;
    using KeepFilter = std::function<bool(gumbo::NodeView, TurndownOptions const&)>;
    using RemoveFilter = KeepFilter;

    enum class RulePlacement {
        BeforeDefaults,
        AfterDefaults
    };

    TurndownService();
    explicit TurndownService(TurndownOptions options);

    TurndownService& configureOptions(std::function<void(TurndownOptions&)> fn);
    TurndownService& use(Plugin plugin);
    TurndownService& addRule(std::string const& key, Rule rule);
    TurndownService& registerRuleFactory(RuleFactory factory,
                                         RulePlacement placement = RulePlacement::AfterDefaults);
    TurndownService& keep(KeepFilter filter);
    TurndownService& keep(std::string const& tag);
    TurndownService& keep(std::vector<std::string> const& tags);
    TurndownService& remove(RemoveFilter filter);
    TurndownService& remove(std::string const& tag);
    TurndownService& remove(std::vector<std::string> const& tags);

    std::string turndown(std::string const& html);
    std::string turndown(gumbo::NodeView root);
    std::string turndown(DomSource const& dom);

    std::string escape(std::string const& text) const;

    TurndownOptions& options();
    TurndownOptions const& options() const;

private:
    void invalidateRules();
    Rules& ensureRules();
    std::string runPipeline(gumbo::NodeView root);
    void enqueueRuleMutation(std::function<void(Rules&)> fn);

    TurndownOptions options_;
    std::unique_ptr<Rules> rules_;
    std::vector<RuleFactory> preRuleFactories_;
    std::vector<RuleFactory> postRuleFactories_;
    std::vector<std::function<void(Rules&)>> ruleMutations_;
};

std::string turndown(std::string const& html, TurndownOptions const& options);

} // namespace turndown_cpp

#endif // TURNDOWN_H