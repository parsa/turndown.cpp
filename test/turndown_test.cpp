// turndown.cpp/test/turndown_test.cpp
#include <gtest/gtest.h>

#include "turndown.h"
#include "rules.h"
#include "dom_source.h"
#include "gumbo_adapter.h"

#include <cctype>
#include <string>

using namespace turndown_cpp;

TEST(TurndownTest, Paragraph) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<p>Lorem ipsum</p>", options), "Lorem ipsum");
}

TEST(TurndownTest, MultipleParagraphs) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<p>Lorem</p><p>ipsum</p><p>sit</p>", options), "Lorem\n\nipsum\n\nsit");
}

TEST(TurndownTest, Em) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<em>em element</em>", options), "_em element_");
}

TEST(TurndownTest, Strong) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<strong>strong element</strong>", options), "**strong element**");
}

TEST(TurndownTest, Code) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<code>code element</code>", options), "`code element`");
}

TEST(TurndownTest, H1) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<h1>Level One Heading</h1>", options), "Level One Heading\n=================");
}

TEST(TurndownTest, H1Atx) {
    TurndownOptions options;
    options.headingStyle = "atx";
    ASSERT_EQ(turndown("<h1>Level One Heading with ATX</h1>", options), "# Level One Heading with ATX");
}

TEST(TurndownTest, H2) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<h2>Level Two Heading</h2>", options), "Level Two Heading\n-----------------");
}

TEST(TurndownTest, H2Atx) {
    TurndownOptions options;
    options.headingStyle = "atx";
    ASSERT_EQ(turndown("<h2>Level Two Heading with ATX</h2>", options), "## Level Two Heading with ATX");
}

TEST(TurndownTest, H3) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<h3>Level Three Heading</h3>", options), "### Level Three Heading");
}

TEST(TurndownTest, HeadingWithChild) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<h4>Level Four Heading with <code>child</code></h4>", options), "#### Level Four Heading with `child`");
}

TEST(TurndownTest, InvalidHeading) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<h7>Level Seven Heading?</h7>", options), "Level Seven Heading?");
}

TEST(TurndownTest, Hr) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<hr>", options), "* * *");
}

TEST(TurndownTest, HrOption) {
    TurndownOptions options;
    options.hr = "- - -";
    ASSERT_EQ(turndown("<hr>", options), "- - -");
}

TEST(TurndownTest, Br) {
    TurndownOptions options;
    ASSERT_EQ(turndown("More<br>after the break", options), "More  \nafter the break");
}

TEST(TurndownTest, ImgNoAlt) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<img src=\"http://example.com/logo.png\" />", options), "![](http://example.com/logo.png)");
}

TEST(TurndownTest, ImgWithAlt) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<img src=\"logo.png\" alt=\"img with alt\">", options), "![img with alt](logo.png)");
}

TEST(TurndownTest, ImgNoSrc) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<img>", options), "");
}

TEST(TurndownTest, A) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<a href=\"http://example.com\">An anchor</a>", options), "[An anchor](http://example.com)");
}

TEST(TurndownTest, AWithTitle) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<a href=\"http://example.com\" title=\"Title for link\">An anchor</a>", options), "[An anchor](http://example.com \"Title for link\")");
}

TEST(TurndownTest, AWithoutHref) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<a id=\"about-anchor\">Anchor without a title</a>", options), "Anchor without a title");
}

TEST(TurndownTest, AWithChild) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<a href=\"http://example.com/code\">Some <code>code</code></a>", options), "[Some `code`](http://example.com/code)");
}

TEST(TurndownTest, AReference) {
    TurndownOptions options;
    options.linkStyle = "referenced";
    ASSERT_EQ(turndown("<a href=\"http://example.com\">Reference link</a>", options), "[Reference link][1]\n\n[1]: http://example.com");
}

TEST(TurndownTest, PreCodeBlock) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<pre><code>def code_block\n  # 42 < 9001\n  \"Hello world!\"\nend</code></pre>", options), "    def code_block\n      # 42 < 9001\n      \"Hello world!\"\n    end");
}

TEST(TurndownTest, FencedPreCodeBlock) {
    TurndownOptions options;
    options.codeBlockStyle = "fenced";
    ASSERT_EQ(turndown("<pre><code>def a_fenced_code block; end</code></pre>", options), "```\ndef a_fenced_code block; end\n```");
}

