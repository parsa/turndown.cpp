/// @file turndown.h
/// @brief C++ port of Turndown - Convert HTML into Markdown
///
/// This is a C++ port of the JavaScript Turndown library
/// (https://github.com/mixmark-io/turndown).
///
/// Turndown converts HTML into Markdown. It provides a flexible and
/// extensible API for customizing the conversion process through
/// options, rules, and plugins.
///
/// @see https://github.com/mixmark-io/turndown for the original JavaScript version
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2017 Dom Christie
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_H
#define TURNDOWN_H

#include "dom_source.h"
#include "dom_adapter.h"
#include "rules.h"
#include "utilities.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace turndown_cpp {

/// @struct TurndownOptions
/// @brief Configuration options for the TurndownService
///
/// Options can be passed to the TurndownService constructor to customize
/// the HTML to Markdown conversion process.
///
/// @code{.cpp}
/// TurndownOptions options;
/// options.headingStyle = "atx";
/// options.codeBlockStyle = "fenced";
/// TurndownService service(options);
/// @endcode
struct TurndownOptions {
    /// @brief Heading style: "setext" (underlined) or "atx" (hash prefixed)
    ///
    /// - "setext": Uses underlines for h1/h2 (=== and ---)
    /// - "atx": Uses hash prefixes (# for h1, ## for h2, etc.)
    ///
    /// @note Setext style only applies to h1 and h2; h3-h6 always use atx style
    std::string headingStyle;

    /// @brief Thematic break (horizontal rule) representation
    ///
    /// Any valid CommonMark thematic break string, e.g.:
    /// - "* * *"
    /// - "---"
    /// - "___"
    std::string hr;

    /// @brief Bullet list marker character
    ///
    /// Valid values: "*", "-", "+"
    std::string bulletListMarker;

    /// @brief Code block style: "indented" or "fenced"
    ///
    /// - "indented": Uses 4-space indentation
    /// - "fenced": Uses fence characters (``` or ~~~)
    std::string codeBlockStyle;

    /// @brief Fence characters for fenced code blocks
    ///
    /// Either "```" or "~~~"
    std::string fence;

    /// @brief Emphasis (italic) delimiter
    ///
    /// Either "_" or "*"
    std::string emDelimiter;

    /// @brief Strong (bold) delimiter
    ///
    /// Either "**" or "__"
    std::string strongDelimiter;

    /// @brief Link style: "inlined" or "referenced"
    ///
    /// - "inlined": [text](url "title")
    /// - "referenced": [text][ref] with references at the end
    std::string linkStyle;

    /// @brief Reference link style when linkStyle is "referenced"
    ///
    /// - "full": [text][1] ... [1]: url
    /// - "collapsed": [text][] ... [text]: url
    /// - "shortcut": [text] ... [text]: url
    std::string linkReferenceStyle;

    /// @brief Line break representation
    ///
    /// The string used to represent \<br\> elements.
    /// Commonly "  " (two spaces) for soft line breaks.
    std::string br;

    /// @brief Whether to treat code elements as preformatted
    ///
    /// When true, whitespace in \<code\> elements is preserved.
    ///
    /// @see https://github.com/lucthev/collapse-whitespace/issues/16
    bool preformattedCode;

    /// @brief Custom escape function for Markdown characters
    ///
    /// A function that takes a string and returns a version with
    /// Markdown-special characters escaped. By default, uses
    /// advancedEscape() which escapes characters that could be
    /// interpreted as Markdown syntax.
    ///
    /// @note Text in code elements is never passed to the escape function
    std::function<std::string(std::string const&)> escapeFunction;

    /// @brief Tags to keep as HTML in the output
    ///
    /// Elements matching these tags will be rendered as HTML
    /// rather than converted to Markdown.
    std::vector<std::string> keepTags;

    /// @brief Replacement function for blank elements
    ///
    /// A node is blank if it only contains whitespace and is not
    /// an \<a\>, \<td\>, \<th\> or a void element.
    ///
    /// @param[in] content The processed content of the node
    /// @param[in] node The DOM node being processed
    /// @return The Markdown replacement string
    std::function<std::string(std::string const&, dom::NodeView)> blankReplacement;

    /// @brief Replacement function for kept elements
    ///
    /// Determines how elements that should be kept as HTML are rendered.
    /// Block-level elements will be separated from surrounding content
    /// by blank lines.
    ///
    /// @param[in] content The processed content of the node
    /// @param[in] node The DOM node being processed
    /// @return The HTML representation of the node
    std::function<std::string(std::string const&, dom::NodeView)> keepReplacement;

