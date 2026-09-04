#include "login.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"

static user_t users[MAX_USERS];
static int user_count = 0;
static int current_user_id = -1;

void init_users() {
    // Add default users
    add_user("admin", "password", 1);    // Admin user
    add_user("user", "12345", 0);        // Regular user
    add_user("guest", "guest", 0);       // Guest user
    
    current_user_id = -1; // No user logged in initially
}

int authenticate_user(const char* username, const char* password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && 
            strcmp(users[i].password, password) == 0) {
            return i; // Return user ID
        }
    }
    return -1; // Authentication failed
}

void add_user(const char* username, const char* password, int is_admin) {
    if (user_count < MAX_USERS) {
        strcpy(users[user_count].username, username);
        strcpy(users[user_count].password, password);
        users[user_count].is_admin = is_admin;
        user_count++;
    }
}

void show_login_screen() {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int attempts = 0;
    
    while (attempts < 3) {
        clear_screen();
        
        // Draw login box
        print("==============================================\n");
        print("|               JUPITER OS                   |\n");
        print("|               LOGIN SCREEN                 |\n");
        print("==============================================\n\n");
        
        if (attempts > 0) {
            print("Login failed! Attempts remaining: ");
            char attempts_buf[4];
            itoa(3 - attempts, attempts_buf, 10);
            print(attempts_buf);
            print("\n\n");
        }
        
        // Get username
        print("Username: ");
        get_line(username, MAX_USERNAME_LEN);
        
        // Get password (with masking)
        print("Password: ");
        int i = 0;
        int c;
        
        while (1) {
            c = get_key();
            if (!c) continue;
            
            if (c == '\r' || c == '\n') {
                password[i] = '\0';
                print("\n");
                break;
            } 
            else if (c == '\b') {
                if (i > 0) { 
                    i--; 
                    print_char('\b');
                    print_char(' ');
                    print_char('\b');
                }
            }
            else if (i < MAX_PASSWORD_LEN - 1 && c >= 32 && c <= 126) {
                password[i++] = (char)c;
                print_char('*'); // Show asterisk instead of actual character
            }
        }
        
        // Try to authenticate
        current_user_id = authenticate_user(username, password);
        if (current_user_id >= 0) {
            clear_screen();
            print("Login successful! Welcome, ");
            print(username);
            print("!\n\n");
            
            // Show user role
            if (users[current_user_id].is_admin) {
                print("Privileges: Administrator\n");
            } else {
                print("Privileges: Standard User\n");
            }
            
            // Small delay
            for (volatile int i = 0; i < 2000000; i++) {}
            return;
        }
        
        attempts++;
        if (attempts >= 3) {
            clear_screen();
            print("Too many failed login attempts!\n");
            print("System will now shutdown for security.\n");
            
            // Infinite loop (simulate shutdown)
            while(1) {
                asm volatile("hlt");
            }
        }
    }
}

int get_current_user_id() {
    return current_user_id;
}

int is_current_user_admin() {
    if (current_user_id >= 0 && current_user_id < user_count) {
        return users[current_user_id].is_admin;
    }
    return 0;
}

const char* get_username(int user_id) {
    if (user_id >= 0 && user_id < user_count) {
        return users[user_id].username;
    }
    return "unknown";
}

int is_user_admin(int user_id) {
    if (user_id >= 0 && user_id < user_count) {
        return users[user_id].is_admin;
    }
    return 0;
}
