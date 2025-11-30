#pragma once
#include "utils.h"
#include "db.h"
#include "handlers.h"

bool start_server(const char *port);
void stop_server(void);