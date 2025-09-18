#include "screen.h"
#include "editor.h"
#include "keyboard.h"

void launch_editor(const char* filename) {
    clear_screen();
    print("=== JupiterOS Text Editor ===\n");
    print("File: ");
    print(filename);
    print("\n");
    print("Text editor coming in v1.1!\n");
    print("For now, use: create ");
    print(filename);
    print(" \"your text here\"\n");
    
    // Use keyboard input instead of delay
    print("\nPress any key to return to shell...\n");
    get_key(); // Wait for key press
    
    clear_screen();
}
