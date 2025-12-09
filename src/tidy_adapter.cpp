/// @file tidy_adapter.cpp
/// @brief Type-safe Tidy HTML parser adapter implementation
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#include "tidy_adapter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace turndown_cpp::tidy {

namespace {

// Thread-local storage for the current document context
// Needed because tidy API requires the document for text extraction
thread_local TidyDoc tl_current_doc = nullptr;

// Thread-local storage for cached text content from text nodes
thread_local std::unordered_map<TidyNode, std::string> tl_text_cache;

void set_current_doc(TidyDoc doc) {
    tl_current_doc = doc;
    tl_text_cache.clear();
}

TidyDoc get_current_doc() {
    return tl_current_doc;
}

std::string_view get_cached_text(TidyDoc doc, TidyNode node) {
    auto it = tl_text_cache.find(node);
    if (it != tl_text_cache.end()) {
        return it->second;
    }
    
    TidyBuffer buf = {0};
    // Use tidyNodeGetValue for raw unescaped text content
    if (tidyNodeGetValue(doc, node, &buf) && buf.bp) {
        std::string text(reinterpret_cast<char*>(buf.bp));
        tidyBufFree(&buf);
        auto [insert_it, _] = tl_text_cache.emplace(node, std::move(text));
        return insert_it->second;
    }
    tidyBufFree(&buf);
    return {};
}

dom::NodeType to_node_type(TidyNode node) {
    if (!node) return dom::NodeType::Unknown;
    TidyNodeType type = tidyNodeGetType(node);
    switch (type) {
        case TidyNode_Root:
            return dom::NodeType::Document;
        case TidyNode_Start:
        case TidyNode_End:
        case TidyNode_StartEnd:
            return dom::NodeType::Element;
        case TidyNode_Text:
            return dom::NodeType::Text;
        case TidyNode_CDATA:
            return dom::NodeType::CData;
        case TidyNode_Comment:
            return dom::NodeType::Comment;
        default:
            return dom::NodeType::Unknown;
    }
}

std::size_t count_children(TidyNode node) {
    std::size_t count = 0;
    for (TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        ++count;
    }
    return count;
}

std::size_t count_attributes(TidyNode node) {
    std::size_t count = 0;
    for (TidyAttr attr = tidyAttrFirst(node); attr; attr = tidyAttrNext(attr)) {
        ++count;
    }
    return count;
}

void collect_text_impl(TidyDoc doc, TidyNode node, std::ostringstream& out) {
    if (!node) return;
    TidyNodeType type = tidyNodeGetType(node);
    
    if (type == TidyNode_Text || type == TidyNode_CDATA) {
        TidyBuffer buf = {0};
        // Use tidyNodeGetValue for unescaped text
        if (tidyNodeGetValue(doc, node, &buf)) {
            if (buf.bp) {
                out << reinterpret_cast<char*>(buf.bp);
            }
            tidyBufFree(&buf);
        }
    } else {
        for (TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
            collect_text_impl(doc, child, out);
        }
    }
}

} // namespace

// --- Utility functions ---

