/// @file rules.h
/// @brief Rule management for HTML to Markdown conversion
///
/// This file defines the Rule structure and Rules class that manage
/// how HTML elements are converted to Markdown. Rules can be extended
/// and customized to support additional HTML elements or modify the
/// default conversion behavior.
///
/// @par Extending with Rules
///
/// Turndown can be extended by adding rules. A rule is an object with
/// `filter` and `replacement` properties:
///
/// - **filter**: Determines whether an element should be handled by this rule.
///   Can be a tag name, array of tag names, or a function.
///
/// - **replacement**: A function that returns the Markdown string for an element.
///   It receives the element's content, the element itself, and the options.
///
/// @par Rule Precedence
///
/// Turndown iterates over rules and picks the first one that matches:
/// -# Blank rule (whitespace-only elements)
/// -# Added rules (via addRule)
/// -# CommonMark rules
/// -# Keep rules
/// -# Remove rules
/// -# Default rule
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2017 Dom Christie
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef RULES_H
#define RULES_H

#include "gumbo_adapter.h"

#include <functional>
#include <string>
#include <vector>

namespace turndown_cpp {

struct TurndownOptions;

/// @struct Rule
/// @brief A conversion rule for HTML to Markdown
///
/// Rules determine how specific HTML elements are converted to Markdown.
/// Each rule has a filter to match elements and a replacement function
/// to generate the Markdown output.
///
/// @code{.cpp}
/// Rule emphasisRule{
///     // Filter: matches <em> or <i> elements
///     [](gumbo::NodeView node, TurndownOptions const&) {
///         return node.has_tag("em") || node.has_tag("i");
///     },
///     // Replacement: wraps content in delimiters
///     [](std::string const& content, gumbo::NodeView, TurndownOptions const& opts) {
///         return opts.emDelimiter + content + opts.emDelimiter;
///     },
///     nullptr,  // No append function
///     "emphasis"
/// };
/// @endcode
struct Rule {
    /// @brief Filter function to determine if this rule applies to a node
    ///
    /// The filter can match based on tag name, attributes, parent elements,
    /// or any other criteria based on the node and current options.
    ///
    /// @param[in] node The DOM node to test
    /// @param[in] options Current conversion options
    /// @retval true if this rule should handle the node
    /// @retval false otherwise
    std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter;

    /// @brief Replacement function to convert the element to Markdown
    ///
    /// Generates the Markdown representation of an element. The function
    /// receives the already-processed content of child elements.
    ///
    /// @param[in] content The processed Markdown content of child elements
    /// @param[in] node The DOM node being converted
    /// @param[in] options Current conversion options
    /// @return The Markdown string for this element
    std::function<std::string(std::string const&, gumbo::NodeView, TurndownOptions const&)> replacement;

    /// @brief Optional append function called after all processing
    ///
    /// Used by rules that need to add content at the end of the document,
    /// such as reference-style links that collect references.
    ///
    /// @param[in] options Current conversion options
    /// @return Content to append to the end of the document
    std::function<std::string(TurndownOptions const&)> append;

    /// @brief Unique identifier for this rule
    ///
    /// Used for debugging and rule management. Should be descriptive
    /// of what the rule handles (e.g., "paragraph", "emphasis", "code").
    std::string key;
};

/// @class Rules
/// @brief Manages a collection of rules used to convert HTML to Markdown
///
/// The Rules class maintains ordered collections of conversion rules
/// and provides methods to add, find, and iterate over them. It handles
/// the rule precedence logic and special rules (blank, keep, remove, default).
///
/// @par Rule Resolution Order
/// -# If node is blank (whitespace-only, not meaningful) → blank rule
/// -# First matching rule in rulesArray (added rules + CommonMark rules)
/// -# First matching keep rule
/// -# First matching remove rule
/// -# Default rule
class Rules {
public:
    /// @brief Construct a Rules object with the given options
    ///
    /// Initializes the built-in rules (blank, keep replacement, default)
    /// based on the provided options.
    ///
    /// @param[in] options The conversion options
    explicit Rules(TurndownOptions const& options);

    /// @brief Destructor
    ~Rules();

    /// @brief Add a rule to the front of the rules array
    ///
    /// Added rules take precedence over CommonMark rules but are
    /// checked after the blank rule.
    ///
    /// @param[in] key Unique identifier for the rule
    /// @param[in] rule The rule to add
    void addRule(std::string const& key, Rule rule);

    /// @brief Add a keep rule for a single tag
    ///
    /// Elements matching this tag will be rendered as HTML.
    ///
    /// @param[in] filter The tag name to keep
    void keep(std::string const& filter);

    /// @brief Add a keep rule for multiple tags
    /// @param[in] filters Vector of tag names to keep
    void keep(std::vector<std::string> const& filters);

    /// @brief Add a keep rule with a custom filter function
    /// @param[in] filter Predicate to determine which elements to keep
    void keep(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter);

    /// @brief Add a remove rule for a single tag
    ///
    /// Elements matching this tag will be removed (converted to empty string).
    ///
    /// @param[in] filter The tag name to remove
    void remove(std::string const& filter);

    /// @brief Add a remove rule for multiple tags
    /// @param[in] filters Vector of tag names to remove
    void remove(std::vector<std::string> const& filters);

    /// @brief Add a remove rule with a custom filter function
    /// @param[in] filter Predicate to determine which elements to remove
    void remove(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter);

    /// @brief Find the appropriate rule for a node
    ///
    /// Searches through rules in precedence order and returns the
    /// first matching rule. Falls back to the default rule if no
    /// other rule matches.
    ///
    /// @param[in] node The DOM node to find a rule for
    /// @return Reference to the matching rule
    Rule const& forNode(gumbo::NodeView node) const;

    /// @brief Iterate over all rules in the rules array
    ///
    /// Used primarily for calling append functions after processing.
    ///
    /// @param[in] fn Callback function to call for each rule
    void forEach(std::function<void(Rule const&)> fn) const;

private:
    /// @brief Create a filter function that matches tag names
    /// @param[in] filters Vector of tag names to match (case-insensitive)
    /// @return Filter function that matches any of the tags
    std::function<bool(gumbo::NodeView, TurndownOptions const&)> makeTagFilter(std::vector<std::string> const& filters) const;

    /// @brief Add a keep rule with a generated key
    /// @param[in] filter The filter function
    /// @param[in] keySuffix Suffix for the generated key
    void addKeepRule(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter, std::string const& keySuffix);

    /// @brief Add a remove rule with a generated key
    /// @param[in] filter The filter function
    /// @param[in] keySuffix Suffix for the generated key
    void addRemoveRule(std::function<bool(gumbo::NodeView, TurndownOptions const&)> filter, std::string const& keySuffix);

    /// @brief Search a rule vector for the first matching rule
    /// @param[in] candidates Vector of rules to search
    /// @param[in] node The node to match against
    /// @return Pointer to matching rule, or nullptr if none found
    Rule const* findRule(std::vector<Rule> const& candidates, gumbo::NodeView node) const;

    TurndownOptions const& options;     ///< Reference to conversion options
    std::vector<Rule> rulesArray;       ///< Main rules (added + CommonMark)
    std::vector<Rule> keepRules;        ///< Rules for keeping elements as HTML
    std::vector<Rule> removeRules;      ///< Rules for removing elements

    Rule blankRule;                     ///< Handles whitespace-only elements
    Rule keepReplacementRule;           ///< Replacement for kept elements
    Rule defaultRule;                   ///< Fallback for unrecognized elements
};

} // namespace turndown_cpp

#endif // RULES_H
