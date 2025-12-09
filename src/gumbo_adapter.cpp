#include "gumbo_adapter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gumbo.h>

namespace turndown_cpp::gumbo {

namespace {

inline GumboNode* as_gumbo_node(void* ptr) {
    return static_cast<GumboNode*>(ptr);
}

inline GumboOutput* as_gumbo_output(void* ptr) {
    return static_cast<GumboOutput*>(ptr);
}

GumboVector* children_vector(GumboNode* node) {
    if (!node) return nullptr;
    if (node->type == GUMBO_NODE_DOCUMENT) {
        return &node->v.document.children;
    }
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        return &node->v.element.children;
    }
    return nullptr;
}

GumboVector* attributes_vector(GumboNode* node) {
    if (!node) return nullptr;
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        return &node->v.element.attributes;
    }
    return nullptr;
}

NodeType to_node_type(GumboNode const* node) {
    if (!node) return NodeType::Unknown;
    switch (node->type) {
        case GUMBO_NODE_DOCUMENT: return NodeType::Document;
        case GUMBO_NODE_ELEMENT: return NodeType::Element;
        case GUMBO_NODE_TEXT: return NodeType::Text;
        case GUMBO_NODE_WHITESPACE: return NodeType::Whitespace;
        case GUMBO_NODE_CDATA: return NodeType::CData;
        case GUMBO_NODE_COMMENT: return NodeType::Comment;
        default: return NodeType::Unknown;
    }
}

void collect_text_impl(GumboNode* node, std::ostringstream& out) {
    if (!node) return;
    switch (node->type) {
        case GUMBO_NODE_TEXT:
        case GUMBO_NODE_WHITESPACE:
        case GUMBO_NODE_CDATA:
            if (node->v.text.text) out << node->v.text.text;
            break;
        case GUMBO_NODE_ELEMENT:
        case GUMBO_NODE_TEMPLATE: {
            if (auto* children = &node->v.element.children) {
                for (unsigned i = 0; i < children->length; ++i) {
                    collect_text_impl(static_cast<GumboNode*>(children->data[i]), out);
                }
            }
            break;
        }
        default:
            break;
    }
}

std::string_view to_string_view(GumboStringPiece const& piece) {
    if (!piece.data || piece.length == 0) {
        return {};
    }
    return std::string_view(piece.data, piece.length);
}

std::string lookup_tag_name(GumboNode* node) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) return "";
    if (node->v.element.tag != GUMBO_TAG_UNKNOWN) {
        char const* normalized = gumbo_normalized_tagname(node->v.element.tag);
        return normalized ? std::string(normalized) : "";
    }
    auto view = to_string_view(node->v.element.original_tag);
    if (view.empty()) return "";
    std::size_t start = view.find_first_not_of("< /");
    if (start == std::string_view::npos) return "";
    std::size_t end = start;
    while (end < view.size()) {
        char c = view[end];
        if (std::isspace(static_cast<unsigned char>(c)) || c == '/' || c == '>') break;
        ++end;
    }
    std::string name(view.substr(start, end - start));
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name;
}

} // namespace

// --- NodeView implementation ---

NodeView::NodeView(void* node) : node_(node) {}

NodeType NodeView::type() const {
    return to_node_type(as_gumbo_node(node_));
}

bool NodeView::is_text_like() const {
    switch (type()) {
        case NodeType::Text:
        case NodeType::Whitespace:
        case NodeType::CData:
            return true;
        default:
            return false;
    }
}

NodeView NodeView::parent() const {
    auto* node = as_gumbo_node(node_);
    return node ? NodeView(node->parent) : NodeView{};
}

NodeView NodeView::first_child() const {
    if (auto* vec = children_vector(as_gumbo_node(node_))) {
        if (vec->length > 0) {
            return NodeView(vec->data[0]);
        }
    }
    return {};
}

NodeView NodeView::next_sibling() const {
    auto* node = as_gumbo_node(node_);
    if (!node || !node->parent) return {};
    if (auto* siblings = children_vector(node->parent)) {
        for (unsigned i = 0; i < siblings->length; ++i) {
            if (siblings->data[i] == node && i + 1 < siblings->length) {
                return NodeView(siblings->data[i + 1]);
            }
        }
    }
    return {};
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> result;
    if (auto* vec = children_vector(as_gumbo_node(node_))) {
        result.reserve(vec->length);
        for (unsigned i = 0; i < vec->length; ++i) {
            result.emplace_back(vec->data[i]);
        }
    }
    return result;
}

ChildRange NodeView::child_range() const {
    return ChildRange(node_);
}

std::string NodeView::tag_name() const {
    return lookup_tag_name(as_gumbo_node(node_));
}

bool NodeView::has_tag(std::string_view tag) const {
    return node_ && is_element() && lookup_tag_name(as_gumbo_node(node_)) == tag;
}

NodeView NodeView::find_child(std::string_view tag) const {
    for (auto child : child_range()) {
        if (child.has_tag(tag)) {
            return child;
        }
    }
    return {};
}

NodeView NodeView::first_text_child() const {
    for (auto child : child_range()) {
        if (child.is_text_like()) {
            return child;
        }
    }
    return {};
}

std::string NodeView::text_content() const {
    std::ostringstream out;
    collect_text_impl(as_gumbo_node(node_), out);
    return out.str();
}

