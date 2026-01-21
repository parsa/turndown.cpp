/// @file dom_adapter.cpp
/// @brief Backend-agnostic DOM facade implementation (selected at library build time)
///
/// This translation unit implements `turndown_cpp::dom::NodeView` and
/// `turndown_cpp::dom::Document` without exposing backend headers to consumers.
///
/// The concrete backend is selected when building the library via
/// TURNDOWN_PARSER_BACKEND_* compile definitions.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#include "dom_adapter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(TURNDOWN_PARSER_BACKEND_LEXBOR)
    #include "lexbor_adapter.h"
    namespace turndown_cpp::dom::detail {
        namespace backend = turndown_cpp::lexbor;
    }
#elif defined(TURNDOWN_PARSER_BACKEND_TIDY)
    #include "tidy_adapter.h"
    namespace turndown_cpp::dom::detail {
        namespace backend = turndown_cpp::tidy;
    }
#elif defined(TURNDOWN_PARSER_BACKEND_LIBXML2)
    #include "libxml2_adapter.h"
    namespace turndown_cpp::dom::detail {
        namespace backend = turndown_cpp::libxml2;
    }
#else
    #include "gumbo_adapter.h"
    namespace turndown_cpp::dom::detail {
        namespace backend = turndown_cpp::gumbo;
    }
#endif

namespace turndown_cpp::dom {

namespace {

using BackendNodePtr = decltype(std::declval<detail::backend::NodeView const&>().get());

template<typename PtrT>
void* to_void_ptr(PtrT p) {
    return const_cast<void*>(static_cast<void const*>(p));
}

detail::backend::NodeView as_backend(void* raw) {
    return detail::backend::NodeView(static_cast<BackendNodePtr>(raw));
}

} // namespace

// --- NodeView ---

NodeType NodeView::type() const {
    return node_ ? as_backend(node_).type() : NodeType::Unknown;
}

bool NodeView::is_text_like() const {
    return node_ ? as_backend(node_).is_text_like() : false;
}

NodeView NodeView::parent() const {
    if (!node_) return {};
    auto p = as_backend(node_).parent();
    return NodeView(to_void_ptr(p.get()));
}

NodeView NodeView::next_sibling() const {
    if (!node_) return {};
    auto sib = as_backend(node_).next_sibling();
    return NodeView(to_void_ptr(sib.get()));
}

NodeView NodeView::first_child() const {
    if (!node_) return {};
    auto child = as_backend(node_).first_child();
    return NodeView(to_void_ptr(child.get()));
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> out;
    for (auto child : child_range()) {
        out.push_back(child);
    }
    return out;
}

ChildRange NodeView::child_range() const {
    return ChildRange(*this);
}

std::string NodeView::tag_name() const {
    return node_ ? as_backend(node_).tag_name() : std::string{};
}

bool NodeView::has_tag(std::string_view tag) const {
    return node_ ? as_backend(node_).has_tag(tag) : false;
}

NodeView NodeView::find_child(std::string_view tag) const {
    if (!node_) return {};
    auto found = as_backend(node_).find_child(tag);
    return NodeView(to_void_ptr(found.get()));
}

NodeView NodeView::first_text_child() const {
    if (!node_) return {};
    auto found = as_backend(node_).first_text_child();
    return NodeView(to_void_ptr(found.get()));
}

std::string_view NodeView::attribute(std::string_view name) const {
    return node_ ? as_backend(node_).attribute(name) : std::string_view{};
}

bool NodeView::has_attribute(std::string_view name) const {
    return node_ ? as_backend(node_).has_attribute(name) : false;
}

AttributeRange NodeView::attribute_range() const {
    if (!node_) return AttributeRange{};

    std::vector<AttributeView> attrs;
    for (auto attr : as_backend(node_).attribute_range()) {
        attrs.push_back(attr);
    }
    return AttributeRange(std::move(attrs));
}

std::string NodeView::text_content() const {
    return node_ ? as_backend(node_).text_content() : std::string{};
}

void NodeView::set_text(std::string const& text) {
    if (!node_) return;
    auto backend_node = as_backend(node_);
    backend_node.set_text(text);
}

std::string_view NodeView::text() const {
    return node_ ? as_backend(node_).text() : std::string_view{};
}

// --- Document ---

struct Document::Impl {
    detail::backend::Document doc;
};

Document::Document() = default;
Document::~Document() = default;

Document::Document(Document&& other) noexcept = default;
Document& Document::operator=(Document&& other) noexcept = default;

Document Document::parse(std::string const& html) {
    Document doc;
    doc.impl_ = std::make_unique<Impl>();
    doc.impl_->doc = detail::backend::Document::parse(html);
    return doc;
}

Document::operator bool() const {
    return impl_ && static_cast<bool>(impl_->doc);
}

NodeView Document::root() const {
    if (!*this) return {};
    auto n = impl_->doc.root();
    return NodeView(to_void_ptr(n.get()));
}

NodeView Document::document() const {
    if (!*this) return {};
    auto n = impl_->doc.document();
    return NodeView(to_void_ptr(n.get()));
}

NodeView Document::html() const {
    if (!*this) return {};
    auto n = impl_->doc.html();
    return NodeView(to_void_ptr(n.get()));
}

NodeView Document::body() const {
    if (!*this) return {};
    auto n = impl_->doc.body();
    return NodeView(to_void_ptr(n.get()));
}

} // namespace turndown_cpp::dom

