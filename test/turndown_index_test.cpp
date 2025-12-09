// File: turndown_index_test.cpp
// Comprehensive test suite ported from JavaScript test files

#include <gtest/gtest.h>

#ifndef TURNDOWN_HAS_BENCHMARK
#define TURNDOWN_HAS_BENCHMARK 0
#endif

#if TURNDOWN_HAS_BENCHMARK
#include <benchmark/benchmark.h>
#endif
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>

// Include the actual turndown implementation
#include "turndown.h"
#include "utilities.h"

using namespace turndown_cpp;

// Wrapper function to convert options map to TurndownOptions
std::string turndownPort(std::string const& htmlInput, std::map<std::string, std::string> const& options = {}) {
    turndown_cpp::TurndownOptions opts;
    
    // Apply options from map
    if (options.find("headingStyle") != options.end()) {
        opts.headingStyle = options.at("headingStyle");
    }
    if (options.find("hr") != options.end()) {
        opts.hr = options.at("hr");
    }
    if (options.find("bulletListMarker") != options.end()) {
        opts.bulletListMarker = options.at("bulletListMarker");
    }
    if (options.find("codeBlockStyle") != options.end()) {
        opts.codeBlockStyle = options.at("codeBlockStyle");
    }
    if (options.find("fence") != options.end()) {
        opts.fence = options.at("fence");
    }
    if (options.find("emDelimiter") != options.end()) {
        opts.emDelimiter = options.at("emDelimiter");
    }
    if (options.find("strongDelimiter") != options.end()) {
        opts.strongDelimiter = options.at("strongDelimiter");
    }
    if (options.find("linkStyle") != options.end()) {
        opts.linkStyle = options.at("linkStyle");
    }
    if (options.find("linkReferenceStyle") != options.end()) {
        opts.linkReferenceStyle = options.at("linkReferenceStyle");
    }
    if (options.find("br") != options.end()) {
        opts.br = options.at("br");
    }
    if (options.find("preformattedCode") != options.end()) {
        opts.preformattedCode = (options.at("preformattedCode") == "true");
    }
    
    return turndown(htmlInput, opts);
}

// A simple structure to hold each test case.
struct TestCase {
    std::string name;
    std::string html;
    std::string expected;
    std::map<std::string, std::string> options; // For test cases with data-options
    
    TestCase(std::string const& n, std::string const& h, std::string const& e, 
             std::map<std::string, std::string> const& opts = {})
        : name(n), html(h), expected(e), options(opts) {}
};

// Helper function to create multiline strings
std::string multiline(std::vector<std::string> const& lines) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        oss << lines[i];
        if (i < lines.size() - 1) oss << "\n";
    }
    return oss.str();
}

