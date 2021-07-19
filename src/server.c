#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "parser.h"
#include "checkargs.h"
#include "errors.h"

#define NO_OF_CLIENT_CMDS 4
#define HUNDRED_MILLI_SECS 100000

// Structure to store each client's commands count
typedef struct ClientCommandsCount {
    int say; 
    int kick;
    int list;
} ClientCommandsCount;

// Structure to store the total commands registered on the server end
typedef struct ServerCommandsCount {
    int auth;
    int name;
    int say;
    int kick;
    int list;
    int leave;
} ServerCommandsCount;

// Structure to store the common variables that will be passed around
// the client threads
typedef struct CommonVars {
    pthread_mutex_t lock;
    char* svrAuthVal;
    ServerCommandsCount cmds;
} CommonVars;

// ClientIO structure stores the necessary client details before assigning
// them to a node in the client list upon sucessful entry of the client.
typedef struct ClientIO {
    char* rcvName;
    FILE* rdEnd;
    FILE* wrEnd;
    int* fdClient;
} ClientIO;

// ClientList structure stores the client details
typedef struct ClientList {  
    char* name;
    FILE* rdEnd;
    FILE* wrEnd;
    ClientCommandsCount cmds;
    //bool active;  
    struct ClientList* next;   
} ClientList;

// Structure to store the arguments to be sent to a sighup signal catching
// thread
typedef struct SighupThreadArgs {
    CommonVars common;
    ClientList* headNode;
    sigset_t sigSet;
} SighupThreadArgs;

// Structure to store the arguments to be sent to a client thread
typedef struct ClientThreadArguments {
    ClientIO* clntIo;
    ClientList** listHeadNode;
    CommonVars* common;
} ClientThreadArguments;

// CLIENT AUTHENTICATION AND NAME NEGOTIATION--------------------------------

// Takes ClientIO structure that stores the client details and the CommonVars
// structure that stores the global variables for the program as @param.
// Matches the authentication value sent by client on a "AUTH:" to the one 
// stored in the server end. On a match, writes "OK:" back to client and
// returns true, else returns false.
bool do_client_auth(ClientIO* clntIo, CommonVars* common) {
    fprintf(clntIo->wrEnd, "AUTH:\n");
    fflush(clntIo->wrEnd);
    char* clntResponse = get_line(clntIo->rdEnd);
    char* authVal;
    if (clntResponse != NULL) {
        common->cmds.auth += 1;
        strtok_r(clntResponse, COLON, &authVal);
        if (is_match(authVal, EMPTY_STR) || is_match(authVal,
                common->svrAuthVal)) {
            fprintf(clntIo->wrEnd, "OK:\n");
            fflush(clntIo->wrEnd);
            return true;
        }
    }
    return false;
}

// Reads the client name after a 'WHO:' call from server using get_line
// function. Extracts the name from the client command (NAME:name) and 
// returns the name, else if the 'NAME:' command is not received by the
// server, returns NULL as the client name.
char* get_client_name(FILE* rdEnd) {
    char* response = get_line(rdEnd);
    char* name;
    if (response != NULL) {
        strtok_r(response, COLON, &name);
        if (is_match(response, "NAME")) {
            return name;
        }
    }
    return NULL;
}

// Takes current client name and the head node as @param and traverses
// the whole client linked list. Returns true, if the client name is not
// present in the list or the list is empty else returns false.
bool is_valid_name(char* currClientName, ClientList* headNode) {
    while (headNode != NULL) {
        if (is_match(currClientName, headNode->name)) {
            return false;
        }
        headNode = headNode->next;
    }
    return true;
}

