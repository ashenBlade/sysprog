#ifndef PARSE_COMMAND_H
#define PARSE_COMMAND_H

#include "parser.h"
#include "command.h"

/**
 * Создать объект команды из промпта пользователя.
 * В случае ошибки возвращается -1, иначе (успех) 0
 */
int parse_command(struct command_line *cmd_line, command_t *command);

/**
 * Освободить ресурсы, выделенные для инициализации команды.
 * Следует вызывать, только если command создан с помощью parse_command функции 
 */
void free_command(command_t *command);

#endif