#include "utils.h"

static FILE *log_fp = NULL;

bool log_init(const char *path) {
    if (!path) return false;
    FILE *f = fopen(path, "a");
    if (!f) return false;
    log_fp = f;
    return true;
}

void log_close(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

void log_message(const char *message, enum log_level level) {
    // Timestamp
    char tbuf[64];
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);

    switch (level) {
        case LOG_DEBUG:
            printf("\033[90m[%s] [DEBUG] %s\033[0m\n", tbuf, message);
            break;
        case LOG_INFO:
            printf("[%s] [INFO] %s\n", tbuf, message);
            break;
        case LOG_WARN:
            printf("\033[33m[%s] [WARN] %s\033[0m\n", tbuf, message);
            break;
        case LOG_ERROR:
            printf("\033[31m[%s] [ERROR] %s\033[0m\n", tbuf, message);
            break;
    }

    if (log_fp) {
        const char *lvl = "INFO";
        switch (level) {
            case LOG_DEBUG: lvl = "DEBUG"; break;
            case LOG_INFO: lvl = "INFO"; break;
            case LOG_WARN: lvl = "WARN"; break;
            case LOG_ERROR: lvl = "ERROR"; break;
        }
        fprintf(log_fp, "%s [%s] %s\n", tbuf, lvl, message);
        fflush(log_fp);
    }
}