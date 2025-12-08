/// @file utilities.h
/// @brief Utility functions for HTML to Markdown conversion
///
/// This file provides various utility functions used throughout the
/// Turndown conversion process, including:
///
/// - Element classification (block, void, meaningful-when-blank)
/// - Text extraction from DOM nodes
/// - String manipulation (trimming, escaping)
/// - Whitespace handling
///
/// These utilities correspond to the utilities.js module in the original
/// JavaScript Turndown implementation.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2017 Dom Christie
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef UTILITIES_H
#define UTILITIES_H

#include "gumbo_adapter.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>


namespace turndown_cpp {

struct TurndownOptions; // Forward declaration

/// @defgroup whitespace_context Whitespace Collapse Context
/// @brief Thread-local context for whitespace-collapsed text lookup
///
/// These functions manage a context that allows text helpers to read
/// pre-collapsed whitespace while converting nodes. This avoids passing
/// the collapse results through every function call.
/// @{

/// @brief Set the whitespace collapse context for text extraction
///
/// When set, text extraction functions will use the collapsed text
/// instead of the original node content.
///
/// @param[in] collapsedText Map of node handles to their collapsed text
/// @param[in] nodesToOmit Set of node handles that should be skipped
void setWhitespaceCollapseContext(
    std::unordered_map<gumbo::NodeHandle, std::string> const& collapsedText,
    std::unordered_set<gumbo::NodeHandle> const& nodesToOmit);

/// @brief Clear the whitespace collapse context
///
/// Should be called after processing is complete to release references.
void clearWhitespaceCollapseContext();

/// @} // end of whitespace_context

/// @defgroup char_classification Character Classification
/// @{

/// @brief Check if a character is ASCII whitespace
///
/// ASCII whitespace includes: space, tab, carriage return, newline
///
/// @param[in] c The character to check
/// @retval true if the character is ASCII whitespace
/// @retval false otherwise
bool isAsciiWhitespace(char c);

/// @brief Check if a code point is ASCII whitespace
///
/// ASCII whitespace code points: U+0009 (tab), U+000A (LF), U+000B (VT),
/// U+000C (FF), U+000D (CR), U+0020 (space)
///
/// @param[in] codepoint The Unicode code point to check
/// @retval true if the code point is ASCII whitespace
/// @retval false otherwise
bool isAsciiWhitespaceCodePoint(std::uint32_t codepoint);

/// @brief Check if a code point is any Unicode whitespace
///
/// Includes ASCII whitespace plus Unicode whitespace characters like
/// NO-BREAK SPACE (U+00A0), EN SPACE (U+2002), etc.
///
/// @param[in] codepoint The Unicode code point to check
/// @retval true if the code point is Unicode whitespace
/// @retval false otherwise
bool isUnicodeWhitespace(std::uint32_t codepoint);

/// @} // end of char_classification

/// @defgroup element_classification Element Classification
/// @brief Functions to classify HTML elements
///
/// These functions classify HTML elements according to their display
/// characteristics and semantic meaning. The classifications match
/// those used in the original Turndown JavaScript implementation.
/// @{

/// @brief Check if a node is a block-level element
///
/// Block elements include: ADDRESS, ARTICLE, ASIDE, AUDIO, BLOCKQUOTE,
/// BODY, CANVAS, CENTER, DD, DIR, DIV, DL, DT, FIELDSET, FIGCAPTION,
/// FIGURE, FOOTER, FORM, FRAMESET, H1-H6, HEADER, HGROUP, HR, HTML,
/// ISINDEX, LI, MAIN, MENU, NAV, NOFRAMES, NOSCRIPT, OL, OUTPUT, P,
/// PRE, SECTION, TABLE, TBODY, TD, TFOOT, TH, THEAD, TR, UL
///
/// @param[in] node The DOM node to check
/// @retval true if the node is a block-level element
/// @retval false otherwise
bool isBlock(gumbo::NodeView node);

/// @brief Check if a node is a void element
///
/// Void elements are self-closing and cannot have children:
/// AREA, BASE, BR, COL, COMMAND, EMBED, HR, IMG, INPUT, KEYGEN,
/// LINK, META, PARAM, SOURCE, TRACK, WBR
///
/// @param[in] node The DOM node to check
/// @retval true if the node is a void element
/// @retval false otherwise
bool isVoid(gumbo::NodeView node);

/// @brief Check if a node is a \<pre\> element
/// @param[in] node The DOM node to check
/// @retval true if the node is a \<pre\> element
/// @retval false otherwise
bool isPre(gumbo::NodeView node);

/// @brief Check if a node is a \<code\> element or inside one
///
/// Recursively checks the node and its ancestors for \<code\> elements.
///
/// @param[in] node The DOM node to check
/// @retval true if the node is \<code\> or has a \<code\> ancestor
/// @retval false otherwise
bool isCodeNode(gumbo::NodeView node);

/// @brief Check if an element is meaningful even when blank
///
/// Some elements have semantic meaning even with no content:
/// A (anchor), TABLE, THEAD, TBODY, TFOOT, TH, TD, IFRAME, SCRIPT,
/// AUDIO, VIDEO
///
/// @param[in] node The DOM node to check
/// @retval true if the element is meaningful when blank
/// @retval false otherwise
bool isMeaningfulWhenBlank(gumbo::NodeView node);

/// @brief Check if any descendant is meaningful when blank
/// @param[in] node The DOM node to check
/// @retval true if any descendant is meaningful when blank
/// @retval false otherwise
bool hasMeaningfulWhenBlank(gumbo::NodeView node);

/// @brief Check if any descendant is a void element
/// @param[in] node The DOM node to check
/// @retval true if any descendant is a void element
/// @retval false otherwise
bool hasVoid(gumbo::NodeView node);

/// @} // end of element_classification

/// @defgroup text_extraction Text Extraction
/// @{

/// @brief Extract text content from a DOM node
///
/// Recursively extracts all text content from a node and its descendants.
/// Uses the whitespace collapse context if one is set.
///
/// @param[in] node The DOM node to extract text from
/// @return Concatenated text content
std::string getNodeText(gumbo::NodeView node);

/// @} // end of text_extraction

/// @defgroup string_utilities String Utilities
/// @{

/// @brief Trim Unicode whitespace from both ends of a string
///
/// Unlike std::isspace, this handles all Unicode whitespace characters.
///
/// @param[in] s The string to trim
/// @return Trimmed string
std::string trimStr(std::string const& s);

/// @brief Repeat a character a specified number of times
///
/// @param[in] c The character to repeat
/// @param[in] count Number of times to repeat (must be >= 0)
/// @return String containing the repeated character
///
/// @pre count >= 0
std::string repeatChar(char c, int count);

/// @} // end of string_utilities

/// @defgroup markdown_escaping Markdown Escaping
/// @brief Functions for escaping Markdown syntax
///
/// Turndown uses backslashes to escape Markdown characters in the HTML input.
/// This ensures that these characters are not interpreted as Markdown when
/// the output is compiled back to HTML.
///
/// For example, the contents of `<h1>1. Hello world</h1>` needs to be escaped
/// to `1\. Hello world`, otherwise it will be interpreted as a list item
/// rather than a heading.
///
/// To avoid the complexity and performance implications of parsing the content
/// of every HTML element as Markdown, Turndown uses a group of regular
/// expressions to escape potential Markdown syntax. As a result, the escaping
/// rules can be quite aggressive.
/// @{

/// @brief Advanced escape function for Markdown syntax
///
/// Escapes characters that could be interpreted as Markdown:
/// - `\\` → `\\\\` (backslash)
/// - `*` → `\\*` (asterisk - emphasis/strong)
/// - `^-` → `\\-` (leading dash - unordered list)
/// - `^+` → `\\+` (leading plus - unordered list)
/// - `^=` → `\\=` (leading equals - setext heading)
/// - `^#` → `\\#` (leading hash - atx heading)
/// - `` ` `` → `` \\` `` (backtick - code)
/// - `^~~~` → `\\~~~` (leading tildes - fenced code)
/// - `[` → `\\[` (opening bracket - link)
/// - `]` → `\\]` (closing bracket - link)
/// - `^>` → `\\>` (leading gt - blockquote)
/// - `_` → `\\_` (underscore - emphasis/strong)
/// - `^N.` → `N\\.` (numbered list prefix)
///
/// @param[in] input The string to escape
/// @return Escaped string safe for Markdown output
std::string advancedEscape(std::string const& input);

/// @brief Minimal escape function for Markdown syntax
///
/// Only escapes backslash and square brackets. Useful when most
/// Markdown syntax should pass through unescaped.
///
/// @param[in] input The string to escape
/// @return Minimally escaped string
std::string minimalEscape(std::string const& input);

/// @brief Escape Markdown using the options' configured escape function
/// @param[in] text The string to escape
/// @param[in] options Options containing the escape function
/// @return Escaped string
std::string escapeMarkdown(std::string const& text, TurndownOptions const& options);

/// @} // end of markdown_escaping

/// @defgroup html_serialization HTML Serialization
/// @{

/// @brief Serialize a DOM node back to HTML
///
/// Converts a gumbo node tree back to an HTML string. Used for
/// "keep" rules that preserve elements as HTML in the Markdown output.
///
/// @param[in] node The DOM node to serialize
/// @return HTML string representation
std::string serializeNode(gumbo::NodeView node);

/// @} // end of html_serialization

} // namespace turndown_cpp

#endif // UTILITIES_H
