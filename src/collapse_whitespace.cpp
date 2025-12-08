// turndown.cpp/src/collapse_whitespace.cpp
#include "collapse_whitespace.h"
#include "gumbo_adapter.h"
#include "utilities.h"

#include <cassert>
#include <regex>
#include <string>

namespace turndown_cpp {

namespace {

// True if node is <pre> or (optionally) <code>.
bool isPreNode(gumbo::NodeView node, bool treatCodeAsPre) {
    if (!node || !node.is_element()) return false;
    std::string tag = node.tag_name();
    if (tag == "pre") return true;
    return treatCodeAsPre && tag == "code";
}

// Traversal helper to walk nodes in document order while honoring pre/code.
gumbo::NodeView nextNode(gumbo::NodeView prev, gumbo::NodeView current, bool treatCodeAsPre) {
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

// Returns the node that would follow the given one if it were removed.
gumbo::NodeView afterRemoval(gumbo::NodeView node) {
    if (!node) return {};
    if (auto sibling = node.next_sibling()) {
        return sibling;
    }
    return node.parent();
}

} // namespace

// Collapses whitespace in the DOM tree, tracking replacements and omissions.
CollapsedWhitespace collapseWhitespace(gumbo::NodeView element, bool treatCodeAsPre) {
    CollapsedWhitespace result;
    if (!element || isPreNode(element, treatCodeAsPre) || !element.first_child()) {
        return result;
    }

    gumbo::NodeView prevTextNode;
    bool keepLeadingWhitespace = false;

    gumbo::NodeView prevNode;
    gumbo::NodeView currentNode = nextNode(prevNode, element, treatCodeAsPre);

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

        gumbo::NodeView next = nextNode(prevNode, currentNode, treatCodeAsPre);
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