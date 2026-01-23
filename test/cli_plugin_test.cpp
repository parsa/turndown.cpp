// turndown.cpp/test/cli_plugin_test.cpp
//
// Integration test for loading a runtime plugin via turndown_cli --plugin.

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef TURNDOWN_CLI_PATH
#define TURNDOWN_CLI_PATH ""
#endif

#ifndef TURNDOWN_PLUGIN_PATH
#define TURNDOWN_PLUGIN_PATH ""
#endif

namespace {

#if defined(_WIN32)
FILE* turndown_popen(char const* cmd, char const* mode) {
    return _popen(cmd, mode);
}

int turndown_pclose(FILE* f) {
    return _pclose(f);
}

std::string quoteShell(std::string const& s) {
    // Avoid quoting unless necessary: _popen ultimately runs via cmd.exe and
    // leading quotes can make command parsing brittle.
    if (s.find_first_of(" \t") == std::string::npos) {
        return s;
    }
    // cmd.exe quoting: minimal wrapper; paths/args we use here won't contain quotes.
    return "\"" + s + "\"";
}

std::string nullRedirect() {
    return "2>&1";
}

std::string catCommand(std::string const& path) {
    return "type " + quoteShell(path);
}
#else
FILE* turndown_popen(char const* cmd, char const* mode) {
    return popen(cmd, mode);
}

int turndown_pclose(FILE* f) {
    return pclose(f);
}

std::string quoteShell(std::string const& s) {
    // POSIX shell single-quote escaping: ' -> '"'"'
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\"'\"'";
        } else {
            out.push_back(c);
        }
    }
    out += "'";
    return out;
}

std::string nullRedirect() {
    return "2>&1";
}

std::string catCommand(std::string const& path) {
    return "cat " + quoteShell(path);
}
#endif

std::filesystem::path makeTempPath(std::string_view prefix, std::string_view suffix) {
    auto dir = std::filesystem::temp_directory_path();
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto filename = std::string(prefix) + std::to_string(now) + std::string(suffix);
    return dir / filename;
}

std::filesystem::path writeTempFile(std::string_view prefix, std::string_view suffix, std::string const& data) {
    auto path = makeTempPath(prefix, suffix);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to create temp file: " + path.string());
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return path;
}

std::string runCli(std::vector<std::string> const& args, std::string const& stdinData) {
    std::optional<std::filesystem::path> stdinPath;
    if (!stdinData.empty()) {
        stdinPath = writeTempFile("turndown_cli_input_", ".txt", stdinData);
    }

    std::string cliPath = TURNDOWN_CLI_PATH;
#if defined(_WIN32)
    // Prefer backslashes for cmd.exe.
    cliPath = std::filesystem::path(cliPath).make_preferred().string();
#endif

    std::string cmd;
    if (stdinPath) {
        cmd += catCommand(stdinPath->string());
        cmd += " | ";
    }

    cmd += quoteShell(cliPath);
    for (auto const& a : args) {
        cmd.push_back(' ');
        cmd += quoteShell(a);
    }
    cmd.push_back(' ');
    cmd += nullRedirect();

    FILE* pipe = turndown_popen(cmd.c_str(), "r");
    if (!pipe) {
        if (stdinPath) {
            std::error_code ec;
            std::filesystem::remove(*stdinPath, ec);
        }
        throw std::runtime_error(std::string("Failed to run CLI: ") + std::strerror(errno) + " (" + cmd + ")");
    }
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    int const exitCode = turndown_pclose(pipe);
    if (exitCode != 0) {
        if (stdinPath) {
            std::error_code ec;
            std::filesystem::remove(*stdinPath, ec);
        }
        throw std::runtime_error("CLI exited with code " + std::to_string(exitCode) + ". Output:\n" + output + "\nCommand:\n" + cmd);
    }

    if (stdinPath) {
        std::error_code ec;
        std::filesystem::remove(*stdinPath, ec);
    }
    return output;
}

} // namespace

TEST(CliPluginTests, LoadsRuntimePlugin) {
    std::string html = "<p>Hello <mark>world</mark></p>";
    std::string pluginPath = TURNDOWN_PLUGIN_PATH;
    ASSERT_FALSE(std::string(TURNDOWN_CLI_PATH).empty()) << "TURNDOWN_CLI_PATH is not set for this test target";
    ASSERT_FALSE(pluginPath.empty()) << "TURNDOWN_PLUGIN_PATH is not set for this test target";
    std::string out = runCli({"--plugin", pluginPath}, html);
    EXPECT_EQ(out, "Hello ==world==");
}

