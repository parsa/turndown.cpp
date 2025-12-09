#ifndef TURNDOWN_CPP_GUMBO_ADAPTER_H
#define TURNDOWN_CPP_GUMBO_ADAPTER_H

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace turndown_cpp::gumbo {

class NodeView;

/// @brief Opaque handle for node identity (used in hash maps)
struct NodeHandle {
    NodeHandle() = default;
    explicit NodeHandle(void* node) : node_(node) {}

    void* raw() const { return node_; }
    explicit operator bool() const { return node_ != nullptr; }

    bool operator==(NodeHandle const& other) const { return node_ == other.node_; }
    bool operator!=(NodeHandle const& other) const { return !(*this == other); }

private:
    void* node_ = nullptr;
};

/// @brief Node type enumeration (parser-agnostic)
enum class NodeType {
    Document,
    Element,
    Text,
    Whitespace,
    CData,
    Comment,
    Unknown
};

/// @brief Attribute name-value pair
struct AttributeView {
    std::string_view name;
    std::string_view value;
};

/// @brief Range for iterating over child nodes
class ChildRange {
public:
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = NodeView;
        using difference_type = std::ptrdiff_t;
        using pointer = NodeView*;
        using reference = NodeView;

        Iterator() = default;
        Iterator(void* parent, std::size_t index, std::size_t count);
        NodeView operator*() const;
        Iterator& operator++();
        bool operator==(Iterator const& other) const;
        bool operator!=(Iterator const& other) const { return !(*this == other); }

    private:
        void* parent_ = nullptr;
        std::size_t index_ = 0;
        std::size_t count_ = 0;
    };

    ChildRange() = default;
    explicit ChildRange(void* parent);

    Iterator begin() const;
    Iterator end() const;
    bool empty() const;

private:
    void* parent_ = nullptr;
    std::size_t count_ = 0;
};

/// @brief Range for iterating over element attributes
class AttributeRange {
public:
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = AttributeView;
        using difference_type = std::ptrdiff_t;
        using pointer = AttributeView*;
        using reference = AttributeView;

        Iterator() = default;
        Iterator(void* node, std::size_t index, std::size_t count);
        AttributeView operator*() const;
        Iterator& operator++();
        bool operator==(Iterator const& other) const;
        bool operator!=(Iterator const& other) const { return !(*this == other); }

    private:
        void* node_ = nullptr;
        std::size_t index_ = 0;
        std::size_t count_ = 0;
    };

    AttributeRange() = default;
    explicit AttributeRange(void* node);

    Iterator begin() const;
    Iterator end() const;
    bool empty() const;

private:
    void* node_ = nullptr;
    std::size_t count_ = 0;
};

/// @brief Lightweight view over a DOM node
class NodeView {
public:
    NodeView() = default;
    explicit NodeView(void* node);

    void* raw() const { return node_; }
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
    void* node_ = nullptr;
};

/// @brief Owns a parsed HTML document
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
    void* raw() const { return output_; }

private:
    explicit Document(void* output);
    void* output_ = nullptr;
};

} // namespace turndown_cpp::gumbo

template<>
struct std::hash<turndown_cpp::gumbo::NodeHandle> {
    std::size_t operator()(turndown_cpp::gumbo::NodeHandle const& handle) const noexcept {
        return std::hash<void*>()(handle.raw());
    }
};

#endif // TURNDOWN_CPP_GUMBO_ADAPTER_H
