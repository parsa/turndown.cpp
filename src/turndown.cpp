/// @file turndown.cpp
/// @brief Main implementation of TurndownService for HTML to Markdown conversion
///
/// This file implements the core TurndownService class and the conversion
/// pipeline that transforms HTML into Markdown. The implementation closely
/// follows the original JavaScript Turndown library.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2017 Dom Christie (original JavaScript)
/// @copyright Copyright (c) 2025 Parsa Amini

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

/**
 * @brief Initialize options with default values
 *
 * Sets default Markdown conversion options that mirror the original
 * JavaScript Turndown defaults. These can be overridden by users.
 */
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

/**
 * @brief Trim leading newlines from a string
 *
 * Removes all leading carriage return and line feed characters.
 * Used in the join algorithm to normalize newlines between chunks.
 */
static std::string trimLeadingNewlines(std::string const& text) {
    std::size_t index = 0;
    while (index < text.size() && (text[index] == '\n' || text[index] == '\r')) {
        ++index;
    }
    return text.substr(index);
}

/**
 * @brief Trim trailing newlines from a string
 *
 * Removes all trailing carriage return and line feed characters.
 * Uses a simple loop instead of regex to avoid performance issues
 * with match-at-end patterns (see original JS issue #370).
 */
static std::string trimTrailingNewlines(std::string const& text) {
    std::size_t index = text.size();
    while (index > 0 && (text[index - 1] == '\n' || text[index - 1] == '\r')) {
        --index;
    }
    return text.substr(0, index);
}

/**
 * @brief Join replacement to output with appropriate newlines
 *
 * Joins two Markdown chunks together with the appropriate number of
 * newline separators. The separator is determined by the maximum number
 * of newlines trimmed from either side, capped at 2 (blank line).
 *
 * This ensures proper spacing between block elements while avoiding
 * excessive whitespace.
 *
 * @param[in] output The current conversion output
 * @param[in] addition The string to append to the output
 * @return Joined output with proper newline separation
 */
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

// Forward declarations for the recursive conversion functions
static std::string processNode(gumbo::NodeView node, TurndownOptions const& options, Rules& rules);
static std::string processChildren(gumbo::NodeView parent, TurndownOptions const& options, Rules& rules);
static std::string replacementForNode(gumbo::NodeView node, TurndownOptions const& options, Rules& rules, NodeMetadata const& meta);

/**
 * @brief Encode non-breaking spaces as HTML entities
 *
 * Replaces UTF-8 NBSP bytes (0xC2 0xA0) with "&nbsp;" entities.
 * This preserves non-breaking spaces in the Markdown output.
 */
static void encodeNbsp(std::string& text) {
    std::string const nbsp = "\xC2\xA0";
    std::size_t pos = 0;
    while ((pos = text.find(nbsp, pos)) != std::string::npos) {
        text.replace(pos, nbsp.size(), "&nbsp;");
        pos += 6;
    }
}

/**
 * @brief Convert a text node to Markdown
 *
 * Processes a text node, applying Markdown escaping unless the node
 * is inside a code element (where text should be preserved verbatim).
 *
 * @param[in] node The text node to process
 * @param[in] options Conversion options (for escape function)
 * @param[in] meta Node metadata (for isCode check)
 * @return The processed text, escaped if necessary
 */
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

/**
 * @brief Reduce a DOM node to its Markdown string equivalent
 *
 * Dispatches processing based on node type:
 * - Text/Whitespace/CData: Process as text (with escaping)
 * - Element: Apply matching rule for conversion
 * - Document: Process all children
 *
 * @param[in] node The node to convert
 * @param[in] options Conversion options
 * @param[in,out] rules Rule set for element conversion
 * @return Markdown representation of the node
 */
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

/**
 * @brief Process all children of a node
 *
 * Iterates through all child nodes, converting each to Markdown
 * and joining the results with appropriate spacing.
 *
 * @param[in] parent The parent node whose children to process
 * @param[in] options Conversion options
 * @param[in,out] rules Rule set for element conversion
 * @return Combined Markdown of all children
 */
static std::string processChildren(gumbo::NodeView parent, TurndownOptions const& options, Rules& rules) {
    if (!parent) return "";

    std::string output;
    for (auto child : parent.child_range()) {
        std::string addition = processNode(child, options, rules);
        output = joinChunks(output, addition);
    }
    return output;
}

/**
 * @brief Convert an element node to its Markdown equivalent
 *
 * Applies the matching rule's replacement function to convert an element
 * to Markdown. Handles flanking whitespace by trimming content and
 * placing whitespace outside the converted output.
 *
 * @param[in] node The element node to convert
 * @param[in] options Conversion options
 * @param[in,out] rules Rule set for finding matching rule
 * @param[in] meta Pre-computed metadata including flanking whitespace
 * @return Markdown representation with proper whitespace
 */
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

/**
 * @brief Construct a TurndownService with default options
 */
TurndownService::TurndownService()
    : options_() {}

/**
 * @brief Construct a TurndownService with custom options
 */
TurndownService::TurndownService(TurndownOptions options)
    : options_(std::move(options)) {}

/**
 * @brief Configure options using a callback
 *
 * Allows modifying options after construction. Invalidates cached
 * rules so they will be rebuilt with new options.
 */
TurndownService& TurndownService::configureOptions(std::function<void(TurndownOptions&)> fn) {
    if (fn) {
        fn(options_);
        invalidateRules();
    }
    return *this;
}

/**
 * @brief Add one or more plugins
 *
 * Plugins provide a convenient way for developers to apply multiple
 * extensions. A plugin is just a function that is called with the
 * TurndownService instance.
 */
TurndownService& TurndownService::use(Plugin plugin) {
    if (plugin) {
        plugin(*this);
    }
    return *this;
}

/**
 * @brief Add a rule
 *
 * Adds a rule to the front of the rules array. The key parameter is
 * a unique name for the rule for easy reference.
 *
 * Returns the TurndownService instance for chaining.
 */
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

/**
 * @brief Keep a node (as HTML) that matches the filter
 *
 * Determines which elements are to be kept and rendered as HTML.
 * By default, Turndown does not keep any elements.
 *
 * Returns the TurndownService instance for chaining.
 */
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

/**
 * @brief Remove a node that matches the filter
 *
 * Determines which elements are to be removed altogether, i.e.,
 * converted to an empty string. By default, Turndown does not
 * remove any elements.
 *
 * Returns the TurndownService instance for chaining.
 */
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

/// The entry point for converting a string to Markdown.
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

/// Escape Markdown syntax.
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

/**
 * @brief Run the full conversion pipeline
 *
 * Executes the complete HTML to Markdown conversion:
 * -# Collapse whitespace in the DOM tree
 * -# Process all nodes recursively
 * -# Apply rule append functions (e.g., for reference links)
 * -# Encode NBSPs and trim edges
 *
 * @param[in] root The root node to convert
 * @return The final Markdown output
 */
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