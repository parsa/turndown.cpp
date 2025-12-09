/// @file gumbo_adapter.cpp
/// @brief Type-safe Gumbo HTML parser adapter implementation
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#include "gumbo_adapter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace turndown_cpp::gumbo {

namespace {

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

dom::NodeType to_node_type(GumboNode const* node) {
    if (!node) return dom::NodeType::Unknown;
    switch (node->type) {
        case GUMBO_NODE_DOCUMENT: return dom::NodeType::Document;
        case GUMBO_NODE_ELEMENT: return dom::NodeType::Element;
        case GUMBO_NODE_TEXT: return dom::NodeType::Text;
        case GUMBO_NODE_WHITESPACE: return dom::NodeType::Whitespace;
        case GUMBO_NODE_CDATA: return dom::NodeType::CData;
        case GUMBO_NODE_COMMENT: return dom::NodeType::Comment;
        default: return dom::NodeType::Unknown;
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

} // namespace

// --- Utility functions ---

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

bool has_tag(GumboNode* node, std::string_view tag) {
    return is_element(node) && lookup_tag_name(node) == tag;
}

// --- ChildIterator implementation ---

ChildIterator::ChildIterator(GumboVector const* vec, unsigned index)
    : vec_(vec), index_(index) {}

NodeView ChildIterator::operator*() const {
    if (!vec_ || index_ >= vec_->length) return {};
    return NodeView(static_cast<GumboNode*>(vec_->data[index_]));
}

ChildIterator& ChildIterator::operator++() {
    ++index_;
    return *this;
}

ChildIterator ChildIterator::operator++(int) {
    ChildIterator tmp = *this;
    ++*this;
    return tmp;
}

bool ChildIterator::operator==(ChildIterator const& other) const {
    return vec_ == other.vec_ && index_ == other.index_;
}

// --- ChildRange implementation ---

ChildRange::ChildRange(GumboNode* parent) {
    vec_ = children_vector(parent);
}

ChildIterator ChildRange::begin() const {
    return ChildIterator(vec_, 0);
}

ChildIterator ChildRange::end() const {
    return ChildIterator(vec_, vec_ ? vec_->length : 0);
}

bool ChildRange::empty() const {
    return !vec_ || vec_->length == 0;
}

std::size_t ChildRange::size() const {
    return vec_ ? vec_->length : 0;
}

// --- AttributeIterator implementation ---

AttributeIterator::AttributeIterator(GumboVector const* vec, unsigned index)
    : vec_(vec), index_(index) {}

dom::AttributeView AttributeIterator::operator*() const {
    if (!vec_ || index_ >= vec_->length) return {};
    GumboAttribute* attr = static_cast<GumboAttribute*>(vec_->data[index_]);
    std::string_view name = attr->name ? std::string_view(attr->name) : std::string_view{};
    std::string_view value = attr->value ? std::string_view(attr->value) : std::string_view{};
    return dom::AttributeView{name, value};
}

AttributeIterator& AttributeIterator::operator++() {
    ++index_;
    return *this;
}

AttributeIterator AttributeIterator::operator++(int) {
    AttributeIterator tmp = *this;
    ++*this;
    return tmp;
}

bool AttributeIterator::operator==(AttributeIterator const& other) const {
    return vec_ == other.vec_ && index_ == other.index_;
}

// --- AttributeRange implementation ---

AttributeRange::AttributeRange(GumboNode* node) {
    vec_ = attributes_vector(node);
}

AttributeIterator AttributeRange::begin() const {
    return AttributeIterator(vec_, 0);
}

AttributeIterator AttributeRange::end() const {
    return AttributeIterator(vec_, vec_ ? vec_->length : 0);
}

bool AttributeRange::empty() const {
    return !vec_ || vec_->length == 0;
}

std::size_t AttributeRange::size() const {
    return vec_ ? vec_->length : 0;
}

// --- NodeView implementation ---

dom::NodeType NodeView::type() const {
    return to_node_type(node_);
}

bool NodeView::is_text_like() const {
    switch (type()) {
        case dom::NodeType::Text:
        case dom::NodeType::Whitespace:
        case dom::NodeType::CData:
            return true;
        default:
            return false;
    }
}

NodeView NodeView::parent() const {
    return node_ ? NodeView(node_->parent) : NodeView{};
}

NodeView NodeView::first_child() const {
    if (auto* vec = children_vector(node_)) {
        if (vec->length > 0) {
            return NodeView(static_cast<GumboNode*>(vec->data[0]));
        }
    }
    return {};
}

NodeView NodeView::next_sibling() const {
    if (!node_ || !node_->parent) return {};
    if (auto* siblings = children_vector(node_->parent)) {
        for (unsigned i = 0; i < siblings->length; ++i) {
            if (siblings->data[i] == node_ && i + 1 < siblings->length) {
                return NodeView(static_cast<GumboNode*>(siblings->data[i + 1]));
            }
        }
    }
    return {};
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> result;
    if (auto* vec = children_vector(node_)) {
        result.reserve(vec->length);
        for (unsigned i = 0; i < vec->length; ++i) {
            result.emplace_back(static_cast<GumboNode*>(vec->data[i]));
        }
    }
    return result;
}

ChildRange NodeView::child_range() const {
    return ChildRange(node_);
}

std::string NodeView::tag_name() const {
    return lookup_tag_name(node_);
}

bool NodeView::has_tag(std::string_view tag) const {
    return node_ && is_element() && lookup_tag_name(node_) == tag;
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
    collect_text_impl(node_, out);
    return out.str();
}

std::string_view NodeView::attribute(std::string_view name) const {
    if (!node_ || node_->type != GUMBO_NODE_ELEMENT) return {};
    GumboAttribute* attr = gumbo_get_attribute(&node_->v.element.attributes, std::string(name).c_str());
    if (!attr || !attr->value) return {};
    return attr->value;
}

bool NodeView::has_attribute(std::string_view name) const {
    return !attribute(name).empty();
}

void NodeView::set_text(std::string const& text) {
    if (!node_) return;
    if (node_->type == GUMBO_NODE_TEXT || node_->type == GUMBO_NODE_WHITESPACE || node_->type == GUMBO_NODE_CDATA) {
        node_->v.text.text = const_cast<char*>(text.c_str());
    }
}

std::string_view NodeView::text() const {
    if (!node_) return {};
    switch (node_->type) {
        case GUMBO_NODE_TEXT:
        case GUMBO_NODE_WHITESPACE:
        case GUMBO_NODE_CDATA:
        case GUMBO_NODE_COMMENT:
            return node_->v.text.text ? std::string_view(node_->v.text.text) : std::string_view{};
        default:
            return {};
    }
}

AttributeRange NodeView::attribute_range() const {
    return AttributeRange(node_);
}

// --- Document implementation ---

Document Document::parse(std::string const& html) {
    return Document(gumbo_parse(html.c_str()));
}

Document::Document(Document&& other) noexcept : output_(other.output_) {
    other.output_ = nullptr;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    if (output_) {
        gumbo_destroy_output(&kGumboDefaultOptions, output_);
    }
    output_ = other.output_;
    other.output_ = nullptr;
    return *this;
}

Document::~Document() {
    if (output_) {
        gumbo_destroy_output(&kGumboDefaultOptions, output_);
        output_ = nullptr;
    }
}

NodeView Document::root() const {
    return output_ ? NodeView(output_->root) : NodeView{};
}

NodeView Document::document() const {
    return output_ ? NodeView(output_->document) : NodeView{};
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

} // namespace turndown_cpp::gumbo
