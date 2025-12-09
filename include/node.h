/// @file node.h
/// @brief Node analysis utilities for HTML to Markdown conversion
///
/// This file provides functions and structures for analyzing DOM nodes
/// during HTML to Markdown conversion. It handles whitespace analysis,
/// blank node detection, and computation of flanking whitespace.
///
/// The node analysis is crucial for proper Markdown generation, especially
/// for handling inline elements where whitespace must be preserved or
/// trimmed appropriately to produce valid Markdown output.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2017 Dom Christie
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef NODE_H
#define NODE_H

#include "dom_adapter.h"

#include <string>

namespace turndown_cpp {

/// @struct FlankingWhitespace
/// @brief Whitespace at the leading and trailing edges of an element
///
/// Flanking whitespace is extracted from element content so it can be
/// placed outside the Markdown delimiters. For example, in " _text_ "
/// the spaces should flank the emphasis markers, not be inside them.
struct FlankingWhitespace {
    std::string leading;   ///< Whitespace before the content
    std::string trailing;  ///< Whitespace after the content
};

/// @struct NodeMetadata
/// @brief Metadata computed for a DOM node during processing
///
/// This structure caches various properties of a node that are used
/// during Markdown conversion. Computing these once per node avoids
/// redundant DOM traversals.
struct NodeMetadata {
    bool isBlock = false;                  ///< True if node is a block-level element
    bool isCode = false;                   ///< True if node is \<code\> or inside one
    bool isBlank = false;                  ///< True if node contains only whitespace
    bool isVoid = false;                   ///< True if node is a void element (br, img, etc.)
    bool isMeaningfulWhenBlank = false;    ///< True if blank content is meaningful (a, td, etc.)
    bool hasMeaningfulWhenBlank = false;   ///< True if has descendant meaningful when blank
    bool hasVoidDescendant = false;        ///< True if contains void element descendants
    FlankingWhitespace flankingWhitespace; ///< Leading/trailing whitespace
};

/// @enum FlankSide
/// @brief Specifies which side to check for flanking whitespace
enum class FlankSide {
    Left,   ///< Check the left (previous) sibling
    Right   ///< Check the right (next) sibling
};

/// @brief Compute the flanking whitespace for a node
///
/// Extracts leading and trailing whitespace from the node's text content,
/// adjusting for adjacent whitespace in sibling nodes to avoid doubling
/// spaces. This ensures proper spacing around Markdown delimiters.
///
/// @param[in] node The DOM node to analyze
/// @param[in] preformattedCode Whether code elements preserve whitespace
/// @return The leading and trailing whitespace
FlankingWhitespace flankingWhitespace(dom::NodeView node, bool preformattedCode);

/// @brief Determine if a node is blank (contains only whitespace)
///
/// A node is considered blank if:
/// - It only contains whitespace characters
/// - It is not a void element (like \<br\> or \<img\>)
/// - It is not an element that is meaningful when blank (like \<a\> or \<td\>)
/// - It does not contain any void elements or meaningful-when-blank elements
///
/// @param[in] node The DOM node to check
/// @retval true if the node is blank
/// @retval false otherwise
bool isBlank(dom::NodeView node);

/// @brief Check if a node is flanked by whitespace on a given side
///
/// Used to determine whether to strip ASCII whitespace from the flanking
/// whitespace of an element. If the adjacent sibling already has whitespace,
/// we don't need to preserve it on this node.
///
/// @param[in] side Which side to check (Left or Right)
/// @param[in] node The DOM node to check around
/// @param[in] preformattedCode Whether code elements are preformatted
/// @retval true if the adjacent content ends/starts with ASCII space
/// @retval false otherwise
bool isFlankedByWhitespace(FlankSide side, dom::NodeView node, bool preformattedCode);

/// @brief Analyze a node and compute all metadata
///
/// Computes various properties of a DOM node that are needed during
/// Markdown conversion. This is called once per element node during
/// processing.
///
/// @param[in] node The DOM node to analyze
/// @param[in] preformattedCode Whether code elements preserve whitespace
/// @return Computed metadata for the node
NodeMetadata analyzeNode(dom::NodeView node, bool preformattedCode);

} // namespace turndown_cpp

#endif // NODE_H