// All test cases from test/index.html, test/internals-test.js, and test/turndown-test.js
static std::vector<TestCase> ALL_TEST_CASES = {
    // Basic paragraph tests
    {"p", "<p>Lorem ipsum</p>", "Lorem ipsum"},
    {"multiple ps", multiline({"<p>Lorem</p>", "<p>ipsum</p>", "<p>sit</p>"}), 
     multiline({"Lorem", "", "ipsum", "", "sit"})},
    
    // Emphasis tests
    {"em", "<em>em element</em>", "_em element_"},
    {"i", "<i>i element</i>", "_i element_"},
    {"strong", "<strong>strong element</strong>", "**strong element**"},
    {"b", "<b>b element</b>", "**b element**"},
    
    // Code tests
    {"code", "<code>code element</code>", "`code element`"},
    {"code containing a backtick", "<code>There is a literal backtick (`) here</code>", 
     "``There is a literal backtick (`) here``"},
    {"code containing three or more backticks", 
     "<code>here are three ``` here are four ```` that's it</code>",
     "`here are three ``` here are four ```` that's it`"},
    {"code containing one or more backticks",
     "<code>here are three ``` here are four ```` here is one ` that's it</code>",
     "``here are three ``` here are four ```` here is one ` that's it``"},
    {"code starting with a backtick", "<code>`starting with a backtick</code>",
     "`` `starting with a backtick ``"},
    {"code containing markdown syntax", "<code>_emphasis_</code>", "`_emphasis_`"},
    {"code containing markdown syntax in a span", "<code><span>_emphasis_</span></code>", "`_emphasis_`"},
    
    // Heading tests
    {"h1", "<h1>Level One Heading</h1>", multiline({"Level One Heading", "================="})},
    {"escape = when used as heading", "===", "\\==="},
    {"not escaping = outside of a heading", "A sentence containing =", "A sentence containing ="},
    {"h1 as atx", "<h1>Level One Heading with ATX</h1>", "# Level One Heading with ATX",
     {{"headingStyle", "atx"}}},
    {"h2", "<h2>Level Two Heading</h2>", multiline({"Level Two Heading", "-----------------"})},
    {"h2 as atx", "<h2>Level Two Heading with ATX</h2>", "## Level Two Heading with ATX",
     {{"headingStyle", "atx"}}},
    {"h3", "<h3>Level Three Heading</h3>", "### Level Three Heading"},
    {"heading with child", "<h4>Level Four Heading with <code>child</code></h4>",
     "#### Level Four Heading with `child`"},
    {"invalid heading", "<h7>Level Seven Heading?</h7>", "Level Seven Heading?"},
    
    // Horizontal rule tests
    {"hr", "<hr>", "* * *"},
    {"hr with closing tag", "<hr></hr>", "* * *"},
    {"hr with option", "<hr>", "- - -", {{"hr", "- - -"}}},
    
    // Line break tests
    {"br", "More<br>after the break", multiline({"More  ", "after the break"})},
    {"br with visible line-ending", "More<br>after the break", multiline({"More\\", "after the break"}),
     {{"br", "\\"}}},
    
    // Image tests
    {"img with no alt", "<img src=\"http://example.com/logo.png\" />", "![](http://example.com/logo.png)"},
    {"img with relative src", "<img src=\"logo.png\">", "![](logo.png)"},
    {"img with alt", "<img src=\"logo.png\" alt=\"img with alt\">", "![img with alt](logo.png)"},
    {"img with no src", "<img>", ""},
    {"img with a new line in alt", 
     multiline({"<img src=\"logo.png\" alt=\"img with", "    alt\">"}),
     multiline({"![img with", "alt](logo.png)"})},
    {"img with more than one new line in alt",
     multiline({"<img src=\"logo.png\" alt=\"img with", "    ", "    alt\">"}),
     multiline({"![img with", "alt](logo.png)"})},
    {"img with new lines in title",
     multiline({"<img src=\"logo.png\" title=\"the", "    ", "    title\">"}),
     multiline({"![](logo.png \"the", "title\")"})},
    
    // Link tests
    {"a", "<a href=\"http://example.com\">An anchor</a>", "[An anchor](http://example.com)"},
    {"a with title", "<a href=\"http://example.com\" title=\"Title for link\">An anchor</a>",
     "[An anchor](http://example.com \"Title for link\")"},
    {"a with multiline title",
     multiline({"<a href=\"http://example.com\" title=\"Title for", "    ", "    link\">An anchor</a>"}),
     multiline({"[An anchor](http://example.com \"Title for", "link\")"})},
    {"a with quotes in title", "<a href=\"http://example.com\" title=\"&quot;hello&quot;\">An anchor</a>",
     "[An anchor](http://example.com \"\\\"hello\\\"\")"},
    {"a with parenthesis in query", "<a href=\"http://example.com?(query)\">An anchor</a>",
     "[An anchor](http://example.com?\\(query\\))"},
    {"a without a src", "<a id=\"about-anchor\">Anchor without a title</a>", "Anchor without a title"},
    {"a with a child", "<a href=\"http://example.com/code\">Some <code>code</code></a>",
     "[Some `code`](http://example.com/code)"},
    {"a reference", "<a href=\"http://example.com\">Reference link</a>",
     multiline({"[Reference link][1]", "", "[1]: http://example.com"}),
     {{"linkStyle", "referenced"}}},
    {"a reference with collapsed style", "<a href=\"http://example.com\">Reference link with collapsed style</a>",
     multiline({"[Reference link with collapsed style][]", "", 
                "[Reference link with collapsed style]: http://example.com"}),
     {{"linkStyle", "referenced"}, {"linkReferenceStyle", "collapsed"}}},
    {"a reference with shortcut style", "<a href=\"http://example.com\">Reference link with shortcut style</a>",
     multiline({"[Reference link with shortcut style]", "",
                "[Reference link with shortcut style]: http://example.com"}),
     {{"linkStyle", "referenced"}, {"linkReferenceStyle", "shortcut"}}},
    
    // Code block tests
    {"pre/code block",
     multiline({"<pre><code>def code_block", "  # 42 &lt; 9001", "  \"Hello world!\"", "end</code></pre>"}),
     multiline({"    def code_block", "      # 42 < 9001", "      \"Hello world!\"", "    end"})},
    {"multiple pre/code blocks",
     multiline({"<pre><code>def first_code_block", "  # 42 &lt; 9001", "  \"Hello world!\"", "end</code></pre>",
                "", "<p>next:</p>", "",
                "<pre><code>def second_code_block", "  # 42 &lt; 9001", "  \"Hello world!\"", "end</code></pre>"}),
     multiline({"    def first_code_block", "      # 42 < 9001", "      \"Hello world!\"", "    end", "",
                "next:", "",
                "    def second_code_block", "      # 42 < 9001", "      \"Hello world!\"", "    end"})},
    {"pre/code block with multiple new lines",
     multiline({"<div><pre><code>Multiple new lines", "", "", "should not be", "", "", "removed</code></pre></div>"}),
     multiline({"    Multiple new lines", "    ", "    ", "    should not be", "    ", "    ", "    removed"})},
    {"fenced pre/code block",
     multiline({"    <pre><code>def a_fenced_code block; end</code></pre>"}),
     multiline({"```", "def a_fenced_code block; end", "```"}),
     {{"codeBlockStyle", "fenced"}}},
    {"pre/code block fenced with ~",
     multiline({"    <pre><code>def a_fenced_code block; end</code></pre>"}),
     multiline({"~~~", "def a_fenced_code block; end", "~~~"}),
     {{"codeBlockStyle", "fenced"}, {"fence", "~~~"}}},
    {"escaping ~~~", "<pre>~~~ foo</pre>", "\\~~~ foo"},
    {"not escaping ~~~", "A sentence containing ~~~", "A sentence containing ~~~"},
    {"fenced pre/code block with language",
     multiline({"    <pre><code class=\"language-ruby\">def a_fenced_code block; end</code></pre>"}),
     multiline({"```ruby", "def a_fenced_code block; end", "```"}),
     {{"codeBlockStyle", "fenced"}}},
    {"empty pre does not throw error", "<pre></pre>", ""},
    
    // List tests
    {"ol",
     multiline({"<ol>", "      <li>Ordered list item 1</li>", "      <li>Ordered list item 2</li>",
                "      <li>Ordered list item 3</li>", "    </ol>"}),
     multiline({"1.  Ordered list item 1", "2.  Ordered list item 2", "3.  Ordered list item 3"})},
    {"ol with start",
     multiline({"<ol start=\"42\">", "      <li>Ordered list item 42</li>", "      <li>Ordered list item 43</li>",
                "      <li>Ordered list item 44</li>", "    </ol>"}),
     multiline({"42.  Ordered list item 42", "43.  Ordered list item 43", "44.  Ordered list item 44"})},
    {"list spacing",
     multiline({"<p>A paragraph.</p>", "    <ol>", "      <li>Ordered list item 1</li>",
                "      <li>Ordered list item 2</li>", "      <li>Ordered list item 3</li>", "    </ol>",
                "    <p>Another paragraph.</p>", "    <ul>", "      <li>Unordered list item 1</li>",
                "      <li>Unordered list item 2</li>", "      <li>Unordered list item 3</li>", "    </ul>"}),
     multiline({"A paragraph.", "", "1.  Ordered list item 1", "2.  Ordered list item 2", "3.  Ordered list item 3",
                "", "Another paragraph.", "", "*   Unordered list item 1", "*   Unordered list item 2",
                "*   Unordered list item 3"})},
    {"ul",
     multiline({"<ul>", "      <li>Unordered list item 1</li>", "      <li>Unordered list item 2</li>",
                "      <li>Unordered list item 3</li>", "    </ul>"}),
     multiline({"*   Unordered list item 1", "*   Unordered list item 2", "*   Unordered list item 3"})},
    {"ul with custom bullet",
     multiline({"<ul>", "      <li>Unordered list item 1</li>", "      <li>Unordered list item 2</li>",
                "      <li>Unordered list item 3</li>", "    </ul>"}),
     multiline({"-   Unordered list item 1", "-   Unordered list item 2", "-   Unordered list item 3"}),
     {{"bulletListMarker", "-"}}},
    {"ul with paragraph",
     multiline({"<ul>", "      <li><p>List item with paragraph</p></li>",
                "      <li>List item without paragraph</li>", "    </ul>"}),
     multiline({"*   List item with paragraph", "    ", "*   List item without paragraph"})},
    {"ol with paragraphs",
     multiline({"<ol>", "      <li>", "        <p>This is a paragraph in a list item.</p>",
                "        <p>This is a paragraph in the same list item as above.</p>", "      </li>", "      <li>",
                "        <p>A paragraph in a second list item.</p>", "      </li>", "    </ol>"}),
     multiline({"1.  This is a paragraph in a list item.", "    ", 
                "    This is a paragraph in the same list item as above.", "    ", 
                "2.  A paragraph in a second list item."})},
    {"nested uls",
     multiline({"<ul>", "      <li>This is a list item at root level</li>",
                "      <li>This is another item at root level</li>", "      <li>",
                "        <ul>", "          <li>This is a nested list item</li>",
                "          <li>This is another nested list item</li>", "          <li>",
                "            <ul>", "              <li>This is a deeply nested list item</li>",
                "              <li>This is another deeply nested list item</li>",
                "              <li>This is a third deeply nested list item</li>", "            </ul>",
                "          </li>", "        </ul>", "      </li>",
                "      <li>This is a third item at root level</li>", "    </ul>"}),
     multiline({"*   This is a list item at root level", "*   This is another item at root level",
                "*   *   This is a nested list item", "    *   This is another nested list item",
                "    *   *   This is a deeply nested list item",
                "        *   This is another deeply nested list item",
                "        *   This is a third deeply nested list item",
                "*   This is a third item at root level"})},
    {"nested ols and uls",
     multiline({"<ul>", "      <li>This is a list item at root level</li>",
                "      <li>This is another item at root level</li>", "      <li>",
                "        <ol>", "          <li>This is a nested list item</li>",
                "          <li>This is another nested list item</li>", "          <li>",
                "            <ul>", "              <li>This is a deeply nested list item</li>",
                "              <li>This is another deeply nested list item</li>",
                "              <li>This is a third deeply nested list item</li>", "            </ul>",
                "          </li>", "        </ol>", "      </li>",
                "      <li>This is a third item at root level</li>", "    </ul>"}),
     multiline({"*   This is a list item at root level", "*   This is another item at root level",
                "*   1.  This is a nested list item", "    2.  This is another nested list item",
                "    3.  *   This is a deeply nested list item",
                "        *   This is another deeply nested list item",
                "        *   This is a third deeply nested list item",
                "*   This is a third item at root level"})},
    {"ul with blockquote",
     multiline({"<ul>", "      <li>",
                "        <p>A list item with a blockquote:</p>", "        <blockquote>",
                "          <p>This is a blockquote inside a list item.</p>", "        </blockquote>",
                "      </li>", "    </ul>"}),
     multiline({"*   A list item with a blockquote:", "    ",
                "    > This is a blockquote inside a list item."})},
    
    // Blockquote tests
    {"blockquote",
     multiline({"<blockquote>", "      <p>This is a paragraph within a blockquote.</p>",
                "      <p>This is another paragraph within a blockquote.</p>", "    </blockquote>"}),
     multiline({"> This is a paragraph within a blockquote.", "> ", 
                "> This is another paragraph within a blockquote."})},
    {"nested blockquotes",
     multiline({"<blockquote>", "      <p>This is the first level of quoting.</p>", "      <blockquote>",
                "        <p>This is a paragraph in a nested blockquote.</p>", "      </blockquote>",
                "      <p>Back to the first level.</p>", "    </blockquote>"}),
     multiline({"> This is the first level of quoting.", "> ", 
                "> > This is a paragraph in a nested blockquote.", "> ", "> Back to the first level."})},
    {"html in blockquote",
     multiline({"<blockquote>", "      <h2>This is a header.</h2>", "      <ol>",
                "        <li>This is the first list item.</li>", "        <li>This is the second list item.</li>",
                "      </ol>", "      <p>A code block:</p>",
                "      <pre><code>return 1 &lt; 2 ? shell_exec('echo $input | $markdown_script') : 0;</code></pre>",
                "    </blockquote>"}),
     multiline({"> This is a header.", "> -----------------", "> ",
                "> 1.  This is the first list item.", "> 2.  This is the second list item.", "> ",
                "> A code block:", "> ",
                ">     return 1 < 2 ? shell_exec('echo $input | $markdown_script') : 0;"})},
    {"multiple divs",
     multiline({"<div>A div</div>", "    <div>Another div</div>"}),
     multiline({"A div", "", "Another div"})},
    
    // Escaping tests
    {"escaping backslashes", "backslash \\", "backslash \\\\"},
    {"escaping headings with #", "### This is not a heading", "\\### This is not a heading"},
    {"not escaping # outside of a heading", "#This is not # a heading", "#This is not # a heading"},
    {"escaping em markdown with *", "To add emphasis, surround text with *. For example: *this is emphasis*",
     "To add emphasis, surround text with \\*. For example: \\*this is emphasis\\*"},
    {"escaping em markdown with _", "To add emphasis, surround text with _. For example: _this is emphasis_",
     "To add emphasis, surround text with \\_. For example: \\_this is emphasis\\_"},
    {"not escaping within code",
     multiline({"<pre><code>def this_is_a_method; end;</code></pre>"}),
     "    def this_is_a_method; end;"},
    {"escaping strong markdown with *",
     "To add strong emphasis, surround text with **. For example: **this is strong**",
     "To add strong emphasis, surround text with \\*\\*. For example: \\*\\*this is strong\\*\\*"},
    {"escaping strong markdown with _",
     "To add strong emphasis, surround text with __. For example: __this is strong__",
     "To add strong emphasis, surround text with \\_\\_. For example: \\_\\_this is strong\\_\\_"},
    {"escaping hr markdown with *", "* * *", "\\* \\* \\*"},
    {"escaping hr markdown with -", "- - -", "\\- - -"},
    {"escaping hr markdown with _", "_ _ _", "\\_ \\_ \\_"},
    {"escaping hr markdown without spaces", "***", "\\*\\*\\*"},
    {"escaping hr markdown with more than 3 characters", "* * * * *", "\\* \\* \\* \\* \\*"},
    {"escaping ol markdown", "1984. by George Orwell", "1984\\. by George Orwell"},
    {"not escaping . outside of an ol", "1984.George Orwell wrote 1984.", "1984.George Orwell wrote 1984."},
    {"escaping ul markdown *", "* An unordered list item", "\\* An unordered list item"},
    {"escaping ul markdown -", "- An unordered list item", "\\- An unordered list item"},
    {"escaping ul markdown +", "+ An unordered list item", "\\+ An unordered list item"},
    {"not escaping - outside of a ul", "Hello-world, 45 - 3 is 42", "Hello-world, 45 - 3 is 42"},
    {"not escaping + outside of a ul", "+1 and another +", "+1 and another +"},
    {"escaping *", "You can use * for multiplication", "You can use \\* for multiplication"},
    {"escaping ** inside strong tags", "<strong>**test</strong>", "**\\*\\*test**"},
    {"escaping _ inside em tags", "<em>test_italics</em>", "_test\\_italics_"},
    {"escaping > as blockquote", "> Blockquote in markdown", "\\> Blockquote in markdown"},
    {"escaping > as blockquote without space", ">Blockquote in markdown", "\\>Blockquote in markdown"},
    {"not escaping > outside of a blockquote", "42 > 1", "42 > 1"},
    {"escaping code", "`not code`", "\\`not code\\`"},
    {"escaping []", "[This] is a sentence with brackets", "\\[This\\] is a sentence with brackets"},
    {"escaping [", "<a href=\"http://www.example.com\">c[iao</a>", "[c\\[iao](http://www.example.com)"},
    
    // Whitespace and formatting tests
    {"leading whitespace in heading", multiline({"<h3>", "    h3 with leading whitespace</h3>"}),
     "### h3 with leading whitespace"},
    {"non-markdown block elements",
     multiline({"Foo", "    <div>Bar</div>", "    Baz"}),
     multiline({"Foo", "", "Bar", "", "Baz"})},
    {"non-markdown inline elements", "Foo <span>Bar</span>", "Foo Bar"},
    {"blank inline elements", "Hello <em></em>world", "Hello world"},
    {"blank block elements", "Text before blank div … <div></div> text after blank div",
     multiline({"Text before blank div …", "", "text after blank div"})},
    {"blank inline element with br", "<strong><br></strong>", ""},
    {"whitespace between blocks",
     multiline({"<div><div>Content in a nested div</div></div>", "<div>Content in another div</div>"}),
     multiline({"Content in a nested div", "", "Content in another div"})},
    {"whitespace between inline elements",
     "<p>I <a href=\"http://example.com/need\">need</a> <a href=\"http://www.example.com/more\">more</a> spaces!</p>",
     "I [need](http://example.com/need) [more](http://www.example.com/more) spaces!"},
    {"whitespace in inline elements",
     "Text with no space after the period.<em> Text in em with leading/trailing spaces </em><strong>text in strong with trailing space </strong>",
     "Text with no space after the period. _Text in em with leading/trailing spaces_ **text in strong with trailing space**"},
    
    // Preformatted code tests (with preformattedCode option)
    {"preformatted code with leading whitespace",
     "Four spaces <code>    make an indented code block in Markdown</code>",
     "Four spaces `    make an indented code block in Markdown`",
     {{"preformattedCode", "true"}}},
    {"preformatted code with trailing whitespace",
     "<code>A line break  </code> <b> note the spaces</b>",
     "`A line break  ` **note the spaces**",
     {{"preformattedCode", "true"}}},
    {"preformatted code tightly surrounded",
     "<b>tight</b><code>code</code><b>wrap</b>",
     "**tight**`code`**wrap**",
     {{"preformattedCode", "true"}}},
    {"preformatted code loosely surrounded",
     "<b>not so tight </b><code>code</code><b> wrap</b>",
     "**not so tight** `code` **wrap**",
     {{"preformattedCode", "true"}}},
    {"preformatted code with newlines",
     multiline({"<code>", "", " nasty", "code", "", "</code>"}),
     "`    nasty code   `",
     {{"preformattedCode", "true"}}},
    
    // Triple tildes/ticks tests
    {"triple tildes inside code",
     multiline({"<pre><code>~~~", "Code", "~~~", "</code></pre>"}),
     multiline({"~~~~", "~~~", "Code", "~~~", "~~~~"}),
     {{"codeBlockStyle", "fenced"}, {"fence", "~~~"}}},
    {"triple ticks inside code",
     multiline({"<pre><code>```", "Code", "```", "</code></pre>"}),
     multiline({"````", "```", "Code", "```", "````"}),
     {{"codeBlockStyle", "fenced"}, {"fence", "```"}}},
    {"four ticks inside code",
     multiline({"<pre><code>````", "Code", "````", "</code></pre>"}),
     multiline({"`````", "````", "Code", "````", "`````"}),
     {{"codeBlockStyle", "fenced"}, {"fence", "```"}}},
    {"empty line in start/end of code block",
     multiline({"<pre><code>", "Code", "", "</code></pre>"}),
     multiline({"```", "", "Code", "", "```"}),
     {{"codeBlockStyle", "fenced"}, {"fence", "```"}}},
    
    // Non-breaking space tests
    {"text separated by a non-breaking space in an element",
     "<p>Foo<span>&nbsp;</span>Bar</p>", "Foo&nbsp;Bar"},
    {"text separated by ASCII and nonASCII space in an element",
     "<p>Foo<span>  &nbsp;  </span>Bar</p>", "Foo &nbsp; Bar"},
    {"list-like text with non-breaking spaces",
     "&nbsp;1. First<br>&nbsp;2. Second",
     multiline({"&nbsp;1. First  ", "&nbsp;2. Second"})},
    {"element with trailing nonASCII WS followed by nonWS",
     "<i>foo&nbsp;</i>bar",
     "_foo_&nbsp;bar"},
    {"element with trailing nonASCII WS followed by nonASCII WS",
     "<i>foo&nbsp;</i>&nbsp;bar",
     "_foo_&nbsp;&nbsp;bar"},
    {"element with trailing ASCII WS followed by nonASCII WS",
     "<i>foo </i>&nbsp;bar",
     "_foo_ &nbsp;bar"},
    {"element with trailing nonASCII WS followed by ASCII WS",
     "<i>foo&nbsp;</i> bar",
     "_foo_&nbsp; bar"},
    {"nonWS followed by element with leading nonASCII WS",
     "foo<i>&nbsp;bar</i>",
     "foo&nbsp;_bar_"},
    {"nonASCII WS followed by element with leading nonASCII WS",
     "foo&nbsp;<i>&nbsp;bar</i>",
     "foo&nbsp;&nbsp;_bar_"},
    {"nonASCII WS followed by element with leading ASCII WS",
     "foo&nbsp;<i> bar</i>",
     "foo&nbsp; _bar_"},
    {"ASCII WS followed by element with leading nonASCII WS",
     "foo <i>&nbsp;bar</i>",
     "foo &nbsp;_bar_"},
    
    // Comment tests
    {"comment", "<!-- comment -->", ""},
    {"pre/code with comment",
     multiline({"<pre ><code>Hello<!-- comment --> world</code></pre>"}),
     "    Hello world"},
    
    // Trailing whitespace tests
    {"trailing whitespace in li",
     multiline({"<ol>", "      <li>Chapter One", "        <ol>", "          <li>Section One</li>",
                "          <li>Section Two with trailing whitespace </li>",
                "          <li>Section Three with trailing whitespace </li>", "        </ol>", "      </li>",
                "      <li>Chapter Two</li>", "      <li>Chapter Three with trailing whitespace  </li>", "    </ol>"}),
     multiline({"1.  Chapter One", "    1.  Section One", "    2.  Section Two with trailing whitespace",
                "    3.  Section Three with trailing whitespace", "2.  Chapter Two",
                "3.  Chapter Three with trailing whitespace"})},
    
    // Complex formatting tests
    {"multilined and bizarre formatting",
     multiline({"<ul>", "      <li>", "        Indented li with leading/trailing newlines", "      </li>",
                "      <li>", "        <strong>Strong with trailing space inside li with leading/trailing whitespace </strong> </li>",
                "      <li>li without whitespace</li>",
                "      <li> Leading space, text, lots of whitespace …", "                          text", "      </li>",
                "    </ol>"}),
     multiline({"*   Indented li with leading/trailing newlines",
                "*   **Strong with trailing space inside li with leading/trailing whitespace**",
                "*   li without whitespace", "*   Leading space, text, lots of whitespace … text"})},
    {"whitespace in nested inline elements",
     "Text at root <strong><a href=\"http://www.example.com\">link text with trailing space in strong </a></strong>more text at root",
     "Text at root **[link text with trailing space in strong](http://www.example.com)** more text at root"},
    {"elements with a single void element",
     "<p><img src=\"http://example.com/logo.png\" /></p>",
     "![](http://example.com/logo.png)"},
    {"elements with a nested void element",
     "<p><span><img src=\"http://example.com/logo.png\" /></span></p>",
     "![](http://example.com/logo.png)"},
    {"text separated by a space in an element",
     "<p>Foo<span> </span>Bar</p>", "Foo Bar"},
    
    // Additional edge cases
    {"escaping multiple asterisks",
     "<p>* * ** It aims to be*</p>",
     "\\* \\* \\*\\* It aims to be\\*"},
    {"escaping delimiters around short words and numbers",
     "<p>_Really_? Is that what it _is_? A **2000** year-old computer?</p>",
     "\\_Really\\_? Is that what it \\_is\\_? A \\*\\*2000\\*\\* year-old computer?"},
    {"escaping * performance",
     "fasdf *883 asdf wer qweasd fsd asdf asdfaqwe rqwefrsdf",
     "fasdf \\*883 asdf wer qweasd fsd asdf asdfaqwe rqwefrsdf"},
    
    // Non-ASCII whitespace tests (from internals-test.js)
    // Note: These test the edgeWhitespace function specifically, which may need separate handling
    // For now, we include them as regular conversion tests
};

