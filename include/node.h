// turndown.cpp/include/node.h
#ifndef NODE_H
#define NODE_H

#include "gumbo_adapter.h"

#include <string>

namespace turndown_cpp {

struct FlankingWhitespace {
    std::string leading;
    std::string trailing;
};

struct NodeMetadata {
    bool isBlock = false;
    bool isCode = false;
    bool isBlank = false;
    bool isVoid = false;
    bool isMeaningfulWhenBlank = false;
    bool hasMeaningfulWhenBlank = false;
    bool hasVoidDescendant = false;
    FlankingWhitespace flankingWhitespace;
};

enum class FlankSide {
    Left,
    Right
};

FlankingWhitespace flankingWhitespace(gumbo::NodeView node, bool preformattedCode);
bool isBlank(gumbo::NodeView node);
bool isFlankedByWhitespace(FlankSide side, gumbo::NodeView node, bool preformattedCode);
NodeMetadata analyzeNode(gumbo::NodeView node, bool preformattedCode);

} // namespace turndown_cpp

#endif // NODE_H