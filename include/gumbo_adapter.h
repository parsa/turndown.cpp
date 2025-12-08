#ifndef TURNDOWN_CPP_GUMBO_ADAPTER_H
#define TURNDOWN_CPP_GUMBO_ADAPTER_H

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <gumbo.h>

namespace turndown_cpp::gumbo {

class NodeView;

struct NodeHandle {
    NodeHandle() = default;
    explicit NodeHandle(GumboNode* node) : node_(node) {}

    GumboNode* raw() const { return node_; }
    explicit operator bool() const { return node_ != nullptr; }

    bool operator==(NodeHandle const& other) const { return node_ == other.node_; }
    bool operator!=(NodeHandle const& other) const { return !(*this == other); }

private:
    GumboNode* node_ = nullptr;
};

enum class NodeType {
    Document,
    Element,
    Text,
    Whitespace,
    CData,
    Comment,
    Unknown
};

class ChildRange {
public:
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = NodeView;
        using difference_type = std::ptrdiff_t;
        using pointer = NodeView*;
        using reference = NodeView;

        explicit Iterator(GumboNode** ptr);
        NodeView operator*() const;
        Iterator& operator++();
        bool operator==(Iterator const& other) const;
        bool operator!=(Iterator const& other) const { return !(*this == other); }

    private:
        GumboNode** current_ = nullptr;
    };

    ChildRange() = default;
    explicit ChildRange(GumboVector* vector);

    Iterator begin() const;
    Iterator end() const;
    bool empty() const;

private:
    GumboNode** begin_ = nullptr;
    GumboNode** end_ = nullptr;
};

struct AttributeView {
    std::string_view name;
    std::string_view value;
};

class AttributeRange {
public:
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = AttributeView;
        using difference_type = std::ptrdiff_t;
        using pointer = AttributeView*;
        using reference = AttributeView;

        explicit Iterator(GumboAttribute** ptr);
        AttributeView operator*() const;
        Iterator& operator++();
        bool operator==(Iterator const& other) const;
        bool operator!=(Iterator const& other) const { return !(*this == other); }

    private:
        GumboAttribute** current_ = nullptr;
    };

    AttributeRange() = default;
    explicit AttributeRange(GumboVector* vector);

    Iterator begin() const;
    Iterator end() const;
    bool empty() const;

private:
    GumboAttribute** begin_ = nullptr;
    GumboAttribute** end_ = nullptr;
};

class NodeView {
public:
    NodeView() = default;
    explicit NodeView(GumboNode* node);

    GumboNode* raw() const { return node_; }
    explicit operator bool() const { return node_ != nullptr; }
    bool operator==(NodeView const& other) const { return node_ == other.node_; }
    bool operator!=(NodeView const& other) const { return node_ != other.node_; }

    NodeType type() const;
    bool is_document() const { return type() == NodeType::Document; }
    bool is_element() const { return type() == NodeType::Element; }
    bool is_text_like() const;

    NodeView parent() const;
    NodeView next_sibling() const;
    NodeView first_child() const;
    std::vector<NodeView> children() const;
    ChildRange child_range() const;

    std::string tag_name() const;
    bool has_tag(std::string_view tag) const;
    NodeView find_child(std::string_view tag) const;
    NodeView first_text_child() const;
    NodeHandle handle() const { return NodeHandle(node_); }

    std::string_view attribute(std::string_view name) const;
    bool has_attribute(std::string_view name) const;
    AttributeRange attribute_range() const;

    std::string text_content() const;
    void set_text(std::string const& text);
    std::string_view text() const;

private:
    GumboNode* node_ = nullptr;
};

class Document {
public:
    Document() = default;
    explicit Document(GumboOutput* output);
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
    GumboOutput* raw() const { return output_; }

private:
    GumboOutput* output_ = nullptr;
};

std::string_view to_string_view(GumboStringPiece const& piece);

std::string lookup_tag_name(GumboNode* node);

bool has_tag(GumboNode* node, std::string_view tag);

inline bool is_element(GumboNode* node) {
    return node && node->type == GUMBO_NODE_ELEMENT;
}

} // namespace turndown_cpp::gumbo

template<>
struct std::hash<turndown_cpp::gumbo::NodeHandle> {
    std::size_t operator()(turndown_cpp::gumbo::NodeHandle const& handle) const noexcept {
        return std::hash<void*>()(handle.raw());
    }
};

#endif // TURNDOWN_CPP_GUMBO_ADAPTER_H

