/// @file gumbo_adapter.h
/// @brief Type-safe Gumbo HTML parser adapter
///
/// This file provides the Gumbo-specific implementation of the DOM concepts.
/// All types use proper Gumbo types (GumboNode*, GumboOutput*, etc.) for
/// full type safety at compile time.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_CPP_GUMBO_ADAPTER_H
#define TURNDOWN_CPP_GUMBO_ADAPTER_H

#include "dom_concepts.h"

#include <gumbo.h>

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace turndown_cpp::gumbo {

// Import types from dom namespace
using dom::NodeType;
using dom::AttributeView;

// Forward declarations
class NodeView;

/// @brief Type-safe handle for node identity (used in hash maps)
struct NodeHandle {
    NodeHandle() = default;
    explicit NodeHandle(GumboNode* node) : node_(node) {}

    void const* raw() const { return node_; }
    GumboNode* get() const { return node_; }
    explicit operator bool() const { return node_ != nullptr; }

    bool operator==(NodeHandle const& other) const { return node_ == other.node_; }
    bool operator!=(NodeHandle const& other) const { return !(*this == other); }

private:
    GumboNode* node_ = nullptr;
};

static_assert(dom::NodeHandleLike<NodeHandle>, "NodeHandle must satisfy NodeHandleLike");

/// @brief Iterator for child nodes
class ChildIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = NodeView;
    using difference_type = std::ptrdiff_t;
    using pointer = NodeView*;
    using reference = NodeView;

    ChildIterator() = default;
    ChildIterator(GumboVector const* vec, unsigned index);

    NodeView operator*() const;
    ChildIterator& operator++();
    ChildIterator operator++(int);
    bool operator==(ChildIterator const& other) const;
    bool operator!=(ChildIterator const& other) const { return !(*this == other); }

private:
    GumboVector const* vec_ = nullptr;
    unsigned index_ = 0;
};

/// @brief Range for iterating over child nodes
class ChildRange {
public:
    using iterator = ChildIterator;

    ChildRange() = default;
    explicit ChildRange(GumboNode* parent);

    ChildIterator begin() const;
    ChildIterator end() const;
    bool empty() const;
    std::size_t size() const;

private:
    GumboVector const* vec_ = nullptr;
};

/// @brief Iterator for attributes
class AttributeIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = dom::AttributeView;
    using difference_type = std::ptrdiff_t;
    using pointer = dom::AttributeView*;
    using reference = dom::AttributeView;

    AttributeIterator() = default;
    AttributeIterator(GumboVector const* vec, unsigned index);

    dom::AttributeView operator*() const;
    AttributeIterator& operator++();
    AttributeIterator operator++(int);
    bool operator==(AttributeIterator const& other) const;
    bool operator!=(AttributeIterator const& other) const { return !(*this == other); }

private:
    GumboVector const* vec_ = nullptr;
    unsigned index_ = 0;
};

/// @brief Range for iterating over element attributes
class AttributeRange {
public:
    using iterator = AttributeIterator;

    AttributeRange() = default;
    explicit AttributeRange(GumboNode* node);

    AttributeIterator begin() const;
    AttributeIterator end() const;
    bool empty() const;
    std::size_t size() const;

private:
    GumboVector const* vec_ = nullptr;
};

/// @brief Type-safe view over a Gumbo DOM node
class NodeView {
public:
    NodeView() = default;
    explicit NodeView(GumboNode* node) : node_(node) {}

    GumboNode* get() const { return node_; }
    explicit operator bool() const { return node_ != nullptr; }
    bool operator==(NodeView const& other) const { return node_ == other.node_; }
    bool operator!=(NodeView const& other) const { return node_ != other.node_; }

    // Type information
    dom::NodeType type() const;
    bool is_document() const { return type() == dom::NodeType::Document; }
    bool is_element() const { return type() == dom::NodeType::Element; }
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
    GumboNode* node_ = nullptr;
};

static_assert(dom::DOMNode<NodeView>, "NodeView must satisfy DOMNode concept");
static_assert(dom::DOMNodeWithHandle<NodeView>, "NodeView must satisfy DOMNodeWithHandle concept");

/// @brief Owns a parsed Gumbo document
class Document {
public:
    Document() = default;
    static Document parse(std::string const& html);

    Document(Document const&) = delete;
    Document& operator=(Document const&) = delete;
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;

    ~Document();

    NodeView root() const;
    NodeView document() const;
    NodeView html() const;
    NodeView body() const;
    explicit operator bool() const { return output_ != nullptr; }
    GumboOutput* get() const { return output_; }

private:
    explicit Document(GumboOutput* output) : output_(output) {}
    GumboOutput* output_ = nullptr;
};

static_assert(dom::DOMDocument<Document, NodeView>, "Document must satisfy DOMDocument concept");

// Utility functions
std::string_view to_string_view(GumboStringPiece const& piece);
std::string lookup_tag_name(GumboNode* node);
bool has_tag(GumboNode* node, std::string_view tag);
inline bool is_element(GumboNode* node) {
    return node && node->type == GUMBO_NODE_ELEMENT;
}

} // namespace turndown_cpp::gumbo

/// @brief Hash specialization for gumbo::NodeHandle
template<>
struct std::hash<turndown_cpp::gumbo::NodeHandle> {
    std::size_t operator()(turndown_cpp::gumbo::NodeHandle const& handle) const noexcept {
        return std::hash<void const*>()(handle.raw());
    }
};

#endif // TURNDOWN_CPP_GUMBO_ADAPTER_H