std::string_view NodeView::attribute(std::string_view name) const {
    auto* node = as_gumbo_node(node_);
    if (!node || node->type != GUMBO_NODE_ELEMENT) return {};
    GumboAttribute* attr = gumbo_get_attribute(&node->v.element.attributes, std::string(name).c_str());
    if (!attr || !attr->value) return {};
    return attr->value;
}

bool NodeView::has_attribute(std::string_view name) const {
    return !attribute(name).empty();
}

void NodeView::set_text(std::string const& text) {
    auto* node = as_gumbo_node(node_);
    if (!node) return;
    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE || node->type == GUMBO_NODE_CDATA) {
        node->v.text.text = const_cast<char*>(text.c_str());
    }
}

std::string_view NodeView::text() const {
    auto* node = as_gumbo_node(node_);
    if (!node) return {};
    switch (node->type) {
        case GUMBO_NODE_TEXT:
        case GUMBO_NODE_WHITESPACE:
        case GUMBO_NODE_CDATA:
        case GUMBO_NODE_COMMENT:
            return node->v.text.text ? std::string_view(node->v.text.text) : std::string_view{};
        default:
            return {};
    }
}

AttributeRange NodeView::attribute_range() const {
    return AttributeRange(node_);
}

// --- Document implementation ---

Document::Document(void* output) : output_(output) {}

Document Document::parse(std::string const& html) {
    return Document(gumbo_parse(html.c_str()));
}

Document::Document(Document&& other) noexcept : output_(other.output_) {
    other.output_ = nullptr;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    if (output_) {
        gumbo_destroy_output(&kGumboDefaultOptions, as_gumbo_output(output_));
    }
    output_ = other.output_;
    other.output_ = nullptr;
    return *this;
}

Document::~Document() {
    if (output_) {
        gumbo_destroy_output(&kGumboDefaultOptions, as_gumbo_output(output_));
        output_ = nullptr;
    }
}

NodeView Document::root() const {
    auto* output = as_gumbo_output(output_);
    return output ? NodeView(output->root) : NodeView{};
}

NodeView Document::document() const {
    auto* output = as_gumbo_output(output_);
    return output ? NodeView(output->document) : NodeView{};
}

NodeView Document::html() const {
    NodeView doc = document();
    for (auto child : doc.child_range()) {
        if (child.is_element()) {
            return child;
        }
    }
    return {};
}

NodeView Document::body() const {
    NodeView html_node = html();
    for (auto child : html_node.child_range()) {
        if (child.has_tag("body")) {
            return child;
        }
    }
    return {};
}

// --- ChildRange implementation ---

ChildRange::ChildRange(void* parent) : parent_(parent) {
    if (auto* vec = children_vector(as_gumbo_node(parent))) {
        count_ = vec->length;
    }
}

ChildRange::Iterator ChildRange::begin() const {
    return Iterator(parent_, 0, count_);
}

ChildRange::Iterator ChildRange::end() const {
    return Iterator(parent_, count_, count_);
}

bool ChildRange::empty() const {
    return count_ == 0;
}

ChildRange::Iterator::Iterator(void* parent, std::size_t index, std::size_t count)
    : parent_(parent), index_(index), count_(count) {}

NodeView ChildRange::Iterator::operator*() const {
    if (!parent_ || index_ >= count_) return {};
    if (auto* vec = children_vector(as_gumbo_node(parent_))) {
        if (index_ < vec->length) {
            return NodeView(vec->data[index_]);
        }
    }
    return {};
}

ChildRange::Iterator& ChildRange::Iterator::operator++() {
    if (index_ < count_) {
        ++index_;
    }
    return *this;
}

bool ChildRange::Iterator::operator==(Iterator const& other) const {
    return parent_ == other.parent_ && index_ == other.index_;
}

// --- AttributeRange implementation ---

AttributeRange::AttributeRange(void* node) : node_(node) {
    if (auto* vec = attributes_vector(as_gumbo_node(node))) {
        count_ = vec->length;
    }
}

AttributeRange::Iterator AttributeRange::begin() const {
    return Iterator(node_, 0, count_);
}

AttributeRange::Iterator AttributeRange::end() const {
    return Iterator(node_, count_, count_);
}

bool AttributeRange::empty() const {
    return count_ == 0;
}

AttributeRange::Iterator::Iterator(void* node, std::size_t index, std::size_t count)
    : node_(node), index_(index), count_(count) {}

AttributeView AttributeRange::Iterator::operator*() const {
    if (!node_ || index_ >= count_) return {};
    if (auto* vec = attributes_vector(as_gumbo_node(node_))) {
        if (index_ < vec->length) {
            GumboAttribute* attr = static_cast<GumboAttribute*>(vec->data[index_]);
            std::string_view name = attr->name ? std::string_view(attr->name) : std::string_view{};
            std::string_view value = attr->value ? std::string_view(attr->value) : std::string_view{};
            return AttributeView{name, value};
        }
    }
    return {};
}

AttributeRange::Iterator& AttributeRange::Iterator::operator++() {
    if (index_ < count_) {
        ++index_;
    }
    return *this;
}

bool AttributeRange::Iterator::operator==(Iterator const& other) const {
    return node_ == other.node_ && index_ == other.index_;
}

} // namespace turndown_cpp::gumbo
