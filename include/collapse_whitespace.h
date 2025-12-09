/// @file collapse_whitespace.h
/// @brief Whitespace collapsing for HTML to Markdown conversion
///
/// The collapseWhitespace function is adapted from collapse-whitespace
/// by Luc Thevenard.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2014 Luc Thevenard <lucthevenard@gmail.com>
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// ---
///
/// @copyright Copyright (c) 2025 Parsa Amini
///
/// This module removes extraneous whitespace from the DOM tree, simulating
/// how browsers collapse whitespace in HTML rendering. In HTML, consecutive
/// whitespace characters are typically collapsed to a single space, and
/// leading/trailing whitespace in certain contexts is removed entirely.
///
/// The algorithm traverses the DOM tree and:
/// - Collapses runs of whitespace (spaces, tabs, newlines) to single spaces
/// - Removes leading whitespace when preceded by whitespace
/// - Removes trailing whitespace before block elements or at document end
/// - Preserves whitespace in \<pre\> elements and optionally \<code\> elements
///
/// This is essential for producing clean Markdown output that matches what
/// a browser would render from the original HTML.

#ifndef COLLAPSE_WHITESPACE_H
#define COLLAPSE_WHITESPACE_H

#include "dom_adapter.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace turndown_cpp {

/// @struct CollapsedWhitespace
/// @brief Result of whitespace collapsing operation
///
/// Rather than modifying the DOM tree directly (which is immutable in gumbo),
/// this structure tracks the changes that should be applied during text
/// extraction.
struct CollapsedWhitespace {
    /// @brief Map from text node handles to their collapsed text content
    ///
    /// Text nodes in this map should use the mapped string value instead
    /// of their original content.
    std::unordered_map<dom::NodeHandle, std::string> textReplacements;

    /// @brief Set of node handles that should be omitted entirely
    ///
    /// These nodes (typically empty after whitespace collapsing) should
    /// be skipped during text extraction.
    std::unordered_set<dom::NodeHandle> nodesToOmit;
};

/// @brief Collapse whitespace in a DOM tree
///
/// Simulates browser whitespace collapsing behavior by traversing the
/// DOM tree and computing text replacements. The results can be used
/// during text extraction to produce properly collapsed output.
///
/// @param[in] element The root element to process
/// @param[in] treatCodeAsPre When true, treat \<code\> elements like \<pre\>
///                           (preserve their whitespace)
/// @return Structure containing text replacements and nodes to skip
///
/// @par Algorithm Details
///
/// The function performs a depth-first traversal of the DOM tree:
///
/// -# For text nodes (TEXT_NODE or CDATA_SECTION_NODE):
///    - Replace runs of [ \\r\\n\\t]+ with single spaces
///    - Strip leading space if previous text ended with space
///    - Remove empty text nodes entirely
///
/// -# For element nodes:
///    - Block elements and \<br\> reset the whitespace state
///    - Void elements and \<pre\> nodes preserve adjacent spaces
///    - Trailing spaces before blocks are trimmed
///
/// -# At the end:
///    - Trailing whitespace from the last text node is trimmed
CollapsedWhitespace collapseWhitespace(dom::NodeView element, bool treatCodeAsPre);

} // namespace turndown_cpp

#endif // COLLAPSE_WHITESPACE_H