std::string lookup_tag_name(TidyNode node) {
    if (!node) return "";
    TidyNodeType type = tidyNodeGetType(node);
    if (type != TidyNode_Start && type != TidyNode_End && type != TidyNode_StartEnd) {
        return "";
    }
    ctmbstr name = tidyNodeGetName(node);
    if (!name) return "";
    std::string result(name);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool has_tag(TidyNode node, std::string_view tag) {
    return is_element(node) && lookup_tag_name(node) == tag;
}

// --- ChildIterator implementation ---

NodeView ChildIterator::operator*() const {
    return NodeView(current_);
}

ChildIterator& ChildIterator::operator++() {
    if (current_) {
        current_ = tidyGetNext(current_);
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

ChildRange::ChildRange(TidyNode parent) {
    if (parent) {
        first_child_ = tidyGetChild(parent);
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
    ctmbstr name = tidyAttrName(current_);
    ctmbstr value = tidyAttrValue(current_);
    return dom::AttributeView{
        name ? std::string_view(name) : std::string_view{},
        value ? std::string_view(value) : std::string_view{}
    };
}

AttributeIterator& AttributeIterator::operator++() {
    if (current_) {
        current_ = tidyAttrNext(current_);
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

AttributeRange::AttributeRange(TidyNode node) {
    if (node && is_element(node)) {
        first_attr_ = tidyAttrFirst(node);
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
    return node_ ? NodeView(tidyGetParent(node_)) : NodeView{};
}

NodeView NodeView::first_child() const {
    return node_ ? NodeView(tidyGetChild(node_)) : NodeView{};
}

NodeView NodeView::next_sibling() const {
    return node_ ? NodeView(tidyGetNext(node_)) : NodeView{};
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> result;
    if (!node_) return result;
    
    std::size_t count = count_children(node_);
    result.reserve(count);
    for (TidyNode child = tidyGetChild(node_); child; child = tidyGetNext(child)) {
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
    TidyDoc doc = get_current_doc();
    if (!doc || !node_) return "";
    std::ostringstream out;
    collect_text_impl(doc, node_, out);
    return out.str();
}

std::string_view NodeView::attribute(std::string_view name) const {
    if (!node_ || !tidy::is_element(node_)) return {};
    for (TidyAttr attr = tidyAttrFirst(node_); attr; attr = tidyAttrNext(attr)) {
        ctmbstr attrName = tidyAttrName(attr);
        if (attrName && name == attrName) {
            ctmbstr value = tidyAttrValue(attr);
            return value ? std::string_view(value) : std::string_view{};
        }
    }
    return {};
}

bool NodeView::has_attribute(std::string_view name) const {
    return !attribute(name).empty();
}

void NodeView::set_text(std::string const& /*text*/) {
    // Tidy doesn't support modifying the DOM in the same way
    // This is used for whitespace collapsing which uses text replacements instead
}

std::string_view NodeView::text() const {
    if (!node_ || !is_text_like()) return {};
    TidyDoc doc = get_current_doc();
    if (!doc) return {};
    return get_cached_text(doc, node_);
}

AttributeRange NodeView::attribute_range() const {
    return AttributeRange(node_);
}

// --- Document implementation ---

Document Document::parse(std::string const& html) {
    TidyDoc doc = tidyCreate();
    if (!doc) return Document(nullptr);
    
    // Configure tidy for parsing - minimal modifications
    tidyOptSetBool(doc, TidyShowWarnings, no);
    tidyOptSetBool(doc, TidyShowInfo, no);
    tidyOptSetBool(doc, TidyQuiet, yes);
    tidyOptSetBool(doc, TidyForceOutput, yes);
    tidyOptSetInt(doc, TidyShowErrors, 0);
    
    // Disable wrapping to preserve whitespace
    tidyOptSetInt(doc, TidyWrapLen, 0);
    
    // Don't clean/modify the markup
    tidyOptSetBool(doc, TidyMakeClean, no);
    tidyOptSetBool(doc, TidyGDocClean, no);
    tidyOptSetBool(doc, TidyDropEmptyElems, no);
    tidyOptSetBool(doc, TidyDropEmptyParas, no);
    
    // Treat <code> as a pre-formatted element to preserve whitespace
    tidyOptSetValue(doc, TidyPreTags, "code");
    
    // Preserve newlines in attribute values
    tidyOptSetBool(doc, TidyLiteralAttribs, yes);
    
    // Register common custom elements as inline tags to prevent tidy from stripping them
    tidyOptSetValue(doc, TidyInlineTags, "custom");
    
    // Parse the HTML
    if (tidyParseString(doc, html.c_str()) < 0) {
        tidyRelease(doc);
        return Document(nullptr);
    }
    
    // Clean and repair is required for proper DOM structure.
    // Note: Tidy normalizes whitespace in inline elements during parsing,
    // which cannot be disabled. This affects whitespace in <code> elements.
    tidyCleanAndRepair(doc);
    
    // Set as current doc for text extraction
    set_current_doc(doc);
    
    return Document(doc);
}

Document::Document(Document&& other) noexcept : doc_(other.doc_) {
    other.doc_ = nullptr;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    if (doc_) {
        tidyRelease(doc_);
    }
    doc_ = other.doc_;
    other.doc_ = nullptr;
    return *this;
}

Document::~Document() {
    if (doc_) {
        tidyRelease(doc_);
        doc_ = nullptr;
    }
}

NodeView Document::root() const {
    return doc_ ? NodeView(tidyGetRoot(doc_)) : NodeView{};
}

NodeView Document::document() const {
    // Tidy's root is equivalent to the document
    return root();
}

NodeView Document::html() const {
    return doc_ ? NodeView(tidyGetHtml(doc_)) : NodeView{};
}

NodeView Document::body() const {
    return doc_ ? NodeView(tidyGetBody(doc_)) : NodeView{};
}

} // namespace turndown_cpp::tidy
