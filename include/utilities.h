// turndown.cpp/include/utilities.h
#ifndef UTILITIES_H
#define UTILITIES_H

#include "gumbo_adapter.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>


namespace turndown_cpp {

struct TurndownOptions; // Forward declaration

// Allows text helpers to read pre-collapsed whitespace while converting.
void setWhitespaceCollapseContext(
    std::unordered_map<gumbo::NodeHandle, std::string> const& collapsedText,
    std::unordered_set<gumbo::NodeHandle> const& nodesToOmit);
void clearWhitespaceCollapseContext();

bool isAsciiWhitespace(char c);
bool isBlock(gumbo::NodeView node);
bool isVoid(gumbo::NodeView node);
bool isPre(gumbo::NodeView node);
bool isCodeNode(gumbo::NodeView node);
bool isAsciiWhitespaceCodePoint(std::uint32_t codepoint);
bool isUnicodeWhitespace(std::uint32_t codepoint);
bool isMeaningfulWhenBlank(gumbo::NodeView node);
bool hasMeaningfulWhenBlank(gumbo::NodeView node);
bool hasVoid(gumbo::NodeView node);
std::string getNodeText(gumbo::NodeView node);
std::string trimStr(std::string const& s);
std::string advancedEscape(std::string const& input);
std::string minimalEscape(std::string const& input);
std::string escapeMarkdown(std::string const& text, TurndownOptions const& options);
std::string repeatChar(char c, int count);
std::string serializeNode(gumbo::NodeView node);

} // namespace turndown_cpp

#endif // UTILITIES_H