    /// @brief Default replacement function for unrecognized elements
    ///
    /// Handles nodes which are not recognized by any other rule.
    /// By default, outputs the node's text content (separated by
    /// blank lines if it is a block-level element).
    ///
    /// @param[in] content The processed content of the node
    /// @param[in] node The DOM node being processed
    /// @return The Markdown replacement string
    std::function<std::string(std::string const&, dom::NodeView)> defaultReplacement;

    /// @brief Constructor that sets default option values
    ///
    /// Initializes options with defaults matching the original
    /// JavaScript Turndown library.
    TurndownOptions();
};

class DomSource;

/// @class TurndownService
/// @brief Main service class for converting HTML to Markdown
///
/// TurndownService is the entry point for converting HTML strings or
/// DOM nodes to Markdown. It can be configured with options and extended
/// with custom rules and plugins.
///
/// @par Basic Usage
/// @code{.cpp}
/// TurndownService service;
/// std::string markdown = service.turndown("<h1>Hello world!</h1>");
/// // Result: "Hello world!\n==========="
/// @endcode
///
/// @par With Options
/// @code{.cpp}
/// TurndownOptions options;
/// options.headingStyle = "atx";
/// TurndownService service(options);
/// std::string markdown = service.turndown("<h1>Hello world!</h1>");
/// // Result: "# Hello world!"
/// @endcode
///
/// @par With Custom Rules
/// @code{.cpp}
/// TurndownService service;
/// service.addRule("strikethrough", {
///     [](dom::NodeView node, TurndownOptions const&) {
///         return node.has_tag("del") || node.has_tag("s");
///     },
///     [](std::string const& content, dom::NodeView, TurndownOptions const&) {
///         return "~" + content + "~";
///     }
/// });
/// @endcode
class TurndownService {
public:
    /// @typedef Plugin
    /// @brief Plugin function type
    ///
    /// A plugin is a function that receives the TurndownService instance
    /// and can modify it by adding rules, changing options, etc.
    using Plugin = std::function<void(TurndownService&)>;

    /// @typedef RuleFactory
    /// @brief Rule factory function type
    ///
    /// A function that receives a Rules object and can add custom rules to it.
    using RuleFactory = std::function<void(Rules&)>;

    /// @typedef KeepFilter
    /// @brief Filter function type for keep/remove operations
    ///
    /// A predicate that determines whether a node should be kept or removed.
    using KeepFilter = std::function<bool(dom::NodeView, TurndownOptions const&)>;

    /// @copydoc KeepFilter
    using RemoveFilter = KeepFilter;

    /// @enum RulePlacement
    /// @brief Specifies when custom rule factories should be applied
    enum class RulePlacement {
        BeforeDefaults,  ///< Apply before CommonMark rules
        AfterDefaults    ///< Apply after CommonMark rules
    };

    /// @brief Construct a TurndownService with default options
    TurndownService();

    /// @brief Construct a TurndownService with custom options
    /// @param[in] options Configuration options for the conversion
    explicit TurndownService(TurndownOptions options);

    /// @brief Configure options using a callback function
    /// @param[in] fn A function that receives and modifies the options
    /// @return Reference to this service for chaining
    TurndownService& configureOptions(std::function<void(TurndownOptions&)> fn);

    /// @brief Add one or more plugins
    ///
    /// Plugins provide a convenient way to apply multiple extensions.
    /// A plugin is just a function that is called with the TurndownService instance.
    ///
    /// @param[in] plugin The plugin function to apply
    /// @return Reference to this service for chaining
    TurndownService& use(Plugin plugin);

    /// @brief Add a conversion rule
    ///
    /// Rules determine how HTML elements are converted to Markdown.
    /// Each rule has a filter (to match elements) and a replacement
    /// function (to generate Markdown).
    ///
    /// @param[in] key A unique identifier for the rule
    /// @param[in] rule The rule definition
    /// @return Reference to this service for chaining
    ///
    /// @code{.cpp}
    /// service.addRule("strikethrough", {
    ///     [](dom::NodeView node, TurndownOptions const&) {
    ///         return node.has_tag("del") || node.has_tag("s");
    ///     },
    ///     [](std::string const& content, dom::NodeView, TurndownOptions const&) {
    ///         return "~" + content + "~";
    ///     }
    /// });
    /// @endcode
    TurndownService& addRule(std::string const& key, Rule rule);