// Takes the pointer to teh ClientIO struct, the headnode of the client list
// and the common variables across all clients as @param. Settles name with
// the client, if succeeded, returns an OK: back to the client else sends
// NAME_TAKEN if the name is taken by a client in the list. Returns the
// settled name, else returns NULL for an EOF on client end.
char* settle_name(ClientIO* clntIo, ClientList** headNode,
        CommonVars* common) {
    pthread_mutex_lock(&(common->lock));
    ClientList* headNodeCopy = *headNode;
    while (1) {
        fprintf(clntIo->wrEnd, "WHO:\n");
        fflush(clntIo->wrEnd);
        char* clientName = get_client_name(clntIo->rdEnd);
        if (clientName != NULL) {
            common->cmds.name += 1;
            non_printable_check(clientName);
            if (is_valid_name(clientName, headNodeCopy) &&
                    !is_match(clientName, EMPTY_STR)) {
                fprintf(clntIo->wrEnd, "OK:\n");
                fflush(clntIo->wrEnd);
                pthread_mutex_unlock(&(common->lock));
                return clientName;
            } else {
                fprintf(clntIo->wrEnd, "NAME_TAKEN:\n");
                fflush(clntIo->wrEnd);
            }
        } else {
            pthread_mutex_unlock(&(common->lock));
            return NULL; // Client EOF or terminated unexpectedly
        }
    }
}

// CLIENT LIST OPERATIONS----------------------------------------------------

// Takes a reference (ptr to ptr) to the head of the client linked list from
// main and the ClientDetails to be added to the node as the @param.
// Appends a new node at the end of the client list.
ClientList* link_client_node(ClientIO* clntIo, ClientList** headNode) {
    ClientList* newClientNode = (ClientList*) malloc(sizeof(ClientList));
    ClientCommandsCount emptyStruct = {0};
    ClientList* last = *headNode;

    // Put client details
    newClientNode->name = clntIo->rcvName;
    newClientNode->rdEnd = clntIo->rdEnd;
    newClientNode->wrEnd = clntIo->wrEnd;
    newClientNode->next = NULL;
    newClientNode->cmds = emptyStruct;
    free(clntIo); // Free after storing details into a node

    if (*headNode == NULL) {       // If for the first node
        *headNode = newClientNode;
        return newClientNode;
    }  
    while (last->next != NULL) {  // Else traverse till the end...
        last = last->next;
    }
    last->next = newClientNode; // & make the newClientNode as the last node
    return newClientNode;     
}

// Takes a client name and a reference to head of the client list as @param
// Traverses through the list searching for the client's name, if found
// unlinks and deallocates the respective node from the list.
void unlink_client_node(char* name, ClientList** headNode) {
    ClientList* headNodeCopy = *headNode;
    ClientList* prevNode; // keep track of previous node for unlinking later
    // If the clientName is matched at the head node:
    if (headNodeCopy != NULL && is_match(headNodeCopy->name, name)) {
        *headNode = headNodeCopy->next; // Changed head
        free(headNodeCopy); // free old head
        return;
    }
    // Traverse the whole list till a match
    while (headNodeCopy != NULL && !is_match(headNodeCopy->name, name)) {
        prevNode = headNodeCopy;
        headNodeCopy = headNodeCopy->next;
    }
    if (headNodeCopy == NULL) {
        return;
    }
    prevNode->next = headNodeCopy->next; // Unlink node from the linked list
    free(headNodeCopy);                  // Free memory
}

// Takes the message to be broadcasted and the client list head node as 
// @param. If message is NULL, returns. Else, broadcasts the message to all
// client's present in the list.
void broadcast_to_clients(char* msg, ClientList** headNode) {
    ClientList* headNodeCopy = *headNode;
    if (msg == NULL) {
        return;
    }
    while (headNodeCopy != NULL) {
        fprintf(headNodeCopy->wrEnd, msg);
        fflush(headNodeCopy->wrEnd);
        headNodeCopy = headNodeCopy->next;
    }
}

// CLIENT UNDERSTANDABLE FORMATS---------------------------------------------

// Takes name and a message as @param and converts it to this format:
// MSG:name:message and returns the format.
char* convert_to_msg_format(char* name, char* msg) {
    char* cmd = "MSG::\n";
    int msgLen = strlen(cmd) + strlen(name) + strlen(msg);
    char* format = malloc(sizeof(char) * msgLen); 
    sprintf(format, "MSG:%s:%s\n", name, msg);
    return format;
}

