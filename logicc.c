#include <stdio.h>
#include <string.h>
#include <time.h>
#include "logic.h"

static int marks_to_grade_point(float marks) {
    if (marks >= 90.0f) return 10;
    if (marks >= 80.0f) return 9;
    if (marks >= 70.0f) return 8;
    if (marks >= 60.0f) return 7;
    if (marks >= 50.0f) return 6;
    if (marks >= 40.0f) return 5;
    return 0;
}

int validate_academic_inputs(float att, float c1, float c2, float c3, float m1, float m2, float m3, float m4, float m5) {
    if (att < 0.0f || att > 100.0f) return 0;
    if (c1 < 0.0f || c1 > 30.0f || c2 < 0.0f || c2 > 30.0f || c3 < 0.0f || c3 > 30.0f) return 0;
    float m = {m1, m2, m3, m4, m5};
    for (int i = 0; i < 5; i++) {
        if (m[i] < 0.0f || m[i] > 100.0f) return 0;
    }
    return 1;
}

void compute_student_metrics(Student *s, float c1, float c2, float c3, float m1, float m2, float m3, float m4, float m5) {
    s->cie_avg = (c1 + c2 + c3) / 3.0f;

    float subs = {m1, m2, m3, m4, m5};
    float credits = {4.0f, 4.0f, 3.0f, 3.0f, 2.0f};
    float total_points = 0.0f;
    float total_credits = 16.0f;

    for (int i = 0; i < 5; i++) {
        total_points += marks_to_grade_point(subs[i]) * credits[i];
    }
    s->sgpa = total_points / total_credits;

    if (s->attendance < 75.0f || s->sgpa < 6.0f || s->cie_avg < 12.0f) {
        strcpy(s->status, "HIGH RISK");
    } else if (s->attendance < 82.0f || s->sgpa < 7.5f) {
        strcpy(s->status, "MODERATE");
    } else {
        strcpy(s->status, "SAFE");
    }
}

void run_stress_test(char *report_buffer, int max_len) {
    clock_t start = clock();
    Student dummy;
    strcpy(dummy.roll, "AEC-STRESS");
    strcpy(dummy.name, "Benchmark");
    strcpy(dummy.dept, "CSE");
    dummy.attendance = 80.0f;

    for (int i = 0; i < 10000; i++) {
        compute_student_metrics(&dummy, 22.0f, 24.0f, 21.0f, 78.0f, 82.0f, 75.0f, 88.0f, 91.0f);
    }

    clock_t end = clock();
    double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    snprintf(report_buffer, max_len,
        "{\"records_processed\":10000,\"time_ms\":%.3f,\"status\":\"SUCCESS\"}",
        elapsed_ms);
}
