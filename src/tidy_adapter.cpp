#include "gumbo_adapter.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <tidy.h>
#include <tidybuffio.h>

namespace turndown_cpp::gumbo {

namespace {

// TidyNode is const void*, so we need const_cast for void* storage
inline TidyNode as_tidy_node(void* ptr) {
    return static_cast<TidyNode>(ptr);
}

inline TidyDoc as_tidy_doc(void* ptr) {
    return static_cast<TidyDoc>(ptr);
}

inline void* to_void_ptr(TidyNode node) {
    return const_cast<void*>(static_cast<const void*>(node));
}

inline void* to_void_ptr(TidyDoc doc) {
    return const_cast<void*>(static_cast<const void*>(doc));
}

// Thread-local storage for the current document context
// Needed because some tidy functions require the document
thread_local TidyDoc tl_current_doc = nullptr;

// Thread-local storage for cached text content from text nodes
// We cache this because tidyNodeGetText requires a buffer allocation
thread_local std::unordered_map<void*, std::string> tl_text_cache;

void set_current_doc(TidyDoc doc) {
    tl_current_doc = doc;
    tl_text_cache.clear();
}

TidyDoc get_current_doc() {
    return tl_current_doc;
}

// Decode common HTML entities that tidy encodes in text output
std::string decode_html_entities(std::string const& input) {
    std::string result;
    result.reserve(input.size());
    
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '&') {
            // Check for common entities
            if (input.compare(i, 4, "&lt;") == 0) {
                result += '<';
                i += 3;
            } else if (input.compare(i, 4, "&gt;") == 0) {
                result += '>';
                i += 3;
            } else if (input.compare(i, 5, "&amp;") == 0) {
                result += '&';
                i += 4;
            } else if (input.compare(i, 6, "&quot;") == 0) {
                result += '"';
                i += 5;
            } else if (input.compare(i, 6, "&apos;") == 0) {
                result += '\'';
                i += 5;
            } else if (input.compare(i, 6, "&nbsp;") == 0) {
                result += "\xC2\xA0"; // UTF-8 NBSP
                i += 5;
            } else {
                result += input[i];
            }
        } else {
            result += input[i];
        }
    }
    return result;
}

std::string_view get_cached_text(void* node_ptr, TidyDoc doc, TidyNode node) {
    auto it = tl_text_cache.find(node_ptr);
    if (it != tl_text_cache.end()) {
        return it->second;
    }
    
    TidyBuffer buf = {0};
    if (tidyNodeGetText(doc, node, &buf) && buf.bp) {
        std::string text = decode_html_entities(reinterpret_cast<char*>(buf.bp));
        tidyBufFree(&buf);
        auto [insert_it, _] = tl_text_cache.emplace(node_ptr, std::move(text));
        return insert_it->second;
    }
    tidyBufFree(&buf);
    return {};
}

NodeType to_node_type(TidyNode node) {
    if (!node) return NodeType::Unknown;
    TidyNodeType type = tidyNodeGetType(node);
    switch (type) {
        case TidyNode_Root:
            return NodeType::Document;
        case TidyNode_Start:
        case TidyNode_End:
        case TidyNode_StartEnd:
            return NodeType::Element;
        case TidyNode_Text:
            return NodeType::Text;
        case TidyNode_CDATA:
            return NodeType::CData;
        case TidyNode_Comment:
            return NodeType::Comment;
        default:
            return NodeType::Unknown;
    }
}

std::size_t count_children(TidyNode node) {
    std::size_t count = 0;
    for (TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        ++count;
    }
    return count;
}

TidyNode get_child_at_index(TidyNode node, std::size_t index) {
    std::size_t i = 0;
    for (TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        if (i == index) return child;
        ++i;
    }
    return nullptr;
}

std::size_t count_attributes(TidyNode node) {
    std::size_t count = 0;
    for (TidyAttr attr = tidyAttrFirst(node); attr; attr = tidyAttrNext(attr)) {
        ++count;
    }
    return count;
}

TidyAttr get_attr_at_index(TidyNode node, std::size_t index) {
    std::size_t i = 0;
    for (TidyAttr attr = tidyAttrFirst(node); attr; attr = tidyAttrNext(attr)) {
        if (i == index) return attr;
        ++i;
    }
    return nullptr;
}