// For correctness testing, we can make a parameterized Google Test.
// Each test case compares turndownPort(html) to "expected".
class TurndownTests : public ::testing::TestWithParam<TestCase> {};

TEST_P(TurndownTests, ConvertsCorrectly)
{
    auto const& tc = GetParam();
    
#ifdef TURNDOWN_PARSER_BACKEND_TIDY
    // Skip tests that require whitespace preservation that tidy doesn't support
    static std::vector<std::string_view> const tidySkippedTests = {
        "preformatted code with leading whitespace",
        "preformatted code with trailing whitespace",
        "preformatted code with newlines"
    };
    for (auto const& skip : tidySkippedTests) {
        if (tc.name == skip) {
            GTEST_SKIP() << "Skipped: Tidy normalizes whitespace in inline code elements";
        }
    }
#endif
    
    std::string result = turndownPort(tc.html, tc.options);
    EXPECT_EQ(result, tc.expected) 
        << "Failure in test case: " << tc.name;
}

static std::string SanitizeName(std::string const& input, int index) {
    std::string out;
    out.reserve(input.size() + 8);
    bool lastUnderscore = false;
    for (char ch : input) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            out.push_back(ch);
            lastUnderscore = false;
        } else {
            if (!lastUnderscore) {
                out.push_back('_');
                lastUnderscore = true;
            }
        }
    }
    while (!out.empty() && out.front() == '_') out.erase(out.begin());
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) {
        out = "Case";
    }
    out += "_" + std::to_string(index);
    return out;
}

