#include "dom_source.h"
#include "gumbo_adapter.h"

#include <string>
#include <utility>

namespace turndown_cpp {

HtmlStringSource::HtmlStringSource(std::string html)
    : html_(std::move(html)) {}

void HtmlStringSource::parseIfNeeded() const {
    if (!document_) {
        document_ = gumbo::Document::parse(html_);
    }
}

gumbo::NodeView HtmlStringSource::root() const {
    parseIfNeeded();
    return document_.root();
}

} // namespace turndown_cpp

