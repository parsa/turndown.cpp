/// @file libxml2_adapter.h
/// @brief Type-safe libxml2 HTML parser adapter
///
/// This file provides the libxml2-specific implementation of the DOM concepts.
/// It uses libxml2's HTML parser (htmlReadMemory) and exposes a lightweight
/// NodeView over libxml2 nodes.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_CPP_LIBXML2_ADAPTER_H
#define TURNDOWN_CPP_LIBXML2_ADAPTER_H

#include "dom_concepts.h"

#include <libxml/tree.h>

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace turndown_cpp::libxml2 {

// Import types from dom namespace
using dom::NodeType;
using dom::AttributeView;

// Forward declarations
class NodeView;

/// @brief Type-safe handle for node identity (used in hash maps)
struct NodeHandle {
    NodeHandle() = default;
    explicit NodeHandle(xmlNodePtr node) : node_(node) {}

    void const* raw() const { return node_; }
    xmlNodePtr get() const { return node_; }
    explicit operator bool() const { return node_ != nullptr; }

    bool operator==(NodeHandle const& other) const { return node_ == other.node_; }
    bool operator!=(NodeHandle const& other) const { return !(*this == other); }

private:
    xmlNodePtr node_ = nullptr;
};

static_assert(dom::NodeHandleLike<NodeHandle>, "NodeHandle must satisfy NodeHandleLike");

/// @brief Iterator for child nodes (linked-list traversal)
class ChildIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = NodeView;
    using difference_type = std::ptrdiff_t;
    using pointer = NodeView*;
    using reference = NodeView;

    ChildIterator() = default;
    explicit ChildIterator(xmlNodePtr node) : current_(node) {}

    NodeView operator*() const;
    ChildIterator& operator++();
    ChildIterator operator++(int);
    bool operator==(ChildIterator const& other) const;
    bool operator!=(ChildIterator const& other) const { return !(*this == other); }

private:
    xmlNodePtr current_ = nullptr;
};

/// @brief Range for iterating over child nodes
class ChildRange {
public:
    using iterator = ChildIterator;

    ChildRange() = default;
    explicit ChildRange(xmlNodePtr parent);

    ChildIterator begin() const;
    ChildIterator end() const;
    bool empty() const;

private:
    xmlNodePtr first_child_ = nullptr;
};

/// @brief Iterator for attributes (linked-list traversal)
class AttributeIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = dom::AttributeView;
    using difference_type = std::ptrdiff_t;
    using pointer = dom::AttributeView*;
    using reference = dom::AttributeView;

    AttributeIterator() = default;
    explicit AttributeIterator(xmlAttrPtr attr) : current_(attr) {}

    dom::AttributeView operator*() const;
    AttributeIterator& operator++();
    AttributeIterator operator++(int);
    bool operator==(AttributeIterator const& other) const;
    bool operator!=(AttributeIterator const& other) const { return !(*this == other); }

private:
    xmlAttrPtr current_ = nullptr;
};

/// @brief Range for iterating over element attributes
class AttributeRange {
public:
    using iterator = AttributeIterator;

    AttributeRange() = default;
    explicit AttributeRange(xmlNodePtr node);

    AttributeIterator begin() const;
    AttributeIterator end() const;
    bool empty() const;

private:
    xmlAttrPtr first_attr_ = nullptr;
};

/// @brief Type-safe view over a libxml2 DOM node
///
/// Note: We represent document nodes by storing an `xmlDocPtr` cast to
/// `xmlNodePtr`. This is safe for the limited fields we access (type/children/
/// parent/next/prev) and allows treating the document as a node in the tree.
class NodeView {
public:
    NodeView() = default;
    explicit NodeView(xmlNodePtr node) : node_(node) {}

    xmlNodePtr get() const { return node_; }
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
    xmlNodePtr node_ = nullptr;
};

static_assert(dom::DOMNode<NodeView>, "NodeView must satisfy DOMNode concept");
static_assert(dom::DOMNodeWithHandle<NodeView>, "NodeView must satisfy DOMNodeWithHandle concept");

/// @brief Owns a parsed libxml2 HTML document
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
    explicit operator bool() const { return doc_ != nullptr; }
    xmlDocPtr get() const { return doc_; }

private:
    explicit Document(xmlDocPtr doc) : doc_(doc) {}
    xmlDocPtr doc_ = nullptr;
};

static_assert(dom::DOMDocument<Document, NodeView>, "Document must satisfy DOMDocument concept");

// Utility functions
std::string lookup_tag_name(xmlNodePtr node);
bool has_tag(xmlNodePtr node, std::string_view tag);
inline bool is_element(xmlNodePtr node) {
    return node && node->type == XML_ELEMENT_NODE;
}

} // namespace turndown_cpp::libxml2

/// @brief Hash specialization for libxml2::NodeHandle
template<>
struct std::hash<turndown_cpp::libxml2::NodeHandle> {
    std::size_t operator()(turndown_cpp::libxml2::NodeHandle const& handle) const noexcept {
        return std::hash<void const*>()(handle.raw());
    }
};

#endif // TURNDOWN_CPP_LIBXML2_ADAPTER_H

