#include "parser.h"

// Takes two string arguments as @param and returns the result of comparison
// between them.
bool is_match(char* str1, char* str2) {
    if (!strcmp(str1, str2)) {
        return true;
    } else {
        return false;
    }
}

// Takes a string as input and returns the characters after a delimiter ':',
// returns NULL otherwise.
char* after_colon(char* inputStr) {
    char* savePtr;
    char* tempStr = strdup(inputStr);
    strtok_r(tempStr, COLON, &savePtr);
    if (is_match(savePtr, EMPTY_STR)) {
        free(tempStr);
        return NULL;
    } else {
        free(tempStr);
        return savePtr;
    }
}

// REFERENCE: C-TUTE.
// Returns a line of input read from the specified input stream. If EOF is
// read at the start, returns 'NULL' instead. The returned value should be
// freed later except for when 'NULL' is returned.  
char* get_line(FILE* stream) {
    int buffer = BUFFER_SIZE;
    char* word = malloc(sizeof(char) * buffer);
    int position = 0;
    int nextChar;

    while (1) {
        nextChar = fgetc(stream);
        // if size of the buffer is reached
        if (position == buffer - 1) {
            buffer *= 2;
            word = realloc(word, sizeof(char) * buffer);
        }
        // if file is empty, frees memory an returns 
        if (nextChar == EOF && position == 0) {
            free(word);
            return NULL;
        }
        if (nextChar == NEXT_LINE_CHAR || nextChar == EOF) {
            word[position] = NULL_CHAR;
            break;
        }       
        word[position++] = (char) nextChar;
    }
    return word;
}

// Takes the authfile path provided at the command line as @param. Opens the
// authfile path for reading and reads the first line of the file and
// populates it as the authentication string, authStr. If the authfile is
// empty, as per get_line(), assigns NULL to authStr. Finally returns the
// authStr.
char* get_auth_string(char* authPath) {
    FILE* authFilePtr = fopen(authPath, "r");
    char* authStr = get_line(authFilePtr);
    fclose(authFilePtr);
    return authStr;
}

// Takes a string as input and replaces any non printable characters in it by
// a question mark '?'.
void non_printable_check(char* str) {
    for (int idx = 0; idx < strlen(str); idx++) {
        if (!isprint(str[idx])) {
            str[idx] = '?';
        }
    }
}
