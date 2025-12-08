// turndown.cpp/src/node.cpp
#include "node.h"
#include "gumbo_adapter.h"
#include "utf8_helpers.h"
#include "utilities.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>

namespace turndown_cpp {

namespace {

using utf8::CodepointSlice;

// Decodes a single UTF-8 code point starting at `index`. Invalid or truncated
// sequences degrade to the leading byte so callers keep progressing through
// malformed input.
bool decodeCodepoint(std::string const& text, std::size_t index, CodepointSlice& slice) {
    if (index >= text.size()) return false;

    auto set_result = [&](std::uint32_t cp, std::size_t length) {
        slice.codepoint = cp;
        slice.start = index;
        slice.length = length;
        return true;
    };

    unsigned char lead = static_cast<unsigned char>(text[index]);

    // ASCII fast-path.
    if (utf8::isAsciiLead(lead)) {
        return set_result(lead, 1);
    }

    // Determine expected length from the leading byte.
    std::size_t expected_len = utf8::expectedLength(lead);

    // If the prefix is invalid or truncated, treat as a single byte.
    if (expected_len == 1 || index + expected_len > text.size()) {
        return set_result(lead, 1);
    }

    // Build code point while validating continuation bytes.
    std::uint32_t codepoint = lead & (utf8::kLeadPayloadMask >> expected_len); // strip leading prefix bits
    for (std::size_t j = 1; j < expected_len; ++j) {
        unsigned char cont = static_cast<unsigned char>(text[index + j]);
        if (!utf8::isContinuation(cont)) {
            return set_result(lead, 1);
        }
        codepoint = (codepoint << 6) | (cont & utf8::kContinuationPayload);
    }

    // Reject overlong sequences, surrogates, or out-of-range code points.
    if (utf8::isInvalidCodepoint(codepoint, expected_len)) {
        return set_result(lead, 1);
        }

    return set_result(codepoint, expected_len);
    }

// Decodes an entire UTF-8 string into codepoint slices with byte spans.
std::vector<CodepointSlice> toCodepoints(std::string const& text) {
    std::vector<CodepointSlice> codepoints;
    std::size_t index = 0;
    while (index < text.size()) {
        CodepointSlice slice;
        if (!decodeCodepoint(text, index, slice)) {
            break;
        }
        codepoints.push_back(slice);
        index += slice.length;
    }
    return codepoints;
}

struct EdgeWhitespaceParts {
    std::string leading;
    std::string leadingAscii;
    std::string leadingNonAscii;
    std::string trailing;
    std::string trailingAscii;
    std::string trailingNonAscii;
};

// Extracts edge whitespace and keeps both the raw bytes and an ASCII/non-ASCII
// split so downstream spacing logic can reason about NBSPs.
EdgeWhitespaceParts computeEdgeWhitespace(std::string const& text) {
    EdgeWhitespaceParts parts;
    auto codepoints = toCodepoints(text);
    if (codepoints.empty()) {
        return parts;
    }

    // Append/prepend helpers keep byte order correct for each side.
    auto appendLeading = [&](std::string const& bytes, bool isAscii) {
        parts.leading += bytes;
        (isAscii ? parts.leadingAscii : parts.leadingNonAscii) += bytes;
    };
    auto prependTrailing = [&](std::string const& bytes, bool isAscii) {
        parts.trailing = bytes + parts.trailing;
        if (isAscii) {
            parts.trailingAscii = bytes + parts.trailingAscii;
        } else {
            parts.trailingNonAscii = bytes + parts.trailingNonAscii;
        }
    };

    auto is_ws = [](CodepointSlice const& s) {
        return isUnicodeWhitespace(s.codepoint);
    };

    // Locate the span of leading whitespace.
    auto leading_end = std::find_if_not(codepoints.begin(), codepoints.end(), is_ws);

    // Collect leading whitespace bytes from the front.
    std::for_each(codepoints.begin(), leading_end, [&](CodepointSlice const& slice) {
        std::string bytes = text.substr(slice.start, slice.length);
        appendLeading(bytes, isAsciiWhitespaceCodePoint(slice.codepoint));
    });

    // If all codepoints were whitespace, we are done.
    if (leading_end == codepoints.end()) {
        return parts;
    }

    // Locate the span of trailing whitespace.
    auto trailing_start = std::find_if_not(codepoints.rbegin(), codepoints.rend(), is_ws);

    // Collect trailing whitespace bytes from the back.
    std::for_each(codepoints.rbegin(), trailing_start, [&](CodepointSlice const& slice) {
        std::string bytes = text.substr(slice.start, slice.length);
        prependTrailing(bytes, isAsciiWhitespaceCodePoint(slice.codepoint));
    });

    return parts;
}

// True if the string begins with an ASCII space.
bool startsWithAsciiSpace(std::string const& text) {
    return !text.empty() && text.front() == ' ';
}

// True if the string ends with an ASCII space.
bool endsWithAsciiSpace(std::string const& text) {
    return !text.empty() && text.back() == ' ';
}

// Returns the zero-based index of a node within its parent, or -1 if absent.
int childIndex(gumbo::NodeView node) {
    auto parent = node.parent();
    if (!parent.is_element()) return -1;
    auto range = parent.child_range();
    int index = 0;
    for (auto it = range.begin(); it != range.end(); ++it, ++index) {
        if (*it == node) return index;
    }
    return -1;
}

// Returns the adjacent sibling on the requested side, or empty if none.
gumbo::NodeView adjacentSibling(gumbo::NodeView node, FlankSide side) {
    auto parent = node.parent();
    if (!parent.is_element()) return {};
    auto range = parent.child_range();
    gumbo::NodeView previous{};
    bool found = false;
    for (auto child : range) {
        if (found) {
            // We are at the right sibling.
            return child;
        }
        if (child == node) {
            if (side == FlankSide::Left) {
                return previous;
            }
            found = true;
        } else {
            previous = child;
        }
    }
    return {}; // not found or no sibling on requested side
}

// Returns the text content of a sibling node (empty if null).
std::string siblingTextContent(gumbo::NodeView node) {
    if (!node) return "";
    return getNodeText(node);
}

} // namespace

// Reproduces the JS edgeWhitespace handling so NBSP spacing survives correctly.
static std::string encodeNbsp(std::string const& text) {
    std::string result;
    result.reserve(text.size());
    std::string const nbsp = "\xC2\xA0";
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, nbsp.size(), nbsp) == 0) {
            result.append("&nbsp;");
            i += nbsp.size();
        } else {
            result.push_back(text[i]);
            ++i;
        }
    }
    return result;
}

