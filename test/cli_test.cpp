// turndown.cpp/test/cli_test.cpp
#include <gtest/gtest.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

#ifndef TURNDOWN_CLI_PATH
#error "TURNDOWN_CLI_PATH is not defined"
#endif

namespace {

std::string runCli(std::vector<std::string> const& args, std::string const& stdinData) {
    std::string tempPath;
    std::string cmd;
    if (!stdinData.empty()) {
        char tmpl[] = "/tmp/turndown_cli_inputXXXXXX";
        int fd = mkstemp(tmpl);
        if (fd == -1) {
            throw std::runtime_error("Failed to create temp file");
        }
        auto const written = write(fd, stdinData.data(), stdinData.size());
        (void)written;
        close(fd);
        tempPath = tmpl;
        cmd += "cat '";
        cmd += tempPath;
        cmd += "' | ";
    }

    cmd += "'";
    cmd += TURNDOWN_CLI_PATH;
    cmd += "'";
    for (auto const& a : args) {
        cmd.push_back(' ');
        cmd += a;
    }
    cmd += " 2>/dev/null";
    if (!tempPath.empty()) {
        cmd += "; rm '";
        cmd += tempPath;
        cmd += "'";
    }

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        if (!tempPath.empty()) {
            unlink(tempPath.c_str());
        }
        throw std::runtime_error(std::string("Failed to run CLI: ") + std::strerror(errno) + " (" + cmd + ")");
    }
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    pclose(pipe);
    return output;
}

TEST(CliTests, ReadsFromStdin) {
    std::string html = "<p>Hello</p>";
    std::string out = runCli({}, html);
    EXPECT_EQ(out, "Hello");
}

TEST(CliTests, ReadsFromFile) {
    char const* path = "/tmp/turndown_cli_file.html";
    {
        FILE* f = fopen(path, "w");
        ASSERT_NE(f, nullptr);
        fputs("<h1>Title</h1>", f);
        fclose(f);
    }
    std::string out = runCli({std::string("--file"), path, "--atx-headings"}, "");
    EXPECT_EQ(out, "# Title");
}

TEST(CliTests, AppliesOptions) {
    std::string html = "<pre><code>code</code></pre>";
    std::string out = runCli({"--fenced"}, html);
    EXPECT_EQ(out, "```\ncode\n```");
}

} // namespace

