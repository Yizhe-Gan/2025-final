#include "handlers.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Response helpers */
static const char *status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "";
    }
}

static int respond_json_str(struct mg_connection *conn, int code, const char *body) {
    char buf[512];
    snprintf(buf, sizeof(buf), "Responding %d %s", code, status_text(code));
    log_message(buf, LOG_INFO);
    mg_printf(conn, 
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, PUT, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n%s", 
        code, status_text(code), body ? body : "");
    return code;
}

static int respond_error(struct mg_connection *conn, int code, const char *msg) {
    char buf[256];
    if (!msg) msg = "error";
    snprintf(buf, sizeof(buf), "{ \"error\": \"%s\" }", msg);
    return respond_json_str(conn, code, buf);
}

/* URL percent-decode (returns malloc'd string) */
static char hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static char *url_decode(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    char *dst = out;
    for (const char *p = s; *p; ++p) {
        if (*p == '+') {
            *dst++ = ' ';
        } else if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hi = hexval(p[1]);
            char lo = hexval(p[2]);
            *dst++ = (char)((hi << 4) | lo);
            p += 2;
        } else {
            *dst++ = *p;
        }
    }
    *dst = '\0';
    return out;
}

static char *read_body(struct mg_connection *conn) {
    char buf[1024];
    int r;
    char *data = NULL;
    size_t size = 0;

    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (ri && ri->content_length > 1024 * 1024) return NULL;
    if (ri) {
        char t[128];
        snprintf(t, sizeof(t), "Reading body (content_length=%d)", (int)ri->content_length);
        log_message(t, LOG_DEBUG);
    }

    while ((r = mg_read(conn, buf, sizeof(buf))) > 0) {
        char *tmp = realloc(data, size + r + 1);
        if (!tmp) { free(data); return NULL; }
        data = tmp;
        memcpy(data + size, buf, r);
        size += r;
    }
    if (data) data[size] = '\0';
    return data;
}

