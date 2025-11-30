#include "db.h"

sqlite3 *db;

static bool valid_course_order(const char *col) {
    if (!col) return false;
    const char *allowed[] = { "course_id", "name", "type", "total_hours", "lecture_hours", "lab_hours", "credit", "semester" };
    for (size_t i = 0; i < sizeof(allowed)/sizeof(allowed[0]); ++i) {
        if (strcmp(col, allowed[i]) == 0) return true;
    }
    return false;
}

static bool valid_student_order(const char *col) {
    if (!col) return false;
    const char *allowed[] = { "student_id", "name", "email", "credits" };
    for (size_t i = 0; i < sizeof(allowed)/sizeof(allowed[0]); ++i) {
        if (strcmp(col, allowed[i]) == 0) return true;
    }
    return false;
}

static bool valid_enrollment_order(const char *col) {
    if (!col) return false;
    const char *allowed[] = { "student_id", "course_id" };
    for (size_t i = 0; i < sizeof(allowed)/sizeof(allowed[0]); ++i) {
        if (strcmp(col, allowed[i]) == 0) return true;
    }
    return false;
}

bool init_db(void) {
    log_message("Initializing database...", LOG_INFO);
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) {
        log_message(sqlite3_errmsg(db), LOG_WARN);
        sqlite3_close(db);

        remove(DB_FILE);
        rc = sqlite3_open(DB_FILE, &db);
        if (rc != SQLITE_OK) {
            log_message("Failed to create new database file", LOG_ERROR);
            return false;
        }
    }

    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_busy_timeout(db, 5000);

    const char *course =
        "CREATE TABLE IF NOT EXISTS course ("
        "course_id TEXT PRIMARY KEY,"
        "name TEXT,"
        "type TEXT,"
        "total_hours REAL,"
        "lecture_hours REAL,"
        "lab_hours REAL,"
        "credit REAL NOT NULL,"
        "semester TEXT"
        ");";

    const char *student =
        "CREATE TABLE IF NOT EXISTS student ("
        "student_id TEXT PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "email TEXT,"
        "credits REAL NOT NULL DEFAULT 0.0 CHECK(credits >= 0.0)"
        ");";

    const char *enrollment =
        "CREATE TABLE IF NOT EXISTS enrollment ("
        "student_id TEXT NOT NULL,"
        "course_id TEXT NOT NULL,"
        "PRIMARY KEY (student_id, course_id),"
        "FOREIGN KEY (student_id) REFERENCES student(student_id),"
        "FOREIGN KEY (course_id) REFERENCES course(course_id)"
        ");";

    const char* tables[] = { course, student, enrollment };

    for (int i = 0; i < (int)(sizeof(tables) / sizeof(tables[0])); i++) {
        char *errmsg = NULL;
        if (sqlite3_exec(db, tables[i], NULL, NULL, &errmsg) != SQLITE_OK) {
            log_message("Failed to create table", LOG_ERROR);
            if (errmsg) {
                log_message(errmsg, LOG_ERROR);
                sqlite3_free(errmsg);
            }
            return false;
        }
    }
    

    return true;
}

void close_db(void) {
    log_message("Closing database...", LOG_INFO);
    sqlite3_close(db);
}

static bool db_exec(const char *sql, DbValue *values, int value_count) {
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        char buf[256];
        snprintf(buf, sizeof(buf), "db_exec: prepare failed: %s", sqlite3_errmsg(db));
        log_message(buf, LOG_ERROR);
        return false;
    }

    for (int i = 0; i < value_count; i++) {
        int idx = i + 1;
        switch (values[i].type) {
        case DB_NULL:
            sqlite3_bind_null(stmt, idx);
            break;
        case DB_TEXT:
            sqlite3_bind_text(stmt, idx, values[i].text, -1, SQLITE_TRANSIENT);
            break;
        case DB_REAL:
            sqlite3_bind_double(stmt, idx, values[i].d);
            break;
        case DB_INT:
            sqlite3_bind_int(stmt, idx, values[i].i);
            break;
        }
    }

    int step = sqlite3_step(stmt);
    bool ok = step == SQLITE_DONE;
    if (!ok) {
        char buf[256];
        snprintf(buf, sizeof(buf), "db_exec: step failed: %s", sqlite3_errmsg(db));
        log_message(buf, LOG_ERROR);
    }
    sqlite3_finalize(stmt);
    return ok;
}

