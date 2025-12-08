// turndown.cpp/include/collapse_whitespace.h
#ifndef COLLAPSE_WHITESPACE_H
#define COLLAPSE_WHITESPACE_H

#include "gumbo_adapter.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace turndown_cpp {

struct CollapsedWhitespace {
    std::unordered_map<gumbo::NodeHandle, std::string> textReplacements;
    std::unordered_set<gumbo::NodeHandle> nodesToOmit;
};

CollapsedWhitespace collapseWhitespace(gumbo::NodeView element, bool treatCodeAsPre);

} // namespace turndown_cpp

#endif // COLLAPSE_WHITESPACE_H