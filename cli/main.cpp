#include "turndown.h"

#include "gumbo_adapter.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace turndown_cpp;

static void usage(char const* prog) {
    std::cerr << "Usage: " << prog << " [--file <path>] [--atx-headings] [--fenced]\n"
              << "Reads HTML from stdin or --file and writes Markdown to stdout.\n"
              << "Options:\n"
              << "  --file <path>       Read HTML from file instead of stdin\n"
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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "--file" && i + 1 < argc) {
            filePath = argv[++i];
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

    std::cout << turndown(html, opts);
    return 0;
}