    /// @brief Register a rule factory for custom rule initialization
    /// @param[in] factory The factory function that adds rules
    /// @param[in] placement When to apply the factory (before or after defaults)
    /// @return Reference to this service for chaining
    TurndownService& registerRuleFactory(RuleFactory factory,
                                         RulePlacement placement = RulePlacement::AfterDefaults);

    /// @brief Keep elements matching the filter as HTML
    ///
    /// Elements matching the filter will be rendered as HTML in the
    /// Markdown output rather than being converted.
    ///
    /// @param[in] filter A predicate function to match elements
    /// @return Reference to this service for chaining
    TurndownService& keep(KeepFilter filter);

    /// @brief Keep elements with the specified tag as HTML
    /// @param[in] tag The tag name to keep (case-insensitive)
    /// @return Reference to this service for chaining
    ///
    /// @code{.cpp}
    /// service.keep("del");
    /// service.turndown("<p>Hello <del>world</del></p>");
    /// // Result: "Hello <del>world</del>"
    /// @endcode
    TurndownService& keep(std::string const& tag);

    /// @brief Keep elements with any of the specified tags as HTML
    /// @param[in] tags Vector of tag names to keep
    /// @return Reference to this service for chaining
    TurndownService& keep(std::vector<std::string> const& tags);

    /// @brief Remove elements matching the filter from output
    ///
    /// Elements matching the filter will be completely removed from
    /// the output (converted to an empty string).
    ///
    /// @param[in] filter A predicate function to match elements
    /// @return Reference to this service for chaining
    TurndownService& remove(RemoveFilter filter);

    /// @brief Remove elements with the specified tag from output
    /// @param[in] tag The tag name to remove (case-insensitive)
    /// @return Reference to this service for chaining
    ///
    /// @code{.cpp}
    /// service.remove("del");
    /// service.turndown("<p>Hello <del>world</del></p>");
    /// // Result: "Hello "
    /// @endcode
    TurndownService& remove(std::string const& tag);

    /// @brief Remove elements with any of the specified tags from output
    /// @param[in] tags Vector of tag names to remove
    /// @return Reference to this service for chaining
    TurndownService& remove(std::vector<std::string> const& tags);

    /// @brief Convert an HTML string to Markdown
    ///
    /// This is the main entry point for converting HTML to Markdown.
    ///
    /// @param[in] html The HTML string to convert
    /// @return The Markdown representation of the HTML
    std::string turndown(std::string const& html);

    /// @brief Convert a DOM node to Markdown
    /// @param[in] root The root node to convert
    /// @return The Markdown representation of the DOM tree
    std::string turndown(dom::NodeView root);

    /// @brief Convert a DomSource to Markdown
    /// @param[in] dom The DOM source to convert
    /// @return The Markdown representation
    std::string turndown(DomSource const& dom);

    /// @brief Escape Markdown syntax in a string
    ///
    /// Uses backslashes to escape Markdown characters, ensuring they
    /// are not interpreted as Markdown when the output is compiled
    /// back to HTML.
    ///
    /// @param[in] text The string to escape
    /// @return The escaped string
    ///
    /// @note Text in code elements is never passed to escape
    std::string escape(std::string const& text) const;

    /// @brief Get mutable access to options
    /// @return Reference to the options
    TurndownOptions& options();

    /// @brief Get read-only access to options
    /// @return Const reference to the options
    TurndownOptions const& options() const;

private:
    void invalidateRules();
    Rules& ensureRules();
    std::string runPipeline(dom::NodeView root);
    void enqueueRuleMutation(std::function<void(Rules&)> fn);

    TurndownOptions options_;
    std::unique_ptr<Rules> rules_;
    std::vector<RuleFactory> preRuleFactories_;
    std::vector<RuleFactory> postRuleFactories_;
    std::vector<std::function<void(Rules&)>> ruleMutations_;
};

/// @brief Convenience function to convert HTML to Markdown
///
/// Creates a temporary TurndownService with the given options
/// and converts the HTML string.
///
/// @param[in] html The HTML string to convert
/// @param[in] options Configuration options
/// @return The Markdown representation
std::string turndown(std::string const& html, TurndownOptions const& options);

} // namespace turndown_cpp

#endif // TURNDOWN_H
