#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <curses.h>
#include <jansson.h>
#include <locale.h>

/* Simple buffer to capture HTTP responses */
struct mem_chunk {
    char *ptr;
    size_t len;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userp) {
    size_t realsz = size * nmemb;
    struct mem_chunk *m = (struct mem_chunk *)userp;
    char *tmp = realloc(m->ptr, m->len + realsz + 1);
    if (!tmp) return 0;
    m->ptr = tmp;
    memcpy(m->ptr + m->len, ptr, realsz);
    m->len += realsz;
    m->ptr[m->len] = '\0';
    return realsz;
}

static char *http_get(const char *url) {
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    struct mem_chunk m = { NULL, 0 };
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &m);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        free(m.ptr);
        return NULL;
    }
    return m.ptr; 
}

static char *http_post_json(const char *url, const char *json) {
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    struct mem_chunk m = { NULL, 0 };
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &m);
    CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        free(m.ptr);
        return NULL;
    }
    return m.ptr;
}

static char *url_encode(const char *s) {
    if (!s) return NULL;
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    char *esc = curl_easy_escape(c, s, 0);
    curl_easy_cleanup(c);
    if (!esc) return NULL;
    char *res = strdup(esc);
    curl_free(esc);
    return res;
}

static void trim(char *s) {
    if (!s) return;
    // trim leading
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    // trim trailing
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}

static void draw_menu(WINDOW *win) {
    clear();  // Clear the entire screen
    werase(win);
    wclear(win);
    box(win, 0, 0);
    if (has_colors()) wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 1, 2, "Curriculum Management System CLI");
    if (has_colors()) wattroff(win, COLOR_PAIR(1) | A_BOLD);
    
    if (has_colors()) wattron(win, COLOR_PAIR(2));
    mvwprintw(win, 3, 2, "1) Test Server Connection");
    mvwprintw(win, 4, 2, "2) List All Courses");
    mvwprintw(win, 5, 2, "3) Add Student");
    mvwprintw(win, 6, 2, "4) Enroll Student in Course");
    mvwprintw(win, 7, 2, "5) Find Course by Name");
    mvwprintw(win, 8, 2, "6) Find Course by Type");
    mvwprintw(win, 9, 2, "7) View Student Enrollments");
    mvwprintw(win, 10, 2, "q) Quit");
    if (has_colors()) wattroff(win, COLOR_PAIR(2));
    
    if (has_colors()) wattron(win, COLOR_PAIR(3));
    mvwprintw(win, 12, 2, "Select an option:");
    if (has_colors()) wattroff(win, COLOR_PAIR(3));
    wrefresh(win);
}

static void show_text(WINDOW *win, const char *title, const char *text) {
    werase(win);
    box(win, 0, 0);
    if (has_colors()) wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 1, 2, "%s", title);
    if (has_colors()) wattroff(win, COLOR_PAIR(1) | A_BOLD);
    int row = 3;
    const char *p = text ? text : "(无内容)";
    char line[256];
    while (*p) {
        size_t i = 0;
        while (*p && *p != '\n' && i < sizeof(line)-1) line[i++] = *p++;
        line[i] = '\0';
        if (has_colors()) wattron(win, COLOR_PAIR(2));
        mvwprintw(win, row++, 2, "%s", line);
        if (has_colors()) wattroff(win, COLOR_PAIR(2));
        if (*p == '\n') p++;
        if (row >= LINES-2) break;
    }
    if (has_colors()) wattron(win, COLOR_PAIR(3));
    mvwprintw(win, LINES-2, 2, "Press any key to continue...");
    if (has_colors()) wattroff(win, COLOR_PAIR(3));
    wrefresh(win);
    wgetch(win);
}

static void input_field(WINDOW *win, int y, int x, char *buf, size_t bufsz) {
    echo();
    mvwgetnstr(win, y, x, buf, (int)(bufsz - 1));
    noecho();
}

