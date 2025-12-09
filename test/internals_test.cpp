// turndown.cpp/test/internals_test.cpp
#include <gtest/gtest.h>

#include "../include/node.h"

#include "dom_adapter.h"

#include <string>
#include <utility>
#include <vector>

using namespace turndown_cpp;

TEST(InternalsTest, EdgeWhitespaceDetection) {
    // This test verifies edge whitespace detection which requires exact whitespace
    // preservation in text nodes. The tidy backend normalizes whitespace during
    // parsing, so this test only applies to the gumbo backend.
#ifdef TURNDOWN_PARSER_BACKEND_TIDY
    GTEST_SKIP() << "Skipped: Tidy normalizes whitespace in text nodes";
#endif
    
    auto ews = [](std::string const& leadingAscii, std::string const& leadingNonAscii, std::string const& trailingNonAscii, std::string const& trailingAscii) {
        FlankingWhitespace result;
        result.leading = leadingAscii + leadingNonAscii;
        result.trailing = trailingNonAscii + trailingAscii;
        return result;
    };

    std::vector<std::pair<std::string, FlankingWhitespace>> testCases = {
        {" \r\n\tHELLO WORLD \r\n\t", ews(" \r\n\t", "", "", " \r\n\t")},
        {" \r\nH \r\n", ews(" \r\n", "", "", " \r\n")},
        {" \r\n\xC2\xA0 \r\nHELLO \r\nWORLD \r\n\xC2\xA0 \r\n", ews(" \r\n", "\xC2\xA0 \r\n", " \r\n\xC2\xA0", " \r\n")},
        {"\xC2\xA0 \r\nHELLO \r\nWORLD \r\n\xC2\xA0", ews("", "\xC2\xA0 \r\n", " \r\n\xC2\xA0", "")},
        {"\xC2\xA0 \r\n\xC2\xA0", ews("", "\xC2\xA0 \r\n\xC2\xA0", "", "")},
        {" \r\n\xC2\xA0 \r\n", ews(" \r\n", "\xC2\xA0 \r\n", "", "")},
        {" \r\n\xC2\xA0", ews(" \r\n", "\xC2\xA0", "", "")},
        {"HELLO WORLD", ews("", "", "", "")},
        {"", ews("", "", "", "")},
        {"TEST" + std::string(32768, ' ') + "END", ews("", "", "", "")} // performance check
    };

    for (auto const& testCase : testCases) {
        // Wrap text in a paragraph to create a proper HTML structure
        std::string html = "<p>" + testCase.first + "</p>";
        dom::Document document = dom::Document::parse(html);
        dom::NodeView bodyNode = document.body();
        ASSERT_TRUE(bodyNode);

        dom::NodeView pElement = bodyNode.find_child("p");
        ASSERT_TRUE(pElement);

        dom::NodeView textNode = pElement.first_text_child();
        if (!textNode) {
            continue;
        }

        FlankingWhitespace result = flankingWhitespace(textNode, false);
        auto normalize = [](std::string value) {
            std::string normalized;
            normalized.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i) {
                char c = value[i];
                if (c == '\r') {
                    if (i + 1 < value.size() && value[i + 1] == '\n') {
                        normalized.push_back('\n');
                        ++i;
                        continue;
                    }
                    c = '\n';
                }
                normalized.push_back(c);
            }
            std::string restored;
            restored.reserve(normalized.size());
            for (std::size_t i = 0; i < normalized.size();) {
                if (normalized.compare(i, 6, "&nbsp;") == 0) {
                    restored.append("\xC2\xA0");
                    i += 6;
                    continue;
                }
                restored.push_back(normalized[i]);
                ++i;
            }
            return restored;
        };
        ASSERT_EQ(normalize(result.leading), normalize(testCase.second.leading)) << "Input: '" << testCase.first << "'";
        ASSERT_EQ(normalize(result.trailing), normalize(testCase.second.trailing)) << "Input: '" << testCase.first << "'";
        
    }
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}