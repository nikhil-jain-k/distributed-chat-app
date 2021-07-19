#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#define NULL_CHAR '\0'
#define NEXT_LINE_CHAR '\n'
#define BUFFER_SIZE 5
#define COLON ":"
#define EMPTY_STR ""
#define COLON_ASCII 58

char* get_line(FILE* stream);
char* after_colon(char* inputStr);
char* get_auth_string(char* authPath);
bool is_match(char* str1, char* str2);
void non_printable_check(char* str);

#endif