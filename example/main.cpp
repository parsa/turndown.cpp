// turndown.cpp/example/main.cpp
#include "dom_source.h"
#include "dom_adapter.h"
#include "turndown.h"
#include "utilities.h"

#include <functional>
#include <iostream>
#include <string>

using namespace turndown_cpp;

int main() {
    std::string html = R"html(
    <h1>Turndown Demo - Enhanced Options</h1>
    <hr/>
    <p>This <em>converts</em> <strong>HTML</strong> to Markdown. Also <code>someCode()</code>.</p>
    <p><a href="http://example.com">Reference link</a> and <a href="http://example.net">another link</a>.</p>
    <pre><code class="language-js">function hello() {  // Code block with leading whitespace
      console.log("Hello world!");
    }</code></pre>
    <p>Inline <code>code  with  spaces</code> example.</p>
    <ul><li>Item 1</li><li>Item 2</li></ul>
    <ol><li>Item A</li><li>Item B</li></ol>
)html";

    TurndownService service;
    TurndownOptions defaults = service.options();

    auto demo = [&](std::string const& title, std::function<void(TurndownOptions&)> configure) {
        service.configureOptions([&](TurndownOptions& opts) {
            opts = defaults;
            configure(opts);
        });
        std::cout << title << "\n" << service.turndown(html) << "\n\n";
    };

    demo("**HR Option: - - -**", [](TurndownOptions& opts) {
        opts.hr = "- - -";
    });
    demo("**HR Option: _ _ _**", [](TurndownOptions& opts) {
        opts.hr = "_ _ _";
    });

    demo("**Bullet List Marker: -**", [](TurndownOptions& opts) {
        opts.bulletListMarker = "-";
    });
    demo("**Bullet List Marker: +**", [](TurndownOptions& opts) {
        opts.bulletListMarker = "+";
    });

    demo("**Fence: ~~~**", [](TurndownOptions& opts) {
        opts.codeBlockStyle = "fenced";
        opts.fence = "~~~";
    });

    demo("**Em Delimiter: ***", [](TurndownOptions& opts) {
        opts.emDelimiter = "*";
    });

    demo("**Strong Delimiter: __**", [](TurndownOptions& opts) {
        opts.strongDelimiter = "__";
    });

    demo("**Link Reference Style: Collapsed**", [](TurndownOptions& opts) {
        opts.linkStyle = "referenced";
        opts.linkReferenceStyle = "collapsed";
    });

    demo("**BR Option: \\\\**", [](TurndownOptions& opts) {
        opts.br = "\\\\";
    });

    demo("**Preformatted Code: True**", [](TurndownOptions& opts) {
        opts.preformattedCode = true;
    });

    demo("**Escaping: Minimal**", [](TurndownOptions& opts) {
        opts.escapeFunction = minimalEscape;
    });

    demo("**Escaping: Custom (asterisk literal)**", [](TurndownOptions& opts) {
        opts.escapeFunction = [](std::string const& text) {
            std::string escapedText;
            for (char c : text) {
                if (c == '*') {
                    escapedText += "*";
                } else {
                    escapedText += c;
                }
            }
            return escapedText;
        };
    });

    service.configureOptions([&](TurndownOptions& opts) {
        opts = defaults;
    });

    service.use([](TurndownService& svc) {
        Rule markRule;
        markRule.filter = [](dom::NodeView node, TurndownOptions const&) {
            return node.has_tag("mark");
        };
        markRule.replacement = [](std::string const& content, dom::NodeView, TurndownOptions const&) {
            return "**" + content + "**";
        };
        svc.addRule("mark", std::move(markRule));
    });
    std::cout << "**Plugin Rule (mark -> bold)**\n" << service.turndown(html) << "\n\n";

    HtmlStringSource dom(html);
    std::cout << "**HtmlStringSource demo**\n" << service.turndown(dom) << "\n";

    return 0;
}