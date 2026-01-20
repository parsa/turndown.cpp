/// @file commonmark_rules.cpp
/// @brief Implementation of CommonMark conversion rules
///
/// This file defines the standard CommonMark rules that convert HTML
/// elements to their Markdown equivalents. These rules handle:
///
/// - Paragraphs (\<p\>)
/// - Line breaks (\<br\>)
/// - Headings (\<h1\> through \<h6\>)
/// - Blockquotes (\<blockquote\>)
/// - Lists (\<ul\>, \<ol\>, \<li\>)
/// - Code blocks (\<pre\>\<code\>)
/// - Horizontal rules (\<hr\>)
/// - Links (\<a\>)
/// - Emphasis (\<em\>, \<i\>)
/// - Strong (\<strong\>, \<b\>)
/// - Inline code (\<code\>)
/// - Images (\<img\>)
///
/// The rules follow the CommonMark specification and match the behavior
/// of the original JavaScript Turndown library.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2017+ Dom Christie
/// @copyright C++ port copyright (c) 2025 Parsa Amini

#include "commonmark_rules.h"
#include "dom_adapter.h"
#include "rules.h"
#include "turndown.h"
#include "utilities.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace turndown_cpp {

// Helper function to get next sibling element in Gumbo (ignoring text/whitespace)
static dom::NodeView getNextSiblingView(dom::NodeView node) {
    for (auto sibling = node.next_sibling(); sibling; sibling = sibling.next_sibling()) {
        if (sibling.is_element()) {
            return sibling;
        }
    }
    return {};
}

// Helper function to get parent node
static dom::NodeView getParentView(dom::NodeView node) {
    return node.parent();
}

// Helper function to check if node has a following element sibling
static bool hasSiblingsView(dom::NodeView node) {
    auto parent = node.parent();
    if (!parent.is_element()) return false;
    for (auto child : parent.child_range()) {
        if (child == node) continue;
        if (child.is_element()) return true;
    }
    return false;
}

// Helper function to get node index in parent (elements only)
static int getNodeIndexView(dom::NodeView node) {
    auto parent = node.parent();
    if (!parent.is_element()) return -1;
    int elementIndex = -1;
    for (auto child : parent.child_range()) {
        if (!child.is_element()) continue;
        ++elementIndex;
        if (child == node) {
            return elementIndex;
        }
    }
    return -1;
}

static dom::NodeView findChildElementView(dom::NodeView node, std::string_view tag) {
    return node.find_child(tag);
}

static bool isElementWithNameView(dom::NodeView node, std::string_view name) {
    return node.has_tag(name);
}

static bool isLastElementChildView(dom::NodeView parent, dom::NodeView node) {
    if (!parent.is_element()) return false;
    dom::NodeView lastElement;
    for (auto child : parent.child_range()) {
        if (child.is_element()) {
            lastElement = child;
        }
    }
    return lastElement && lastElement == node;
}

static std::string cleanAttribute(std::string_view attribute) {
    if (attribute.empty()) return "";
    return std::regex_replace(std::string(attribute), std::regex(R"((\n+\s*)+)"), "\n");
}

