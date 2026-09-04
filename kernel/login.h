#ifndef LOGIN_H
#define LOGIN_H

#define MAX_USERS 10
#define MAX_USERNAME_LEN 20
#define MAX_PASSWORD_LEN 20

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int is_admin;
} user_t;

void init_users();
int authenticate_user(const char* username, const char* password);
void add_user(const char* username, const char* password, int is_admin);
void show_login_screen();
int get_current_user_id();
int is_current_user_admin();

// ADD THESE GETTER FUNCTIONS:
const char* get_username(int user_id);
int is_user_admin(int user_id);

#endif
