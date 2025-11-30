#pragma once
#include "utils.h"
#include "db.h"

int handle_ping(struct mg_connection *conn);

int handle_course_add(struct mg_connection *conn);
int handle_course_update(struct mg_connection *conn);
int handle_course_remove(struct mg_connection *conn);
int handle_course_list(struct mg_connection *conn);
int handle_course_find_by_id(struct mg_connection *conn);
int handle_course_find_by_name(struct mg_connection *conn);
int handle_course_find_by_type(struct mg_connection *conn);
int handle_course_find_by_semester(struct mg_connection *conn);

int handle_enrollment_add(struct mg_connection *conn);
int handle_enrollment_remove(struct mg_connection *conn);
int handle_enrollment_list(struct mg_connection *conn);
int handle_enrollment_find_by_course_id(struct mg_connection *conn);
int handle_enrollment_find_by_student_id(struct mg_connection *conn);
int handle_enrollment_remove_all(struct mg_connection *conn);

int handle_student_add(struct mg_connection *conn);
int handle_student_update(struct mg_connection *conn);
int handle_student_remove(struct mg_connection *conn);
int handle_student_remove_all(struct mg_connection *conn);
int handle_student_list(struct mg_connection *conn);
int handle_student_find_by_id(struct mg_connection *conn);
int handle_student_find_by_name(struct mg_connection *conn);
int handle_course_remove_all(struct mg_connection *conn);