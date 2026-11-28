/*
 * logic.h  —  Member 2: Backend C Developer (The Processing Engine)
 * ==================================================================
 * Project : ACADEMIX — Asansol Engineering College (AEC)
 *
 * What this file gives the team:
 *   1. Processing engine  : total, percentage, grade per subject, SGPA,
 *                           CIE average, pass/fail
 *   2. At-Risk Matrix     : status "SAFE" / "MODERATE" / "HIGH RISK"
 *   3. Benchmark          : 10,000 SGPA + risk evaluations, timed
 *   4. Telemetry          : live RAM (KB), CPU time, uptime, requests
 *   5. Terminal log       : lines like "[DATABASE 19:12:45] SUCCESS: ..."
 *
 * It works directly with the team's Student struct from database.h.
 *
 * ---------------- HOW MEMBER 4 USES IT (in main.c) ----------------
 *   #include "database.h"
 *   #include "logic.h"
 *
 *   // in /api/save, after reading att, c1..c3, m1..m5 from the URL:
 *   MarksInput in = { att, {c1, c2, c3}, {m1, m2, m3, m4, m5} };
 *   const char *err = logic_validate(&in);
 *   if (err) { reply {"ok":false,"error":err}; }
 *   Student s = {0};
 *   strcpy(s.roll, roll); strcpy(s.name, name); strcpy(s.dept, dept);
 *   logic_fill_student(&s, &in, NULL);   // fills attendance, cie_avg, sgpa, status
 *   add_or_update_student(s, is_update);
 *   reply {"ok":true,"sgpa":s.sgpa,"status":s.status}
 *
 *   // in /api/stress:
 *   char json[128]; logic_benchmark_json(json, sizeof json, 10000);
 *
 * ---------------- BUILD ----------------
 *   gcc main.c database.c logic.c mongoose.c -o academix
 *   Windows (MinGW): add -lws2_32          Linux: add -lm
 *   (Do not add -std=c99: mongoose.c does not compile with it.)
 */
#ifndef LOGIC_H
#define LOGIC_H

#include "database.h"

#define LOGIC_SUBJECTS     5
#define LOGIC_CIE_TESTS    3
#define LOGIC_CIE_MAX      30.0f    /* each CIE is out of 30            */
#define LOGIC_MARK_MAX     100.0f   /* each subject is out of 100       */
#define LOGIC_PASS_MARK    40.0f    /* pass mark per subject and overall*/
#define LOGIC_ATT_REQUIRED 75.0f    /* attendance rule                  */
#define LOGIC_MAX_REASONS  5

/* Credits as shown on the form: Sub1 4, Sub2 4, Sub3 3, Sub4 3, Sub5 2 */
extern const int LOGIC_CREDITS[LOGIC_SUBJECTS];

/* ---------- Input: exactly what the admin form sends ---------- */
typedef struct {
    float attendance;                   /* 0..100  */
    float cie[LOGIC_CIE_TESTS];         /* 0..30   */
    float marks[LOGIC_SUBJECTS];        /* 0..100  */
} MarksInput;

/* ---------- Output of the processing engine ---------- */
typedef struct {
    float total_marks;                  /* out of 500                        */
    float percentage;
    char  subject_grade[LOGIC_SUBJECTS][3]; /* O, E, A, B, C, D, F           */
    char  overall_grade[3];
    float sgpa;                         /* credit-weighted, 10-point scale   */
    float cie_avg;                      /* out of 30                         */
    int   failed_subjects;              /* subjects below 40                 */
    int   passed;                       /* 1 = PASS, 0 = FAIL                */
    float cie_trend;                    /* CIE change in % per test (+ good) */
    float risk_score;                   /* 0 (safe) .. 100 (very risky)      */
    char  status[20];                   /* "SAFE", "MODERATE", "HIGH RISK"   */
    char  reasons[LOGIC_MAX_REASONS][96];
    int   reason_count;
} AcademicResult;

/* ---------- 1 + 2. Processing engine and At-Risk Matrix ---------- */
/* Returns NULL if the input is valid, otherwise an error message. */
const char *logic_validate(const MarksInput *in);

/* Calculates everything. Returns 0 on success, -1 if the input is invalid. */
int logic_calculate(const MarksInput *in, AcademicResult *out);

/* Fills s->attendance, s->cie_avg, s->sgpa and s->status for database.c.
 * 'out' may be NULL. Returns 0 on success, -1 if the input is invalid. */
int logic_fill_student(Student *s, const MarksInput *in, AcademicResult *out);

/* ---------- 3. Benchmark ---------- */
double logic_benchmark(int n);          /* returns milliseconds */
/* Writes {"records_processed":n,"time_ms":x,"records_per_sec":y} */
void logic_benchmark_json(char *buf, int max_len, int n);

/* ---------- 4. Telemetry ---------- */
typedef struct {
    long          ram_kb;               /* memory used now by the server */
    long          peak_ram_kb;
    double        cpu_sec;              /* CPU time used so far          */
    double        uptime_sec;
    unsigned long requests;
} Telemetry;

void logic_count_request(void);         /* call once per API request     */
void logic_telemetry(Telemetry *t);
void logic_telemetry_json(char *buf, int max_len);

/* ---------- 5. Terminal Mirror log ---------- */
/* category: "DATABASE", "ENGINE", "RISK", "SERVER" ...
 * level   : "INFO", "SUCCESS", "WARN", "ERROR"
 * Prints "[DATABASE 19:12:45] SUCCESS: Student AEC/CSE/25/01 saved"
 * to the terminal and keeps the last 100 lines for the web page. */
void logic_log(const char *category, const char *level, const char *fmt, ...);

/* Writes {"last_id":N,"lines":[{"id":1,"text":"..."}, ...]} with lines
 * whose id > since_id. The page calls /api/logs?since=N to get new lines. */
void logic_logs_json(char *buf, int max_len, int since_id);

#endif /* LOGIC_H */