int main(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    // Initialize colors
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);    // Title/headers
        init_pair(2, COLOR_GREEN, -1);   // Success/data
        init_pair(3, COLOR_YELLOW, -1);  // Warnings/prompts
        init_pair(4, COLOR_RED, -1);     // Errors
    }

    WINDOW *mainw = newwin(LINES, COLS, 0, 0);
    draw_menu(mainw);

    curl_global_init(CURL_GLOBAL_ALL);

    int done = 0;
    while (!done) {
        int ch = wgetch(mainw);
        switch (ch) {
            case '1': {
                char *res = http_get("http://localhost:8080/ping");
                show_text(mainw, "Test Result", res ? res : "(Request failed)");
                free(res);
                draw_menu(mainw);
                break;
            }

            case '2': {
                /* List courses with client-side pagination */
                char *res = http_get("http://localhost:8080/course");
                if (!res) {
                    show_text(mainw, "Course List", "(Request failed)");
                    draw_menu(mainw);
                    break;
                }
                json_error_t err;
                json_t *j = json_loads(res, 0, &err);
                free(res);
                if (!j || !json_is_array(j)) {
                    if (j) json_decref(j);
                    show_text(mainw, "Course List", "(Invalid JSON)");
                    draw_menu(mainw);
                    break;
                }

                size_t per_page = 5;
                size_t total = json_array_size(j);
                size_t page = 0;
                int page_done = 0;
                while (!page_done) {
                    werase(mainw);
                    box(mainw, 0, 0);
                    if (has_colors()) wattron(mainw, COLOR_PAIR(1));
                    mvwprintw(mainw, 1, 2, "Course List (Page %zu/%zu)", page + 1, (total + per_page - 1) / per_page);
                    if (has_colors()) wattroff(mainw, COLOR_PAIR(1));
                    size_t start = page * per_page;
                    for (size_t i = 0; i < per_page && start + i < total; ++i) {
                        json_t *ci = json_array_get(j, start + i);
                        const char *cid = json_string_value(json_object_get(ci, "course_id"));
                        const char *name = json_string_value(json_object_get(ci, "name"));
                        double credit = json_real_value(json_object_get(ci, "credit"));
                        if (has_colors()) wattron(mainw, COLOR_PAIR(2));
                        mvwprintw(mainw, 3 + i, 2, "%s: %s (Credits: %.1f)", cid ? cid : "", name ? name : "", credit);
                        if (has_colors()) wattroff(mainw, COLOR_PAIR(2));
                    }
                    if (has_colors()) wattron(mainw, COLOR_PAIR(3));
                    mvwprintw(mainw, LINES-3, 2, "n: Next page, p: Previous page, Enter: Exit list");
                    if (has_colors()) wattroff(mainw, COLOR_PAIR(3));
                    wrefresh(mainw);
                    int k = wgetch(mainw);
                    if (k == 'n' && (start + per_page) < total) page++;
                    else if (k == 'p' && page > 0) page--;
                    else page_done = 1;
                }
                json_decref(j);
                draw_menu(mainw);
                break;
            }

            case '3': {
                WINDOW *f = newwin(10, COLS-4, 2, 2);
                box(f, 0, 0);
                if (has_colors()) wattron(f, COLOR_PAIR(1));
                mvwprintw(f, 1, 2, "Add Student");
                if (has_colors()) wattroff(f, COLOR_PAIR(1));
                mvwprintw(f, 3, 2, "Student ID: ");
                mvwprintw(f, 4, 2, "Name: ");
                mvwprintw(f, 5, 2, "Email: ");
                char sid[64] = {0}, namebuf[128] = {0}, email[128] = {0};
                wrefresh(f);
                input_field(f, 3, 9, sid, sizeof(sid)); trim(sid);
                input_field(f, 4, 9, namebuf, sizeof(namebuf)); trim(namebuf);
                input_field(f, 5, 9, email, sizeof(email)); trim(email);

                /* Build JSON using jansson to ensure proper escaping */
                json_t *root = json_object();
                json_object_set_new(root, "student_id", json_string(sid));
                json_object_set_new(root, "name", json_string(namebuf));
                json_object_set_new(root, "email", json_string(email));
                json_object_set_new(root, "credits", json_real(0.0));
                char *s = json_dumps(root, 0);
                json_decref(root);

                char url[256];
                snprintf(url, sizeof(url), "http://localhost:8080/student");
                char *res = http_post_json(url, s);
                free(s);
                show_text(mainw, "Add Student Result", res ? res : "(Request failed)");
                free(res);
                delwin(f);
                draw_menu(mainw);
                break;
            }

            case '4': {
                /* Enroll student in course */
                WINDOW *f = newwin(8, COLS-4, 2, 2);
                box(f, 0, 0);
                if (has_colors()) wattron(f, COLOR_PAIR(1));
                mvwprintw(f, 1, 2, "Enroll Student in Course");
                if (has_colors()) wattroff(f, COLOR_PAIR(1));
                mvwprintw(f, 3, 2, "Student ID: ");
                mvwprintw(f, 4, 2, "Course ID: ");
                char sid[64] = {0}, cid[64] = {0};
                wrefresh(f);
                input_field(f, 3, 9, sid, sizeof(sid)); trim(sid);
                input_field(f, 4, 13, cid, sizeof(cid)); trim(cid);
                json_t *root = json_object();
                json_object_set_new(root, "student_id", json_string(sid));
                json_object_set_new(root, "course_id", json_string(cid));
                char *s = json_dumps(root, 0);
                json_decref(root);
                char url[256];
                snprintf(url, sizeof(url), "http://localhost:8080/enrollment");
                char *res = http_post_json(url, s);
                free(s);
                show_text(mainw, "Enrollment Result", res ? res : "(Request failed)");
                free(res);
                delwin(f);
                draw_menu(mainw);
                break;
            }

            case '5':
            case '6': {
                /* Find by name or type */
                int mode = (ch == '5') ? 0 : 1; // 0=name,1=type
                WINDOW *f = newwin(6, COLS-4, 2, 2);
                box(f, 0, 0);
                if (has_colors()) wattron(f, COLOR_PAIR(1));
                mvwprintw(f, 1, 2, mode == 0 ? "Find Course by Name" : "Find Course by Type");
                if (has_colors()) wattroff(f, COLOR_PAIR(1));
                mvwprintw(f, 3, 2, mode == 0 ? "Name: " : "Type: ");
                char qry[128] = {0};
                wrefresh(f);
                input_field(f, 3, mode == 0 ? 9 : 9, qry, sizeof(qry)); trim(qry);
                delwin(f);
                char *enc = url_encode(qry);
                if (!enc) { show_text(mainw, "Search Result", "(Encoding failed)"); draw_menu(mainw); break; }
                char url[256];
                if (mode == 0)
                    snprintf(url, sizeof(url), "http://localhost:8080/course/find?name=%s", enc);
                else
                    snprintf(url, sizeof(url), "http://localhost:8080/course/find?type=%s", enc);
                free(enc);
                char *res = http_get(url);
                if (!res) { show_text(mainw, "Search Result", "(Request failed)"); draw_menu(mainw); break; }
                /* Show raw JSON or parse and pretty print */
                json_error_t err;
                json_t *j = json_loads(res, 0, &err);
                free(res);
                if (!j) { show_text(mainw, "Search Result", "(Invalid JSON)"); draw_menu(mainw); break; }
                /* Pretty print results */
                char *pretty = json_dumps(j, JSON_INDENT(2));
                show_text(mainw, "Search Result", pretty);
                free(pretty);
                json_decref(j);
                draw_menu(mainw);
                break;
            }

            case '7': {
                /* View student enrollments and statistics */
                WINDOW *f = newwin(6, COLS-4, 2, 2);
                box(f, 0, 0);
                if (has_colors()) wattron(f, COLOR_PAIR(1));
                mvwprintw(f, 1, 2, "View Student Enrollments");
                if (has_colors()) wattroff(f, COLOR_PAIR(1));
                mvwprintw(f, 3, 2, "Enter Student ID: ");
                char sid[64] = {0};
                wrefresh(f);
                input_field(f, 3, 15, sid, sizeof(sid)); trim(sid);
                delwin(f);
                
                if (strlen(sid) == 0) {
                    show_text(mainw, "Error", "Student ID cannot be empty");
                    draw_menu(mainw);
                    break;
                }
                
                /* Get student info */
                char *enc_sid = url_encode(sid);
                if (!enc_sid) { show_text(mainw, "Error", "(Encoding failed)"); draw_menu(mainw); break; }
                
                char url[256];
                snprintf(url, sizeof(url), "http://localhost:8080/student/find?student_id=%s", enc_sid);
                char *student_res = http_get(url);
                
                snprintf(url, sizeof(url), "http://localhost:8080/enrollment?student_id=%s", enc_sid);
                char *enroll_res = http_get(url);
                free(enc_sid);
                
                if (!student_res || !enroll_res) {
                    show_text(mainw, "Error", "(Request failed)");
                    free(student_res);
                    free(enroll_res);
                    draw_menu(mainw);
                    break;
                }
                
                json_error_t err;
                json_t *students = json_loads(student_res, 0, &err);
                json_t *enrollments = json_loads(enroll_res, 0, &err);
                free(student_res);
                free(enroll_res);
                
                if (!students || !enrollments || !json_is_array(students) || !json_is_array(enrollments)) {
                    if (students) json_decref(students);
                    if (enrollments) json_decref(enrollments);
                    show_text(mainw, "Error", "(Invalid JSON)");
                    draw_menu(mainw);
                    break;
                }
                
                if (json_array_size(students) == 0) {
                    json_decref(students);
                    json_decref(enrollments);
                    show_text(mainw, "Error", "Student not found");
                    draw_menu(mainw);
                    break;
                }
                
                json_t *student = json_array_get(students, 0);
                const char *name = json_string_value(json_object_get(student, "name"));
                double total_credits = json_real_value(json_object_get(student, "credits"));
                size_t course_count = json_array_size(enrollments);
                
                /* Get course details for each enrollment */
                werase(mainw);
                box(mainw, 0, 0);
                if (has_colors()) wattron(mainw, COLOR_PAIR(1) | A_BOLD);
                mvwprintw(mainw, 1, 2, "Student Enrollment Details");
                if (has_colors()) wattroff(mainw, COLOR_PAIR(1) | A_BOLD);
                
                if (has_colors()) wattron(mainw, COLOR_PAIR(2));
                mvwprintw(mainw, 3, 2, "Student ID: %s", sid);
                mvwprintw(mainw, 4, 2, "Name: %s", name ? name : "-");
                mvwprintw(mainw, 5, 2, "Total Credits: %.1f", total_credits);
                mvwprintw(mainw, 6, 2, "Enrolled Courses: %zu", course_count);
                if (has_colors()) wattroff(mainw, COLOR_PAIR(2));
                
                int row = 8;
                if (course_count > 0) {
                    if (has_colors()) wattron(mainw, COLOR_PAIR(1));
                    mvwprintw(mainw, row++, 2, "Enrolled Courses:");
                    if (has_colors()) wattroff(mainw, COLOR_PAIR(1));
                    for (size_t i = 0; i < course_count && row < LINES - 3; ++i) {
                        json_t *enroll = json_array_get(enrollments, i);
                        const char *cid = json_string_value(json_object_get(enroll, "course_id"));
                        if (has_colors()) wattron(mainw, COLOR_PAIR(2));
                        mvwprintw(mainw, row++, 4, "- %s", cid ? cid : "");
                        if (has_colors()) wattroff(mainw, COLOR_PAIR(2));
                    }
                } else {
                    if (has_colors()) wattron(mainw, COLOR_PAIR(3));
                    mvwprintw(mainw, row++, 2, "This student has not enrolled in any courses yet");
                    if (has_colors()) wattroff(mainw, COLOR_PAIR(3));
                }
                
                json_decref(students);
                json_decref(enrollments);
                
                if (has_colors()) wattron(mainw, COLOR_PAIR(3));
                mvwprintw(mainw, LINES-2, 2, "Press any key to continue...");
                if (has_colors()) wattroff(mainw, COLOR_PAIR(3));
                wrefresh(mainw);
                wgetch(mainw);
                draw_menu(mainw);
                break;
            }

            case 'q':
            case 'Q':
                done = 1;
                break;

            default:
                break;
        }
    }

    curl_global_cleanup();
    delwin(mainw);
    endwin();
    return 0;
}