// Instantiate all tests in ALL_TEST_CASES with readable names:
INSTANTIATE_TEST_SUITE_P(
    AllTurndownTests,
    TurndownTests,
    ::testing::ValuesIn(ALL_TEST_CASES),
    [](::testing::TestParamInfo<TestCase> const& info) {
        return SanitizeName(info.param.name, info.index);
    });

#if TURNDOWN_HAS_BENCHMARK
// Now we also register benchmarks for each test case using Google Benchmark.
static void BM_Turndown(benchmark::State& state, TestCase const& testCase) 
{
    for (auto _ : state) {
        // We don't do correctness checks here. Just measure performance.
        auto output = turndownPort(testCase.html, testCase.options);
        benchmark::DoNotOptimize(output);
    }
}

static void RegisterAllBenchmarks()
{
    for (auto const& tc : ALL_TEST_CASES) {
        // We'll use the name as the test's "benchmark name".
        benchmark::RegisterBenchmark(tc.name.c_str(),
            [tc](benchmark::State& st){ BM_Turndown(st, tc); });
    }
}
#endif

// Typical main() for combined GTest + GBenchmark:
// Benchmarks are opt-in via --run_benchmarks to avoid running per-discovered test.
int main(int argc, char** argv)
{
#if TURNDOWN_HAS_BENCHMARK
    bool runBenchmarks = false;
#endif
    std::vector<char*> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--run_benchmarks") == 0) {
#if TURNDOWN_HAS_BENCHMARK
            runBenchmarks = true;
#else
            std::fprintf(stderr, "--run_benchmarks ignored: turndown_cpp built without benchmark support\n");
#endif
            continue;
        }
        args.push_back(argv[i]);
    }
    int gargc = static_cast<int>(args.size());

    // 1) Run Google Test
    testing::InitGoogleTest(&gargc, args.data());
    int testResult = RUN_ALL_TESTS();
    // During gtest_discover_tests (uses --gtest_list_tests) we just need test names.
    if (testing::GTEST_FLAG(list_tests)) {
        return testResult;
    }
    if (testResult != 0) {
        return testResult;
    }

    // Skip benchmarks unless explicitly requested and not running filtered gtests
#if TURNDOWN_HAS_BENCHMARK
    if (!runBenchmarks || !testing::GTEST_FLAG(filter).empty()) {
        return 0;
    }

    // 2) Run Google Benchmark
    int benchArgc = gargc;
    benchmark::Initialize(&benchArgc, args.data());
    RegisterAllBenchmarks();
    benchmark::RunSpecifiedBenchmarks();
#endif
    return 0;
}
