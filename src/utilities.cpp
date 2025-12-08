// turndown.cpp/src/utilities.cpp
#include "gumbo_adapter.h"
#include "turndown.h"
#include "utf8_helpers.h"
#include "utilities.h"

#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace turndown_cpp {

namespace {

std::unordered_map<gumbo::NodeHandle, std::string> const kEmptyCollapsedText{};
std::unordered_set<gumbo::NodeHandle> const kEmptyCollapsedSkip{};

struct CollapseContext {
    std::unordered_map<gumbo::NodeHandle, std::string> const* text = &kEmptyCollapsedText;
    std::unordered_set<gumbo::NodeHandle> const* skip = &kEmptyCollapsedSkip;
    bool engaged = false;
};

CollapseContext gCollapse{};

} // namespace

// Sets the optional whitespace collapse context for gumbo nodes.
void setWhitespaceCollapseContext(
    std::unordered_map<gumbo::NodeHandle, std::string> const& collapsedText,
    std::unordered_set<gumbo::NodeHandle> const& nodesToOmit) {
    gCollapse.text = &collapsedText;
    gCollapse.skip = &nodesToOmit;
    gCollapse.engaged = true;
}

// Clears the whitespace collapse context.
void clearWhitespaceCollapseContext() {
    gCollapse.text = &kEmptyCollapsedText;
    gCollapse.skip = &kEmptyCollapsedSkip;
    gCollapse.engaged = false;
}

namespace {

using utf8::Utf8Char;

// Decodes `text` into Unicode code points while remembering where each
// character started in the original byte stream. Invalid sequences degrade to
// a single-byte code point so callers don't skip over potentially malformed
// input.
// Decodes `text` into Unicode code points while remembering where each
// character started in the original byte stream. Invalid sequences degrade to
// a single-byte code point so callers don't skip over potentially malformed
// input.
std::vector<Utf8Char> decodeUtf8(std::string const& text) {
    std::vector<Utf8Char> result;
    result.reserve(text.size());

    auto decode_one = [&](std::size_t i) -> Utf8Char {
        unsigned char lead = static_cast<unsigned char>(text[i]);

        // ASCII fast-path.
        if (utf8::isAsciiLead(lead)) {
            return Utf8Char{lead, i, 1};
        }

        // Determine expected length from the leading byte.
        std::size_t expected_len = utf8::expectedLength(lead);

        // If the prefix is invalid or truncated, treat as a single byte.
        if (expected_len == 1 || i + expected_len > text.size()) {
            return Utf8Char{lead, i, 1};
        }

        // Build code point while validating continuation bytes.
        std::uint32_t codepoint = lead & (utf8::kLeadPayloadMask >> expected_len); // strip leading prefix bits
        for (std::size_t j = 1; j < expected_len; ++j) {
            unsigned char cont = static_cast<unsigned char>(text[i + j]);
            if (!utf8::isContinuation(cont)) {
                return Utf8Char{lead, i, 1};
            }
            codepoint = (codepoint << 6) | (cont & utf8::kContinuationPayload);
        }

        // Reject overlong sequences, surrogates, or out-of-range code points.
        if (utf8::isInvalidCodepoint(codepoint, expected_len)) {
            return Utf8Char{lead, i, 1};
        }

        return Utf8Char{codepoint, i, expected_len};
    };

    for (std::size_t i = 0; i < text.size();) {
        Utf8Char decoded = decode_one(i);
        result.push_back(decoded);
        i += decoded.length;
    }

    return result;
}

using TagList = std::span<std::string_view const>;

// True if the node's tag matches any in the provided list.
bool nodeHasTag(gumbo::NodeView node, TagList tags) {
    if (!node.is_element()) return false;
    std::string tag = node.tag_name();
    return std::any_of(tags.begin(), tags.end(),
        [&](std::string_view candidate) { return tag == candidate; });
        }

// True if any descendant matches a tag in the provided list.
bool hasDescendantWithTag(gumbo::NodeView node, TagList tags) {
    if (!node.is_element()) return false;
    return std::any_of(node.child_range().begin(), node.child_range().end(),
        [&](gumbo::NodeView child) {
            return nodeHasTag(child, tags) || hasDescendantWithTag(child, tags);
        });
        }

// Collects text content for a node, honoring collapse context omissions.
std::string collectText(gumbo::NodeView node) {
    if (!node) return "";
    gumbo::NodeHandle handle = node.handle();
    if (gCollapse.engaged && gCollapse.skip->count(handle)) {
        return "";
    }
    switch (node.type()) {
        case gumbo::NodeType::Text:
        case gumbo::NodeType::Whitespace:
        case gumbo::NodeType::CData: {
            if (gCollapse.engaged) {
                auto it = gCollapse.text->find(handle);
                if (it != gCollapse.text->end()) {
                    return it->second;
                }
            }
            std::string text(node.text());
            return text;
        }
        case gumbo::NodeType::Element:
        case gumbo::NodeType::Document: {
            std::string text;
            for (auto child : node.child_range()) {
                text += collectText(child);
            }
            return text;
        }
        default:
            return "";
    }
}

} // namespace

