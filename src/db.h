#pragma once
#include "utils.h"

#define DB_FILE "curriculum.db"

typedef enum {
    DB_NULL,
    DB_TEXT,
    DB_REAL,
    DB_INT
} DbValueType;

typedef struct {
    DbValueType type;
    union {
        const char *text;
        int i;
        double d;
    };
} DbValue;

typedef enum {
    SORT_ASC,
    SORT_DESC
} SortOrder;

typedef struct {
    const char *order_by;   // NULL = default
    SortOrder order;

    int limit;              // <= 0 = default
    int offset;             // >= 0
} QueryOptions;


bool init_db(void);
void close_db(void);



// Course //

typedef struct {
    const char *course_id;   // required
    const char *name;
    const char *type;
    double total_hours;
    double lecture_hours;
    double lab_hours;
    double credit;           // required
    const char *semester;
} Course;

typedef void (*CourseVisitor)(const Course *, void *);

bool db_course_add(const Course *course);
bool db_course_update(const Course *course);
bool db_course_remove(const char *course_id);
bool db_course_list(const QueryOptions *opt, CourseVisitor visitor, void *user);
bool db_course_find_by_id(const char *course_id, const QueryOptions *opt,  CourseVisitor visitor, void *user);
bool db_course_find_by_name(const char *name, const QueryOptions *opt, CourseVisitor visitor, void *user);
bool db_course_find_by_type(const char *type, const QueryOptions *opt, CourseVisitor visitor, void *user);
bool db_course_find_by_semester(const char *semester, const QueryOptions *opt, CourseVisitor visitor, void *user);



// Enrollment //

typedef struct {
    const char *course_id;// required
    const char *student_id;   // required
} Enrollment;

typedef void (*EnrollmentVisitor)(const Enrollment *, void *);

bool db_enrollment_add(const Enrollment *enrollment);
bool db_enrollment_remove(const char *student_id, const char *course_id);
bool db_enrollment_list(const QueryOptions *opt, EnrollmentVisitor visitor, void *user);
bool db_enrollment_find_by_course_id(const char *course_id, const QueryOptions *opt, EnrollmentVisitor visitor, void *user);
bool db_enrollment_find_by_student_id(const char *student_id, const QueryOptions *opt, EnrollmentVisitor visitor, void *user);
bool db_enrollment_remove_all(void);



// Student //

typedef struct {
    const char *student_id;   // required
    const char *name;
    const char *email;
    double credits;          // total credits the student has taken (>= 0)
} Student;

typedef void (*StudentVisitor)(const Student *, void *);

bool db_student_add(const Student *student);
bool db_student_update(const Student *student);
bool db_student_remove(const char *student_id);
bool db_student_list(const QueryOptions *opt, StudentVisitor visitor, void *user);
bool db_student_find_by_id(const char *student_id, const QueryOptions *opt, StudentVisitor visitor, void *user);
bool db_student_find_by_name(const char *name, const QueryOptions *opt, StudentVisitor visitor, void *user);
bool db_student_remove_all(void);

// Delete all courses/enrollments/students
bool db_course_remove_all(void);
