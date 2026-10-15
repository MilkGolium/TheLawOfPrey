#include "test_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TEST_LOG_CAPACITY 32768
#define TEST_LOG_LINE_MAX 1024

static const char* sSuiteName = "";
static const char* sCaseName = "";
static int sCaseCount = 0;
static int sFailedCases = 0;
static int sFailureCount = 0;
static int sCaseFailed = 0;

static char sLogBuffer[TEST_LOG_CAPACITY];
static int sLogLength = 0;
static int sWarnCount = 0;

// === 日志捕获 ===

static void FormatInto(char* out, int outSize, const char* text, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int written = vsnprintf(out, (size_t)outSize, text, copy);
  va_end(copy);
  if (written < 0) out[0] = '\0';
}

static void CaptureTrace(int level, const char* text, va_list args) {
  if (level != LOG_WARNING) {
    char line[TEST_LOG_LINE_MAX];
    FormatInto(line, sizeof(line), text, args);
    fprintf(stderr, "[forwarded level %d log] %s\n", level, line);
    return;
  }

  sWarnCount++;
  if (sLogLength > 0 && sLogLength < TEST_LOG_CAPACITY - 1)
    sLogBuffer[sLogLength++] = '\n';

  char line[TEST_LOG_LINE_MAX];
  FormatInto(line, sizeof(line), text, args);
  int room = TEST_LOG_CAPACITY - 1 - sLogLength;
  int length = (int)strlen(line);
  if (length > room) length = room;
  memcpy(sLogBuffer + sLogLength, line, (size_t)length);
  sLogLength += length;
  sLogBuffer[sLogLength] = '\0';
}

void TestLogBegin(void) {
  TestLogReset();
  SetTraceLogCallback(CaptureTrace);
}

void TestLogEnd(void) { SetTraceLogCallback(NULL); }

void TestLogReset(void) {
  sLogLength = 0;
  sWarnCount = 0;
  sLogBuffer[0] = '\0';
}

int TestLogWarnCount(void) { return sWarnCount; }

bool TestLogHasWarn(const char* needle) {
  return strstr(sLogBuffer, needle) != NULL;
}

bool TestLogMissingWarn(const char* needle) {
  return strstr(sLogBuffer, needle) == NULL;
}

void TestLogPrint(void) {
  const char* p = sLogBuffer;
  while (p != NULL && *p != '\0') {
    const char* eol = strchr(p, '\n');
    int length = (eol != NULL) ? (int)(eol - p) : (int)strlen(p);
    printf("      captured warning: %.*s\n", length, p);
    p = (eol != NULL) ? eol + 1 : NULL;
  }
  if (sWarnCount == 0) printf("      captured warning: (none)\n");
  fflush(stdout);
}

// === 用例骨架 ===

void TestBegin(const char* suiteName) {
  sSuiteName = suiteName;
  printf("=== %s ===\n", sSuiteName);
  fflush(stdout);
}

void TestBeginCase(const char* name) {
  sCaseName = name;
  sCaseFailed = 0;
  sCaseCount++;
  printf("  [case] %s\n", sCaseName);
  fflush(stdout);
}

void TestEndCase(void) {
  if (sCaseFailed) {
    sFailedCases++;
    printf("  [result] FAILED: %s\n", sCaseName);
  } else {
    printf("  [result] passed: %s\n", sCaseName);
  }
  fflush(stdout);
}

// === 断言 ===

static void ReportFailure(const char* what, const char* detail) {
  sFailureCount++;
  sCaseFailed = 1;
  if (detail != NULL && detail[0] != '\0') {
    printf("    [FAIL] %s: %s -- %s\n", sCaseName, what, detail);
  } else {
    printf("    [FAIL] %s: %s\n", sCaseName, what);
  }
  fflush(stdout);
}

void TestCheck(bool ok, const char* what) {
  if (ok) {
    printf("    [pass] %s\n", what);
  } else {
    ReportFailure(what, "");
  }
  fflush(stdout);
}

void TestFail(const char* what) {
  ReportFailure(what, "should never get here");
}

