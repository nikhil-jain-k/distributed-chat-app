#ifndef SERVERCOMMANDS_H
#define SERVERCOMMANDS_H

#include "errors.h"
#include "parser.h"

#define NO_OF_SVR_CMDS 9
#define NO_OF_SVR_CMDS_STDOUT_EMIT 4


typedef struct ServerCommands {
    char* command;
    int switchCaseNo;    
} ServerCommands;

// Structure to store a client's name and an appending number that is added
// when NAME_TAKEN: is received.
typedef struct ClientId {
    char* name;
    int number; 
} ClientId;

typedef struct ServerIO {
    FILE* rdEnd;
    FILE* wrEnd;
    char* authStr;
    int noOfOk; // no. of OK: sent by server
    ClientId* client;
} ServerIO;

int evaluate_server_input(char* svrInput);
void return_authorization_value(ServerIO* svr);
char* is_authorized(FILE* svrRdEnd);
void compute_server_who(ServerIO* svr);
int stdout_type(char* inputCmd);
void compute_server_msg(char* strAfterCommand);
void display_to_stdout(ClientId* client, char* svrInput);

#endif