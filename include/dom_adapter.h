/// @file dom_adapter.h
/// @brief Unified DOM adapter that selects the backend at compile time
///
/// This header provides a unified interface to the DOM parser backend.
/// The actual backend (Gumbo, Tidy, or Lexbor) is selected at compile time via
/// the TURNDOWN_PARSER_BACKEND_* macros.
///
/// Usage:
/// @code
/// #include <dom_adapter.h>
/// 
/// turndown_cpp::dom::Document doc = turndown_cpp::dom::Document::parse("<p>Hello</p>");
/// turndown_cpp::dom::NodeView body = doc.body();
/// @endcode
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_CPP_DOM_ADAPTER_H
#define TURNDOWN_CPP_DOM_ADAPTER_H

#include "dom_concepts.h"

#if defined(TURNDOWN_PARSER_BACKEND_LEXBOR)
    #include "lexbor_adapter.h"
#elif defined(TURNDOWN_PARSER_BACKEND_TIDY)
    #include "tidy_adapter.h"
#else
    // Default to Gumbo
    #include "gumbo_adapter.h"
#endif

namespace turndown_cpp::dom {

#if defined(TURNDOWN_PARSER_BACKEND_LEXBOR)
    /// @brief The DOM parser backend namespace
    namespace backend = turndown_cpp::lexbor;
#elif defined(TURNDOWN_PARSER_BACKEND_TIDY)
    /// @brief The DOM parser backend namespace
    namespace backend = turndown_cpp::tidy;
#else
    /// @brief The DOM parser backend namespace
    namespace backend = turndown_cpp::gumbo;
#endif

/// @brief NodeView type from the selected backend
using NodeView = backend::NodeView;

/// @brief Document type from the selected backend
using Document = backend::Document;

/// @brief NodeHandle type from the selected backend
using NodeHandle = backend::NodeHandle;

/// @brief ChildRange type from the selected backend
using ChildRange = backend::ChildRange;

/// @brief AttributeRange type from the selected backend
using AttributeRange = backend::AttributeRange;

// Verify our types satisfy the concepts
static_assert(DOMNode<NodeView>, "NodeView must satisfy DOMNode concept");
static_assert(DOMNodeWithHandle<NodeView>, "NodeView must satisfy DOMNodeWithHandle concept");
static_assert(DOMDocument<Document, NodeView>, "Document must satisfy DOMDocument concept");
static_assert(NodeHandleLike<NodeHandle>, "NodeHandle must satisfy NodeHandleLike concept");

} // namespace turndown_cpp::dom

#endif // TURNDOWN_CPP_DOM_ADAPTER_H

