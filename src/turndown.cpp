// turndown.cpp/src/turndown.cpp
#include "turndown.h"
#include "collapse_whitespace.h"
#include "commonmark_rules.h"
#include "dom_source.h"
#include "gumbo_adapter.h"
#include "node.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace turndown_cpp {

// Sets default Markdown conversion options mirroring Turndown JS defaults.
turndown_cpp::TurndownOptions::TurndownOptions() :
    headingStyle("setext"),
    hr("* * *"),
    bulletListMarker("*"),
    codeBlockStyle("indented"),
    fence("```"),
    emDelimiter("_"),
    strongDelimiter("**"),
    linkStyle("inlined"),
    linkReferenceStyle("full"),
    br("  "),
    preformattedCode(false),
    escapeFunction(advancedEscape),
    keepTags({}),
    blankReplacement([](std::string const& content, gumbo::NodeView node) -> std::string {
        return isBlock(node) ? "\n\n" : "";
    }),
    keepReplacement([](std::string const& content, gumbo::NodeView node) -> std::string {
        (void)content;
        return serializeNode(node);
    }),
    defaultReplacement([](std::string const& content, gumbo::NodeView node) -> std::string {
        return isBlock(node) ? "\n\n" + content + "\n\n" : content;
    })
{}

// Drops leading CR/LF characters from a string.
static std::string trimLeadingNewlines(std::string const& text) {
    std::size_t index = 0;
    while (index < text.size() && (text[index] == '\n' || text[index] == '\r')) {
        ++index;
    }
    return text.substr(index);
}

// Drops trailing CR/LF characters from a string.
static std::string trimTrailingNewlines(std::string const& text) {
    std::size_t index = text.size();
    while (index > 0 && (text[index - 1] == '\n' || text[index - 1] == '\r')) {
        --index;
    }
    return text.substr(0, index);
}

// Concatenates two Markdown chunks, normalizing surrounding newlines.
static std::string joinChunks(std::string const& output, std::string const& addition) {
    if (output.empty()) return addition;
    if (addition.empty()) return output;

    std::string left = trimTrailingNewlines(output);
    std::string right = trimLeadingNewlines(addition);

    std::size_t leftRemoved = output.size() - left.size();
    std::size_t rightRemoved = addition.size() - right.size();
    std::size_t separatorLength = std::max(leftRemoved, rightRemoved);
    if (separatorLength > 2) separatorLength = 2;

    std::string separator(separatorLength, '\n');
    return left + separator + right;
}

// Dispatches a gumbo node to the appropriate conversion routine.
static std::string processNode(gumbo::NodeView node, TurndownOptions const& options, Rules& rules);
// Converts all children of a node to Markdown and concatenates them.
static std::string processChildren(gumbo::NodeView parent, TurndownOptions const& options, Rules& rules);
// Applies the matching rule to produce Markdown for a node, adding flanking whitespace.
static std::string replacementForNode(gumbo::NodeView node, TurndownOptions const& options, Rules& rules, NodeMetadata const& meta);

// Replaces UTF-8 NBSP bytes with "&nbsp;" entities in-place.
static void encodeNbsp(std::string& text) {
    std::string const nbsp = "\xC2\xA0";
    std::size_t pos = 0;
    while ((pos = text.find(nbsp, pos)) != std::string::npos) {
        text.replace(pos, nbsp.size(), "&nbsp;");
        pos += 6;
    }
}

// Converts a text-like node to Markdown, escaping unless inside code.
static std::string processTextNode(gumbo::NodeView node, TurndownOptions const& options, NodeMetadata const& meta) {
    std::string text = getNodeText(node);
    if (text.empty()) {
        return "";
    }
    if (meta.isCode) {
        return text;
    }
    return options.escapeFunction(text);
}

// Recursively converts a gumbo node to Markdown.
static std::string processNode(gumbo::NodeView node, TurndownOptions const& options, Rules& rules) {
    if (!node) return "";
    switch (node.type()) {
        case gumbo::NodeType::Text:
        case gumbo::NodeType::Whitespace:
        case gumbo::NodeType::CData: {
            NodeMetadata meta = analyzeNode(node, options.preformattedCode);
            return processTextNode(node, options, meta);
        }
        case gumbo::NodeType::Element: {
            NodeMetadata meta = analyzeNode(node, options.preformattedCode);
            return replacementForNode(node, options, rules, meta);
        }
        case gumbo::NodeType::Document:
            return processChildren(node, options, rules);
        default:
            return "";
    }
}

// Converts all children of a gumbo node to Markdown.
static std::string processChildren(gumbo::NodeView parent, TurndownOptions const& options, Rules& rules) {
    if (!parent) return "";

    std::string output;
    for (auto child : parent.child_range()) {
        std::string addition = processNode(child, options, rules);
        output = joinChunks(output, addition);
    }
    return output;
}

// Runs rule replacement for an element, trimming flanking whitespace when needed.
static std::string replacementForNode(gumbo::NodeView node, TurndownOptions const& options, Rules& rules, NodeMetadata const& meta) {
    for (auto const& keep : options.keepTags) {
        if (node.is_element() && keep == node.tag_name()) {
            std::string kept = processChildren(node, options, rules);
            return options.keepReplacement(kept, node);
        }
    }

    std::string content = processChildren(node, options, rules);
    if (!meta.flankingWhitespace.leading.empty() || !meta.flankingWhitespace.trailing.empty()) {
        content = trimStr(content);
    }

    Rule const& rule = rules.forNode(node);
    std::string converted = rule.replacement(content, node, options);
    return meta.flankingWhitespace.leading + converted + meta.flankingWhitespace.trailing;
}

// Constructs a service with default options.
TurndownService::TurndownService()
    : options_() {}

