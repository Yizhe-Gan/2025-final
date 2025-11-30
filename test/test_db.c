#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/db.h"

static int student_found = 0;
static void student_visitor(const Student *s, void *user) {
    int *cnt = user;
    if (!s) return;
    if (strcmp(s->student_id, "s1") == 0 && strcmp(s->name, "Alice") == 0 && s->credits == 2.5) {
        *cnt = 1;
    }
}

static int course_found = 0;
static void course_visitor(const Course *c, void *user) {
    int *cnt = user;
    if (!c) return;
    if (strcmp(c->course_id, "c1") == 0 && strcmp(c->name, "Intro") == 0) {
        *cnt = 1;
    }
}

static int enrollment_found = 0;
static void enrollment_visitor(const Enrollment *e, void *user) {
    int *cnt = user;
    if (!e) return;
    if (strcmp(e->student_id, "s1") == 0 && strcmp(e->course_id, "c1") == 0) {
        *cnt = 1;
    }
}

/* Visitor used by ordering tests to capture course name */
static void order_visitor(const Course *c, void *user) {
    char *out = user;
    if (!c || !out) return;
    strncpy(out, c->name ? c->name : "", 63);
}

int main(void) {
    /* Initialize DB (creates `curriculum.db` in working directory) */
    if (!init_db()) {
        fprintf(stderr, "init_db failed\n");
        return 1;
    }

    /* Clean any previous test artifacts */
    db_enrollment_remove("s1", "c1");
    db_student_remove("s1");
    db_course_remove("c1");

    /* Student tests */
    Student s = { "s1", "Alice", "alice@example.com", 2.5 };
    if (!db_student_add(&s)) { fprintf(stderr, "db_student_add failed\n"); close_db(); return 1; }
    int cnt = 0;
    if (!db_student_find_by_id("s1", NULL, student_visitor, &cnt)) { fprintf(stderr, "db_student_find_by_id failed\n"); close_db(); return 1; }
    if (!cnt) { fprintf(stderr, "student not found or incorrect\n"); close_db(); return 1; }

    /* Negative credits should be rejected by CHECK constraint */
    Student sneg = { "sneg", "Neg", NULL, -1.0 };
    if (db_student_add(&sneg)) { fprintf(stderr, "db_student_add accepted negative credits\n"); close_db(); return 1; }

    /* Course tests */
    Course c = { "c1", "Intro", "Core", 10.0, 5.0, 5.0, 3.0, "Fall" };
    if (!db_course_add(&c)) { fprintf(stderr, "db_course_add failed\n"); close_db(); return 1; }
    cnt = 0;
    if (!db_course_find_by_id("c1", NULL, course_visitor, &cnt)) { fprintf(stderr, "db_course_find_by_id failed\n"); close_db(); return 1; }
    if (!cnt) { fprintf(stderr, "course not found or incorrect\n"); close_db(); return 1; }

    /* Ordering / limit / offset tests */
    Course ca = { "cA", "Alpha", "Core", 1.0, 0.0, 0.0, 1.0, "Fall" };
    Course cb = { "cB", "Beta", "Core", 1.0, 0.0, 0.0, 1.0, "Fall" };
    Course cc = { "cC", "Gamma", "Core", 1.0, 0.0, 0.0, 1.0, "Fall" };
    db_course_add(&ca);
    db_course_add(&cb);
    db_course_add(&cc);

    char got_name[64] = {0};
    QueryOptions opt = { .order_by = "name", .order = SORT_ASC, .limit = 1, .offset = 1 };
    if (!db_course_list(&opt, order_visitor, got_name)) { fprintf(stderr, "db_course_list (ordered) failed\n"); close_db(); return 1; }
    if (strcmp(got_name, "Beta") != 0) { fprintf(stderr, "ordering/limit/offset failed, got '%s'\n", got_name); close_db(); return 1; }

    /* Enrollment tests */
    Enrollment e = { "c1", "s1" };
    if (!db_enrollment_add(&e)) { fprintf(stderr, "db_enrollment_add failed\n"); close_db(); return 1; }
    cnt = 0;
    if (!db_enrollment_find_by_student_id("s1", NULL, enrollment_visitor, &cnt)) { fprintf(stderr, "db_enrollment_find_by_student_id failed\n"); close_db(); return 1; }
    if (!cnt) { fprintf(stderr, "enrollment not found or incorrect\n"); close_db(); return 1; }

    /* Cleanup */
    db_enrollment_remove("s1", "c1");
    db_student_remove("s1");
    db_course_remove("c1");
    close_db();

    /* Remove DB file created by test for hygiene */
    remove("curriculum.db");
    printf("All tests passed\n");
    return 0;
}
