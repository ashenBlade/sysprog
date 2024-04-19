#ifndef BUILTIN_COMMAND_H
#define BUILTIN_COMMAND_H

/** 
 * Выполнить встроенную команду с указанным именем.
 * Если команда нашлась, то выполняется и возвращается 0.
 * В противном случае возвращается -1.
 * 
 * argv - массив строк, которые указал пользователь в командной строке, не содержит самого названия команды
 */
int 
exec_builtin_command(const char *name, const char **argv, int argc);

#endif