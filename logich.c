#ifndef LOGIC_H
#define LOGIC_H

#include "database.h"

int validate_academic_inputs(float att, float c1, float c2, float c3, float m1, float m2, float m3, float m4, float m5);
void compute_student_metrics(Student *s, float c1, float c2, float c3, float m1, float m2, float m3, float m4, float m5);
void run_stress_test(char *report_buffer, int max_len);

#endif