// Takes name as @param and converts it to this format:
// ENTER:name and returns the format.
char* client_entry_format(char* name) {
    char* cmd = "ENTER:\n";
    int msgLen = strlen(cmd) + strlen(name);
    char* format = malloc(sizeof(char) * msgLen); 
    sprintf(format, "ENTER:%s\n", name);
    return format;
}

// Takes name as @param and converts it to this format:
// LEAVE:name and returns the format.
char* client_left_format(char* name) {
    char* cmd = "LEAVE:\n";
    int msgLen = strlen(cmd) + strlen(name);
    char* format = malloc(sizeof(char) * msgLen); 
    sprintf(format, "LEAVE:%s\n", name);
    return format;
}

// SERVER STDOUT CONTENTS----------------------------------------------------

// Takes the client's message and its name as @param and returns NULL if 
// message is NULL, else displays its name and msg on stdout and returns a
// string in a client understandable "MSG:" format.
char* display_client_say(char* msg, char* clientName) {
    if (msg != NULL) {
        fprintf(stdout, "%s: %s\n", clientName, msg);
        fflush(stdout);
        return convert_to_msg_format(clientName, msg);
    }
    return NULL;
}

// Takes the client's name as @param and displays its entry. Returns a string
// in a client understandable "ENTER:" format.
char* display_client_entry(char* name) {
    fprintf(stdout, "(%s has entered the chat)\n", name);
    fflush(stdout);
    return client_entry_format(name);  
}

// Takes the client's name as @param and displays its leave on stdout.
// Returns a string in client understandable "LEAVE:" format.
char* display_client_left(char* name) {
    fprintf(stdout, "(%s has left the chat)\n", name);
    fflush(stdout);
    return client_left_format(name);  
}

// CLIENT INPUTS PROCESSING--------------------------------------------------

// Takes the pointer to the ClientIO struct that contains client details, 
// the headnode of the client list and a mutex lock as @param. Creates a new
// node, stores the details of the client, links the node to list. Displays
// client entry on stdout and broadcasts its entry to all clients in list.
// Returns the client node. 
ClientList* compute_client_enter(ClientIO* clntIo, ClientList** headNode,
        pthread_mutex_t lock) {
    pthread_mutex_lock(&lock);
    ClientList* clientNode = link_client_node(clntIo, headNode);
    char* (*enterMsg)(char*) = display_client_entry;
    broadcast_to_clients(enterMsg(clientNode->name), headNode);
    pthread_mutex_unlock(&lock);
    return clientNode;
}

// Takes the client's name, its message, the headnode of the client list,
// and a mutex lock as @param. Prints the clients message on stdout and
// broadcasts the message to all clients in the "MSG:" format.
void compute_client_say(char* name, char* message, ClientList** headNode,
        pthread_mutex_t lock) {
    pthread_mutex_lock(&lock);
    char* (*msg)(char*, char*) = display_client_say;
    broadcast_to_clients(msg(message, name), headNode);
    pthread_mutex_unlock(&lock);
}

// Takes the client's name and the client list node as @param. Check if the
// client's name matches with any client in the list. Returns true if found,
// else returns false.
bool is_kicked(char* name, ClientList** headNode) {
    ClientList* headNodeCopy = *headNode;
    if (name == NULL) {
        return false;
    }
    while (headNodeCopy != NULL) {
        if (is_match(headNodeCopy->name, name)) {
            fprintf(headNodeCopy->wrEnd, "KICK:\n");
            fflush(headNodeCopy->wrEnd);
            return true;
        }
        headNodeCopy = headNodeCopy->next;
    }
    return false;
}

// Takes the current client name, the headnode of the client list and a mutex
// lock as @param. If the client to be kicked is found in the list, unlinks
// it from the list, sends a "KICK:" command to the client to be kicked,
// displays its leave on server's stdout, and broadcasts its leave to all
// other participating clients.
void compute_client_kick(char* name, ClientList** headNode,
        pthread_mutex_t lock) {
    pthread_mutex_lock(&lock);
    if (name != NULL && is_kicked(name, headNode)) {
        unlink_client_node(name, headNode);
        char* (*leftMsg)(char*) = display_client_left;
        broadcast_to_clients(leftMsg(name), headNode);
    }
    pthread_mutex_unlock(&lock);
}

