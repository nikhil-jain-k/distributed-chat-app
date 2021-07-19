#include "errors.h"

// CLIENT--------------------------------------------------------------------

// Terminates the program with the error code '1' if there there are invalid
// number of arguments or the authfile file cannot be opened. 
void client_usage_error(void) {
    fprintf(stderr, "Usage: client name authfile port\n");
    exit(USAGE_ERR_CODE);
}

// Terminates the program with the error code '3' if the client is kicked by
// the server.
void client_kicked(void) {
    fprintf(stderr, "Kicked\n");
    exit(KICKED_ERR_CODE);
}

// Terminates the program with the error code '4' if the client's
// authentication to teh server is failed.
void auth_error(void) {
    fprintf(stderr, "Authentication error\n");
    exit(AUTH_ERR_CODE);
}

// SERVER--------------------------------------------------------------------

// Terminates the program with the error code '1' if there there are invalid
// number of arguments or the authfile file cannot be opened. 
void server_usage_error(void) {
    fprintf(stderr, "Usage: server authfile [port]\n");
    exit(USAGE_ERR_CODE);
}

// CLIENT & SERVER-----------------------------------------------------------

// Terminates the program with the error code '2' if there the socket cannot
// be opened or [(only for client) the connection is lost with the server].
void communications_error(void) {
    fprintf(stderr, "Communications error\n");
    exit(COMMS_ERR_CODE);
}