TEST(TurndownTest, FencedPreCodeBlockTilde) {
    TurndownOptions options;
    options.codeBlockStyle = "fenced";
    options.fence = "~~~";
    ASSERT_EQ(turndown("<pre><code>def a_fenced_code block; end</code></pre>", options), "~~~\ndef a_fenced_code block; end\n~~~");
}

TEST(TurndownTest, FencedPreCodeBlockLanguage) {
    TurndownOptions options;
    options.codeBlockStyle = "fenced";
    ASSERT_EQ(turndown("<pre><code class=\"language-ruby\">def a_fenced_code block; end</code></pre>", options), "```ruby\ndef a_fenced_code block; end\n```");
}

TEST(TurndownTest, Ol) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<ol><li>Ordered list item 1</li><li>Ordered list item 2</li><li>Ordered list item 3</li></ol>", options), "1.  Ordered list item 1\n2.  Ordered list item 2\n3.  Ordered list item 3");
}

TEST(TurndownTest, Ul) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<ul><li>Unordered list item 1</li><li>Unordered list item 2</li><li>Unordered list item 3</li></ul>", options), "*   Unordered list item 1\n*   Unordered list item 2\n*   Unordered list item 3");
}

TEST(TurndownTest, UlCustomBullet) {
    TurndownOptions options;
    options.bulletListMarker = "-";
    ASSERT_EQ(turndown("<ul><li>Unordered list item 1</li><li>Unordered list item 2</li><li>Unordered list item 3</li></ul>", options), "-   Unordered list item 1\n-   Unordered list item 2\n-   Unordered list item 3");
}

TEST(TurndownTest, Blockquote) {
    TurndownOptions options;
    ASSERT_EQ(turndown("<blockquote><p>This is a paragraph within a blockquote.</p><p>This is another paragraph within a blockquote.</p></blockquote>", options), "> This is a paragraph within a blockquote.\n> \n> This is another paragraph within a blockquote.");
}

TEST(TurndownServiceTest, PluginAddsRule) {
    TurndownService service;
    service.use([](TurndownService& svc) {
        Rule markRule;
        markRule.filter = [](gumbo::NodeView node, TurndownOptions const&) {
            return node.has_tag("mark");
        };
        markRule.replacement = [](std::string const& content, gumbo::NodeView, TurndownOptions const&) {
            return "==" + content + "==";
        };
        svc.addRule("mark", std::move(markRule));
    });
    EXPECT_EQ(service.turndown("<p>Hello <mark>world</mark></p>"), "Hello ==world==");
}

TEST(TurndownServiceTest, KeepPredicateUsesOuterHtml) {
    TurndownService service;
    service.keep([](gumbo::NodeView node, TurndownOptions const&) {
        return node.tag_name() == "custom";
    });

    auto result = service.turndown("<p><custom data-id=\"1\">special</custom> data</p>");
    EXPECT_NE(result.find("<custom data-id=\"1\">special</custom>"), std::string::npos);
}

TEST(TurndownServiceTest, RemovePredicateStripsNodes) {
    TurndownService service;
    service.remove([](gumbo::NodeView node, TurndownOptions const&) {
        return node.has_tag("script");
    });

    EXPECT_EQ(service.turndown("<p>safe<script>alert('x')</script>content</p>"), "safecontent");
}

TEST(TurndownServiceTest, RuleFactoryBeforeDefaultsOverridesParagraph) {
    TurndownService service;
    service.registerRuleFactory([](Rules& rules) {
        Rule paragraphRule;
        paragraphRule.filter = [](gumbo::NodeView node, TurndownOptions const&) {
            return node.has_tag("p");
        };
        paragraphRule.replacement = [](std::string const& content, gumbo::NodeView, TurndownOptions const&) {
            return "[[" + content + "]]";
        };
        rules.addRule("wrappedParagraph", std::move(paragraphRule));
    }, TurndownService::RulePlacement::BeforeDefaults);

    EXPECT_EQ(service.turndown("<p>custom</p>"), "[[custom]]");
}

TEST(TurndownServiceTest, GumboNodeSourceAllowsExistingTree) {
    TurndownService service;
    std::string html = "<ul><li>A</li><li>B</li></ul>";
    gumbo::Document doc = gumbo::Document::parse(html);
    GumboNodeSource source(doc.root());
    auto markdown = service.turndown(source);
    EXPECT_NE(markdown.find("*   A"), std::string::npos);
    EXPECT_NE(markdown.find("*   B"), std::string::npos);
}

// More tests to be added based on Javascript tests...


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}