// Compare function for the qsort function
int compare_str(const void* str1, const void* str2) {
    return strcasecmp(*(char**)str1, *(char**)str2);
}

// Takes read end of the client and the clients names array and its length as
// @param and writes the LIST: response back to the client.
void send_names_to_client(FILE* wrEnd, char** namesArr, int len) {
    int idx;
    fprintf(wrEnd, "LIST:");
    for (idx = 0; idx < len - 1; idx++) {
        fprintf(wrEnd, "%s,", namesArr[idx]);
    }
    fprintf(wrEnd, "%s\n", namesArr[idx]);
    fflush(wrEnd);
}

// Takes the current client, the headnode of the client list and a mutex lock
// as @param. Returns the list of client names in the list in a client
// understandable format and in lexicographical order.
void send_chatters_list(ClientList* client, ClientList** headNode,
        pthread_mutex_t lock) {
    pthread_mutex_lock(&lock);
    ClientList* headNodeCopy = *headNode;
    int idx = 0;
    int buf = BUFFER_SIZE;
    char** namesArr = malloc(sizeof(char*) * buf);
    while (headNodeCopy != NULL) {
        namesArr[idx] = headNodeCopy->name;
        if (idx == buf - 1) {
            namesArr = realloc(namesArr, (sizeof(char*) * buf));
        } else {
            idx++;
        }
        headNodeCopy = headNodeCopy->next;
    }
    qsort(namesArr, idx, sizeof(char*), compare_str);
    send_names_to_client(client->wrEnd, namesArr, idx);
    pthread_mutex_unlock(&lock);
}

// Takes the name of the client, the headnode of the client list and a
// mutex lock as @param. Unlinks the client node with the matching name
// displays its leave on stdout, and broadcasts to all current client nodes 
// in the list about the client's leave. Ignores unlinking and broadcasting 
// if name is NULL.
void client_left(char* name, ClientList** headNode,
        pthread_mutex_t lock) {
    pthread_mutex_lock(&lock);
    if (name != NULL) {
        unlink_client_node(name, headNode);
        char* (*leftMsg)(char*) = display_client_left;
        broadcast_to_clients(leftMsg(name), headNode);
    }
    pthread_mutex_unlock(&lock);
}

// CLIENT COMMAND PROCESSING-------------------------------------------------

// Takes the input from the client as @param and for a matched token in
// clientCommands[], returns the index for a switch case input in another
// function.
int evaluate_client_command(char* inputStr) {
    char* clientCommands[] = {"SAY", "KICK", "LIST", "LEAVE"};
    int index = 0;
    while (index < NO_OF_CLIENT_CMDS) {
        if (is_match(inputStr, clientCommands[index])) {
            break;
        }
        index++;
    }
    return index;
}

// Takes the current client being processed, the client list headnode, and
// the common variables across all clients as @param. Processes valid 
// cleint commands and ignores invalid ones. On a leave or a EOF on the
// client's read end, assumes client has left and unlinks the current client 
// from the list.
void process_client_input(ClientList* currClient, ClientList** headNode,
        CommonVars* common) {
    char* clientCmd = NULL;
    while ((clientCmd = get_line(currClient->rdEnd)) != NULL &&
            !ferror(currClient->rdEnd)) {
        char* strAfterCmd = NULL;
        if (!strchr(clientCmd, COLON_ASCII)) {
            continue;
        }
        strtok_r(clientCmd, COLON, &strAfterCmd);
        non_printable_check(strAfterCmd);
        switch (evaluate_client_command(clientCmd)) {
            case 0: // SAY
                common->cmds.say += 1;
                currClient->cmds.say += 1;
                compute_client_say(currClient->name, strAfterCmd, headNode,
                        common->lock);
                break;
            case 1: // KICK
                common->cmds.kick += 1;
                currClient->cmds.kick += 1;
                compute_client_kick(strAfterCmd, headNode, common->lock);
                if (ferror(currClient->rdEnd) || ferror(currClient->wrEnd)) {
                    return;
                }
                break;
            case 2: // LIST
                common->cmds.list += 1;
                currClient->cmds.list += 1;
                send_chatters_list(currClient, headNode, common->lock);
                break;
            case 3: { // LEAVE
                if (is_match(strAfterCmd, EMPTY_STR)) {
                    common->cmds.leave += 1;
                    client_left(currClient->name, headNode, common->lock);
                    return;
                }
                break;
            }
        }
        usleep(HUNDRED_MILLI_SECS);
    }
    // Client unexpectedly left the chat
    client_left(currClient->name, headNode, common->lock);
}

