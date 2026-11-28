/*
 * logic.c  —  Member 2: Backend C Developer (The Processing Engine)
 * Project : ACADEMIX — Asansol Engineering College (AEC)
 * See logic.h for what each function does and how to call it.
 */
#if !defined(_WIN32)
  #define _POSIX_C_SOURCE 200809L      /* for clock_gettime() with -std=c99 */
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "logic.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #define PSAPI_VERSION 2              /* uses kernel32: no -lpsapi needed */
  #include <psapi.h>
#else
  #include <sys/resource.h>
#endif

/* =================================================================
 * PART 1: PROCESSING ENGINE  (grades, SGPA, pass / fail)
 * ================================================================= */

const int LOGIC_CREDITS[LOGIC_SUBJECTS] = { 4, 4, 3, 3, 2 };

/* MAKAUT 10-point grade table (highest first) */
typedef struct { float min_percent; const char *grade; float point; } GradeBand;
static const GradeBand GRADE_TABLE[] = {
    { 90.0f, "O", 10.0f },   /* Outstanding */
    { 80.0f, "E",  9.0f },   /* Excellent   */
    { 70.0f, "A",  8.0f },   /* Very Good   */
    { 60.0f, "B",  7.0f },   /* Good        */
    { 50.0f, "C",  6.0f },   /* Fair        */
    { 40.0f, "D",  5.0f },   /* Pass        */
    {  0.0f, "F",  0.0f },   /* Fail        */
};
#define GRADE_COUNT ((int)(sizeof GRADE_TABLE / sizeof GRADE_TABLE[0]))

static const GradeBand *find_grade(float percent)
{
    int i;
    for (i = 0; i < GRADE_COUNT; i++)
        if (percent >= GRADE_TABLE[i].min_percent) return &GRADE_TABLE[i];
    return &GRADE_TABLE[GRADE_COUNT - 1];
}

const char *logic_validate(const MarksInput *in)
{
    int i;
    if (!in) return "No input given";
    if (in->attendance < 0.0f || in->attendance > 100.0f)
        return "Attendance must be between 0 and 100";
    for (i = 0; i < LOGIC_CIE_TESTS; i++)
        if (in->cie[i] < 0.0f || in->cie[i] > LOGIC_CIE_MAX)
            return "Each CIE mark must be between 0 and 30";
    for (i = 0; i < LOGIC_SUBJECTS; i++)
        if (in->marks[i] < 0.0f || in->marks[i] > LOGIC_MARK_MAX)
            return "Each subject mark must be between 0 and 100";
    return NULL;
}

/* =================================================================
 * PART 2: PURE-C AT-RISK MATRIX
 *  Step 1  five warning signs, each turned into 0 (fine) .. 1 (bad)
 *  Step 2  weighted score = 100 x (w1*f1 + ... + w5*f5)
 *  Step 3  3x3 matrix (attendance band x marks band) gives a 2nd opinion
 *  Step 4  final status = the WORSE of the two (safe side)
 * ================================================================= */

/* weights: low attendance, CIE falling, low marks, low CIE, failed subjects */
static const float RISK_WEIGHTS[5] = { 0.25f, 0.20f, 0.25f, 0.15f, 0.15f };

/* rows: attendance >=75 | 60..75 | <60     cols: marks >=60 | 40..60 | <40
 * 0 = SAFE, 1 = MODERATE, 2 = HIGH RISK */
static const int RISK_MATRIX[3][3] = {
    { 0, 0, 1 },
    { 0, 1, 2 },
    { 1, 2, 2 },
};

static const char *RISK_NAMES[3] = { "SAFE", "MODERATE", "HIGH RISK" };

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

/* Least-squares straight line through the points; returns the slope.
 * Example: 80, 60, 40  -> slope -20 (falling 20 % per test). */
static float trend_slope(const float *y, int n)
{
    int i;
    float x_mean = (float)(n - 1) / 2.0f, y_mean = 0.0f, num = 0.0f, den = 0.0f;
    if (n < 2) return 0.0f;
    for (i = 0; i < n; i++) y_mean += y[i];
    y_mean /= (float)n;
    for (i = 0; i < n; i++) {
        float dx = (float)i - x_mean;
        num += dx * (y[i] - y_mean);
        den += dx * dx;
    }
    return den > 0.0f ? num / den : 0.0f;
}

