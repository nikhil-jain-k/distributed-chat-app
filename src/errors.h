#ifndef ERRORS_H
#define ERRORS_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define NORMAL_EXIT 0
#define USAGE_ERR_CODE 1
#define COMMS_ERR_CODE 2
#define KICKED_ERR_CODE 3
#define AUTH_ERR_CODE 4

void client_usage_error(void);
void client_kicked(void);
void auth_error(void);
void server_usage_error(void);
void communications_error(void);

#endif