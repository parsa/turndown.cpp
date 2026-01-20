# turndown.cpp

Convert HTML into Markdown with C++.

## About

This is a C++ port of the JavaScript [Turndown](https://github.com/mixmark-io/turndown) library. It provides the same functionality and options as the original, leveraging the [Gumbo HTML5 parser](https://github.com/google/gumbo-parser) for DOM parsing.

**Note:** This port is fully functional but ongoing development continues. Contributions and feedback are welcome.

## Features

- Core HTML to Markdown conversion
- Full CommonMark rule support (paragraphs, headings, lists, blockquotes, code blocks, links, images, emphasis, strong)
- Setext and ATX heading styles
- Options for horizontal rules, bullet list markers, code block styles, emphasis/strong delimiters
- Inline and referenced link styles
- Whitespace collapsing (matching browser behavior)
- Customizable Markdown character escaping
- Plugin system for custom rules
- Keep/remove filters for HTML elements
- Command-line interface (CLI) tool

## Installation

### Prerequisites

- C++ compiler with C++20 support (e.g., g++ 10+, clang++ 10+)
- CMake (version 3.16 or higher)
- Gumbo HTML5 parser library
- Google Test (optional, for running tests)
- Doxygen (optional, for building documentation)

### Build Steps

```bash
# Clone the repository
git clone [repository URL]
cd turndown.cpp

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .

# Run tests (optional)
ctest

# Install (optional)
cmake --install .
```

If CMake cannot find Gumbo automatically:
```bash
cmake -DGUMBO_INCLUDE_DIR=/path/to/gumbo/include \
      -DGUMBO_LIBRARY=/path/to/gumbo/lib/libgumbo.so ..
```

### Building Documentation

To build the API documentation using Doxygen:

```bash
# Configure with documentation enabled
cmake -DTURNDOWN_BUILD_DOCS=ON ..

# Build documentation
cmake --build . --target docs

# Documentation will be in build/docs/html/
# Open build/docs/html/index.html in a browser
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `TURNDOWN_BUILD_CLI` | ON | Build the turndown CLI frontend |
| `TURNDOWN_BUILD_EXAMPLES` | ON | Build the example executable |
| `TURNDOWN_BUILD_TESTING` | ON | Build the test suite |
| `TURNDOWN_BUILD_BENCHMARKS` | ON | Build Google Benchmark integration |
| `TURNDOWN_BUILD_DOCS` | OFF | Build Doxygen documentation |
| `TURNDOWN_PARSER_BACKEND` | gumbo | HTML parser backend (`gumbo`, `tidy`, or `lexbor`) |
| `TURNDOWN_PREFER_STATIC` | OFF | Prefer static libraries for parser backend (for release builds) |

### Parser Backends

The library supports multiple HTML parser backends:

- **gumbo** (default): Uses [Google's Gumbo parser](https://github.com/google/gumbo-parser). Mature and well-tested, though archived.
- **tidy**: Uses [tidy-html5](https://github.com/htacg/tidy-html5). Actively maintained. Note: normalizes some whitespace.
- **lexbor**: Uses [Lexbor](https://github.com/lexbor/lexbor). Fast, modern, actively maintained.

To use a different backend:
```bash
cmake -DTURNDOWN_PARSER_BACKEND=lexbor ..  # or tidy, gumbo
```

## Usage

### Basic Usage

```cpp
#include <turndown.h>
#include <iostream>

int main() {
    turndown_cpp::TurndownService service;
    std::string markdown = service.turndown("<h1>Hello world!</h1>");
    std::cout << markdown << std::endl;
    // Output: Hello world!
    //         ===========
    return 0;
}
```

### With Options

Options can be passed to the constructor on instantiation:

```cpp
turndown_cpp::TurndownOptions options;
options.headingStyle = "atx";
options.codeBlockStyle = "fenced";

turndown_cpp::TurndownService service(options);
std::string markdown = service.turndown("<h1>Hello world!</h1>");
// Output: # Hello world!
```

## Options

| Option | Valid values | Default |
| :----- | :----------- | :------ |
| `headingStyle` | `"setext"` or `"atx"` | `"setext"` |
| `hr` | Any [Thematic break](http://spec.commonmark.org/0.27/#thematic-breaks) | `"* * *"` |
| `bulletListMarker` | `"-"`, `"+"`, or `"*"` | `"*"` |
| `codeBlockStyle` | `"indented"` or `"fenced"` | `"indented"` |
| `fence` | `` "`​`​`" `` or `"~~~"` | `` "`​`​`" `` |
| `emDelimiter` | `"_"` or `"*"` | `"_"` |
| `strongDelimiter` | `"**"` or `"__"` | `"**"` |
| `linkStyle` | `"inlined"` or `"referenced"` | `"inlined"` |
| `linkReferenceStyle` | `"full"`, `"collapsed"`, or `"shortcut"` | `"full"` |
| `preformattedCode` | `false` or `true` | `false` |

### Advanced Options

| Option | Description |
| :----- | :---------- |
| `blankReplacement` | Rule replacement function for blank elements |
| `keepReplacement` | Rule replacement function for kept elements |
| `defaultReplacement` | Rule replacement function for unrecognized elements |
| `escapeFunction` | Custom function to escape Markdown characters |

## Methods

### `addRule(key, rule)`

Add a conversion rule. The `key` parameter is a unique name for the rule.

```cpp
service.addRule("strikethrough", {
    [](gumbo::NodeView node, turndown_cpp::TurndownOptions const&) {
        return node.has_tag("del") || node.has_tag("s") || node.has_tag("strike");
    },
    [](std::string const& content, gumbo::NodeView, turndown_cpp::TurndownOptions const&) {
        return "~" + content + "~";
    }
});
```

Returns the `TurndownService` instance for chaining.

### `keep(filter)`

Determines which elements are to be kept and rendered as HTML. By default, Turndown does not keep any elements.

```cpp
service.keep({"del", "ins"});
service.turndown("<p>Hello <del>world</del><ins>World</ins></p>");
// Result: "Hello <del>world</del><ins>World</ins>"
```

The filter can be:
- A single tag name: `service.keep("del")`
- A vector of tag names: `service.keep({"del", "ins"})`
- A custom filter function

`keep` can be called multiple times, with newly added keep filters taking precedence over older ones. Keep filters will be overridden by the standard CommonMark rules and any added rules.

Returns the `TurndownService` instance for chaining.

### `remove(filter)`

Determines which elements are to be removed altogether (converted to an empty string). By default, Turndown does not remove any elements.

```cpp
service.remove("del");
service.turndown("<p>Hello <del>world</del><ins>World</ins></p>");
// Result: "Hello World"
```

`remove` can be called multiple times. Remove filters will be overridden by keep filters, standard CommonMark rules, and any added rules.

Returns the `TurndownService` instance for chaining.

### `use(plugin)`

Use a plugin to extend functionality:

```cpp
auto myPlugin = [](turndown_cpp::TurndownService& service) {
    service.addRule("custom", { /* ... */ });
};
service.use(myPlugin);
```

Returns the `TurndownService` instance for chaining.

## Extending with Rules

Turndown can be extended by adding **rules**. A rule is an object with `filter` and `replacement` properties:

```cpp
turndown_cpp::Rule paragraphRule{
    // Filter: matches <p> elements
    [](gumbo::NodeView node, turndown_cpp::TurndownOptions const&) {
        return node.has_tag("p");
    },
    // Replacement: wraps content in newlines
    [](std::string const& content, gumbo::NodeView, turndown_cpp::TurndownOptions const&) {
        return "\n\n" + content + "\n\n";
    }
};
```

### Filter

The filter property determines whether or not an element should be replaced with the rule's replacement:

- **Tag name**: `node.has_tag("p")` matches `<p>` elements
- **Multiple tags**: `node.has_tag("em") || node.has_tag("i")` matches `<em>` or `<i>`
- **Custom logic**: Any function returning `bool`

Example filter for inline links:

```cpp
[](gumbo::NodeView node, turndown_cpp::TurndownOptions const& options) {
    return options.linkStyle == "inlined" &&
           node.has_tag("a") &&
           !node.attribute("href").empty();
}
```

### Replacement Function

The replacement function determines how an element should be converted. It receives:
- `content`: The processed Markdown content of child elements
- `node`: The DOM node being converted
- `options`: Current conversion options

```cpp
[](std::string const& content, gumbo::NodeView node, turndown_cpp::TurndownOptions const& options) {
    return options.emDelimiter + content + options.emDelimiter;
}
```

### Special Rules

**Blank rule** determines how to handle blank elements. A node is blank if it only contains whitespace, and it's not an `<a>`, `<td>`, `<th>` or a void element. Customize with `blankReplacement` option.

**Keep rules** determine how to handle elements that should not be converted, i.e., rendered as HTML in the Markdown output. Customize with `keepReplacement` option.

**Remove rules** determine which elements to remove altogether.

**Default rule** handles nodes which are not recognized by any other rule. Customize with `defaultReplacement` option.

### Rule Precedence

Turndown iterates over the set of rules and picks the first one that matches:

1. Blank rule
2. Added rules (via `addRule`)
3. CommonMark rules
4. Keep rules
5. Remove rules
6. Default rule

## Escaping Markdown Characters

Turndown uses backslashes (`\`) to escape Markdown characters in the HTML input. This ensures that these characters are not interpreted as Markdown when the output is compiled back to HTML.

For example, `<h1>1. Hello world</h1>` is escaped to `1\. Hello world`, otherwise it would be interpreted as a list item rather than a heading.

### Custom Escape Function

You can customize the escaping behavior:

```cpp
options.escapeFunction = [](std::string const& text) {
    // Your custom escaping logic
    return turndown_cpp::minimalEscape(text);
};
```

**Note:** Text in code elements is never passed to the escape function.

## Command-Line Interface

The project includes a CLI tool for converting HTML to Markdown:

```bash
# Convert HTML from stdin
echo "<h1>Hello</h1>" | ./cli/turndown_cli

# Convert HTML from a file
./cli/turndown_cli --file input.html

# Use options
./cli/turndown_cli --atx-headings --fenced --file input.html

# See all options
./cli/turndown_cli --help
```

## API Reference

See the header file documentation for the complete API reference:
- `turndown.h` - Main `TurndownService` class and options
- `rules.h` - Rule structure and `Rules` class
- `node.h` - Node analysis utilities
- `utilities.h` - Utility functions

## Credits

This is a C++ port of [Turndown](https://github.com/mixmark-io/turndown) by Dom Christie.

The whitespace collapsing algorithm is adapted from [collapse-whitespace](https://github.com/lucthev/collapse-whitespace) by Luc Thevenard.

## License

turndown is copyright © 2017+ Dom Christie and released under the MIT license.

This C++ port is copyright © 2025 Parsa Amini and released under the MIT license.