namespace {

// Escapes HTML text or attribute values.
std::string escapeHtml(std::string const& text, bool attribute) {
    std::string output;
    output.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"':
                output += attribute ? "&quot;" : "\"";
                break;
            case '\'':
                output += attribute ? "&#39;" : "'";
                break;
            default:
                output.push_back(c);
                break;
        }
    }
    return output;
}

// Serializes a gumbo node back to HTML.
void serializeNodeRecursive(gumbo::NodeView node, std::string& output) {
    if (!node) return;

    switch (node.type()) {
        case gumbo::NodeType::Text:
        case gumbo::NodeType::Whitespace:
        case gumbo::NodeType::CData: {
            std::string text(node.text());
            output += escapeHtml(text, false);
            break;
        }
        case gumbo::NodeType::Comment: {
            std::string comment(node.text());
            output += "<!--" + comment + "-->";
            break;
        }
        case gumbo::NodeType::Document: {
            for (auto child : node.child_range()) {
                serializeNodeRecursive(child, output);
            }
            break;
        }
        case gumbo::NodeType::Element: {
            std::string tag = node.tag_name();
            output += "<";
            output += tag;

            for (auto attr : node.attribute_range()) {
                output += " ";
                output += attr.name;
                output += "=\"";
                output += escapeHtml(std::string(attr.value), true);
                output += "\"";
            }

            bool voidElement = isVoid(node);
            output += ">";

            if (!voidElement) {
                for (auto child : node.child_range()) {
                    serializeNodeRecursive(child, output);
                }
                output += "</";
                output += tag;
                output += ">";
            }
            break;
        }
        default:
            break;
    }
}

} // namespace

// True if code point is ASCII whitespace (U+0009..0D or U+0020).
bool isAsciiWhitespaceCodePoint(std::uint32_t cp) {
    return cp == 0x20 || (cp >= 0x09 && cp <= 0x0D);
}

// True if code point is any Unicode whitespace.
bool isUnicodeWhitespace(std::uint32_t cp) {
    if (isAsciiWhitespaceCodePoint(cp)) return true;
    switch (cp) {
        case 0x0085:  // NEXT LINE
        case 0x00A0:  // NO-BREAK SPACE
        case 0x1680:  // OGHAM SPACE MARK
        case 0x180E:  // MONGOLIAN VOWEL SEPARATOR
        case 0x2000:  // EN QUAD
        case 0x2001:  // EM QUAD
        case 0x2002:  // EN SPACE
        case 0x2003:  // EM SPACE
        case 0x2004:  // THREE-PER-EM SPACE
        case 0x2005:  // FOUR-PER-EM SPACE
        case 0x2006:  // SIX-PER-EM SPACE
        case 0x2007:  // FIGURE SPACE
        case 0x2008:  // PUNCTUATION SPACE
        case 0x2009:  // THIN SPACE
        case 0x200A:  // HAIR SPACE
        case 0x2028:  // LINE SEPARATOR
        case 0x2029:  // PARAGRAPH SEPARATOR
        case 0x202F:  // NARROW NO-BREAK SPACE
        case 0x205F:  // MEDIUM MATHEMATICAL SPACE
        case 0x3000:  // IDEOGRAPHIC SPACE
            return true;
        default:
            return false;
    }
}

