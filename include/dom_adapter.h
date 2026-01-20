/// @file dom_adapter.h
/// @brief Backend-agnostic DOM facade for turndown_cpp
///
/// This header defines the public DOM types used by the turndown_cpp API
/// (`dom::NodeView`, `dom::Document`, etc.) without pulling in the headers of
/// the selected parser backend (Gumbo, Tidy, or Lexbor).
///
/// The concrete backend is selected when building the turndown_cpp library.
/// Consumers of the installed library should not need backend headers to
/// compile, as long as they only use the turndown_cpp public API.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_CPP_DOM_ADAPTER_H
#define TURNDOWN_CPP_DOM_ADAPTER_H

#include "dom_concepts.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace turndown_cpp::dom {

class Document;
class ChildRange;
class AttributeRange;

/// @brief Opaque handle for node identity (hash-map key)
///
/// This is backend-agnostic and does not require the backend's headers.
struct NodeHandle {
    NodeHandle() = default;
    explicit NodeHandle(void const* raw) : raw_(raw) {}

    void const* raw() const { return raw_; }
    explicit operator bool() const { return raw_ != nullptr; }

    bool operator==(NodeHandle const& other) const { return raw_ == other.raw_; }
    bool operator!=(NodeHandle const& other) const { return !(*this == other); }

private:
    void const* raw_ = nullptr;
};

static_assert(NodeHandleLike<NodeHandle>, "NodeHandle must satisfy NodeHandleLike");

/// @brief Backend-agnostic view over a DOM node
///
/// This type intentionally does not expose any backend headers or types.
/// All member functions are implemented in the turndown_cpp library.
class NodeView {
public:
    NodeView() = default;

    explicit operator bool() const { return node_ != nullptr; }
    bool operator==(NodeView const& other) const { return node_ == other.node_; }
    bool operator!=(NodeView const& other) const { return !(*this == other); }

    // Type information
    NodeType type() const;
    bool is_document() const { return type() == NodeType::Document; }
    bool is_element() const { return type() == NodeType::Element; }
    bool is_text_like() const;

    // Navigation
    NodeView parent() const;
    NodeView next_sibling() const;
    NodeView first_child() const;
    std::vector<NodeView> children() const;
    ChildRange child_range() const;

    // Element operations
    std::string tag_name() const;
    bool has_tag(std::string_view tag) const;
    NodeView find_child(std::string_view tag) const;
    NodeView first_text_child() const;
    NodeHandle handle() const { return NodeHandle(node_); }

    // Attributes
    std::string_view attribute(std::string_view name) const;
    bool has_attribute(std::string_view name) const;
    AttributeRange attribute_range() const;

    // Text content
    std::string text_content() const;
    void set_text(std::string const& text);
    std::string_view text() const;

private:
    friend class Document;
    explicit NodeView(void* node) : node_(node) {}

    void* node_ = nullptr;
};

/// @brief Range for iterating over child nodes (backend-agnostic)
class ChildRange {
public:
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = NodeView;
        using difference_type = std::ptrdiff_t;
        using pointer = NodeView*;
        using reference = NodeView;

        iterator() = default;
        explicit iterator(NodeView current) : current_(current) {}

        NodeView operator*() const { return current_; }
        iterator& operator++() {
            current_ = current_.next_sibling();
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(iterator const& other) const { return current_ == other.current_; }
        bool operator!=(iterator const& other) const { return !(*this == other); }

    private:
        NodeView current_{};
    };

    ChildRange() = default;
    explicit ChildRange(NodeView parent) : first_(parent.first_child()) {}

    iterator begin() const { return iterator(first_); }
    iterator end() const { return iterator(NodeView{}); }
    bool empty() const { return !first_; }
    std::size_t size() const {
        std::size_t count = 0;
        for (auto it = begin(); it != end(); ++it) {
            ++count;
        }
        return count;
    }

private:
    NodeView first_{};
};

/// @brief Range for iterating over attributes (backend-agnostic)
///
/// This range is materialized as a vector of `AttributeView` values.
class AttributeRange {
public:
    using iterator = std::vector<AttributeView>::const_iterator;

    AttributeRange() = default;
    explicit AttributeRange(std::vector<AttributeView> attrs) : attrs_(std::move(attrs)) {}

    iterator begin() const { return attrs_.begin(); }
    iterator end() const { return attrs_.end(); }
    bool empty() const { return attrs_.empty(); }
    std::size_t size() const { return attrs_.size(); }

private:
    std::vector<AttributeView> attrs_;
};

static_assert(DOMNode<NodeView>, "NodeView must satisfy DOMNode concept");
static_assert(DOMNodeWithHandle<NodeView>, "NodeView must satisfy DOMNodeWithHandle concept");

/// @brief Owns a parsed document (backend-agnostic)
///
/// The document implementation is provided by the compiled turndown_cpp library.
class Document {
public:
    Document();
    static Document parse(std::string const& html);

    Document(Document const&) = delete;
    Document& operator=(Document const&) = delete;
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;

    ~Document();

    explicit operator bool() const;

    NodeView root() const;
    NodeView document() const;
    NodeView html() const;
    NodeView body() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

static_assert(DOMDocument<Document, NodeView>, "Document must satisfy DOMDocument concept");

} // namespace turndown_cpp::dom

#endif // TURNDOWN_CPP_DOM_ADAPTER_H