// Computes flanking whitespace chunks for a node, honoring preformatted/code rules.
FlankingWhitespace flankingWhitespace(gumbo::NodeView node, bool preformattedCode) {
    FlankingWhitespace ws{"", ""};
    if (!node) return ws;
    if (isBlock(node) || (preformattedCode && isCodeNode(node))) {
        return ws;
    }

    std::string text = getNodeText(node);
    if (text.empty()) return ws;

    EdgeWhitespaceParts edges = computeEdgeWhitespace(text);
    ws.leading = edges.leading;
    ws.trailing = edges.trailing;

    if (!edges.leadingAscii.empty() && isFlankedByWhitespace(FlankSide::Left, node, preformattedCode)) {
        ws.leading = edges.leadingNonAscii;
    }
    if (!edges.trailingAscii.empty() && isFlankedByWhitespace(FlankSide::Right, node, preformattedCode)) {
        ws.trailing = edges.trailingNonAscii;
    }

    ws.leading = encodeNbsp(ws.leading);
    ws.trailing = encodeNbsp(ws.trailing);
    return ws;
}

// True when a node and its contents are whitespace-only and not meaningful blank.
bool isBlank(gumbo::NodeView node) {
    if (!node) return false;
    if (node.is_element()) {
        if (isVoid(node) || isMeaningfulWhenBlank(node)) return false;
    }

    std::string text = getNodeText(node);
    auto codepoints = toCodepoints(text);
    for (auto const& slice : codepoints) {
        if (!isUnicodeWhitespace(slice.codepoint)) {
            return false;
        }
    }

    if (node.is_element()) {
        if (hasVoid(node) || hasMeaningfulWhenBlank(node)) return false;
    }
    return true;
}

// Checks if a node has ASCII-space text on the requested sibling side.
bool isFlankedByWhitespace(FlankSide side, gumbo::NodeView node, bool preformattedCode) {
    gumbo::NodeView sibling = adjacentSibling(node, side);
    if (!sibling) return false;

    if (sibling.type() == gumbo::NodeType::Element) {
        if (preformattedCode && isCodeNode(sibling)) {
            return false;
        }
        if (isBlock(sibling)) {
            return false;
        }
    } else if (sibling.type() != gumbo::NodeType::Text &&
               sibling.type() != gumbo::NodeType::Whitespace &&
               sibling.type() != gumbo::NodeType::CData) {
        return false;
    }

    std::string text = siblingTextContent(sibling);
    if (text.empty()) return false;
    return (side == FlankSide::Left) ? endsWithAsciiSpace(text) : startsWithAsciiSpace(text);
}

// Builds metadata about a node used by turndown processing.
NodeMetadata analyzeNode(gumbo::NodeView node, bool preformattedCode) {
    NodeMetadata meta;
    if (!node) return meta;
    meta.isBlock = isBlock(node);
    meta.isCode = isCodeNode(node);
    meta.isBlank = isBlank(node);
    meta.isVoid = isVoid(node);
    meta.isMeaningfulWhenBlank = isMeaningfulWhenBlank(node);
    meta.hasMeaningfulWhenBlank = hasMeaningfulWhenBlank(node);
    meta.hasVoidDescendant = hasVoid(node);
    meta.flankingWhitespace = flankingWhitespace(node, preformattedCode);
    return meta;
}

} // namespace turndown_cpp