// CLIENT THREAD ------------------------------------------------------------

// Client thread function, takes pointer to a ClientIO struct that stores the
// client details, the Client list and the common variables across all
// clients as input. Performs authentication and name negotiation, if 
// sucessful, then processes input from the client. Else, closes the IO ends
// and the thread terminates.
void* client_thread(void* arg) {
    ClientThreadArguments* ctArgs = malloc(sizeof(ClientThreadArguments));
    ClientIO* clntIo = malloc(sizeof(ClientIO));
    ClientList** headNode = malloc(sizeof(ClientList*));
    CommonVars* common = malloc(sizeof(CommonVars));
    ctArgs = arg; 
    clntIo = ctArgs->clntIo;
    headNode = ctArgs->listHeadNode;
    common = ctArgs->common;
    
    int fd[2];
    fd[0] = *(int*)clntIo->fdClient;
    fd[1] = dup(fd[0]);
    free(clntIo->fdClient);

    clntIo->wrEnd = fdopen(fd[0], "w");
    clntIo->rdEnd = fdopen(fd[1], "r");

    ClientList* clientNode;
    if (do_client_auth(clntIo, common) && ((clntIo->rcvName = settle_name(
            clntIo, headNode, common)) != NULL)) {
        clientNode = compute_client_enter(clntIo, headNode, common->lock);
        process_client_input(clientNode, headNode, common);
    } else {
        fclose(clntIo->wrEnd);
        fclose(clntIo->rdEnd);
    }
    pthread_exit(NULL);
}

// STRUCTS INITS-------------------------------------------------------------

// Takes the authentication path from the command line as @param, initializes
// the CommonVars struct variable and returns it.
CommonVars init_common_vars(char* authPath) {
    CommonVars common;
    ServerCommandsCount emptyStruct = {0};
    pthread_mutex_init(&common.lock, NULL);
    common.svrAuthVal = get_auth_string(authPath);
    common.cmds = emptyStruct;
    return common;
}

// Takes the client file descripter as param. Initializes the ClientIO* struct 
// variable and returns it.
ClientIO* init_client_io(int clientFd) {
    ClientIO* clntIo = malloc(sizeof(ClientIO));
    clntIo->fdClient = malloc(sizeof(int));
    *(clntIo->fdClient) = clientFd;
    return clntIo;
}

// NETWORKING AND CLIENT THREAD CREATION-------------------------------------

// Listens on given port. Returns listening socket (or exits on failure).
int open_listen(const char* port) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;   // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    // listen on all IP addresses
    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &ai))) {
        freeaddrinfo(ai);
        communications_error();
    }
    
    // Create a socket and bind it to a port
    int listenFd = socket(AF_INET, SOCK_STREAM, 0); // 0=default protocol (TCP)

    // Allow address (port number) to be reused immediately
    int optVal = 1;
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, 
            &optVal, sizeof(int)) < 0) {
        communications_error();
    }

    if (bind(listenFd, (struct sockaddr*) ai->ai_addr, 
            sizeof(struct sockaddr)) < 0) {
        communications_error();
    }
    
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(listenFd, (struct sockaddr*) &ad, &len)) {
        communications_error();
    }
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));

    if (listen(listenFd, SOMAXCONN) < 0) {
        communications_error();
    }
    return listenFd;
}

