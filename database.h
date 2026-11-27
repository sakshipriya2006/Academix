#ifndef DATABASE_H
#define DATABASE_H

#define MAX_USERS 200
#define MAX_STUDENTS 500

typedef struct {
    char id[50];
    char password[50];
    char role[20]; /* "ADMIN" or "USER" */
} User;

typedef struct {
    char roll[20];
    char name[100];
    char dept[50];
    float attendance;
    float cie_avg;
    float sgpa;
    char status[20];
} Student;

void init_storage();
int register_user(const char *id, const char *password, const char *role);
int authenticate_user(const char *id, const char *password, char *out_role);

int add_or_update_student(Student s, int is_update);
int delete_student(const char *roll);
int get_student_by_roll(const char *roll, Student *out_s);
void get_all_students_json(char *buffer, int max_len);
void get_summary_report_json(char *buffer, int max_len);
void print_console_report();

#endif