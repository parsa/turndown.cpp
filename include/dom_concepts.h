/// @file dom_concepts.h
/// @brief C++20 concepts for DOM abstraction
///
/// This file defines the concepts that any HTML parser backend must satisfy
/// to be usable with turndown_cpp. This provides type-safe, compile-time
/// checked polymorphism without runtime overhead.
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_CPP_DOM_CONCEPTS_H
#define TURNDOWN_CPP_DOM_CONCEPTS_H

#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace turndown_cpp::dom {

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

/// @brief Concept for a type that can be used as a node handle for hashing
template<typename T>
concept NodeHandleLike = requires(T const& h, T const& other) {
    { h.raw() } -> std::convertible_to<void const*>;
    { static_cast<bool>(h) } -> std::same_as<bool>;
    { h == other } -> std::same_as<bool>;
    { h != other } -> std::same_as<bool>;
};

/// @brief Concept for a range of child nodes
template<typename R, typename NodeT>
concept ChildRangeLike = std::ranges::input_range<R> && 
    std::same_as<std::ranges::range_value_t<R>, NodeT>;

/// @brief Concept for a range of attributes
template<typename R>
concept AttributeRangeLike = std::ranges::input_range<R> &&
    requires(std::ranges::range_value_t<R> const& attr) {
        { attr.name } -> std::convertible_to<std::string_view>;
        { attr.value } -> std::convertible_to<std::string_view>;
    };

/// @brief Concept for a DOM node view
/// 
/// A NodeView provides access to a node in the DOM tree.
/// Implementations must be lightweight (typically just a pointer wrapper).
template<typename T>
concept DOMNode = requires(T const& node, T& mut_node, T const& other, std::string_view sv, std::string const& s) {
    // Construction and identity
    { T{} };  // default constructible
    { static_cast<bool>(node) } -> std::same_as<bool>;
    { node == other } -> std::same_as<bool>;
    { node != other } -> std::same_as<bool>;
    
    // Type information
    { node.type() } -> std::same_as<NodeType>;
    { node.is_document() } -> std::same_as<bool>;
    { node.is_element() } -> std::same_as<bool>;
    { node.is_text_like() } -> std::same_as<bool>;
    
    // Navigation
    { node.parent() } -> std::same_as<T>;
    { node.next_sibling() } -> std::same_as<T>;
    { node.first_child() } -> std::same_as<T>;
    { node.children() } -> std::convertible_to<std::vector<T>>;
    
    // Element operations
    { node.tag_name() } -> std::convertible_to<std::string>;
    { node.has_tag(sv) } -> std::same_as<bool>;
    { node.find_child(sv) } -> std::same_as<T>;
    { node.first_text_child() } -> std::same_as<T>;
    
    // Attributes
    { node.attribute(sv) } -> std::convertible_to<std::string_view>;
    { node.has_attribute(sv) } -> std::same_as<bool>;
    
    // Text content (read)
    { node.text_content() } -> std::convertible_to<std::string>;
    { node.text() } -> std::convertible_to<std::string_view>;
    
    // Text content (write - requires mutable node)
    { mut_node.set_text(s) } -> std::same_as<void>;
};

/// @brief Concept for a DOM node view with handle support
template<typename T>
concept DOMNodeWithHandle = DOMNode<T> && requires(T const& node) {
    { node.handle() } -> NodeHandleLike;
    { node.child_range() };  // Must have child_range()
    { node.attribute_range() };  // Must have attribute_range()
};

/// @brief Concept for a parsed DOM document
template<typename D, typename NodeT>
concept DOMDocument = requires(D const& doc, D& mut_doc, D&& rval, std::string const& html) {
    // Parsing
    { D::parse(html) } -> std::same_as<D>;
    
    // Move semantics (documents own resources)
    { D{std::move(rval)} };
    { mut_doc = std::move(rval) } -> std::same_as<D&>;
    
    // No copying (documents own unique resources)
    requires !std::copy_constructible<D>;
    requires !std::assignable_from<D&, D const&>;
    
    // Validity check
    { static_cast<bool>(doc) } -> std::same_as<bool>;
    
    // Node access
    { doc.root() } -> std::same_as<NodeT>;
    { doc.body() } -> std::same_as<NodeT>;
};

/// @brief Helper to get the node handle type from a node type
template<DOMNodeWithHandle NodeT>
using HandleOf = decltype(std::declval<NodeT const&>().handle());

} // namespace turndown_cpp::dom

/// @brief Hash specialization for any NodeHandleLike type
template<turndown_cpp::dom::NodeHandleLike T>
struct std::hash<T> {
    std::size_t operator()(T const& handle) const noexcept {
        return std::hash<void const*>()(handle.raw());
    }
};

#endif // TURNDOWN_CPP_DOM_CONCEPTS_H