// Constructs a service with provided options.
TurndownService::TurndownService(TurndownOptions options)
    : options_(std::move(options)) {}

// Mutates options via callback and invalidates cached rules.
TurndownService& TurndownService::configureOptions(std::function<void(TurndownOptions&)> fn) {
    if (fn) {
        fn(options_);
        invalidateRules();
    }
    return *this;
}

// Applies a plugin to the service.
TurndownService& TurndownService::use(Plugin plugin) {
    if (plugin) {
        plugin(*this);
    }
    return *this;
}

// Enqueues a rule addition (applied lazily).
TurndownService& TurndownService::addRule(std::string const& key, Rule rule) {
    enqueueRuleMutation([key, rule = std::move(rule)](Rules& rules) mutable {
        rules.addRule(key, std::move(rule));
    });
    return *this;
}

// Registers a rule factory to run before or after defaults.
TurndownService& TurndownService::registerRuleFactory(RuleFactory factory,
                                                      RulePlacement placement) {
    if (!factory) return *this;
    if (placement == RulePlacement::BeforeDefaults) {
        preRuleFactories_.push_back(factory);
    } else {
        postRuleFactories_.push_back(factory);
    }
    invalidateRules();
    return *this;
}

// Adds a keep rule via callback.
TurndownService& TurndownService::keep(KeepFilter filter) {
    enqueueRuleMutation([filter = std::move(filter)](Rules& rules) {
        rules.keep(filter);
    });
    return *this;
}

// Adds a keep rule for a tag name.
TurndownService& TurndownService::keep(std::string const& tag) {
    enqueueRuleMutation([tag](Rules& rules) {
        rules.keep(tag);
    });
    return *this;
}

// Adds keep rules for multiple tag names.
TurndownService& TurndownService::keep(std::vector<std::string> const& tags) {
    enqueueRuleMutation([tags](Rules& rules) {
        rules.keep(tags);
    });
    return *this;
}

// Adds a remove rule via callback.
TurndownService& TurndownService::remove(RemoveFilter filter) {
    enqueueRuleMutation([filter = std::move(filter)](Rules& rules) {
        rules.remove(filter);
    });
    return *this;
}

// Adds a remove rule for a tag name.
TurndownService& TurndownService::remove(std::string const& tag) {
    enqueueRuleMutation([tag](Rules& rules) {
        rules.remove(tag);
    });
    return *this;
}

// Adds remove rules for multiple tag names.
TurndownService& TurndownService::remove(std::vector<std::string> const& tags) {
    enqueueRuleMutation([tags](Rules& rules) {
        rules.remove(tags);
    });
    return *this;
}

// Converts an HTML string to Markdown.
std::string TurndownService::turndown(std::string const& html) {
    HtmlStringSource source(html);
    return turndown(source.root());
}

// Converts a gumbo root node to Markdown.
std::string TurndownService::turndown(gumbo::NodeView root) {
    return runPipeline(root);
}

// Converts a DomSource to Markdown.
std::string TurndownService::turndown(DomSource const& dom) {
    return turndown(dom.root());
}

// Escapes markdown-special chars using configured escape function.
std::string TurndownService::escape(std::string const& text) const {
    return options_.escapeFunction ? options_.escapeFunction(text) : text;
}

// Mutable access to options.
TurndownOptions& TurndownService::options() {
    return options_;
}

// Const access to options.
TurndownOptions const& TurndownService::options() const {
    return options_;
}

// Clears cached rules so they will be rebuilt.
void TurndownService::invalidateRules() {
    rules_.reset();
}

// Lazily builds or returns cached rules.
Rules& TurndownService::ensureRules() {
    if (!rules_) {
        rules_ = std::make_unique<Rules>(options_);
        defineCommonMarkRules(*rules_, options_);
        for (auto& factory : preRuleFactories_) {
            factory(*rules_);
        }
        for (auto& factory : postRuleFactories_) {
            factory(*rules_);
        }
        for (auto& mutation : ruleMutations_) {
            mutation(*rules_);
        }
    }
    return *rules_;
}

// Applies a pending rule mutation and caches it for future rebuilds.
void TurndownService::enqueueRuleMutation(std::function<void(Rules&)> fn) {
    if (!fn) return;
    ruleMutations_.push_back(fn);
    if (rules_) {
        fn(*rules_);
    }
}

// Full pipeline: collapse whitespace, process nodes, apply appenders, trim edges.
std::string TurndownService::runPipeline(gumbo::NodeView root) {
    if (!root) return "";

    Rules& rules = ensureRules();
    CollapsedWhitespace collapsed = collapseWhitespace(root, options_.preformattedCode);
    setWhitespaceCollapseContext(collapsed.textReplacements, collapsed.nodesToOmit);

    std::string markdown = processChildren(root, options_, rules);
    clearWhitespaceCollapseContext();

    encodeNbsp(markdown);

    rules.forEach([&](Rule const& rule) {
        if (rule.append) {
            markdown = joinChunks(markdown, rule.append(options_));
        }
    });
    encodeNbsp(markdown);

    markdown = trimLeadingNewlines(markdown);
    // Match Turndown JS: strip trailing whitespace but keep leading spaces (e.g., indented code)
    auto trimTrailingWhitespace = [](std::string_view text) {
        std::size_t end = text.size();
        while (end > 0) {
            char ch = text[end - 1];
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                --end;
            } else {
                break;
            }
        }
        return std::string(text.substr(0, end));
    };
    markdown = trimTrailingWhitespace(markdown);
    return markdown;
}

std::string turndown(std::string const& html, TurndownOptions const& options) {
    TurndownService service(options);
    return service.turndown(html);
}

} // namespace turndown_cpp