#ifndef BUILTIN_COMMAND_H
#define BUILTIN_COMMAND_H

typedef struct builtin_command builtin_command_t;

/**
 * Получить встроенную команду с указанным именем.
 * Если команда нашлась, то возвращается указатель на нее, иначе NULL.
 */
const builtin_command_t* get_builtin_command(const char* name);

/**
 * Выполнить встроенную команду.
 * Команда получается через вызов get_builtin_command.
 *
 * argv - массив строк, которые указал пользователь в командной строке, не
 * содержит самого названия команды
 */
int exec_builtin_command(const builtin_command_t *cmd, const char** argv, int argc);

#endif