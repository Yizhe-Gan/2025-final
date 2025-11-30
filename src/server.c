#include "server.h"

static struct mg_context *ctx = NULL;

static int respond_405(struct mg_connection *conn, const char *allow) {
    char buf[256];
    snprintf(buf, sizeof(buf), "405 Method Not Allowed (Allow: %s)", allow);
    log_message(buf, LOG_WARN);
    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\nAllow: %s\r\nContent-Type: application/json\r\n\r\n{ \"error\": \"method not allowed\" }", allow);
    return 405;
}

static int respond_400(struct mg_connection *conn, const char *msg) {
    char buf[256];
    snprintf(buf, sizeof(buf), "400 Bad Request: %s", msg);
    log_message(buf, LOG_WARN);
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n{ \"error\": \"%s\" }", msg);
    return 400;
}

static int request_handler(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char buf[512];
    snprintf(buf, sizeof(buf), "Request %s %s?%s", ri->request_method, ri->local_uri, ri->query_string ? ri->query_string : "");
    log_message(buf, LOG_INFO);

    // Handle CORS preflight requests
    if (strcmp(ri->request_method, "OPTIONS") == 0) {
        mg_printf(conn,
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, DELETE, PUT, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n\r\n");
        return 200;
    }

    if (strcmp(ri->local_uri, "/ping") == 0) {
        return handle_ping(conn);
    }

    if (strcmp(ri->local_uri, "/course") == 0) {
        if (strcmp(ri->request_method, "GET") == 0) return handle_course_list(conn);
        if (strcmp(ri->request_method, "POST") == 0) return handle_course_add(conn);
        if (strcmp(ri->request_method, "PUT") == 0) return handle_course_update(conn);
        if (strcmp(ri->request_method, "DELETE") == 0) return handle_course_remove(conn);
        return respond_405(conn, "GET, POST, PUT, DELETE");
    }

    if (strcmp(ri->local_uri, "/course/all") == 0) {
        if (strcmp(ri->request_method, "DELETE") == 0) return handle_course_remove_all(conn);
        return respond_405(conn, "DELETE");
    }

    if (strcmp(ri->local_uri, "/course/add") == 0) {
        if (strcmp(ri->request_method, "POST") == 0) return handle_course_add(conn);
        return respond_405(conn, "POST");
    }

    if (strcmp(ri->local_uri, "/course/update") == 0) {
        if (strcmp(ri->request_method, "PUT") == 0) return handle_course_update(conn);
        return respond_405(conn, "PUT");
    }

    if (strcmp(ri->local_uri, "/course/find") == 0) {
        if (strcmp(ri->request_method, "GET") == 0) {
            const char *qs = ri->query_string ? ri->query_string : "";
            if (strstr(qs, "id=")) return handle_course_find_by_id(conn);
            if (strstr(qs, "name=")) return handle_course_find_by_name(conn);
            if (strstr(qs, "type=")) return handle_course_find_by_type(conn);
            if (strstr(qs, "semester=")) return handle_course_find_by_semester(conn);
        }
        return respond_400(conn, "missing find parameter");
    }


    if (strcmp(ri->local_uri, "/student") == 0) {
        if (strcmp(ri->request_method, "POST") == 0) return handle_student_add(conn);
        if (strcmp(ri->request_method, "GET") == 0) return handle_student_list(conn);
        if (strcmp(ri->request_method, "PUT") == 0) return handle_student_update(conn);
        if (strcmp(ri->request_method, "DELETE") == 0) return handle_student_remove(conn);
        return respond_405(conn, "GET, POST, PUT, DELETE");
    }

    if (strcmp(ri->local_uri, "/student/all") == 0) {
        if (strcmp(ri->request_method, "DELETE") == 0) return handle_student_remove_all(conn);
        return respond_405(conn, "DELETE");
    }

    if (strcmp(ri->local_uri, "/student/add") == 0) {
        if (strcmp(ri->request_method, "POST") == 0) return handle_student_add(conn);
        return respond_405(conn, "POST");
    }

    if (strcmp(ri->local_uri, "/student/update") == 0) {
        if (strcmp(ri->request_method, "PUT") == 0) return handle_student_update(conn);
        return respond_405(conn, "PUT");
    }

    if (strcmp(ri->local_uri, "/student/find") == 0) {
        if (strcmp(ri->request_method, "GET") == 0) {
            const char *qs = ri->query_string ? ri->query_string : "";
            if (strstr(qs, "student_id=")) return handle_student_find_by_id(conn);
            if (strstr(qs, "name=")) return handle_student_find_by_name(conn);
        }
        return respond_400(conn, "missing find parameter");
    }

    if (strcmp(ri->local_uri, "/enrollment") == 0) {
        if (strcmp(ri->request_method, "POST") == 0) return handle_enrollment_add(conn);
        if (strcmp(ri->request_method, "GET") == 0) {
            const char *qs = ri->query_string ? ri->query_string : "";
            if (strstr(qs, "student_id=")) return handle_enrollment_find_by_student_id(conn);
            if (strstr(qs, "course_id=")) return handle_enrollment_find_by_course_id(conn);
            return handle_enrollment_list(conn);
        }
        if (strcmp(ri->request_method, "DELETE") == 0) return handle_enrollment_remove(conn);
        return respond_405(conn, "GET, POST, DELETE");
    }

    if (strcmp(ri->local_uri, "/enrollment/all") == 0) {
        if (strcmp(ri->request_method, "DELETE") == 0) return handle_enrollment_remove_all(conn);
        return respond_405(conn, "DELETE");
    }

    mg_printf(conn,
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{ \"error\": \"not found\" }");

    return 404;
}

bool start_server(const char *port) {
    const char *options[] = {
        "listening_ports", port,
        "num_threads", "4",
        NULL
    };

    ctx = mg_start(NULL, NULL, options);
    if (!ctx)
        return false;

    mg_set_request_handler(ctx, "/", request_handler, NULL);

    if (!init_db()) return false;

    return true;
}

void stop_server(void) {
    if (ctx)
        mg_stop(ctx);

    close_db();
}