static void add_reason(AcademicResult *r, const char *text)
{
    if (r->reason_count < LOGIC_MAX_REASONS)
        snprintf(r->reasons[r->reason_count++], sizeof r->reasons[0], "%s", text);
}

static void evaluate_risk(const MarksInput *in, AcademicResult *r)
{
    float cie_percent[LOGIC_CIE_TESTS], f[5], score = 0.0f, cie_avg_percent;
    int i, score_level, row, col, matrix_level;
    char text[96];

    for (i = 0; i < LOGIC_CIE_TESTS; i++)
        cie_percent[i] = in->cie[i] / LOGIC_CIE_MAX * 100.0f;
    cie_avg_percent = r->cie_avg / LOGIC_CIE_MAX * 100.0f;
    r->cie_trend = trend_slope(cie_percent, LOGIC_CIE_TESTS);

    /* Step 1: warning signs */
    f[0] = clamp01((LOGIC_ATT_REQUIRED - in->attendance) / 40.0f); /* low attendance */
    f[1] = clamp01(-r->cie_trend / 8.0f);                         /* CIE falling    */
    f[2] = clamp01((60.0f - r->percentage) / 30.0f);              /* low marks      */
    f[3] = clamp01((60.0f - cie_avg_percent) / 30.0f);            /* low CIE        */
    f[4] = clamp01((float)r->failed_subjects / 3.0f);             /* failed subjects*/

    /* Step 2: weighted score */
    for (i = 0; i < 5; i++) score += RISK_WEIGHTS[i] * f[i];
    r->risk_score = score * 100.0f;
    score_level = r->risk_score >= 60.0f ? 2 : (r->risk_score >= 30.0f ? 1 : 0);

    /* Step 3: matrix */
    row = in->attendance >= 75.0f ? 0 : (in->attendance >= 60.0f ? 1 : 2);
    col = r->percentage  >= 60.0f ? 0 : (r->percentage  >= 40.0f ? 1 : 2);
    matrix_level = RISK_MATRIX[row][col];

    /* Step 4: final = worse of the two */
    snprintf(r->status, sizeof r->status, "%s",
             RISK_NAMES[score_level > matrix_level ? score_level : matrix_level]);

    /* Plain-English reasons */
    if (in->attendance < LOGIC_ATT_REQUIRED) {
        snprintf(text, sizeof text, "Attendance %.1f%% is below 75%%", in->attendance);
        add_reason(r, text);
    }
    if (r->cie_trend <= -2.0f) {
        snprintf(text, sizeof text, "CIE marks falling %.1f%% per test", -r->cie_trend);
        add_reason(r, text);
    }
    if (r->percentage < 60.0f) {
        snprintf(text, sizeof text, "Semester marks %.1f%% are weak", r->percentage);
        add_reason(r, text);
    }
    if (r->failed_subjects > 0) {
        snprintf(text, sizeof text, "Failing %d subject%s", r->failed_subjects,
                 r->failed_subjects == 1 ? "" : "s");
        add_reason(r, text);
    }
    if (r->reason_count == 0) add_reason(r, "On track: attendance and marks are good");
}

int logic_calculate(const MarksInput *in, AcademicResult *out)
{
    int i, credit_sum = 0;
    float points = 0.0f;
    const GradeBand *g;

    if (!out || logic_validate(in) != NULL) return -1;
    memset(out, 0, sizeof *out);

    /* every subject: grade, credits, pass/fail */
    for (i = 0; i < LOGIC_SUBJECTS; i++) {
        g = find_grade(in->marks[i] / LOGIC_MARK_MAX * 100.0f);
        snprintf(out->subject_grade[i], sizeof out->subject_grade[i], "%s", g->grade);
        points     += g->point * (float)LOGIC_CREDITS[i];
        credit_sum += LOGIC_CREDITS[i];
        out->total_marks += in->marks[i];
        if (in->marks[i] < LOGIC_PASS_MARK) out->failed_subjects++;
    }

    /* totals */
    out->percentage = out->total_marks / (LOGIC_MARK_MAX * LOGIC_SUBJECTS) * 100.0f;
    g = find_grade(out->percentage);
    snprintf(out->overall_grade, sizeof out->overall_grade, "%s", g->grade);
    out->sgpa = points / (float)credit_sum;          /* SGPA = sum(GP x Cr) / sum(Cr) */

    for (i = 0; i < LOGIC_CIE_TESTS; i++) out->cie_avg += in->cie[i];
    out->cie_avg /= (float)LOGIC_CIE_TESTS;

    /* PASS only if overall >= 40 % and no subject failed */
    out->passed = out->percentage >= LOGIC_PASS_MARK && out->failed_subjects == 0;

    evaluate_risk(in, out);
    return 0;
}

