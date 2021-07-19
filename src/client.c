#include <netdb.h>
#include <pthread.h>
#include "parser.h"
#include "checkargs.h"
#include "servercommands.h"
#include "errors.h"

#define AUTH_OK 1
#define CLIENT_ENTRY_OK 2
#define THREE_HUNDRED_MILLI_SECS 300000

// Function Prototypes- description in respective definition
ServerIO* initialize_server_io(int fd[2], char** argv);
int connect_to_server(const char* port);
bool process_stdin_input(char* inputStr, FILE* wrEnd);
bool process_server_input(char* svrInput, ServerIO* svr);
void check_authorization(ServerIO* svr);
void* stdin_read_thread(void* tempSvr);
void* server_read_thread(void* tempSvr);

// Takes an array of two file descriptors and the command line args as @param
// initializes a pointer to the ServerIO struct and returns the variable.
ServerIO* initialize_server_io(int fd[2], char** argv) {
    ServerIO* svr = malloc(sizeof(ServerIO));
    svr->client = malloc(sizeof(ClientId));
    svr->wrEnd = fdopen(fd[0], "w");
    svr->rdEnd = fdopen(fd[1], "r");
    svr->noOfOk = 0; 
    svr->client->name = argv[1];
    svr->client->number = -1;
    svr->authStr = get_auth_string(argv[2]);
    return svr;
}

// Takes the port value as @param and attempts to connect to the port on
// localhost and returns file descriptor on sucessful connection. Else,
// generates a communications error and terminates the program.
int connect_to_server(const char* port) {
    struct addrinfo* addrInfo = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &addrInfo))) {
        freeaddrinfo(addrInfo);
        communications_error();
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol
    if (connect(fd, (struct sockaddr*)addrInfo->ai_addr, sizeof(struct sockaddr))) {
        freeaddrinfo(addrInfo);
        communications_error();
    }
    return fd;
}

// Takes the input string at stdin and the server's write end as @param. If
// input str matches "*LEAVE:", exits the program with a status '0'. Else
// formates the string and writes it to the server. Returns true on success,
// else on a EOF on client stdin returns false.
bool process_stdin_input(char* inputStr, FILE* wrEnd) {
    if (inputStr != NULL) {
        if (inputStr[0] == '*') {
            fprintf(wrEnd, "%s\n", inputStr + 1);
            fflush(wrEnd);
            if (is_match(inputStr, "*LEAVE:")) {
                exit(NORMAL_EXIT);
            }
        } else {
            fprintf(wrEnd, "SAY:%s\n", inputStr);
            fflush(wrEnd);
        }
    } else {
        return false; // EOF on stdin
    }
    return true;
}

// Takes the input received from the server and the pointer to the ServerIO
// struct as @param. If the input is not NULL, evaluates the command received
// and returns true, else returns false if input is NULL- meaning, EOF on
// server read or connection lost.
bool process_server_input(char* svrInput, ServerIO* svr) {
    if (svrInput != NULL) {
        switch (evaluate_server_input(svrInput)) {
            case 0: // AUTH:
                if (svr->noOfOk != CLIENT_ENTRY_OK) {
                    return_authorization_value(svr);
                    check_authorization(svr);
                }
                break;
            case 1: // WHO:
                if (svr->noOfOk == AUTH_OK) {
                    compute_server_who(svr);
                }
                break; 
            case 2: // NAME_TAKEN:
                if (svr->noOfOk == AUTH_OK) {
                    svr->client->number += 1;
                }
                break;
            case 3: // OK:
                if (svr->noOfOk != CLIENT_ENTRY_OK) {
                    svr->noOfOk += 1;
                }
                break;
            case 4: // ENTER:, LEAVE:, MSG:, LIST: 
                if (svr->noOfOk == CLIENT_ENTRY_OK) {
                    display_to_stdout(svr->client, svrInput);
                }
                break;
            case 5: // KICK:
                if (svr->noOfOk == CLIENT_ENTRY_OK) {
                    free(svr);
                    client_kicked();
                }
                break;
        }
    } else {
        return false; // EOF on server read, connection to server is lost
    }
    return true;
}

// Takes the pointer to the struct ServerIO as @param. The function is run
// after responding to AUTH: from server. It reads a line from server, if 
// the line read is NULL (meaning EOF), the server has ended communication
// from its end, and thus, the client program is terminated returning an
// authentication error with a exit value of 4. If line read is not NULL, the
// line is sent for processing the input (most likely an OK: from server). 
void check_authorization(ServerIO* svr) {
    char* nextCommand = is_authorized(svr->rdEnd);
    if (nextCommand) {
        process_server_input(nextCommand, svr);
    } else {
        auth_error();
    }
}

// stdin read thread, takes the pointer to the ServerIO struct as @param and
// for no. of OK: from server = 2, processes the stdin input. If stdin reaches
// EOF teminates the program with an exit statuf of '0'.
void* stdin_read_thread(void* tempSvr) {
    ServerIO* svr = (ServerIO*) tempSvr;
    while (1) {
        if (svr->noOfOk == 2) {
            char* inputStr = get_line(stdin);
            if (!process_stdin_input(inputStr, svr->wrEnd)) {
                usleep(THREE_HUNDRED_MILLI_SECS);
                exit(NORMAL_EXIT);
            }
        }
    }
    free(svr);
    return NULL;
}

// Server read thread, takes the pointer to the ServerIO struct as @param and
// reads and processes the input from the server until its read end returns
// EOF, at which terminates the program throwing a communications error.
void* server_read_thread(void* tempSvr) {
    ServerIO* svr = (ServerIO*) tempSvr;
    while (1) {
        char* svrInput = get_line(svr->rdEnd);
        if (!process_server_input(svrInput, svr)) {
            free(svr);
            communications_error(); // If connection to server disconnects
        }
    }
    free(svr);
    return NULL;
}

int main(int argc, char** argv) {
    check_client_args(argc, argv);

    const char* port = argv[3];
    pthread_t tId[2]; // Thread to read client input from stdin
    int fd[2];
    fd[0] = connect_to_server(port);
    fd[1] = dup(fd[0]);

    ServerIO* svr = initialize_server_io(fd, argv);
    
    pthread_create(&tId[1], NULL, stdin_read_thread, svr);
    pthread_create(&tId[2], NULL, server_read_thread, svr);
    pthread_join(tId[1], 0);
    pthread_join(tId[2], 0);

    free(svr->client);
    free(svr);
    return 0;
}
