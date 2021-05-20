#ifndef NNFS_COMMAND_PARSER_H
#define NNFS_COMMAND_PARSER_H

#include <stdbool.h>
#include <request.h>

int parse_command(char *command, request *r);

#endif //NNFS_COMMAND_PARSER_H
