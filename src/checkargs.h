#ifndef CHECKARGS_H
#define CHECKARGS_H

#include <stdbool.h>
#include "errors.h"

#define ARGS_FOR_CLIENT 4
#define MIN_ARGS_FOR_SERVER 2
#define MAX_ARGS_FOR_SERVER 3
#define MIN_PORT_RANGE 1024
#define MAX_PORT_RANGE 65535

bool is_file(char* filePath);
void check_client_args(int argc, char** argv);
const char* check_server_args(int argc, char** argv);

#endif