int logic_fill_student(Student *s, const MarksInput *in, AcademicResult *out)
{
    AcademicResult r;
    if (!s || logic_calculate(in, &r) != 0) return -1;
    s->attendance = in->attendance;
    s->cie_avg    = r.cie_avg;
    s->sgpa       = r.sgpa;
    snprintf(s->status, sizeof s->status, "%s", r.status);
    if (out) *out = r;
    logic_log("ENGINE", "SUCCESS", "Processed %s: SGPA %.2f, CIE avg %.2f, %s",
              s->roll[0] ? s->roll : "student", r.sgpa, r.cie_avg, r.status);
    return 0;
}

/* =================================================================
 * PART 3: BENCHMARK
 * ================================================================= */

static double now_seconds(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

double logic_benchmark(int n)
{
    MarksInput in;
    AcademicResult r;
    volatile float sink = 0.0f;
    double start;
    int i, j;

    if (n <= 0) n = 10000;
    start = now_seconds();
    for (i = 0; i < n; i++) {                /* different marks every time */
        in.attendance = (float)(50 + (i * 7) % 51);
        for (j = 0; j < LOGIC_CIE_TESTS; j++) in.cie[j]   = (float)((i * 3 + j * 5) % 31);
        for (j = 0; j < LOGIC_SUBJECTS;  j++) in.marks[j] = (float)((i * 11 + j * 13) % 101);
        logic_calculate(&in, &r);
        sink += r.sgpa;
    }
    (void)sink;
    return (now_seconds() - start) * 1000.0;
}

void logic_benchmark_json(char *buf, int max_len, int n)
{
    double ms;
    if (n <= 0) n = 10000;
    logic_log("ENGINE", "INFO", "Benchmark started: %d SGPA + risk evaluations", n);
    ms = logic_benchmark(n);
    snprintf(buf, (size_t)max_len,
             "{\"records_processed\":%d,\"time_ms\":%.3f,\"records_per_sec\":%.0f}",
             n, ms, ms > 0.0 ? n / (ms / 1000.0) : 0.0);
    logic_log("ENGINE", "SUCCESS", "Benchmark done: %d records in %.3f ms", n, ms);
}

/* =================================================================
 * PART 4: TELEMETRY  (real operating-system calls)
 * ================================================================= */

static double        g_start_time = -1.0;
static unsigned long g_requests   = 0;

static void start_clock(void) { if (g_start_time < 0.0) g_start_time = now_seconds(); }

void logic_count_request(void) { start_clock(); g_requests++; }

void logic_telemetry(Telemetry *t)
{
    if (!t) return;
    start_clock();
    memset(t, 0, sizeof *t);
#ifdef _WIN32
    {
        PROCESS_MEMORY_COUNTERS pmc;
        FILETIME created, exited, kernel, user;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
            t->ram_kb      = (long)(pmc.WorkingSetSize / 1024);
            t->peak_ram_kb = (long)(pmc.PeakWorkingSetSize / 1024);
        }
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
            ULARGE_INTEGER k, u;
            k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
            u.LowPart = user.dwLowDateTime;   u.HighPart = user.dwHighDateTime;
            t->cpu_sec = (double)(k.QuadPart + u.QuadPart) / 1e7;   /* 100 ns units */
        }
    }
