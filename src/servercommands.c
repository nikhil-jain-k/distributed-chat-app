#include "servercommands.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

// Takes the server input and returns the index at which a specific command is
// recognized to a switch case.
int evaluate_server_input(char* svrInput) {
    int idx;
    char* savePtr;
    char* prefix = strdup(svrInput);
    strtok_r(prefix, COLON, &savePtr);
    ServerCommands svrCommands[NO_OF_SVR_CMDS] = { {"AUTH", 0}, {"WHO", 1},
            {"NAME_TAKEN", 2}, {"OK", 3}, {"ENTER", 4}, {"MSG", 4}, 
            {"LEAVE", 4}, {"LIST", 4}, {"KICK", 5}};
    
    for (idx = 0; idx < NO_OF_SVR_CMDS; idx++) {
        if (!strcmp(prefix, svrCommands[idx].command)) {
            free(prefix);
            return svrCommands[idx].switchCaseNo;
        }
    }
    free(prefix);
    return idx;
}

// Takes the pointer to a ServerIO struct as @param and if the authentication
// string in ServerIO is not NULL then writes the string in a server readable
// format to the server's write end and flushes the write end after writing.
void return_authorization_value(ServerIO* svr) {
    if (svr->authStr != NULL) {
        fprintf(svr->wrEnd, "AUTH:%s\n", svr->authStr);
        fflush(svr->wrEnd);
    } else {
        fprintf(svr->wrEnd, "AUTH:\n");
        fflush(svr->wrEnd);
    }
}

// Takes a server's read end as parameter and returns the immediate next line
// read from the server. Usually called after responding to server's "AUTH:".
char* is_authorized(FILE* svrRdEnd) {
    return get_line(svrRdEnd);
}

// Takes the pointer to the ServerIO struct as @param and returns the name of
// the current client.
void compute_server_who(ServerIO* svr) {
    if (svr->client->number == -1) { // initially set to -1
        fprintf(svr->wrEnd, "NAME:%s\n", svr->client->name); 
    } else {
        fprintf(svr->wrEnd, "NAME:%s%d\n", svr->client->name, 
                svr->client->number);
    }
    fflush(svr->wrEnd);
}

// Takes the server command input as parameter and checks if it matches with
// any server command that issues message to client's stdout (ENTER:,LEAVE:,
// LIST:, MSG:). If match is found returns the switchNo i.e used to switch
// to the corresponding case.
int stdout_type(char* inputCmd) {
    char* svrCommands[] = {"ENTER", "LEAVE", "LIST", "MSG"};
    int switchNo = 0;
    while (switchNo < NO_OF_SVR_CMDS_STDOUT_EMIT) {
        if (!strcmp(inputCmd, svrCommands[switchNo])) {
            break;
        }
        switchNo++;
    }
    return switchNo;
}

// Takes the name and message received from the server's "MSG:name:message" 
// command as @param and returns the msg to stdout if bith are not NULL.
void compute_server_msg(char* strAfterCommand) {
    if (after_colon(strAfterCommand)) {
        char* message = NULL;
        strtok_r(strAfterCommand, COLON, &message);
        if (message != NULL) {
            fprintf(stdout, "%s: %s\n", strAfterCommand, message);
            fflush(stdout);
        }
    }
}

// Takes a pointer to the ClientId struct and the input received from the
// server as @param. Checks the type of command received from the server
// and returns the appropriate stdout message. Ignores if command is not any
// of "ENTER:", "LEAVE:", "LIST:" or "MSG:" from the server in correct syntax.
void display_to_stdout(ClientId* client, char* svrInput) {
    if (after_colon(svrInput)) {
        char* strAfterCommand;
        strtok_r(svrInput, COLON, &strAfterCommand);
        switch (stdout_type(svrInput)) {
            case 0: // ENTER:
                fprintf(stdout, "(%s has entered the chat)\n",
                        strAfterCommand);
                fflush(stdout);
                break;
            case 1: // LEAVE:
                fprintf(stdout, "(%s has left the chat)\n", strAfterCommand);
                fflush(stdout);
                break;
            case 2: // LIST:
                fprintf(stdout, "(current chatters: %s)\n", strAfterCommand);
                fflush(stdout);
                break;
            case 3: // MSG:
                compute_server_msg(strAfterCommand);
                break;
        }
    }
}
