// A minimal runtime plugin for turndown_cli used by tests.
//
// This is built as a shared library and loaded via `--plugin <path>`.

#include "../include/cli_plugin.h"
#include "../include/rules.h"
#include "../include/turndown.h"

#include <cstdint>
#include <string>

extern "C" TURNDOWN_CPP_CLI_PLUGIN_EXPORT std::uint32_t turndown_cpp_cli_plugin_abi_version() {
    return turndown_cpp::cli_plugin::kAbiVersion;
}

extern "C" TURNDOWN_CPP_CLI_PLUGIN_EXPORT char const* turndown_cpp_cli_plugin_name() {
    return "cli_sample_plugin_mark";
}

extern "C" TURNDOWN_CPP_CLI_PLUGIN_EXPORT void turndown_cpp_cli_register_plugin(turndown_cpp::TurndownService& svc) {
    turndown_cpp::Rule markRule;
    markRule.filter = [](turndown_cpp::dom::NodeView node, turndown_cpp::TurndownOptions const&) {
        return node.has_tag("mark");
    };
    markRule.replacement = [](std::string const& content,
                              turndown_cpp::dom::NodeView,
                              turndown_cpp::TurndownOptions const&) {
        return "==" + content + "==";
    };
    markRule.key = "mark";

    svc.addRule("mark", std::move(markRule));
}

