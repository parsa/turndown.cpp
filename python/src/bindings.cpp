#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "dom_adapter.h"
#include "turndown.h"

namespace py = pybind11;

namespace {

using turndown_cpp::TurndownOptions;
using turndown_cpp::TurndownService;
using turndown_cpp::dom::NodeType;
using turndown_cpp::dom::NodeView;

std::function<bool(NodeView, TurndownOptions const&)> wrap_filter(py::function fn) {
    return [fn = std::move(fn)](NodeView node, TurndownOptions const& options) -> bool {
        py::gil_scoped_acquire gil;
        py::object result = fn(node, options);
        return result.cast<bool>();
    };
}

std::function<std::string(std::string const&, NodeView, TurndownOptions const&)> wrap_replacement(py::function fn) {
    return [fn = std::move(fn)](std::string const& content, NodeView node, TurndownOptions const& options) -> std::string {
        py::gil_scoped_acquire gil;
        py::object result = fn(content, node, options);
        return result.cast<std::string>();
    };
}

void apply_option(TurndownOptions& opts, std::string const& key, py::handle value) {
    if (key == "headingStyle") {
        opts.headingStyle = value.cast<std::string>();
    } else if (key == "hr") {
        opts.hr = value.cast<std::string>();
    } else if (key == "bulletListMarker") {
        opts.bulletListMarker = value.cast<std::string>();
    } else if (key == "codeBlockStyle") {
        opts.codeBlockStyle = value.cast<std::string>();
    } else if (key == "fence") {
        opts.fence = value.cast<std::string>();
    } else if (key == "emDelimiter") {
        opts.emDelimiter = value.cast<std::string>();
    } else if (key == "strongDelimiter") {
        opts.strongDelimiter = value.cast<std::string>();
    } else if (key == "linkStyle") {
        opts.linkStyle = value.cast<std::string>();
    } else if (key == "linkReferenceStyle") {
        opts.linkReferenceStyle = value.cast<std::string>();
    } else if (key == "br") {
        opts.br = value.cast<std::string>();
    } else if (key == "preformattedCode") {
        opts.preformattedCode = value.cast<bool>();
    } else if (key == "keepTags") {
        opts.keepTags = value.cast<std::vector<std::string>>();
    } else {
        throw py::key_error("Unknown option: " + key);
    }
}

} // namespace

