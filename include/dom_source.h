#ifndef TURNDOWN_CPP_DOM_SOURCE_H
#define TURNDOWN_CPP_DOM_SOURCE_H

#include "dom_adapter.h"

#include <string>

namespace turndown_cpp {

class DomSource {
public:
    virtual ~DomSource() = default;
    virtual dom::NodeView root() const = 0;
};

class GumboNodeSource final : public DomSource {
public:
    explicit GumboNodeSource(dom::NodeView node) : node_(node) {}
    dom::NodeView root() const override { return node_; }
private:
    dom::NodeView node_;
};

class HtmlStringSource final : public DomSource {
public:
    explicit HtmlStringSource(std::string html);
    HtmlStringSource(HtmlStringSource&& other) noexcept = default;
    HtmlStringSource& operator=(HtmlStringSource&& other) noexcept = default;

    HtmlStringSource(HtmlStringSource const&) = delete;
    HtmlStringSource& operator=(HtmlStringSource const&) = delete;

    ~HtmlStringSource() override = default;

    dom::NodeView root() const override;
    std::string const& html() const { return html_; }

private:
    void parseIfNeeded() const;

    std::string html_;
    mutable dom::Document document_;
};

} // namespace turndown_cpp

#endif // TURNDOWN_CPP_DOM_SOURCE_H

