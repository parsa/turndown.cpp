/// @file lexbor_adapter.cpp
/// @brief Type-safe Lexbor HTML parser adapter implementation
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#include "lexbor_adapter.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace turndown_cpp::lexbor {

namespace {

// Thread-local storage for cached text content
thread_local std::unordered_map<lxb_dom_node_t*, std::string> tl_text_cache;

void clear_text_cache() {
    tl_text_cache.clear();
}

dom::NodeType to_node_type(lxb_dom_node_t* node) {
    if (!node) return dom::NodeType::Unknown;
    switch (node->type) {
        case LXB_DOM_NODE_TYPE_DOCUMENT:
            return dom::NodeType::Document;
        case LXB_DOM_NODE_TYPE_ELEMENT:
            return dom::NodeType::Element;
        case LXB_DOM_NODE_TYPE_TEXT:
            return dom::NodeType::Text;
        case LXB_DOM_NODE_TYPE_CDATA_SECTION:
            return dom::NodeType::CData;
        case LXB_DOM_NODE_TYPE_COMMENT:
            return dom::NodeType::Comment;
        default:
            return dom::NodeType::Unknown;
    }
}

void collect_text_impl(lxb_dom_node_t* node, std::ostringstream& out) {
    if (!node) return;
    
    switch (node->type) {
        case LXB_DOM_NODE_TYPE_TEXT: {
            lxb_dom_text_t* text = lxb_dom_interface_text(node);
            if (text && text->char_data.data.data) {
                out << std::string_view(
                    reinterpret_cast<char const*>(text->char_data.data.data),
                    text->char_data.data.length
                );
            }
            break;
        }
        case LXB_DOM_NODE_TYPE_CDATA_SECTION: {
            lxb_dom_cdata_section_t* cdata = lxb_dom_interface_cdata_section(node);
            if (cdata && cdata->text.char_data.data.data) {
                out << std::string_view(
                    reinterpret_cast<char const*>(cdata->text.char_data.data.data),
                    cdata->text.char_data.data.length
                );
            }
            break;
        }
        case LXB_DOM_NODE_TYPE_ELEMENT:
        case LXB_DOM_NODE_TYPE_DOCUMENT: {
            for (lxb_dom_node_t* child = node->first_child; child; child = child->next) {
                collect_text_impl(child, out);
            }
            break;
        }
        default:
            break;
    }
}

std::string_view get_text_content(lxb_dom_node_t* node) {
    if (!node) return {};
    
    switch (node->type) {
        case LXB_DOM_NODE_TYPE_TEXT: {
            lxb_dom_text_t* text = lxb_dom_interface_text(node);
            if (text && text->char_data.data.data) {
                return std::string_view(
                    reinterpret_cast<char const*>(text->char_data.data.data),
                    text->char_data.data.length
                );
            }
            break;
        }
        case LXB_DOM_NODE_TYPE_CDATA_SECTION: {
            lxb_dom_cdata_section_t* cdata = lxb_dom_interface_cdata_section(node);
            if (cdata && cdata->text.char_data.data.data) {
                return std::string_view(
                    reinterpret_cast<char const*>(cdata->text.char_data.data.data),
                    cdata->text.char_data.data.length
                );
            }
            break;
        }
        case LXB_DOM_NODE_TYPE_COMMENT: {
            lxb_dom_comment_t* comment = lxb_dom_interface_comment(node);
            if (comment && comment->char_data.data.data) {
                return std::string_view(
                    reinterpret_cast<char const*>(comment->char_data.data.data),
                    comment->char_data.data.length
                );
            }
            break;
        }
        default:
            break;
    }
    return {};
}

} // namespace

// --- Utility functions ---