void TestCheckInt(int actual, int expected, const char* what) {
  if (actual == expected) {
    printf("    [pass] %s = %d\n", what, actual);
    fflush(stdout);
    return;
  }
  char detail[128];
  snprintf(detail, sizeof(detail), "actual %d, expected %d", actual, expected);
  ReportFailure(what, detail);
}

void TestCheckStr(const char* actual, const char* expected, const char* what) {
  if (actual != NULL && strcmp(actual, expected) == 0) {
    printf("    [pass] %s = \"%s\"\n", what, actual);
    fflush(stdout);
    return;
  }
  char detail[512];
  snprintf(detail, sizeof(detail), "actual \"%s\", expected \"%s\"",
           (actual != NULL) ? actual : "(null)", expected);
  ReportFailure(what, detail);
}

static void FormatColor(char* out, int outSize, Color c) {
  snprintf(out, (size_t)outSize, "(%u, %u, %u, %u)", (unsigned)c.r,
           (unsigned)c.g, (unsigned)c.b, (unsigned)c.a);
}

static bool SameColor(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void TestCheckColor(Color actual, Color expected, const char* what) {
  if (SameColor(actual, expected)) {
    char detail[64];
    FormatColor(detail, sizeof(detail), actual);
    printf("    [pass] %s = %s\n", what, detail);
    fflush(stdout);
    return;
  }
  char got[64];
  char want[64];
  FormatColor(got, sizeof(got), actual);
  FormatColor(want, sizeof(want), expected);
  char detail[160];
  snprintf(detail, sizeof(detail), "actual %s, expected %s", got, want);
  ReportFailure(what, detail);
}

// 逐字段比对整套配色。任一字段不同即失败，并点名是哪个字段变了，
// 免得坏文件留下半套配色时只看到一句「不相等」。
void TestCheckStyle(const UiStyle* actual, const UiStyle* expected,
                    const char* what) {
  char got[64];
  char want[64];
  char detail[192];

#define CHECK_COLOR_FIELD(field)                                          \
  do {                                                                    \
    if (!SameColor(actual->field, expected->field)) {                     \
      FormatColor(got, sizeof(got), actual->field);                       \
      FormatColor(want, sizeof(want), expected->field);                   \
      snprintf(detail, sizeof(detail), "field %s actual %s, expected %s", \
               #field, got, want);                                        \
      ReportFailure(what, detail);                                        \
      return;                                                             \
    }                                                                     \
  } while (0)

  CHECK_COLOR_FIELD(desktop);
  CHECK_COLOR_FIELD(background);
  CHECK_COLOR_FIELD(line);
  CHECK_COLOR_FIELD(lineDim);
  CHECK_COLOR_FIELD(text);
  CHECK_COLOR_FIELD(textDim);
  CHECK_COLOR_FIELD(button);
  CHECK_COLOR_FIELD(buttonText);
  CHECK_COLOR_FIELD(titleBar);
  CHECK_COLOR_FIELD(titleText);
  CHECK_COLOR_FIELD(shadow);

#undef CHECK_COLOR_FIELD

  if (actual->borderStyle != expected->borderStyle) {
    snprintf(detail, sizeof(detail), "field borderStyle actual %d, expected %d",
             (int)actual->borderStyle, (int)expected->borderStyle);
    ReportFailure(what, detail);
    return;
  }
  if (actual->shadowStyle != expected->shadowStyle) {
    snprintf(detail, sizeof(detail), "field shadowStyle actual %d, expected %d",
             (int)actual->shadowStyle, (int)expected->shadowStyle);
    ReportFailure(what, detail);
    return;
  }

  printf("    [pass] %s (all 11 color keys and 2 enum keys are equal)\n", what);
  fflush(stdout);
}

int TestEnd(void) {
  printf(
      "=== %s done: %d case(s), %d failed, %d assertion failure(s) total ===\n",
      sSuiteName, sCaseCount, sFailedCases, sFailureCount);
  if (sFailedCases > 0) {
    printf("TESTS FAILED.\n");
    return 1;
  }
  printf("All cases passed.\n");
  return 0;
}