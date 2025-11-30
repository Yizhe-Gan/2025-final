#include "server.h"
#include "utils.h"

int main(void) {

    if (log_init("curriculum.log")) {
        log_message("File logging enabled: curriculum.log", LOG_INFO);
    } else {
        log_message("File logging not enabled (will log to console)", LOG_WARN);
    }

    log_message("Starting course server...", LOG_INFO);

    if (!start_server("8080")) {
        log_message("Failed to start server", LOG_ERROR);
        return 1;
    }

    log_message("Server running on http://localhost:8080", LOG_INFO);
    log_message("Press ENTER to quit...", LOG_INFO);
    getchar();

    stop_server();
    log_message("Server stopped, cleaning up logs", LOG_INFO);
    log_close();
    return 0;
}