static bool db_query(const char *base_sql, const QueryOptions *opt, DbValue *values, int value_count, sqlite3_stmt **stmt, const char *entity_type) {
    char query[1024];
    int n = snprintf(query, sizeof(query), "%s", base_sql);
    
    // Add ORDER BY clause if specified and valid
    if (opt && opt->order_by) {
        bool valid = false;
        if (entity_type) {
            if (strcmp(entity_type, "course") == 0) {
                valid = valid_course_order(opt->order_by);
            } else if (strcmp(entity_type, "student") == 0) {
                valid = valid_student_order(opt->order_by);
            } else if (strcmp(entity_type, "enrollment") == 0) {
                valid = valid_enrollment_order(opt->order_by);
            }
        }
        if (valid) {
            n += snprintf(query + n, sizeof(query) - n, " ORDER BY %s %s", opt->order_by, opt->order == SORT_DESC ? "DESC" : "ASC");
        }
    }
    
    // Add LIMIT and OFFSET clauses (OFFSET requires LIMIT in SQLite)
    int param_idx = value_count + 1;
    if (opt && opt->limit > 0) {
        n += snprintf(query + n, sizeof(query) - n, " LIMIT ?");
        if (opt->offset > 0) {
            n += snprintf(query + n, sizeof(query) - n, " OFFSET ?");
        }
    } else if (opt && opt->offset > 0) {
        // If only offset is specified, use a very large limit
        // SQLite requires LIMIT when using OFFSET
        n += snprintf(query + n, sizeof(query) - n, " LIMIT -1 OFFSET ?");
    }

    if (sqlite3_prepare_v2(db, query, -1, stmt, NULL) != SQLITE_OK)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "db_query: prepare failed: %s", sqlite3_errmsg(db));
        log_message(buf, LOG_ERROR);
        return false;
    }

    // Bind WHERE clause parameters
    for (int i = 0; i < value_count; i++) {
        int idx = i + 1;
        switch (values[i].type) {
        case DB_NULL:
            sqlite3_bind_null(*stmt, idx);
            break;
        case DB_TEXT:
            sqlite3_bind_text(*stmt, idx, values[i].text, -1, SQLITE_TRANSIENT);
            break;
        case DB_REAL:
            sqlite3_bind_double(*stmt, idx, values[i].d);
            break;
        case DB_INT:
            sqlite3_bind_int(*stmt, idx, values[i].i);
            break;
        }
    }

    // Bind pagination parameters
    if (opt && opt->limit > 0) {
        sqlite3_bind_int(*stmt, param_idx++, opt->limit);
        if (opt->offset > 0) {
            sqlite3_bind_int(*stmt, param_idx++, opt->offset);
        }
    } else if (opt && opt->offset > 0) {
        // Only offset specified, already added LIMIT -1 in query
        sqlite3_bind_int(*stmt, param_idx++, opt->offset);
    }

    return true;
}

#pragma region Course

static bool db_visit_course(CourseVisitor visitor, void *user, sqlite3_stmt *stmt) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Course c = {
            .course_id = (const char *)sqlite3_column_text(stmt, 0),
            .name      = (const char *)sqlite3_column_text(stmt, 1),
            .type      = (const char *)sqlite3_column_text(stmt, 2),
            .total_hours = sqlite3_column_double(stmt, 3),
            .lecture_hours = sqlite3_column_double(stmt, 4),
            .lab_hours = sqlite3_column_double(stmt, 5),
            .credit    = sqlite3_column_double(stmt, 6),
            .semester  = (const char *)sqlite3_column_text(stmt, 7),
        };
        visitor(&c, user);
    }

    return true;
}

bool db_course_add(const Course *c) {
    const char *sql =
        "INSERT INTO course VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    DbValue v[] = {
        { DB_TEXT, .text = c->course_id },
        { c->name ? DB_TEXT : DB_NULL, .text = c->name },
        { c->type ? DB_TEXT : DB_NULL, .text = c->type },
        { DB_REAL, .d = c->total_hours },
        { DB_REAL, .d = c->lecture_hours },
        { DB_REAL, .d = c->lab_hours },
        { DB_REAL, .d = c->credit },
        { c->semester ? DB_TEXT : DB_NULL, .text = c->semester }
    };

    return db_exec(sql, v, 8);
}

bool db_course_update(const Course *c) {
    const char *sql =
        "UPDATE course SET name = ?, type = ?, total_hours = ?, lecture_hours = ?, "
        "lab_hours = ?, credit = ?, semester = ? WHERE course_id = ?;";

    DbValue v[] = {
        { c->name ? DB_TEXT : DB_NULL, .text = c->name },
        { c->type ? DB_TEXT : DB_NULL, .text = c->type },
        { DB_REAL, .d = c->total_hours },
        { DB_REAL, .d = c->lecture_hours },
        { DB_REAL, .d = c->lab_hours },
        { DB_REAL, .d = c->credit },
        { c->semester ? DB_TEXT : DB_NULL, .text = c->semester },
        { DB_TEXT, .text = c->course_id }
    };

    return db_exec(sql, v, 8);
}

