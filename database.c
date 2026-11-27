#include <stdio.h>
#include <string.h>
#include "database.h"

static User users[MAX_USERS];
static int user_count = 0;

static Student students[MAX_STUDENTS];
static int student_count = 0;

static void save_users_to_file() {
    FILE *fp = fopen("users.dat", "wb");
    if (!fp) return;
    fwrite(&user_count, sizeof(int), 1, fp);
    if (user_count > 0) {
        fwrite(users, sizeof(User), user_count, fp);
    }
    fclose(fp);
}

static void load_users_from_file() {
    FILE *fp = fopen("users.dat", "rb");
    if (!fp) {
        /* Default built-in Admin account */
        strcpy(users.id, "admin");
        strcpy(users.password, "admin123");
        strcpy(users.role, "ADMIN");
        user_count = 1;
        save_users_to_file();
        return;
    }
    if (fread(&user_count, sizeof(int), 1, fp) == 1 && user_count > 0 && user_count <= MAX_USERS) {
        fread(users, sizeof(User), user_count, fp);
    } else {
        user_count = 0;
    }
    fclose(fp);
}

static void save_students_to_file() {
    FILE *fp = fopen("students.dat", "wb");
    if (!fp) return;
    fwrite(&student_count, sizeof(int), 1, fp);
    if (student_count > 0) {
        fwrite(students, sizeof(Student), student_count, fp);
    }
    fclose(fp);
}

static void load_students_from_file() {
    FILE *fp = fopen("students.dat", "rb");
    if (!fp) {
        student_count = 0;
        return;
    }
    if (fread(&student_count, sizeof(int), 1, fp) == 1 && student_count > 0 && student_count <= MAX_STUDENTS) {
        fread(students, sizeof(Student), student_count, fp);
    } else {
        student_count = 0;
    }
    fclose(fp);
}

void init_storage() {
    load_users_from_file();
    load_students_from_file();
}

int register_user(const char *id, const char *password, const char *role) {
    if (!id || !password || strlen(id) < 2 || strlen(password) < 3) return -1; /* Invalid input */
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].id, id) == 0) return 0; /* Duplicate ID */
    }
    if (user_count >= MAX_USERS) return -2;

    strncpy(users[user_count].id, id, 31);
    users[user_count].id = '\0';
    strncpy(users[user_count].password, password, 31);
    users[user_count].password = '\0';
    strncpy(users[user_count].role, (strcmp(role, "ADMIN") == 0) ? "ADMIN" : "USER", 15);
    users[user_count].role = '\0';
    user_count++;
    save_users_to_file();
    return 1;
}

int authenticate_user(const char *id, const char *password, char *out_role) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].id, id) == 0 && strcmp(users[i].password, password) == 0) {
            strcpy(out_role, users[i].role);
            return 1;
        }
    }
    return 0;
}

int add_or_update_student(Student s, int is_update) {
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, s.roll) == 0) {
            if (!is_update) return 0; /* Duplicate record prevented */
            students[i] = s;
            save_students_to_file();
            return 1;
        }
    }
    if (student_count >= MAX_STUDENTS) return -1;
    students[student_count++] = s;
    save_students_to_file();
    return 1;
}

int delete_student(const char *roll) {
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, roll) == 0) {
            for (int j = i; j < student_count - 1; j++) {
                students[j] = students[j + 1];
            }
            student_count--;
            save_students_to_file();
            return 1;
        }
    }
    return 0;
}

int get_student_by_roll(const char *roll, Student *out_s) {
    for (int i = 0; i < student_count; i++) {
        if (strcmp(students[i].roll, roll) == 0) {
            *out_s = students[i];
            return 1;
        }
    }
    return 0;
}

void get_all_students_json(char *buffer, int max_len) {
    int pos = snprintf(buffer, max_len, "[");
    for (int i = 0; i < student_count; i++) {
        if (i > 0 && pos < max_len - 2) {
            pos += snprintf(buffer + pos, max_len - pos, ",");
        }
        pos += snprintf(buffer + pos, max_len - pos,
            "{\"roll\":\"%s\",\"name\":\"%s\",\"dept\":\"%s\",\"attendance\":%.2f,\"cie_avg\":%.2f,\"sgpa\":%.2f,\"status\":\"%s\"}",
            students[i].roll, students[i].name, students[i].dept,
            students[i].attendance, students[i].cie_avg, students[i].sgpa, students[i].status);
    }
    snprintf(buffer + pos, max_len - pos, "]");
}

void get_summary_report_json(char *buffer, int max_len) {
    int safe = 0, mod = 0, high = 0;
    float sgpa_sum = 0.0f;
    for (int i = 0; i < student_count; i++) {
        sgpa_sum += students[i].sgpa;
        if (strcmp(students[i].status, "SAFE") == 0) safe++;
        else if (strcmp(students[i].status, "MODERATE") == 0) mod++;
        else high++;
    }
    float avg_sgpa = (student_count > 0) ? (sgpa_sum / student_count) : 0.0f;
    snprintf(buffer, max_len,
        "{\"total\":%d,\"safe\":%d,\"moderate\":%d,\"high_risk\":%d,\"avg_sgpa\":%.2f}",
        student_count, safe, mod, high, avg_sgpa);
}

void print_console_report() {
    printf("\n--- ACADEMIX SUMMARY REPORT (Total Records: %d) ---\n", student_count);
    for (int i = 0; i < student_count; i++) {
        printf("%d. [%s] %s (%s) | Att: %.1f%% | CIE: %.2f | SGPA: %.2f | %s\n",
            i + 1, students[i].roll, students[i].name, students[i].dept,
            students[i].attendance, students[i].cie_avg, students[i].sgpa, students[i].status);
    }
    printf("---------------------------------------------------\n");
}