// Takes the server's file descripter, the client list's head node and the
// common variables across all clients as @param. Accepts connections to the
// port and starts a client thread at each new sucessful connection. If 
// connection is unsuccessful, then terminates the server generating a 
// communications error.
void process_connections(int fdServer, ClientList** headNode,
        CommonVars* common) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    // Repeatedly accept connections and process data (capitalise)
    while (1) {
        fromAddrSize = sizeof(struct sockaddr_in);
	// Block, waiting for a new connection. (fromAddr will be populated
	// with address of client)
        fd = accept(fdServer, (struct sockaddr*) &fromAddr, &fromAddrSize);
        if (fd < 0) {
            communications_error();
        }
     
	// Turn our client address into a hostname and print out both 
        // the address and hostname as well as the port number
        char hostname[NI_MAXHOST];
        int error = getnameinfo((struct sockaddr*)&fromAddr, 
                fromAddrSize, hostname, NI_MAXHOST, NULL, 0, 0);
        if (error) {
            communications_error();
        }
    
	// Start a child thread
        ClientIO* clntIo = init_client_io(fd);
        ClientThreadArguments ctArgs;
        ctArgs.clntIo = clntIo;
        ctArgs.listHeadNode = headNode;
        ctArgs.common = common;
	pthread_t threadId;
	pthread_create(&threadId, NULL, client_thread, &ctArgs);
	pthread_detach(threadId);
    }
}

// SIGHUP HANDLING-----------------------------------------------------------

// Takes the pointer to the ClientList as argument and displays each client's
// statistics till the end of the list on a SIGHUP signal.
void display_currclient_command_counts(ClientList* client) {
    while (client != NULL) {
        fprintf(stderr, "%s:SAY:%d:KICK:%d:LIST:%d\n", client->name,
                client->cmds.say, client->cmds.kick, client->cmds.list);
        client = client->next;
    }
}

// Takes the commands count received at the server as @param, and
// Displays the server statistics on stdout on a SIGHUP signal.
void display_server_command_counts(ServerCommandsCount cmds) {
    fprintf(stderr, "server:AUTH:%d:NAME:%d:SAY:%d:KICK:%d:LIST:%d:LEAVE:%d"
            "\n", cmds.auth, cmds.name, cmds.say, cmds.kick, cmds.list,
            cmds.leave);
}

// SIGHUP thread function, waits for a SIGHUP signal on the server. Displays
// client and server statistics when found one.
void* sighup_signal_waiter(void* arg) {
    SighupThreadArgs* stArgs = malloc(sizeof(SighupThreadArgs));
    stArgs = arg;
    int sigNum;
    while (1) {
        sigwait(&stArgs->sigSet, &sigNum);
        if (sigNum == SIGHUP) {
            pthread_mutex_lock(&(stArgs->common.lock));
            fprintf(stderr, "@CLIENTS@\n");
            display_currclient_command_counts(stArgs->headNode);
            fprintf(stderr, "@SERVER@\n");
            display_server_command_counts(stArgs->common.cmds);
            pthread_mutex_unlock(&(stArgs->common.lock));
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int fdServer;
    const char* port = check_server_args(argc, argv);
    char* authPath = argv[1];
    
    // SIGPIPE Handling
    struct sigaction sa1;
    sa1.sa_handler = SIG_IGN;
    sa1.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa1, NULL);

    // SIGHUP Handling
    pthread_t sighupThreadId;
    SighupThreadArgs stArgs;
    stArgs.common = init_common_vars(authPath);
    stArgs.headNode = NULL;
    sigemptyset(&(stArgs.sigSet));
    sigaddset(&(stArgs.sigSet), SIGHUP);
    pthread_sigmask(SIG_BLOCK, &(stArgs.sigSet), NULL);
    pthread_create(&sighupThreadId, NULL, sighup_signal_waiter, &stArgs);

    // Processing connections
    fdServer = open_listen(port);
    process_connections(fdServer, &stArgs.headNode, &stArgs.common);

    return 0;
}