PYBIND11_MODULE(turndown, m) {
    m.doc() = "turndown.cpp Python bindings";

    py::enum_<NodeType>(m, "NodeType")
        .value("Document", NodeType::Document)
        .value("Element", NodeType::Element)
        .value("Text", NodeType::Text)
        .value("Whitespace", NodeType::Whitespace)
        .value("CData", NodeType::CData)
        .value("Comment", NodeType::Comment)
        .value("Unknown", NodeType::Unknown);

    py::class_<NodeView>(m, "NodeView", R"doc(
Backend-agnostic view over a DOM node.

Warning: NodeView values are only valid during a conversion call. Do not store
them beyond the scope of a rule/plugin callback.
)doc")
        .def(py::init<>())
        .def("__bool__", [](NodeView const& self) { return static_cast<bool>(self); })
        .def("type", &NodeView::type)
        .def("is_document", &NodeView::is_document)
        .def("is_element", &NodeView::is_element)
        .def("is_text_like", &NodeView::is_text_like)
        .def("parent", &NodeView::parent)
        .def("next_sibling", &NodeView::next_sibling)
        .def("first_child", &NodeView::first_child)
        .def("children", &NodeView::children)
        .def("tag_name", &NodeView::tag_name)
        .def("has_tag", &NodeView::has_tag)
        .def("find_child", &NodeView::find_child)
        .def("first_text_child", &NodeView::first_text_child)
        .def(
            "attribute",
            [](NodeView const& self, std::string const& name) {
                return std::string(self.attribute(name));
            },
            py::arg("name")
        )
        .def("has_attribute", &NodeView::has_attribute)
        .def("text_content", &NodeView::text_content)
        .def(
            "text",
            [](NodeView const& self) {
                return std::string(self.text());
            }
        );

    py::class_<TurndownOptions>(m, "TurndownOptions")
        .def(py::init<>())
        .def_readwrite("headingStyle", &TurndownOptions::headingStyle)
        .def_readwrite("hr", &TurndownOptions::hr)
        .def_readwrite("bulletListMarker", &TurndownOptions::bulletListMarker)
        .def_readwrite("codeBlockStyle", &TurndownOptions::codeBlockStyle)
        .def_readwrite("fence", &TurndownOptions::fence)
        .def_readwrite("emDelimiter", &TurndownOptions::emDelimiter)
        .def_readwrite("strongDelimiter", &TurndownOptions::strongDelimiter)
        .def_readwrite("linkStyle", &TurndownOptions::linkStyle)
        .def_readwrite("linkReferenceStyle", &TurndownOptions::linkReferenceStyle)
        .def_readwrite("br", &TurndownOptions::br)
        .def_readwrite("preformattedCode", &TurndownOptions::preformattedCode)
        .def_readwrite("keepTags", &TurndownOptions::keepTags);

    py::class_<TurndownService>(m, "TurndownService")
        .def(py::init<>())
        .def(py::init<TurndownOptions>(), py::arg("options"))
        .def(
            "turndown",
            [](TurndownService& self, std::string const& html) {
                py::gil_scoped_release release;
                return self.turndown(html);
            },
            py::arg("html")
        )
        .def(
            "add_rule",
            [](TurndownService& self, std::string const& key, py::function filter, py::function replacement) -> TurndownService& {
                turndown_cpp::Rule rule;
                rule.filter = wrap_filter(std::move(filter));
                rule.replacement = wrap_replacement(std::move(replacement));
                rule.append = nullptr;
                self.addRule(key, std::move(rule));
                return self;
            },
            py::arg("key"),
            py::arg("filter"),
            py::arg("replacement"),
            py::return_value_policy::reference_internal
        )
        .def(
            "use",
            [](TurndownService& self, py::function plugin) -> TurndownService& {
                plugin(py::cast(&self, py::return_value_policy::reference));
                return self;
            },
            py::arg("plugin"),
            py::return_value_policy::reference_internal
        )
        .def(
            "keep",
            [](TurndownService& self, py::function filter) -> TurndownService& {
                self.keep(wrap_filter(std::move(filter)));
                return self;
            },
            py::arg("filter"),
            py::return_value_policy::reference_internal
        )
        .def("keep", py::overload_cast<std::string const&>(&TurndownService::keep),
             py::arg("tag"),
             py::return_value_policy::reference_internal)
        .def("keep", py::overload_cast<std::vector<std::string> const&>(&TurndownService::keep),
             py::arg("tags"),
             py::return_value_policy::reference_internal)
        .def(
            "remove",
            [](TurndownService& self, py::function filter) -> TurndownService& {
                self.remove(wrap_filter(std::move(filter)));
                return self;
            },
            py::arg("filter"),
            py::return_value_policy::reference_internal
        )
        .def("remove", py::overload_cast<std::string const&>(&TurndownService::remove),
             py::arg("tag"),
             py::return_value_policy::reference_internal)
        .def("remove", py::overload_cast<std::vector<std::string> const&>(&TurndownService::remove),
             py::arg("tags"),
             py::return_value_policy::reference_internal)
        .def(
            "configure_options",
            [](TurndownService& self, py::kwargs kwargs) -> TurndownService& {
                self.configureOptions([&](TurndownOptions& opts) {
                    for (auto item : kwargs) {
                        std::string key = py::cast<std::string>(item.first);
                        apply_option(opts, key, item.second);
                    }
                });
                return self;
            },
            py::return_value_policy::reference_internal
        )
        .def(
            "options",
            py::overload_cast<>(&TurndownService::options),
            py::return_value_policy::reference_internal
        )
        .def(
            "options",
            py::overload_cast<>(&TurndownService::options, py::const_),
            py::return_value_policy::reference_internal
        );
}

