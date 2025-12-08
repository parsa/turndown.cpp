# turndown.cpp

## C++ Port of Turndown

This is a C++ port of the Javascript [Turndown](https://github.com/mixmark-io/turndown) library, which converts HTML to Markdown. This port aims to provide similar functionality and options as the original Javascript library, leveraging the [Gumbo HTML5 parser](https://github.com/google/gumbo-parser) for C++.

**Note:** This C++ port is currently a work in progress and may not yet implement all features of the Javascript version. Contributions and feedback are welcome.

## Features Implemented

- Core HTML to Markdown conversion
- Basic CommonMark rule support (paragraphs, headings, lists, blockquotes, code blocks, links, images, emphasis, strong)
- Option for Setext and ATX heading styles
- Options for horizontal rules, bullet list markers, code block styles, delimiters for emphasis and strong, link styles (inline and referenced)
- Basic whitespace handling and collapsing
- Customizable escaping of Markdown characters
- Plugin system for custom rules
- Keep/remove filters for HTML elements
- Command-line interface (CLI) tool
- Example usage and basic test suite
- Lightweight C++ wrapper around the Gumbo parser for safer traversal

## Building

### Prerequisites

- C++ compiler with C++20 support (e.g., g++ 10+, clang++ 10+)
- CMake (version 3.10 or higher)
- Gumbo HTML5 parser library (installable via package manager or build from source)
- Google Test (for running tests, optional)

### Build Steps

1.  **Clone the repository:**
    ```bash
    git clone [repository URL]
    cd turndown.cpp
    ```

2.  **Create a build directory:**
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure CMake:**
    ```bash
    cmake ..
    ```
    *   You may need to specify the path to Gumbo if CMake cannot find it automatically. For example:
        ```bash
        cmake -DGUMBO_INCLUDE_DIR=/path/to/gumbo/include -DGUMBO_LIBRARY=/path/to/gumbo/lib/libgumbo.so ..
        ```
    *   If you want to build the tests, ensure Google Test is installed and found by CMake.

4.  **Build the project:**
    ```bash
    cmake --build .
    ```
    *   Or if using Make: `make`
    *   Or if using Ninja: `ninja`

5.  **(Optional) Run tests:**
    ```bash
    ctest
    ```

6.  **Install the library (optional):**
    ```bash
    cmake --install .
    ```

### Example Usage

See the `example/main.cpp` for a demonstration of how to use the `turndown_cpp` library and configure options. After building, you can run the example executable from the `build` directory:

```bash
./example/turndown_example
```

### Command-Line Interface

The project includes a CLI tool (`turndown_cli`) that can convert HTML to Markdown from the command line. After building, you can use it like this:

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