// True if char is ASCII whitespace.
bool isAsciiWhitespace(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

// True if node is a block-level element.
bool isBlock(gumbo::NodeView node) {
    static constexpr auto kBlockTags = std::to_array<std::string_view>({
        "address", "article", "aside", "audio", "blockquote", "body", "canvas",
        "center", "dd", "dir", "div", "dl", "dt", "fieldset", "figcaption",
        "figure", "footer", "form", "frameset", "h1", "h2", "h3", "h4", "h5",
        "h6", "header", "hgroup", "hr", "html", "isindex", "li", "main", "menu",
        "nav", "noframes", "noscript", "ol", "output", "p", "pre", "section",
        "table", "tbody", "td", "tfoot", "th", "thead", "tr", "ul"
    });
    return nodeHasTag(node, kBlockTags);
}

// True if node is a void element.
bool isVoid(gumbo::NodeView node) {
    static constexpr auto kVoidTags = std::to_array<std::string_view>({
        "area", "base", "br", "col", "command", "embed", "hr",
        "img", "input", "keygen", "link", "meta", "param",
        "source", "track", "wbr"
    });
    return nodeHasTag(node, kVoidTags);
}

// True if node has <pre> tag.
bool isPre(gumbo::NodeView node) {
    return node.has_tag("pre");
}

// True if node is a <code> element or has a <code> ancestor.
bool isCodeNode(gumbo::NodeView node) {
    if (!node) return false;
    if (node.type() == gumbo::NodeType::Element && node.tag_name() == "code") {
        return true;
    }
    auto parent = node.parent();
    return parent ? isCodeNode(parent) : false;
}

// True if the element is meaningful even when blank.
bool isMeaningfulWhenBlank(gumbo::NodeView node) {
    static constexpr auto kMeaningfulTags = std::to_array<std::string_view>({
        "a", "table", "thead", "tbody", "tfoot", "th", "td",
        "iframe", "script", "audio", "video"
    });
    return nodeHasTag(node, kMeaningfulTags);
}

// True if any descendant is meaningful when blank.
bool hasMeaningfulWhenBlank(gumbo::NodeView node) {
    static constexpr auto kMeaningfulTags = std::to_array<std::string_view>({
        "a", "table", "thead", "tbody", "tfoot", "th", "td",
        "iframe", "script", "audio", "video"
    });
    return hasDescendantWithTag(node, kMeaningfulTags);
}

// True if any descendant is a void element.
bool hasVoid(gumbo::NodeView node) {
    static constexpr auto kVoidTags = std::to_array<std::string_view>({
        "area", "base", "br", "col", "command", "embed", "hr",
        "img", "input", "keygen", "link", "meta", "param",
        "source", "track", "wbr"
    });
    return hasDescendantWithTag(node, kVoidTags);
}

// Returns concatenated text content from a gumbo node.
std::string getNodeText(gumbo::NodeView node) {
    return collectText(node);
}

// Trims Unicode whitespace from both ends of a UTF-8 string.
std::string trimStr(std::string const& s) {
    if (s.empty()) return "";
    auto chars = decodeUtf8(s);
    if (chars.empty()) return "";

    auto not_ws_front = std::find_if(chars.begin(), chars.end(), [](Utf8Char const& ch) {
        return !isUnicodeWhitespace(ch.codepoint);
    });
    if (not_ws_front == chars.end()) {
        return "";
    }

    auto not_ws_back = std::find_if(chars.rbegin(), chars.rend(), [](Utf8Char const& ch) {
        return !isUnicodeWhitespace(ch.codepoint);
    });

    std::size_t startByte = not_ws_front->start;
    std::size_t endByte = not_ws_back->start + not_ws_back->length;
    return s.substr(startByte, endByte - startByte);
}


// Escapes Markdown-special chars according to Turndown JS advanced rules.
std::string advancedEscape(std::string const& input) {
    auto escape_char = [](std::string& text, char needle, std::string const& replacement) {
        std::string result;
        result.reserve(text.size() + 8);
        for (char c : text) {
            if (c == needle) {
                result += replacement;
            } else {
                result.push_back(c);
            }
        }
        text.swap(result);
    };

    std::string output = input;

    escape_char(output, '\\', "\\\\"); // backslash
    escape_char(output, '*', "\\*");   // asterisk

    if (!output.empty() && output.front() == '-') { // leading dash
        output.insert(0, "\\");
    }

    if (output.size() >= 2 && output[0] == '+' && output[1] == ' ') { // leading plus+space
        output.insert(0, "\\");
    }

    if (!output.empty() && output.front() == '=') { // leading equals
        std::size_t count = 0;
        while (count < output.size() && output[count] == '=') ++count;
        output.insert(0, "\\");
    }

    if (!output.empty() && output.front() == '#') { // heading prefix
        std::size_t count = 0;
        while (count < output.size() && output[count] == '#') ++count;
        if (count >= 1 && count <= 6 && output.size() > count && output[count] == ' ') {
            output.insert(0, "\\");
        }
    }

    escape_char(output, '`', "\\`"); // backtick

    if (output.rfind("~~~", 0) == 0) { // fenced code ~~~ at start
        output.insert(0, "\\");
    }

    escape_char(output, '[', "\\[");
    escape_char(output, ']', "\\]");

    if (!output.empty() && output.front() == '>') { // blockquote
        output.insert(0, "\\");
    }

    escape_char(output, '_', "\\_");

    // ordered list prefix: digits + ". "
    if (!output.empty() && std::isdigit(static_cast<unsigned char>(output[0]))) {
        std::size_t idx = 0;
        while (idx < output.size() && std::isdigit(static_cast<unsigned char>(output[idx]))) {
            ++idx;
        }
        if (idx + 1 < output.size() && output[idx] == '.' && output[idx + 1] == ' ') {
            output.insert(idx, "\\");
        }
    }

    return output;
}


// Escapes only backslash and brackets.
std::string minimalEscape(std::string const& input) {
    std::string output;
    for (char c : input) {
        if (c == '\\' || c == '[' || c == ']') {
            output.push_back('\\');
        }
        output.push_back(c);
    }
    return output;
}

// Escapes Markdown using configured options.
std::string escapeMarkdown(std::string const& text, TurndownOptions const& options) {
    return options.escapeFunction(text);
}

// Repeats a character count times.
std::string repeatChar(char c, int count) {
    assert(count >= 0);
    return std::string(count, c);
}

// Serializes a gumbo node to HTML string.
std::string serializeNode(gumbo::NodeView node) {
    std::string html;
    serializeNodeRecursive(node, html);
    return html;
}


} // namespace turndown_cpp