static std::string ltrimNewlines(std::string const& text) {
    auto const first = text.find_first_not_of("\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first);
}

static std::string rtrimNewlines(std::string const& text) {
    auto const last = text.find_last_not_of("\r\n");
    if (last == std::string::npos) return "";
    return text.substr(0, last + 1);
}

static std::string trimNewlines(std::string const& text) {
    return rtrimNewlines(ltrimNewlines(text));
}

static bool hasNextSiblingNodeView(dom::NodeView node) {
    return static_cast<bool>(getNextSiblingView(node));
}

// Pointer wrappers for existing call sites
void defineCommonMarkRules(Rules& rules, TurndownOptions const& options) {
    rules.addRule("paragraph", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "p");
        },
        [](std::string const& content, dom::NodeView, TurndownOptions const&) -> std::string {
            return "\n\n" + content + "\n\n";
        },
        nullptr,
        "paragraph"
    });

    rules.addRule("lineBreak", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "br");
        },
        [&options](std::string const&, dom::NodeView, TurndownOptions const&) -> std::string {
            return options.br + "\n";
        },
        nullptr,
        "lineBreak"
    });

    for (int i = 1; i <= 6; ++i) {
        std::string tagName = "h" + std::to_string(i);
        rules.addRule(tagName, {
            [tagName](dom::NodeView node, TurndownOptions const&) {
                return isElementWithNameView(node, tagName);
            },
            [&options, i](std::string const& content, dom::NodeView, TurndownOptions const&) -> std::string {
                if (options.headingStyle == "setext" && i <= 2) {
                    std::string underline = repeatChar((i == 1 ? '=' : '-'), content.length());
                    return "\n\n" + content + "\n" + underline + "\n\n";
                }
                return "\n\n" + std::string(i, '#') + " " + content + "\n\n";
            },
            nullptr,
            tagName
        });
    }

    rules.addRule("blockquote", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "blockquote");
        },
        [](std::string const& content, dom::NodeView, TurndownOptions const&) -> std::string {
            // Keep this simple and non-regex: MSVC std::regex_replace has been
            // observed to treat `$` as end-of-line and strip internal blank lines.
            std::string trimmed = trimNewlines(content);
            std::istringstream iss(trimmed);
            std::string line;
            std::string block;
            while (std::getline(iss, line)) {
                block += "> " + line + "\n";
            }
            return "\n\n" + block + "\n\n";
        },
        nullptr,
        "blockquote"
    });

    rules.addRule("list", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "ul") || isElementWithNameView(node, "ol");
        },
        [](std::string const& content, dom::NodeView node, TurndownOptions const&) -> std::string {
            std::string inner = trimNewlines(content);
            auto parent = getParentView(node);
            if (isElementWithNameView(parent, "li")) {
                if (isLastElementChildView(parent, node)) {
                    return "\n" + inner;
                }
            }
            return "\n\n" + inner + "\n\n";
        },
        nullptr,
        "list"
    });

    rules.addRule("listItem", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "li");
        },
        [&options](std::string const& content, dom::NodeView node, TurndownOptions const&) -> std::string {
            std::string result = ltrimNewlines(content);
            std::string trimmed = rtrimNewlines(result);
            bool hadTrailingNewlines = trimmed.size() != result.size();
            result = trimmed;
            if (hadTrailingNewlines) {
                result += "\n";
            }
            result = std::regex_replace(result, std::regex(R"(\n)"), "\n    ");

            std::string prefix = options.bulletListMarker + "   ";
            auto parent = getParentView(node);
            if (isElementWithNameView(parent, "ol")) {
                int index = getNodeIndexView(node);
                std::string_view startAttr = parent.attribute("start");
                int start = startAttr.empty() ? 1 : std::stoi(std::string(startAttr));
                prefix = (index >= 0 ? std::to_string(start + index) : "1") + ".  ";
            }
            bool hasNext = hasNextSiblingNodeView(node);
            if (hasNext && result.find('\n') != std::string::npos) {
                // Ensure a blank, indented line between multi-paragraph items and the next list item.
                result = std::regex_replace(result, std::regex(R"(\n\s*$)"), "\n    ");
            }
            bool needsTrailingNewline = hasNext && !std::regex_search(result, std::regex(R"(\n$)"));
            return prefix + result + (needsTrailingNewline ? "\n" : "");
        },
        nullptr,
        "listItem"
    });

    rules.addRule("indentedCodeBlock", {
        [&options](dom::NodeView node, TurndownOptions const&) {
            return options.codeBlockStyle == "indented" &&
                   isElementWithNameView(node, "pre") &&
                   findChildElementView(node, "code");
        },
        [](std::string const&, dom::NodeView node, TurndownOptions const&) -> std::string {
            auto codeNode = findChildElementView(node, "code");
            auto source = codeNode ? codeNode : node;
            std::string code = getNodeText(source);
            if (!code.empty() && code.back() == '\n') {
                code.pop_back();
            }
            code = std::regex_replace(code, std::regex(R"(\n)"), "\n    ");
            return "\n\n    " + code + "\n\n";
        },
        nullptr,
        "indentedCodeBlock"
    });

    rules.addRule("fencedCodeBlock", {
        [&options](dom::NodeView node, TurndownOptions const&) {
            if (options.codeBlockStyle != "fenced") return false;
            if (!isElementWithNameView(node, "pre")) return false;
            return static_cast<bool>(findChildElementView(node, "code"));
        },
        [&options](std::string const&, dom::NodeView node, TurndownOptions const&) -> std::string {
            dom::NodeView codeNode = findChildElementView(node, "code");
            std::string className;
            std::string_view classAttr = codeNode.attribute("class");
            if (!classAttr.empty()) className = std::string(classAttr);
            std::regex langRegex(R"(language-(\S+))");
            std::smatch langMatch;
            std::string language;
            if (std::regex_search(className, langMatch, langRegex) && langMatch.size() > 1) {
                language = langMatch[1].str();
            }

            std::string code = getNodeText(codeNode);
            char fenceChar = options.fence.empty() ? '`' : options.fence.front();
            int fenceSize = 3;
            std::regex fenceInside(std::string("(^|\\n)") + fenceChar + "{3,}");
            for (std::sregex_iterator it(code.begin(), code.end(), fenceInside), end; it != end; ++it) {
                std::string match = it->str();
                std::size_t run = match.size();
                if (!match.empty() && match.front() == '\n') {
                    --run;
                }
                fenceSize = std::max(fenceSize, static_cast<int>(run) + 1);
            }
            std::string fence = repeatChar(fenceChar, fenceSize);
            if (!code.empty() && code.back() == '\n') {
                code.pop_back();
            }
            return "\n\n" + fence + language + "\n" + code + "\n" + fence + "\n\n";
        },
        nullptr,
        "fencedCodeBlock"
    });

    rules.addRule("horizontalRule", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "hr");
        },
        [&options](std::string const&, dom::NodeView, TurndownOptions const&) -> std::string {
            return "\n\n" + options.hr + "\n\n";
        },
        nullptr,
        "horizontalRule"
    });

    rules.addRule("inlineLink", {
        [&options](dom::NodeView node, TurndownOptions const&) {
            return options.linkStyle == "inlined" &&
                   isElementWithNameView(node, "a") &&
                   !node.attribute("href").empty();
        },
        [](std::string const& content, dom::NodeView node, TurndownOptions const&) -> std::string {
            std::string href(node.attribute("href"));
            std::string escapedHref;
            escapedHref.reserve(href.size() * 2);
            for (char ch : href) {
                if (ch == '(' || ch == ')') {
                    escapedHref.push_back('\\');
                }
                escapedHref.push_back(ch);
            }
            std::string title;
            std::string_view titleAttr = node.attribute("title");
            if (!titleAttr.empty()) {
                title = cleanAttribute(titleAttr);
            }
            std::string titlePart = title.empty() ? "" : " \"" + std::regex_replace(title, std::regex(R"(")"), "\\\"") + "\"";
            return "[" + content + "](" + escapedHref + titlePart + ")";
        },
        nullptr,
        "inlineLink"
    });

    auto referenceStore = std::make_shared<std::vector<std::string>>();
    rules.addRule("referenceLink", {
        [&options](dom::NodeView node, TurndownOptions const&) {
            return options.linkStyle == "referenced" &&
                   isElementWithNameView(node, "a") &&
                   !node.attribute("href").empty();
        },
        [referenceStore, &options](std::string const& content, dom::NodeView node, TurndownOptions const&) -> std::string {
            std::string href(node.attribute("href"));
            std::string title;
            std::string_view titleAttr = node.attribute("title");
            if (!titleAttr.empty()) {
                title = cleanAttribute(titleAttr);
            }
            std::string titlePart = title.empty() ? "" : " \"" + title + "\"";
            std::string replacement;
            std::string reference;
            if (options.linkReferenceStyle == "collapsed") {
                replacement = "[" + content + "][]";
                reference = "[" + content + "]: " + href + titlePart;
            } else if (options.linkReferenceStyle == "shortcut") {
                replacement = "[" + content + "]";
                reference = "[" + content + "]: " + href + titlePart;
            } else {
                std::string id = std::to_string(referenceStore->size() + 1);
                replacement = "[" + content + "][" + id + "]";
                reference = "[" + id + "]: " + href + titlePart;
            }
            referenceStore->push_back(reference);
            return replacement;
        },
        [referenceStore](TurndownOptions const&) -> std::string {
            if (referenceStore->empty()) return "";
            std::string output = "\n\n";
            for (auto const& ref : *referenceStore) {
                output += ref + "\n";
            }
            output += "\n\n";
            referenceStore->clear();
            return output;
        },
        "referenceLink"
    });

    rules.addRule("emphasis", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "em") || isElementWithNameView(node, "i");
        },
        [&options](std::string const& content, dom::NodeView, TurndownOptions const&) -> std::string {
            if (trimStr(content).empty()) return "";
            return options.emDelimiter + content + options.emDelimiter;
        },
        nullptr,
        "emphasis"
    });

    rules.addRule("strong", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "strong") || isElementWithNameView(node, "b");
        },
        [&options](std::string const& content, dom::NodeView, TurndownOptions const&) -> std::string {
            if (trimStr(content).empty()) return "";
            return options.strongDelimiter + content + options.strongDelimiter;
        },
        nullptr,
        "strong"
    });

    rules.addRule("code", {
        [](dom::NodeView node, TurndownOptions const&) {
            dom::NodeView parent = getParentView(node);
            bool isCodeBlock = parent && isElementWithNameView(parent, "pre") && !hasSiblingsView(node);
            return isElementWithNameView(node, "code") && !isCodeBlock;
        },
        [](std::string const& content, dom::NodeView, TurndownOptions const&) -> std::string {
            if (content.empty()) return "";
            std::string normalized = std::regex_replace(content, std::regex(R"(\r?\n|\r)"), " ");
            std::regex extraSpaceRegex(R"(^`|^ .*?[^ ].* $|`$)");
            bool needsSpace = std::regex_search(normalized, extraSpaceRegex);

            std::vector<std::string> matches;
            std::regex ticks("`+");
            for (std::sregex_iterator it(normalized.begin(), normalized.end(), ticks), end; it != end; ++it) {
                matches.push_back(it->str());
            }
            auto contains = [&matches](std::string const& needle) {
                return std::find(matches.begin(), matches.end(), needle) != matches.end();
            };
            std::string delimiter = "`";
            while (contains(delimiter)) {
                delimiter.push_back('`');
            }
            std::string pad = needsSpace ? " " : "";
            return delimiter + pad + normalized + pad + delimiter;
        },
        nullptr,
        "code"
    });

    rules.addRule("image", {
        [](dom::NodeView node, TurndownOptions const&) {
            return isElementWithNameView(node, "img");
        },
        [](std::string const&, dom::NodeView node, TurndownOptions const&) -> std::string {
            std::string alt;
            std::string_view altAttr = node.attribute("alt");
            if (!altAttr.empty()) {
                alt = cleanAttribute(altAttr);
            }
            std::string src(node.attribute("src"));
            std::string title;
            std::string_view titleAttr = node.attribute("title");
            if (!titleAttr.empty()) {
                title = cleanAttribute(titleAttr);
            }
            if (src.empty()) return "";
            std::string titlePart = title.empty() ? "" : " \"" + title + "\"";
            return "![" + alt + "](" + src + titlePart + ")";
        },
        nullptr,
        "image"
    });
}

} // namespace turndown_cpp