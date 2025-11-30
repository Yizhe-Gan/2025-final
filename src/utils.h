#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "civetweb.h"
#include <jansson.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <time.h>

enum log_level {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

void log_message(const char *message, enum log_level level);
bool log_init(const char *path);
void log_close(void);