#include "turndown.h"

#include "cli_plugin.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

using namespace turndown_cpp;

namespace {

#if defined(_WIN32)
using LibraryHandle = HMODULE;

std::string lastLibraryError() {
    DWORD err = GetLastError();
    if (err == 0) return "unknown error";

    LPSTR buf = nullptr;
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buf),
        0,
        nullptr
    );
    std::string msg = (len && buf) ? std::string(buf, len) : "unknown error";
    if (buf) LocalFree(buf);
    return msg;
}

LibraryHandle loadLibrary(std::string const& path) {
    auto h = LoadLibraryA(path.c_str());
    if (!h) {
        throw std::runtime_error("LoadLibrary failed: " + lastLibraryError());
    }
    return h;
}

void* loadSymbol(LibraryHandle lib, char const* symbol) {
    FARPROC p = GetProcAddress(lib, symbol);
    if (!p) {
        throw std::runtime_error(std::string("GetProcAddress failed for '") + symbol + "': " + lastLibraryError());
    }
    return reinterpret_cast<void*>(p);
}

void* tryLoadSymbol(LibraryHandle lib, char const* symbol) {
    FARPROC p = GetProcAddress(lib, symbol);
    return p ? reinterpret_cast<void*>(p) : nullptr;
}

void unloadLibrary(LibraryHandle lib) {
    if (lib) FreeLibrary(lib);
}
#else
using LibraryHandle = void*;

std::string lastLibraryError() {
    char const* e = dlerror();
    return e ? std::string(e) : std::string("unknown error");
}

LibraryHandle loadLibrary(std::string const& path) {
    dlerror(); // clear
    void* h = dlopen(path.c_str(), RTLD_NOW);
    if (!h) {
        throw std::runtime_error("dlopen failed: " + lastLibraryError());
    }
    return h;
}

void* loadSymbol(LibraryHandle lib, char const* symbol) {
    dlerror(); // clear
    void* p = dlsym(lib, symbol);
    char const* err = dlerror();
    if (err) {
        throw std::runtime_error(std::string("dlsym failed for '") + symbol + "': " + err);
    }
    return p;
}

void* tryLoadSymbol(LibraryHandle lib, char const* symbol) {
    dlerror(); // clear
    void* p = dlsym(lib, symbol);
    dlerror(); // ignore errors
    return p;
}

void unloadLibrary(LibraryHandle lib) {
    if (lib) dlclose(lib);
}
#endif

struct LoadedPlugin {
    std::string path;
    std::string name;
    LibraryHandle handle = nullptr;

    LoadedPlugin() = default;
    LoadedPlugin(LoadedPlugin const&) = delete;
    LoadedPlugin& operator=(LoadedPlugin const&) = delete;

    LoadedPlugin(LoadedPlugin&& other) noexcept
        : path(std::move(other.path)), name(std::move(other.name)), handle(other.handle) {
        other.handle = nullptr;
    }
    LoadedPlugin& operator=(LoadedPlugin&& other) noexcept {
        if (this == &other) return *this;
        if (handle) unloadLibrary(handle);
        path = std::move(other.path);
        name = std::move(other.name);
        handle = other.handle;
        other.handle = nullptr;
        return *this;
    }

    ~LoadedPlugin() {
        if (handle) {
            unloadLibrary(handle);
            handle = nullptr;
        }
    }
};

LoadedPlugin loadAndRegisterPlugin(std::string const& path, TurndownService& service) {
    LoadedPlugin plugin;
    plugin.path = path;
    plugin.handle = loadLibrary(path);

    auto abiFn = reinterpret_cast<turndown_cpp::cli_plugin::AbiVersionFn>(
        loadSymbol(plugin.handle, turndown_cpp::cli_plugin::kAbiVersionSymbol)
    );
    std::uint32_t const abi = abiFn ? abiFn() : 0;
    if (abi != turndown_cpp::cli_plugin::kAbiVersion) {
        throw std::runtime_error(
            "Plugin ABI mismatch for '" + path + "': plugin=" + std::to_string(abi) +
            ", expected=" + std::to_string(turndown_cpp::cli_plugin::kAbiVersion)
        );
    }

    if (auto nameSym = tryLoadSymbol(plugin.handle, turndown_cpp::cli_plugin::kNameSymbol)) {
        auto nameFn = reinterpret_cast<turndown_cpp::cli_plugin::NameFn>(nameSym);
        if (nameFn) {
            if (auto n = nameFn()) {
                plugin.name = n;
            }
        }
    }
    if (plugin.name.empty()) {
        plugin.name = path;
    }

    auto regFn = reinterpret_cast<turndown_cpp::cli_plugin::RegisterFn>(
        loadSymbol(plugin.handle, turndown_cpp::cli_plugin::kRegisterSymbol)
    );
    if (!regFn) {
        throw std::runtime_error("Plugin missing register function: " + path);
    }

    try {
        regFn(service);
    } catch (std::exception const& e) {
        throw std::runtime_error("Plugin '" + plugin.name + "' threw exception: " + std::string(e.what()));
    } catch (...) {
        throw std::runtime_error("Plugin '" + plugin.name + "' threw unknown exception");
    }

    return plugin;
}

} // namespace

static void usage(char const* prog) {
    std::cerr << "Usage: " << prog << " [--file <path>] [--plugin <path>] [--atx-headings] [--fenced]\n"
              << "Reads HTML from stdin or --file and writes Markdown to stdout.\n"
              << "Options:\n"
              << "  --file <path>       Read HTML from file instead of stdin\n"
              << "  --plugin <path>     Load a runtime plugin (.so/.dylib/.dll). Can be repeated.\n"
              << "  --atx-headings      Use ATX headings (#)\n"
              << "  --fenced            Use fenced code blocks (```)\n"
              << "  --br <text>         Set line break marker (default: two spaces)\n"
              << "  --bullet <char>     Bullet list marker (*, -, +)\n"
              << "  --help              Show this help\n";
}

static std::string read_all(std::istream& in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    std::string filePath;
    turndown_cpp::TurndownOptions opts;
    std::vector<std::string> pluginPaths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "--file" && i + 1 < argc) {
            filePath = argv[++i];
        } else if (arg == "--plugin" && i + 1 < argc) {
            pluginPaths.emplace_back(argv[++i]);
        } else if (arg == "--atx-headings") {
            opts.headingStyle = "atx";
        } else if (arg == "--fenced") {
            opts.codeBlockStyle = "fenced";
        } else if (arg == "--br" && i + 1 < argc) {
            opts.br = argv[++i];
        } else if (arg == "--bullet" && i + 1 < argc) {
            opts.bulletListMarker = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    std::string html;
    if (!filePath.empty()) {
        std::ifstream f(filePath);
        if (!f) {
            std::cerr << "Failed to open " << filePath << "\n";
            return 1;
        }
        html = read_all(f);
    } else {
        html = read_all(std::cin);
    }

    std::vector<LoadedPlugin> loadedPlugins;
    TurndownService service(opts);
    for (auto const& p : pluginPaths) {
        try {
            loadedPlugins.push_back(loadAndRegisterPlugin(p, service));
        } catch (std::exception const& e) {
            std::cerr << "Failed to load plugin '" << p << "': " << e.what() << "\n";
            return 1;
        }
    }

    std::cout << service.turndown(html);
    return 0;
}

