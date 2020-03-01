#pragma once

void logging_initialize(void);

void logging_message(const char *);

void logging_warning(const char *);

void logging_error(const char *);

void logging_std_error(void);
