/// @file libxml2_adapter.cpp
/// @brief Type-safe libxml2 HTML parser adapter implementation
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#include "libxml2_adapter.h"

#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace turndown_cpp::libxml2 {

namespace {

thread_local xmlDocPtr tl_current_doc = nullptr;
thread_local std::unordered_map<xmlAttrPtr, std::string> tl_attr_value_cache;

void clear_attr_cache_for_current_doc(xmlDocPtr doc) {
    if (tl_current_doc == doc) {
        tl_attr_value_cache.clear();
        tl_current_doc = nullptr;
    }
}

void set_current_doc(xmlDocPtr doc) {
    tl_current_doc = doc;
    tl_attr_value_cache.clear();
}

std::string_view cached_attr_value(xmlAttrPtr attr) {
    if (!attr) return {};
    auto it = tl_attr_value_cache.find(attr);
    if (it != tl_attr_value_cache.end()) {
        return it->second;
    }

    xmlChar* value = xmlNodeListGetString(attr->doc, attr->children, 1 /*inLine*/);
    std::string str;
    if (value) {
        str = reinterpret_cast<char const*>(value);
        xmlFree(value);
    }

    auto [ins_it, _] = tl_attr_value_cache.emplace(attr, std::move(str));
    return ins_it->second;
}

dom::NodeType to_node_type(xmlNodePtr node) {
    if (!node) return dom::NodeType::Unknown;
    switch (node->type) {
        case XML_DOCUMENT_NODE:
#ifdef XML_HTML_DOCUMENT_NODE
        case XML_HTML_DOCUMENT_NODE:
#endif
            return dom::NodeType::Document;
        case XML_ELEMENT_NODE:
            return dom::NodeType::Element;
        case XML_TEXT_NODE:
            return (xmlIsBlankNode(node) != 0) ? dom::NodeType::Whitespace : dom::NodeType::Text;
        case XML_CDATA_SECTION_NODE:
            return dom::NodeType::CData;
        case XML_COMMENT_NODE:
            return dom::NodeType::Comment;
        default:
            return dom::NodeType::Unknown;
    }
}

void collect_text_impl(xmlNodePtr node, std::ostringstream& out) {
    if (!node) return;
    switch (node->type) {
        case XML_TEXT_NODE:
        case XML_CDATA_SECTION_NODE:
            if (node->content) {
                out << reinterpret_cast<char const*>(node->content);
            }
            break;
        case XML_ELEMENT_NODE:
        case XML_DOCUMENT_NODE:
#ifdef XML_HTML_DOCUMENT_NODE
        case XML_HTML_DOCUMENT_NODE:
#endif
            for (xmlNodePtr child = node->children; child; child = child->next) {
                collect_text_impl(child, out);
            }
            break;
        default:
            break;
    }
}

std::string lowercase(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

// libxml2's HTML parser is based on an older HTML parsing model and can
// mis-handle a literal '<' in text (e.g. "< 1") by treating it as markup.
// To better match other backends (and HTML5 parsing rules), we escape '<'
// when it is not starting a tag open sequence.
std::string escape_non_tag_angle_brackets(std::string const& html) {
    auto is_ascii_alpha = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    };

    std::string out;
    out.reserve(html.size());
    for (std::size_t i = 0; i < html.size(); ++i) {
        char c = html[i];
        if (c == '<') {
            unsigned char next = (i + 1 < html.size())
                ? static_cast<unsigned char>(html[i + 1])
                : 0;
            bool tag_open = is_ascii_alpha(next) || next == '/' || next == '!' || next == '?';
            if (!tag_open) {
                out += "&lt;";
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

} // namespace

// --- Utility functions ---

std::string lookup_tag_name(xmlNodePtr node) {
    if (!node || node->type != XML_ELEMENT_NODE || !node->name) return "";
    auto sv = std::string_view(reinterpret_cast<char const*>(node->name));
    return lowercase(sv);
}

bool has_tag(xmlNodePtr node, std::string_view tag) {
    if (!node || node->type != XML_ELEMENT_NODE || !node->name) return false;
    std::string needle(tag);
    // HTML is case-insensitive; compare with libxml2 helper.
    return xmlStrcasecmp(node->name, reinterpret_cast<xmlChar const*>(needle.c_str())) == 0;
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

ChildRange::ChildRange(xmlNodePtr parent) {
    if (parent) {
        first_child_ = parent->children;
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
    std::string_view name = current_->name
        ? std::string_view(reinterpret_cast<char const*>(current_->name))
        : std::string_view{};
    std::string_view value = cached_attr_value(current_);
    return dom::AttributeView{name, value};
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

AttributeRange::AttributeRange(xmlNodePtr node) {
    if (node && node->type == XML_ELEMENT_NODE) {
        first_attr_ = node->properties;
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

NodeView NodeView::next_sibling() const {
    return node_ ? NodeView(node_->next) : NodeView{};
}

NodeView NodeView::first_child() const {
    return node_ ? NodeView(node_->children) : NodeView{};
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> result;
    if (!node_) return result;
    for (xmlNodePtr child = node_->children; child; child = child->next) {
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
    // Qualify to avoid calling NodeView::has_tag recursively (name hiding).
    return ::turndown_cpp::libxml2::has_tag(node_, tag);
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

std::string_view NodeView::attribute(std::string_view name) const {
    if (!node_ || node_->type != XML_ELEMENT_NODE) return {};
    std::string needle(name);
    for (xmlAttrPtr attr = node_->properties; attr; attr = attr->next) {
        if (!attr->name) continue;
        if (xmlStrcasecmp(attr->name, reinterpret_cast<xmlChar const*>(needle.c_str())) == 0) {
            return cached_attr_value(attr);
        }
    }
    return {};
}

bool NodeView::has_attribute(std::string_view name) const {
    return !attribute(name).empty();
}

AttributeRange NodeView::attribute_range() const {
    return AttributeRange(node_);
}

std::string NodeView::text_content() const {
    std::ostringstream out;
    collect_text_impl(node_, out);
    return out.str();
}

void NodeView::set_text(std::string const& /*text*/) {
    // DOM mutation is not required for turndown's whitespace collapsing:
    // we track text replacements externally.
}

std::string_view NodeView::text() const {
    if (!node_) return {};
    switch (node_->type) {
        case XML_TEXT_NODE:
        case XML_CDATA_SECTION_NODE:
        case XML_COMMENT_NODE:
            return node_->content ? std::string_view(reinterpret_cast<char const*>(node_->content))
                                  : std::string_view{};
        default:
            return {};
    }
}

// --- Document implementation ---

Document Document::parse(std::string const& html) {
    // Ensure libxml2 is initialized.
    xmlInitParser();

    // Parse as HTML (tolerant), suppress errors/warnings, and forbid network fetches.
    int const options = HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    std::string sanitized = escape_non_tag_angle_brackets(html);

    htmlDocPtr doc = htmlReadMemory(
        sanitized.c_str(),
        static_cast<int>(sanitized.size()),
        nullptr,          // URL
        "UTF-8",          // encoding
        options
    );

    if (!doc) {
        return Document(nullptr);
    }

    set_current_doc(doc);
    return Document(doc);
}

Document::Document(Document&& other) noexcept : doc_(other.doc_) {
    other.doc_ = nullptr;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    if (doc_) {
        clear_attr_cache_for_current_doc(doc_);
        xmlFreeDoc(doc_);
    }
    doc_ = other.doc_;
    other.doc_ = nullptr;
    return *this;
}

Document::~Document() {
    if (doc_) {
        clear_attr_cache_for_current_doc(doc_);
        xmlFreeDoc(doc_);
        doc_ = nullptr;
    }
}

NodeView Document::document() const {
    if (!doc_) return {};
    return NodeView(reinterpret_cast<xmlNodePtr>(doc_));
}

NodeView Document::html() const {
    if (!doc_) return {};
    return NodeView(xmlDocGetRootElement(doc_));
}

NodeView Document::body() const {
    if (!doc_) return {};
    xmlNodePtr root = xmlDocGetRootElement(doc_);
    if (!root) return {};

    // Prefer <body> inside <html>.
    if (root->type == XML_ELEMENT_NODE && root->name &&
        xmlStrcasecmp(root->name, BAD_CAST "html") == 0) {
        for (xmlNodePtr child = root->children; child; child = child->next) {
            if (child->type != XML_ELEMENT_NODE || !child->name) continue;
            if (xmlStrcasecmp(child->name, BAD_CAST "body") == 0) {
                return NodeView(child);
            }
        }
    }

    // Fallback: depth-first search for the first <body> element.
    std::vector<xmlNodePtr> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        xmlNodePtr node = stack.back();
        stack.pop_back();
        if (node && node->type == XML_ELEMENT_NODE && node->name &&
            xmlStrcasecmp(node->name, BAD_CAST "body") == 0) {
            return NodeView(node);
        }
        for (xmlNodePtr child = node ? node->children : nullptr; child; child = child->next) {
            stack.push_back(child);
        }
    }

    return {};
}

NodeView Document::root() const {
    if (!doc_) return {};
    // Match Turndown JS semantics: operate on <body> when available.
    if (auto b = body()) return b;
    if (auto h = html()) return h;
    return document();
}

} // namespace turndown_cpp::libxml2

