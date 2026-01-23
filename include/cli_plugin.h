/// @file cli_plugin.h
/// @brief Runtime plugin ABI for turndown_cli
///
/// The C++ TurndownService supports "plugins" as in-process callables via
/// `TurndownService::use(...)`. The CLI additionally supports loading plugins
/// at runtime from shared libraries (.so/.dylib/.dll).
///
/// ABI notes:
/// - This is a C++ ABI boundary. Plugins must be built with a compatible C++
///   toolchain/standard library and against matching turndown_cpp headers.
/// - The exported function names are `extern "C"` to avoid name mangling.
///
/// A plugin shared library must export:
/// - `turndown_cpp_cli_plugin_abi_version()` returning `kAbiVersion`
/// - `turndown_cpp_cli_register_plugin(TurndownService&)` which registers
///    rules/options on the provided service
///
/// Optional export:
/// - `turndown_cpp_cli_plugin_name()` returning a short plugin name
///
/// @copyright The MIT License (MIT)
/// @copyright Copyright (c) 2025 Parsa Amini

#ifndef TURNDOWN_CPP_CLI_PLUGIN_H
#define TURNDOWN_CPP_CLI_PLUGIN_H

#include "turndown.h"

#include <cstdint>

// Export helper for plugin entrypoints.
#if defined(_WIN32)
    #define TURNDOWN_CPP_CLI_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define TURNDOWN_CPP_CLI_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace turndown_cpp::cli_plugin {

/// @brief Runtime plugin ABI version.
inline constexpr std::uint32_t kAbiVersion = 1;

/// @brief Required symbol names.
inline constexpr char kAbiVersionSymbol[] = "turndown_cpp_cli_plugin_abi_version";
inline constexpr char kRegisterSymbol[] = "turndown_cpp_cli_register_plugin";

/// @brief Optional symbol name.
inline constexpr char kNameSymbol[] = "turndown_cpp_cli_plugin_name";

using AbiVersionFn = std::uint32_t (*)();
using RegisterFn = void (*)(turndown_cpp::TurndownService&);
using NameFn = char const* (*)();

} // namespace turndown_cpp::cli_plugin

#endif // TURNDOWN_CPP_CLI_PLUGIN_H