std::string lookup_tag_name(lxb_dom_node_t* node) {
    if (!node || node->type != LXB_DOM_NODE_TYPE_ELEMENT) return "";
    
    std::size_t len = 0;
    lxb_char_t const* name = lxb_dom_node_name(node, &len);
    if (!name || len == 0) return "";
    
    std::string result(reinterpret_cast<char const*>(name), len);
    // Convert to lowercase
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool has_tag(lxb_dom_node_t* node, std::string_view tag) {
    return is_element(node) && lookup_tag_name(node) == tag;
}

// --- ChildIterator implementation ---

NodeView ChildIterator::operator*() const {
    return NodeView(current_);
}

ChildIterator& ChildIterator::operator++() {
    if (current_) {
        current_ = current_->next;
    }
    return *this;
}

ChildIterator ChildIterator::operator++(int) {
    ChildIterator tmp = *this;
    ++*this;
    return tmp;
}

bool ChildIterator::operator==(ChildIterator const& other) const {
    return current_ == other.current_;
}

// --- ChildRange implementation ---

ChildRange::ChildRange(lxb_dom_node_t* parent) {
    if (parent) {
        first_child_ = parent->first_child;
    }
}

ChildIterator ChildRange::begin() const {
    return ChildIterator(first_child_);
}

ChildIterator ChildRange::end() const {
    return ChildIterator(nullptr);
}

bool ChildRange::empty() const {
    return first_child_ == nullptr;
}

// --- AttributeIterator implementation ---

dom::AttributeView AttributeIterator::operator*() const {
    if (!current_) return {};
    
    std::size_t name_len = 0;
    lxb_char_t const* name = lxb_dom_attr_qualified_name(current_, &name_len);
    
    std::size_t value_len = 0;
    lxb_char_t const* value = lxb_dom_attr_value(current_, &value_len);
    
    return dom::AttributeView{
        name ? std::string_view(reinterpret_cast<char const*>(name), name_len) : std::string_view{},
        value ? std::string_view(reinterpret_cast<char const*>(value), value_len) : std::string_view{}
    };
}

AttributeIterator& AttributeIterator::operator++() {
    if (current_) {
        current_ = current_->next;
    }
    return *this;
}

AttributeIterator AttributeIterator::operator++(int) {
    AttributeIterator tmp = *this;
    ++*this;
    return tmp;
}

bool AttributeIterator::operator==(AttributeIterator const& other) const {
    return current_ == other.current_;
}

// --- AttributeRange implementation ---

AttributeRange::AttributeRange(lxb_dom_node_t* node) {
    if (node && is_element(node)) {
        lxb_dom_element_t* elem = lxb_dom_interface_element(node);
        if (elem) {
            first_attr_ = elem->first_attr;
        }
    }
}

AttributeIterator AttributeRange::begin() const {
    return AttributeIterator(first_attr_);
}

AttributeIterator AttributeRange::end() const {
    return AttributeIterator(nullptr);
}

bool AttributeRange::empty() const {
    return first_attr_ == nullptr;
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
    return node_ ? NodeView(node_->first_child) : NodeView{};
}

NodeView NodeView::next_sibling() const {
    return node_ ? NodeView(node_->next) : NodeView{};
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> result;
    if (!node_) return result;
    
    for (lxb_dom_node_t* child = node_->first_child; child; child = child->next) {
        result.emplace_back(child);
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
    if (!node_) return "";
    std::ostringstream out;
    collect_text_impl(node_, out);
    return out.str();
}

std::string_view NodeView::attribute(std::string_view name) const {
    if (!node_ || !lexbor::is_element(node_)) return {};
    
    lxb_dom_element_t* elem = lxb_dom_interface_element(node_);
    if (!elem) return {};
    
    std::size_t value_len = 0;
    lxb_char_t const* value = lxb_dom_element_get_attribute(
        elem,
        reinterpret_cast<lxb_char_t const*>(name.data()),
        name.size(),
        &value_len
    );
    
    if (!value) return {};
    return std::string_view(reinterpret_cast<char const*>(value), value_len);
}

bool NodeView::has_attribute(std::string_view name) const {
    return !attribute(name).empty();
}

void NodeView::set_text(std::string const& /*text*/) {
    // Lexbor DOM modification would require more complex handling
    // For whitespace collapsing, we use text replacements instead
}

std::string_view NodeView::text() const {
    return get_text_content(node_);
}

AttributeRange NodeView::attribute_range() const {
    return AttributeRange(node_);
}

// --- Document implementation ---

Document Document::parse(std::string const& html) {
    lxb_html_document_t* doc = lxb_html_document_create();
    if (!doc) return Document(nullptr);
    
    lxb_status_t status = lxb_html_document_parse(
        doc,
        reinterpret_cast<lxb_char_t const*>(html.c_str()),
        html.size()
    );
    
    if (status != LXB_STATUS_OK) {
        lxb_html_document_destroy(doc);
        return Document(nullptr);
    }
    
    // Clear text cache for new document
    clear_text_cache();
    
    return Document(doc);
}

Document::Document(Document&& other) noexcept : doc_(other.doc_) {
    other.doc_ = nullptr;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    if (doc_) {
        lxb_html_document_destroy(doc_);
    }
    doc_ = other.doc_;
    other.doc_ = nullptr;
    return *this;
}

Document::~Document() {
    if (doc_) {
        lxb_html_document_destroy(doc_);
        doc_ = nullptr;
    }
}

NodeView Document::root() const {
    if (!doc_) return {};
    // The root element (html) is the document element
    return NodeView(lxb_dom_interface_node(doc_->body)->parent);
}

NodeView Document::document() const {
    if (!doc_) return {};
    return NodeView(lxb_dom_interface_node(doc_));
}

NodeView Document::html() const {
    if (!doc_) return {};
    // Get the <html> element
    lxb_dom_node_t* doc_node = lxb_dom_interface_node(doc_);
    for (lxb_dom_node_t* child = doc_node->first_child; child; child = child->next) {
        if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) {
            return NodeView(child);
        }
    }
    return {};
}

NodeView Document::body() const {
    if (!doc_ || !doc_->body) return {};
    return NodeView(lxb_dom_interface_node(doc_->body));
}

} // namespace turndown_cpp::lexbor