static char *get_qs_param(const struct mg_request_info *ri, const char *key) {
    if (!ri || !ri->query_string || !key) return NULL;
    size_t keylen = strlen(key);
    const char *p = ri->query_string;
    while (*p) {
        if (strncmp(p, key, keylen) == 0 && p[keylen] == '=') {
            const char *v = p + keylen + 1;
            const char *end = strchr(v, '&');
            size_t len = end ? (size_t)(end - v) : strlen(v);
            char *raw = malloc(len + 1);
            if (!raw) return NULL;
            memcpy(raw, v, len);
            raw[len] = '\0';
            char *decoded = url_decode(raw);
            free(raw);
            return decoded;
        }
        const char *amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return NULL;
}

static QueryOptions *parse_query_options(const struct mg_request_info *ri) {
    static QueryOptions opt;
    static char *last_order_by = NULL;
    
    // Free previous order_by if any
    if (last_order_by) {
        free(last_order_by);
        last_order_by = NULL;
    }
    
    opt.order_by = NULL;
    opt.order = SORT_ASC;
    opt.limit = -1;
    opt.offset = 0;

    if (!ri || !ri->query_string) return &opt;

    char *limit_str = get_qs_param(ri, "limit");
    if (limit_str) {
        opt.limit = atoi(limit_str);
        free(limit_str);
    }

    char *offset_str = get_qs_param(ri, "offset");
    if (offset_str) {
        opt.offset = atoi(offset_str);
        free(offset_str);
    }

    char *order_str = get_qs_param(ri, "order");
    if (order_str) {
        if (strcmp(order_str, "desc") == 0) {
            opt.order = SORT_DESC;
        }
        free(order_str);
    }

    char *order_by = get_qs_param(ri, "order_by");
    if (order_by) {
        last_order_by = order_by;
        opt.order_by = order_by;
    }

    return &opt;
}

static void course_to_json(const Course *c, void *user) {
    json_t *arr = user;
    json_t *obj = json_object();
    json_object_set_new(obj, "course_id", json_string(c->course_id ? c->course_id : ""));
    json_object_set_new(obj, "name", json_string(c->name ? c->name : ""));
    json_object_set_new(obj, "type", json_string(c->type ? c->type : ""));
    json_object_set_new(obj, "total_hours", json_real(c->total_hours));
    json_object_set_new(obj, "lecture_hours", json_real(c->lecture_hours));
    json_object_set_new(obj, "lab_hours", json_real(c->lab_hours));
    json_object_set_new(obj, "credit", json_real(c->credit));
    json_object_set_new(obj, "semester", json_string(c->semester ? c->semester : ""));
    json_array_append_new(arr, obj);
}

static void student_to_json(const Student *s, void *user) {
    json_t *arr = user;
    json_t *obj = json_object();
    json_object_set_new(obj, "student_id", json_string(s->student_id ? s->student_id : ""));
    json_object_set_new(obj, "name", json_string(s->name ? s->name : ""));
    json_object_set_new(obj, "email", json_string(s->email ? s->email : ""));
    json_object_set_new(obj, "credits", json_real(s->credits));
    json_array_append_new(arr, obj);
}

static void enrollment_to_json(const Enrollment *e, void *user) {
    json_t *arr = user;
    json_t *obj = json_object();
    json_object_set_new(obj, "student_id", json_string(e->student_id ? e->student_id : ""));
    json_object_set_new(obj, "course_id", json_string(e->course_id ? e->course_id : ""));
    json_array_append_new(arr, obj);
}

/* Ping */
int handle_ping(struct mg_connection *conn) {
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

// Course //

int handle_course_add(struct mg_connection *conn) {
    char *body = read_body(conn);
    if (!body) return respond_error(conn, 400, "empty body");
    json_error_t err;
    json_t *j = json_loads(body, 0, &err);
    free(body);
    if (!j) return respond_error(conn, 400, "invalid json");

    json_t *jc_id = json_object_get(j, "course_id");
    json_t *jc_credit = json_object_get(j, "credit");
    if (!json_is_string(jc_id) || !json_is_number(jc_credit)) { json_decref(j); return respond_error(conn, 400, "course_id and credit required"); }

    Course c = {0};
    c.course_id = json_string_value(jc_id);
    c.credit = json_real_value(jc_credit);
    json_t *tmp;
    if ((tmp = json_object_get(j, "name")) && json_is_string(tmp)) c.name = json_string_value(tmp);
    if ((tmp = json_object_get(j, "type")) && json_is_string(tmp)) c.type = json_string_value(tmp);
    if ((tmp = json_object_get(j, "semester")) && json_is_string(tmp)) c.semester = json_string_value(tmp);
    if ((tmp = json_object_get(j, "total_hours")) && json_is_number(tmp)) c.total_hours = json_real_value(tmp);
    if ((tmp = json_object_get(j, "lecture_hours")) && json_is_number(tmp)) c.lecture_hours = json_real_value(tmp);
    if ((tmp = json_object_get(j, "lab_hours")) && json_is_number(tmp)) c.lab_hours = json_real_value(tmp);

    bool ok = db_course_add(&c);
    json_decref(j);
    if (!ok) return respond_error(conn, 500, "failed to add course");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_course_remove(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *course_id = get_qs_param(ri, "course_id");
    if (!course_id) return respond_error(conn, 400, "course_id required");
    bool ok = db_course_remove(course_id);
    free(course_id);
    if (!ok) return respond_error(conn, 500, "failed to remove course");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_course_remove_all(struct mg_connection *conn) {
    bool ok = db_course_remove_all();
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_course_update(struct mg_connection *conn) {
    char *body = read_body(conn);
    if (!body) return respond_error(conn, 400, "empty body");
    json_error_t err;
    json_t *j = json_loads(body, 0, &err);
    free(body);
    if (!j) return respond_error(conn, 400, "invalid json");

    json_t *jc_id = json_object_get(j, "course_id");
    json_t *jc_credit = json_object_get(j, "credit");
    if (!json_is_string(jc_id) || !json_is_number(jc_credit)) { json_decref(j); return respond_error(conn, 400, "course_id and credit required"); }

    Course c = {0};
    c.course_id = json_string_value(jc_id);
    c.credit = json_real_value(jc_credit);
    json_t *tmp;
    if ((tmp = json_object_get(j, "name")) && json_is_string(tmp)) c.name = json_string_value(tmp);
    if ((tmp = json_object_get(j, "type")) && json_is_string(tmp)) c.type = json_string_value(tmp);
    if ((tmp = json_object_get(j, "semester")) && json_is_string(tmp)) c.semester = json_string_value(tmp);
    if ((tmp = json_object_get(j, "total_hours")) && json_is_number(tmp)) c.total_hours = json_real_value(tmp);
    if ((tmp = json_object_get(j, "lecture_hours")) && json_is_number(tmp)) c.lecture_hours = json_real_value(tmp);
    if ((tmp = json_object_get(j, "lab_hours")) && json_is_number(tmp)) c.lab_hours = json_real_value(tmp);

    bool ok = db_course_update(&c);
    json_decref(j);
    if (!ok) return respond_error(conn, 500, "failed to update course");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_course_list(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    QueryOptions *opt = parse_query_options(ri);
    json_t *arr = json_array();
    if (!db_course_list(opt, course_to_json, arr)) { json_decref(arr); return respond_error(conn, 500, "db error"); }
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_course_find_by_id(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *id = get_qs_param(ri, "id");
    if (!id) return respond_error(conn, 400, "id required");
    json_t *arr = json_array();
    if (!db_course_find_by_id(id, NULL, course_to_json, arr)) { free(id); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(id);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_course_find_by_name(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *name = get_qs_param(ri, "name");
    if (!name) return respond_error(conn, 400, "name required");
    json_t *arr = json_array();
    if (!db_course_find_by_name(name, NULL, course_to_json, arr)) { free(name); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(name);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_course_find_by_type(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *type = get_qs_param(ri, "type");
    if (!type) return respond_error(conn, 400, "type required");
    json_t *arr = json_array();
    if (!db_course_find_by_type(type, NULL, course_to_json, arr)) { free(type); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(type);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_course_find_by_semester(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *semester = get_qs_param(ri, "semester");
    if (!semester) return respond_error(conn, 400, "semester required");
    json_t *arr = json_array();
    if (!db_course_find_by_semester(semester, NULL, course_to_json, arr)) { free(semester); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(semester);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

// Enrollment //

int handle_enrollment_add(struct mg_connection *conn) {
    char *body = read_body(conn);
    if (!body) return respond_error(conn, 400, "empty body");
    json_error_t err;
    json_t *j = json_loads(body, 0, &err);
    free(body);
    if (!j) return respond_error(conn, 400, "invalid json");
    json_t *js = json_object_get(j, "student_id");
    json_t *jc = json_object_get(j, "course_id");
    if (!json_is_string(js) || !json_is_string(jc)) { json_decref(j); return respond_error(conn, 400, "student_id and course_id required"); }
    Enrollment e = { json_string_value(jc), json_string_value(js) };
    bool ok = db_enrollment_add(&e);
    json_decref(j);
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_enrollment_remove(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *student_id = get_qs_param(ri, "student_id");
    char *course_id = get_qs_param(ri, "course_id");
    if (!student_id || !course_id) { 
        if (student_id) free(student_id);
        if (course_id) free(course_id);
        return respond_error(conn, 400, "student_id and course_id required"); 
    }
    bool ok = db_enrollment_remove(student_id, course_id);
    free(student_id);
    free(course_id);
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_enrollment_list(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    QueryOptions *opt = parse_query_options(ri);
    json_t *arr = json_array();
    if (!db_enrollment_list(opt, enrollment_to_json, arr)) { json_decref(arr); return respond_error(conn, 500, "db error"); }
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_enrollment_find_by_course_id(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *course_id = get_qs_param(ri, "course_id");
    if (!course_id) return respond_error(conn, 400, "course_id required");
    json_t *arr = json_array();
    if (!db_enrollment_find_by_course_id(course_id, NULL, enrollment_to_json, arr)) { free(course_id); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(course_id);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_enrollment_remove_all(struct mg_connection *conn) {
    bool ok = db_enrollment_remove_all();
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_enrollment_find_by_student_id(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *student_id = get_qs_param(ri, "student_id");
    if (!student_id) return respond_error(conn, 400, "student_id required");
    json_t *arr = json_array();
    if (!db_enrollment_find_by_student_id(student_id, NULL, enrollment_to_json, arr)) { free(student_id); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(student_id);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

// Student //
int handle_student_add(struct mg_connection *conn) {
    char *body = read_body(conn);
    if (!body) return respond_error(conn, 400, "empty body");
    json_error_t err;
    json_t *j = json_loads(body, 0, &err);
    free(body);
    if (!j) return respond_error(conn, 400, "invalid json");
    json_t *js = json_object_get(j, "student_id");
    json_t *jn = json_object_get(j, "name");
    json_t *je = json_object_get(j, "email");
    if (!json_is_string(js) || !json_is_string(jn)) { json_decref(j); return respond_error(conn, 400, "student_id and name required"); }
    // Credits are always initialized to 0 and auto-calculated from enrollments
    Student s = { json_string_value(js), json_string_value(jn), json_is_string(je) ? json_string_value(je) : NULL, 0.0 };
    bool ok = db_student_add(&s);
    json_decref(j);
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_student_remove(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *student_id = get_qs_param(ri, "student_id");
    if (!student_id) return respond_error(conn, 400, "student_id required");
    bool ok = db_student_remove(student_id);
    free(student_id);
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_student_remove_all(struct mg_connection *conn) {
    bool ok = db_student_remove_all();
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_student_update(struct mg_connection *conn) {
    char *body = read_body(conn);
    if (!body) return respond_error(conn, 400, "empty body");
    json_error_t err;
    json_t *j = json_loads(body, 0, &err);
    free(body);
    if (!j) return respond_error(conn, 400, "invalid json");
    json_t *js = json_object_get(j, "student_id");
    json_t *jn = json_object_get(j, "name");
    json_t *je = json_object_get(j, "email");
    json_t *jc = json_object_get(j, "credits");
    if (!json_is_string(js) || !json_is_string(jn)) { json_decref(j); return respond_error(conn, 400, "student_id and name required"); }
    double credits = 0.0;
    if (jc && json_is_number(jc)) credits = json_number_value(jc);
    Student s = { json_string_value(js), json_string_value(jn), json_is_string(je) ? json_string_value(je) : NULL, credits };
    bool ok = db_student_update(&s);
    json_decref(j);
    if (!ok) return respond_error(conn, 500, "db error");
    return respond_json_str(conn, 200, "{ \"ok\": true }");
}

int handle_student_list(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    QueryOptions *opt = parse_query_options(ri);
    json_t *arr = json_array();
    if (!db_student_list(opt, student_to_json, arr)) { json_decref(arr); return respond_error(conn, 500, "db error"); }
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_student_find_by_id(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *id = get_qs_param(ri, "student_id");
    if (!id) return respond_error(conn, 400, "student_id required");
    json_t *arr = json_array();
    if (!db_student_find_by_id(id, NULL, student_to_json, arr)) { free(id); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(id);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}

int handle_student_find_by_name(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    char *name = get_qs_param(ri, "name");
    if (!name) return respond_error(conn, 400, "name required");
    json_t *arr = json_array();
    if (!db_student_find_by_name(name, NULL, student_to_json, arr)) { free(name); json_decref(arr); return respond_error(conn, 500, "db error"); }
    free(name);
    char *s = json_dumps(arr, 0);
    json_decref(arr);
    int r = respond_json_str(conn, 200, s);
    free(s);
    return r;
}