void collect_text_impl(TidyDoc doc, TidyNode node, std::ostringstream& out) {
    if (!node) return;
    TidyNodeType type = tidyNodeGetType(node);
    
    if (type == TidyNode_Text || type == TidyNode_CDATA) {
        TidyBuffer buf = {0};
        if (tidyNodeGetText(doc, node, &buf)) {
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

} // namespace

// --- NodeView implementation ---

NodeView::NodeView(void* node) : node_(node) {}

NodeType NodeView::type() const {
    return to_node_type(as_tidy_node(node_));
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
    TidyNode node = as_tidy_node(node_);
    return node ? NodeView(to_void_ptr(tidyGetParent(node))) : NodeView{};
}

NodeView NodeView::first_child() const {
    TidyNode node = as_tidy_node(node_);
    return node ? NodeView(to_void_ptr(tidyGetChild(node))) : NodeView{};
}

NodeView NodeView::next_sibling() const {
    TidyNode node = as_tidy_node(node_);
    return node ? NodeView(to_void_ptr(tidyGetNext(node))) : NodeView{};
}

std::vector<NodeView> NodeView::children() const {
    std::vector<NodeView> result;
    TidyNode node = as_tidy_node(node_);
    if (!node) return result;
    
    std::size_t count = count_children(node);
    result.reserve(count);
    for (TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
        result.emplace_back(to_void_ptr(child));
    }
    return result;
}

ChildRange NodeView::child_range() const {
    return ChildRange(node_);
}

std::string NodeView::tag_name() const {
    return lookup_tag_name(as_tidy_node(node_));
}

bool NodeView::has_tag(std::string_view tag) const {
    return node_ && is_element() && lookup_tag_name(as_tidy_node(node_)) == tag;
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
    if (!doc) return "";
    std::ostringstream out;
    collect_text_impl(doc, as_tidy_node(node_), out);
    return out.str();
}

std::string_view NodeView::attribute(std::string_view name) const {
    TidyNode node = as_tidy_node(node_);
    if (!node) return {};
    
    for (TidyAttr attr = tidyAttrFirst(node); attr; attr = tidyAttrNext(attr)) {
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
    if (!node_) return {};
    TidyNode node = as_tidy_node(node_);
    TidyNodeType type = tidyNodeGetType(node);
    
    // Only return text for text-like nodes
    if (type != TidyNode_Text && type != TidyNode_CDATA && type != TidyNode_Comment) {
        return {};
    }
    
    TidyDoc doc = get_current_doc();
    if (!doc) return {};
    
    return get_cached_text(node_, doc, node);
}

AttributeRange NodeView::attribute_range() const {
    return AttributeRange(node_);
}

// --- Document implementation ---

namespace {

struct TidyDocWrapper {
    TidyDoc doc = nullptr;
    
    ~TidyDocWrapper() {
        if (doc) {
            tidyRelease(doc);
        }
    }
};

} // namespace

Document::Document(void* output) : output_(output) {}

Document Document::parse(std::string const& html) {
    TidyDoc doc = tidyCreate();
    if (!doc) return Document(nullptr);
    
    // Configure tidy for parsing (not cleaning)
    tidyOptSetBool(doc, TidyShowWarnings, no);
    tidyOptSetBool(doc, TidyShowInfo, no);
    tidyOptSetBool(doc, TidyQuiet, yes);
    tidyOptSetBool(doc, TidyForceOutput, yes);
    tidyOptSetInt(doc, TidyShowErrors, 0);
    
    // Parse the HTML
    if (tidyParseString(doc, html.c_str()) < 0) {
        tidyRelease(doc);
        return Document(nullptr);
    }
    
    // Clean and repair (required for proper DOM structure)
    tidyCleanAndRepair(doc);
    
    // Set as current doc for text extraction
    set_current_doc(doc);
    
    return Document(to_void_ptr(doc));
}

Document::Document(Document&& other) noexcept : output_(other.output_) {
    other.output_ = nullptr;
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    if (output_) {
        tidyRelease(as_tidy_doc(output_));
    }
    output_ = other.output_;
    other.output_ = nullptr;
    return *this;
}

Document::~Document() {
    if (output_) {
        if (get_current_doc() == as_tidy_doc(output_)) {
            set_current_doc(nullptr);
        }
        tidyRelease(as_tidy_doc(output_));
        output_ = nullptr;
    }
}

NodeView Document::root() const {
    TidyDoc doc = as_tidy_doc(output_);
    if (!doc) return {};
    // Set current doc for text extraction
    set_current_doc(doc);
    return NodeView(to_void_ptr(tidyGetRoot(doc)));
}

NodeView Document::document() const {
    return root();
}

NodeView Document::html() const {
    TidyDoc doc = as_tidy_doc(output_);
    if (!doc) return {};
    set_current_doc(doc);
    return NodeView(to_void_ptr(tidyGetHtml(doc)));
}

NodeView Document::body() const {
    TidyDoc doc = as_tidy_doc(output_);
    if (!doc) return {};
    set_current_doc(doc);
    return NodeView(to_void_ptr(tidyGetBody(doc)));
}

// --- ChildRange implementation ---

ChildRange::ChildRange(void* parent) : parent_(parent) {
    if (parent) {
        count_ = count_children(as_tidy_node(parent));
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
    return NodeView(to_void_ptr(get_child_at_index(as_tidy_node(parent_), index_)));
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
    if (node) {
        count_ = count_attributes(as_tidy_node(node));
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
    TidyAttr attr = get_attr_at_index(as_tidy_node(node_), index_);
    if (!attr) return {};
    
    ctmbstr name = tidyAttrName(attr);
    ctmbstr value = tidyAttrValue(attr);
    return AttributeView{
        name ? std::string_view(name) : std::string_view{},
        value ? std::string_view(value) : std::string_view{}
    };
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

