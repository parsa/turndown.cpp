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

} // namespace

NodeView::NodeView(GumboNode* node) : node_(node) {}

NodeType NodeView::type() const {
    return to_node_type(node_);
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
    return ChildRange(children_vector(node_));
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
    if (!node_ || (node_->type != GUMBO_NODE_ELEMENT && node_->type != GUMBO_NODE_TEMPLATE)) return {};
    return AttributeRange(&node_->v.element.attributes);
}

Document::Document(GumboOutput* output) : output_(output) {}

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
    if (!node || node->type != GUMBO_NODE_ELEMENT) return false;
    return lookup_tag_name(node) == tag;
}

ChildRange::ChildRange(GumboVector* vector) {
    if (vector && vector->length > 0) {
        begin_ = reinterpret_cast<GumboNode**>(vector->data);
        end_ = begin_ + vector->length;
    } else {
        begin_ = end_ = nullptr;
    }
}

ChildRange::Iterator ChildRange::begin() const {
    return Iterator(begin_);
}

ChildRange::Iterator ChildRange::end() const {
    return Iterator(end_);
}

bool ChildRange::empty() const {
    return begin_ == end_;
}

ChildRange::Iterator::Iterator(GumboNode** ptr) : current_(ptr) {}

NodeView ChildRange::Iterator::operator*() const {
    return (current_ && *current_) ? NodeView(*current_) : NodeView{};
}

ChildRange::Iterator& ChildRange::Iterator::operator++() {
    if (current_) {
        ++current_;
    }
    return *this;
}

bool ChildRange::Iterator::operator==(Iterator const& other) const {
    return current_ == other.current_;
}

AttributeRange::AttributeRange(GumboVector* vector) {
    if (vector && vector->length > 0) {
        begin_ = reinterpret_cast<GumboAttribute**>(vector->data);
        end_ = begin_ + vector->length;
    } else {
        begin_ = end_ = nullptr;
    }
}

AttributeRange::Iterator AttributeRange::begin() const {
    return Iterator(begin_);
}

AttributeRange::Iterator AttributeRange::end() const {
    return Iterator(end_);
}

bool AttributeRange::empty() const {
    return begin_ == end_;
}

AttributeRange::Iterator::Iterator(GumboAttribute** ptr) : current_(ptr) {}

AttributeView AttributeRange::Iterator::operator*() const {
    if (!current_ || !*current_) return {};
    GumboAttribute* attr = *current_;
    std::string_view name = attr->name ? std::string_view(attr->name) : std::string_view{};
    std::string_view value = attr->value ? std::string_view(attr->value) : std::string_view{};
    return AttributeView{name, value};
}

AttributeRange::Iterator& AttributeRange::Iterator::operator++() {
    if (current_) {
        ++current_;
    }
    return *this;
}

bool AttributeRange::Iterator::operator==(Iterator const& other) const {
    return current_ == other.current_;
}

} // namespace turndown_cpp::gumbo

