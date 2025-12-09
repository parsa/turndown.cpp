/// @file collapse_whitespace.cpp
/// @brief Whitespace collapsing implementation
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
/// @copyright C++ port copyright (c) 2025 Parsa Amini

#include "collapse_whitespace.h"
#include "dom_adapter.h"
#include "utilities.h"

#include <cassert>
#include <regex>
#include <string>

namespace turndown_cpp {

namespace {

/**
 * @brief Check if node is a preformatted element
 *
 * Returns true for \<pre\> elements, and optionally for \<code\> elements
 * when treatCodeAsPre is true. Preformatted elements preserve their
 * whitespace and should not have whitespace collapsed.
 *
 * @param[in] node The node to check
 * @param[in] treatCodeAsPre Whether to treat \<code\> as preformatted
 * @retval true if the node is preformatted
 * @retval false otherwise
 */
bool isPreNode(dom::NodeView node, bool treatCodeAsPre) {
    if (!node || !node.is_element()) return false;
    std::string tag = node.tag_name();
    if (tag == "pre") return true;
    return treatCodeAsPre && tag == "code";
}

/**
 * @brief Get the next node in document order
 *
 * Returns the next node in the sequence, given the current and previous nodes.
 * Skips into children unless we just came from a child or the current node
 * is preformatted (in which case we skip its contents).
 *
 * @param[in] prev The previously visited node
 * @param[in] current The current node
 * @param[in] treatCodeAsPre Whether to treat \<code\> as preformatted
 * @return The next node in document order
 */
dom::NodeView nextNode(dom::NodeView prev, dom::NodeView current, bool treatCodeAsPre) {
    if (!current) return {};
    bool prevIsParent = prev && prev.parent() == current;
    if (prevIsParent || isPreNode(current, treatCodeAsPre)) {
        if (auto sibling = current.next_sibling()) {
            return sibling;
        }
        return current.parent();
    }
    if (auto child = current.first_child()) {
        return child;
    }
    if (auto sibling = current.next_sibling()) {
        return sibling;
    }
    return current.parent();
}

/**
 * @brief Get the node that would follow if this node were removed
 *
 * Used to continue traversal after removing a node from processing.
 *
 * @param[in] node The node being removed
 * @return The next node in the sequence
 */
dom::NodeView afterRemoval(dom::NodeView node) {
    if (!node) return {};
    if (auto sibling = node.next_sibling()) {
        return sibling;
    }
    return node.parent();
}

} // namespace

/// Collapse whitespace in a DOM tree.
CollapsedWhitespace collapseWhitespace(dom::NodeView element, bool treatCodeAsPre) {
    CollapsedWhitespace result;
    if (!element || isPreNode(element, treatCodeAsPre) || !element.first_child()) {
        return result;
    }

    dom::NodeView prevTextNode;
    bool keepLeadingWhitespace = false;

    dom::NodeView prevNode;
    dom::NodeView currentNode = nextNode(prevNode, element, treatCodeAsPre);

    while (currentNode && currentNode != element) {
        if (currentNode.is_text_like()) {
            std::string text = currentNode.text_content();
            text = std::regex_replace(text, std::regex("[ \\r\\n\\t]+"), " ");

            bool prevEndedWithSpace = false;
            if (prevTextNode) {
                auto handle = prevTextNode.handle();
                auto it = result.textReplacements.find(handle);
                if (it != result.textReplacements.end() && !it->second.empty() && it->second.back() == ' ') {
                    prevEndedWithSpace = true;
                }
            }

            if ((!prevTextNode || prevEndedWithSpace) && !keepLeadingWhitespace && !text.empty() && text.front() == ' ') {
                text.erase(text.begin());
            }

            if (text.empty()) {
                result.nodesToOmit.insert(currentNode.handle());
                currentNode = afterRemoval(currentNode);
                continue;
            }

            result.textReplacements[currentNode.handle()] = text;
            prevTextNode = currentNode;
        } else if (currentNode.is_element()) {
            std::string tag = currentNode.tag_name();
            bool blockLike = isBlock(currentNode);
            bool isBr = tag == "br";
            bool preNode = isPreNode(currentNode, treatCodeAsPre);
            bool voidNode = isVoid(currentNode);

            if (blockLike || isBr) {
                if (prevTextNode) {
                    auto handle = prevTextNode.handle();
                    auto it = result.textReplacements.find(handle);
                    if (it != result.textReplacements.end() && !it->second.empty() && it->second.back() == ' ') {
                        it->second.pop_back();
                        if (it->second.empty()) {
                            result.nodesToOmit.insert(handle);
                        }
                    }
                }
                prevTextNode = {};
                keepLeadingWhitespace = false;
            } else if (voidNode || preNode) {
                prevTextNode = {};
                keepLeadingWhitespace = true;
            } else if (prevTextNode) {
                keepLeadingWhitespace = false;
            }
        } else {
            result.nodesToOmit.insert(currentNode.handle());
            currentNode = afterRemoval(currentNode);
            continue;
        }

        dom::NodeView next = nextNode(prevNode, currentNode, treatCodeAsPre);
        prevNode = currentNode;
        currentNode = next;
    }

    if (prevTextNode) {
        auto handle = prevTextNode.handle();
        auto it = result.textReplacements.find(handle);
        if (it != result.textReplacements.end() && !it->second.empty() && it->second.back() == ' ') {
            it->second.pop_back();
            if (it->second.empty()) {
                result.nodesToOmit.insert(handle);
            }
        }
    }

    return result;
}

} // namespace turndown_cpp