#else
    {
        struct rusage ru;
        if (getrusage(RUSAGE_SELF, &ru) == 0) {
            t->cpu_sec = (double)ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6
                       + (double)ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
  #ifdef __APPLE__
            t->peak_ram_kb = (long)(ru.ru_maxrss / 1024);   /* bytes on macOS */
  #else
            t->peak_ram_kb = (long)ru.ru_maxrss;             /* KB on Linux    */
  #endif
        }
        t->ram_kb = t->peak_ram_kb;
  #ifdef __linux__
        {   /* current memory, from the Linux kernel */
            FILE *fp = fopen("/proc/self/status", "r");
            char line[128];
            if (fp) {
                while (fgets(line, sizeof line, fp))
                    if (strncmp(line, "VmRSS:", 6) == 0) sscanf(line + 6, "%ld", &t->ram_kb);
                fclose(fp);
            }
        }
  #endif
    }
#endif
    if (t->peak_ram_kb < t->ram_kb) t->peak_ram_kb = t->ram_kb;
    t->uptime_sec = now_seconds() - g_start_time;
    t->requests   = g_requests;
}

void logic_telemetry_json(char *buf, int max_len)
{
    Telemetry t;
    logic_telemetry(&t);
    snprintf(buf, (size_t)max_len,
             "{\"ram_kb\":%ld,\"peak_ram_kb\":%ld,\"cpu_sec\":%.4f,"
             "\"uptime_sec\":%.1f,\"requests\":%lu}",
             t.ram_kb, t.peak_ram_kb, t.cpu_sec, t.uptime_sec, t.requests);
}

/* =================================================================
 * PART 5: TERMINAL MIRROR LOG
 *  Keeps the last 100 lines in a "ring": when full, the oldest line
 *  is overwritten. Each line gets an id 1, 2, 3 ...
 * ================================================================= */

#define LOG_LINES 100
#define LOG_LEN   160

static char g_log[LOG_LINES][LOG_LEN];
static int  g_log_last_id = 0;

void logic_log(const char *category, const char *level, const char *fmt, ...)
{
    char msg[LOG_LEN - 40];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    va_list ap;
    char *slot;

    start_clock();
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    g_log_last_id++;
    slot = g_log[g_log_last_id % LOG_LINES];
    snprintf(slot, LOG_LEN, "[%s %02d:%02d:%02d] %s: %s",
             category ? category : "ENGINE",
             tmv ? tmv->tm_hour : 0, tmv ? tmv->tm_min : 0, tmv ? tmv->tm_sec : 0,
             level ? level : "INFO", msg);
    printf("%s\n", slot);
    fflush(stdout);
}

/* copy text into buf as a JSON string (adds quotes, escapes " and \) */
static int append_json_string(char *buf, int pos, int max_len, const char *s)
{
    if (pos < max_len - 1) buf[pos++] = '"';
    for (; *s && pos < max_len - 3; s++) {
        if (*s == '"' || *s == '\\') buf[pos++] = '\\';
        buf[pos++] = (*s == '\n' || *s == '\r' || *s == '\t') ? ' ' : *s;
    }
    if (pos < max_len - 1) buf[pos++] = '"';
    buf[pos] = '\0';
    return pos;
}

void logic_logs_json(char *buf, int max_len, int since_id)
{
    int id, first, pos, n;
    if (!buf || max_len < 32) return;
    first = since_id + 1;
    if (first < g_log_last_id - LOG_LINES + 1) first = g_log_last_id - LOG_LINES + 1;
    if (first < 1) first = 1;

    pos = snprintf(buf, (size_t)max_len, "{\"last_id\":%d,\"lines\":[", g_log_last_id);
    for (id = first; id <= g_log_last_id; id++) {
        if (pos > max_len - LOG_LEN - 40) break;          /* buffer nearly full */
        n = snprintf(buf + pos, (size_t)(max_len - pos), "%s{\"id\":%d,\"text\":",
                     id == first ? "" : ",", id);
        pos += n;
        pos = append_json_string(buf, pos, max_len, g_log[id % LOG_LINES]);
        if (pos < max_len - 1) { buf[pos++] = '}'; buf[pos] = '\0'; }
    }
    snprintf(buf + pos, (size_t)(max_len - pos), "]}");
}