bool db_course_remove(const char *course_id) {
    // First remove all enrollments for this course
    const char *sql_enrollments = "DELETE FROM enrollment WHERE course_id = ?;";
    DbValue v_enroll[] = { { DB_TEXT, .text = course_id } };
    if (!db_exec(sql_enrollments, v_enroll, 1)) return false;

    // Then remove the course
    const char *sql = "DELETE FROM course WHERE course_id = ?;";
    DbValue v[] = { { DB_TEXT, .text = course_id } };

    return db_exec(sql, v, 1);
}

bool db_course_list(const QueryOptions *opt, CourseVisitor visitor, void *user) {
    sqlite3_stmt *stmt;
    char sql[256] =
        "SELECT course_id, name, type, total_hours, lecture_hours, lab_hours, credit, semester "
        "FROM course";
    
    if(!db_query(sql, opt, NULL, 0, &stmt, "course")) return false;
    if(!db_visit_course(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_course_find_by_id(const char *course_id, const QueryOptions *opt,  CourseVisitor visitor, void *user) {
    sqlite3_stmt *stmt;
    
    char sql[256] =
        "SELECT course_id, name, type, total_hours, lecture_hours, lab_hours, credit, semester "
        "FROM course WHERE course_id = ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = course_id }}, 1, &stmt, "course")) return false;
    if (!db_visit_course(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_course_find_by_name(const char *name, const QueryOptions *opt, CourseVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT course_id, name, type, total_hours, lecture_hours, lab_hours, credit, semester "
        "FROM course WHERE name LIKE ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = name }}, 1, &stmt, "course")) return false;
    if (!db_visit_course(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_course_find_by_type(const char *type, const QueryOptions *opt, CourseVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT course_id, name, type, total_hours, lecture_hours, lab_hours, credit, semester "
        "FROM course WHERE type LIKE ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = type }}, 1, &stmt, "course")) return false;
    if (!db_visit_course(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_course_find_by_semester(const char *semester, const QueryOptions *opt, CourseVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT course_id, name, type, total_hours, lecture_hours, lab_hours, credit, semester "
        "FROM course WHERE semester LIKE ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = semester }}, 1, &stmt, "course")) return false;
    if (!db_visit_course(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}  

bool db_course_remove_all(void) {
    // Remove all enrollments first to maintain consistency
    const char *sql_del_enr = "DELETE FROM enrollment;";
    if (!db_exec(sql_del_enr, NULL, 0)) return false;

    const char *sql_del = "DELETE FROM course;";
    if (!db_exec(sql_del, NULL, 0)) return false;

    return true;
}

#pragma endregion Course

#pragma region Enrollment

static bool db_visit_enrollment(EnrollmentVisitor visitor, void *user, sqlite3_stmt *stmt) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Enrollment e = {
            .student_id = (const char *)sqlite3_column_text(stmt, 0),
            .course_id  = (const char *)sqlite3_column_text(stmt, 1),
        };
        visitor(&e, user);
    }

    return true;
}

bool db_enrollment_add(const Enrollment *e) {
    // First verify both student and course exist
    sqlite3_stmt *stmt;
    const char *check_student = "SELECT student_id FROM student WHERE student_id = ?;";
    if (sqlite3_prepare_v2(db, check_student, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, e->student_id, -1, SQLITE_TRANSIENT);
    int student_exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    if (!student_exists) return false;

    const char *check_course = "SELECT credit FROM course WHERE course_id = ?;";
    if (sqlite3_prepare_v2(db, check_course, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, e->course_id, -1, SQLITE_TRANSIENT);
    int course_exists = sqlite3_step(stmt) == SQLITE_ROW;
    double course_credit = 0.0;
    if (course_exists) {
        course_credit = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (!course_exists) return false;

    // Add enrollment
    const char *sql = "INSERT INTO enrollment VALUES (?, ?);";
    DbValue v[] = {
        { DB_TEXT, .text = e->student_id },
        { DB_TEXT, .text = e->course_id }
    };
    if (!db_exec(sql, v, 2)) return false;

    // Update student credits
    const char *update_credits = "UPDATE student SET credits = credits + ? WHERE student_id = ?;";
    DbValue v_credits[] = {
        { DB_REAL, .d = course_credit },
        { DB_TEXT, .text = e->student_id }
    };
    return db_exec(update_credits, v_credits, 2);
}

bool db_enrollment_remove(const char *student_id, const char *course_id) {
    // Get course credit before removing
    sqlite3_stmt *stmt;
    const char *get_credit = "SELECT credit FROM course WHERE course_id = ?;";
    if (sqlite3_prepare_v2(db, get_credit, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, course_id, -1, SQLITE_TRANSIENT);
    double course_credit = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        course_credit = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);

    // Remove enrollment
    const char *sql = "DELETE FROM enrollment WHERE student_id = ? AND course_id = ?;";
    DbValue v[] = {
        { DB_TEXT, .text = student_id },
        { DB_TEXT, .text = course_id }
    };
    if (!db_exec(sql, v, 2)) return false;

    // Update student credits
    const char *update_credits = "UPDATE student SET credits = credits - ? WHERE student_id = ?;";
    DbValue v_credits[] = {
        { DB_REAL, .d = course_credit },
        { DB_TEXT, .text = student_id }
    };
    return db_exec(update_credits, v_credits, 2);
}

bool db_enrollment_remove_all(void) {
    const char *sql_del = "DELETE FROM enrollment;";
    if (!db_exec(sql_del, NULL, 0)) return false;

    const char *sql_reset = "UPDATE student SET credits = 0.0;";
    if (!db_exec(sql_reset, NULL, 0)) return false;

    return true;
}

bool db_enrollment_list(const QueryOptions *opt, EnrollmentVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT student_id, course_id "
        "FROM enrollment";

    if (!db_query(sql, opt, NULL, 0, &stmt, "enrollment")) return false;
    if (!db_visit_enrollment(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_enrollment_find_by_student_id(const char *student_id, const QueryOptions *opt, EnrollmentVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT student_id, course_id "
        "FROM enrollment WHERE student_id = ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = student_id }}, 1, &stmt, "enrollment")) return false;
    if (!db_visit_enrollment(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_enrollment_find_by_course_id(const char *course_id, const QueryOptions *opt, EnrollmentVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT student_id, course_id "
        "FROM enrollment WHERE course_id = ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = course_id }}, 1, &stmt, "enrollment")) return false;
    if (!db_visit_enrollment(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

#pragma endregion Enrollment

#pragma region Student

static bool db_visit_student(StudentVisitor visitor, void *user, sqlite3_stmt *stmt) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Student s = {
            .student_id = (const char *)sqlite3_column_text(stmt, 0),
            .name       = (const char *)sqlite3_column_text(stmt, 1),
            .email      = (const char *)sqlite3_column_text(stmt, 2),
            .credits    = sqlite3_column_double(stmt, 3),
        };
        visitor(&s, user);
    }

    return true;
}

bool db_student_add(const Student *s) {
    const char *sql =
        "INSERT INTO student VALUES (?, ?, ?, 0.0);";
    DbValue v[] = {
        { DB_TEXT, .text = s->student_id },
        { DB_TEXT, .text = s->name },
        { s->email ? DB_TEXT : DB_NULL, .text = s->email }
    };
    return db_exec(sql, v, 3);
}

bool db_student_update(const Student *s) {
    const char *sql =
        "UPDATE student SET name = ?, email = ?, credits = ? WHERE student_id = ?;";
    DbValue v[] = {
        { DB_TEXT, .text = s->name },
        { s->email ? DB_TEXT : DB_NULL, .text = s->email },
        { DB_REAL, .d = s->credits },
        { DB_TEXT, .text = s->student_id }
    };
    return db_exec(sql, v, 4);
}

bool db_student_remove(const char *student_id) {
    // First remove all enrollments for this student
    const char *sql_enrollments = "DELETE FROM enrollment WHERE student_id = ?;";
    DbValue v_enroll[] = { { DB_TEXT, .text = student_id } };
    if (!db_exec(sql_enrollments, v_enroll, 1)) return false;

    // Then remove the student
    const char *sql = "DELETE FROM student WHERE student_id = ?;";
    DbValue v[] = { { DB_TEXT, .text = student_id } };
    return db_exec(sql, v, 1);
}

bool db_student_remove_all(void) {
    // Remove all enrollments first
    const char *sql_del_enr = "DELETE FROM enrollment;";
    if (!db_exec(sql_del_enr, NULL, 0)) return false;

    // Then remove all students
    const char *sql_del = "DELETE FROM student;";
    if (!db_exec(sql_del, NULL, 0)) return false;

    return true;
}

bool db_student_list(const QueryOptions *opt, StudentVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT student_id, name, email, credits "
        "FROM student";

    if (!db_query(sql, opt, NULL, 0, &stmt, "student")) return false;
    if (!db_visit_student(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_student_find_by_id(const char *student_id, const QueryOptions *opt, StudentVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT student_id, name, email, credits "
        "FROM student WHERE student_id = ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = student_id }}, 1, &stmt, "student")) return false;
    if (!db_visit_student(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

bool db_student_find_by_name(const char *name, const QueryOptions *opt, StudentVisitor visitor, void *user) {
    sqlite3_stmt *stmt;

    char sql[256] =
        "SELECT student_id, name, email, credits "
        "FROM student WHERE name LIKE ?";

    if (!db_query(sql, opt, (DbValue[]){{ DB_TEXT, .text = name }}, 1, &stmt, "student")) return false;
    if (!db_visit_student(visitor, user, stmt)) return false;

    sqlite3_finalize(stmt);
    return true;
}

#pragma endregion Student