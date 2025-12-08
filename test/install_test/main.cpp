// Test that the installed turndown_cpp library works correctly
#include <turndown.h>
#include <iostream>
#include <cstdlib>

int main() {
    turndown_cpp::TurndownService service;
    
    std::string html = "<h1>Hello World</h1><p>This is a <strong>test</strong>.</p>";
    std::string markdown = service.turndown(html);
    
    // Verify output contains expected content
    if (markdown.find("Hello World") == std::string::npos) {
        std::cerr << "ERROR: Missing 'Hello World' in output\n";
        std::cerr << "Output was: " << markdown << "\n";
        return EXIT_FAILURE;
    }
    
    if (markdown.find("**test**") == std::string::npos) {
        std::cerr << "ERROR: Missing '**test**' in output\n";
        std::cerr << "Output was: " << markdown << "\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "Install test passed!\n";
    std::cout << "Converted:\n" << markdown << "\n";
    return EXIT_SUCCESS;
}

