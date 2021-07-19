#include "checkargs.h"

// Receives file filePath as an argument. Returns true, if the file can be
// opened successfully, else returns false.
bool is_file(char* filePath) {
    FILE* filePtr = fopen(filePath, "r");
    if (filePtr == NULL) {
        return false;
    }
    fclose(filePtr);
    return true;
}

// Takes the arguments count and the command line arguments as @param and
// checks if the authfile can be accessed or if valid no. of arguments
// are present, if false returns a usage error and terminates the program.
void check_client_args(int argc, char** argv) {
    if (argc != ARGS_FOR_CLIENT || !is_file(argv[2])) {
        client_usage_error();
    }
}

// Takes the arguments count and the command line arguments as @param and
// checks if the authfile can be accessed or if valid no. of arguments
// are present, if false returns a usage error and terminates the program.
// If port is provided as an arg, assigns the port arg to a port variable &
// checks if the port lies between the ports range, else, returns a comms
// error and terminates the program. 
// If port is not provided, assigns an ephemeral port to the port variable. 
// Returns the port variable at the end as return value.
const char* check_server_args(int argc, char** argv) {
    const char* port;
    if (argc < MIN_ARGS_FOR_SERVER || argc > MAX_ARGS_FOR_SERVER ||
            !is_file(argv[1])) {
        server_usage_error();
    }
    // See if a portnum was specified, otherwise use zero (ephemeral)
    if (argc < MAX_ARGS_FOR_SERVER) {
        port = "0";
    } else {
        port = argv[2];
        int portnum = atoi(port);
        if (portnum < MIN_PORT_RANGE || portnum > MAX_PORT_RANGE) {
            communications_error();
        }
    }
    return port;
}
