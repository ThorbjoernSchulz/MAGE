#include "logging.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

static FILE *log_file;

void logging_initialize(void) {
  log_file = stderr;
}

void logging_message(const char *message) {
  fprintf(log_file, "Log: %s\n", message);
}

void logging_warning(const char *warning) {
  fprintf(log_file, "Warning: %s\n", warning);
}

void logging_error(const char *error) {
  fprintf(log_file, "Error: %s\n", error);
}

void logging_std_error(void) {
  logging_error(strerror(errno));
}
