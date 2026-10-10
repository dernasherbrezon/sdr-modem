#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile int current_level = LOG_LEVEL_INFO;
static int journal_prefix = 0;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void log_init(void) {
  // systemd sets JOURNAL_STREAM when stdout or stderr is connected to the journal
  journal_prefix = getenv("JOURNAL_STREAM") != NULL;
}

void log_set_level(int level) {
  current_level = level;
}

int log_parse_level(const char *name, int *level) {
  if (strcmp(name, "error") == 0) {
    *level = LOG_LEVEL_ERROR;
  } else if (strcmp(name, "warn") == 0) {
    *level = LOG_LEVEL_WARN;
  } else if (strcmp(name, "info") == 0) {
    *level = LOG_LEVEL_INFO;
  } else if (strcmp(name, "debug") == 0) {
    *level = LOG_LEVEL_DEBUG;
  } else {
    return -1;
  }
  return 0;
}

void log_write(int level, const char *format, ...) {
  if (level > current_level) {
    return;
  }
  pthread_once(&init_once, log_init);

  FILE *out = level <= LOG_LEVEL_WARN ? stderr : stdout;
  flockfile(out);
  if (journal_prefix) {
    fprintf(out, "<%d>", level);
  } else if (level == LOG_LEVEL_ERROR) {
    fputs("error: ", out);
  } else if (level == LOG_LEVEL_WARN) {
    fputs("warning: ", out);
  }
  va_list args;
  va_start(args, format);
  vfprintf(out, format, args);
  va_end(args);
  fputc('\n', out);
  funlockfile(out);
}
