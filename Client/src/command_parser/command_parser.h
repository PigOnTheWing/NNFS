#ifndef NNFS_COMMAND_PARSER_H
#define NNFS_COMMAND_PARSER_H

#include <stdbool.h>
#include <request.h>

#define MAX_ARGS 2

#define CD_DIR 0

#define RW_SRC 0
#define RW_DEST 1

char *get_arg(size_t arg_i);
int parse_command(char *command, request *r);

#endif //NNFS_COMMAND_PARSER_H
