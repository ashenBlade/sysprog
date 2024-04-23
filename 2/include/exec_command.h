#ifndef EXEC_COMMAND_H
#define EXEC_COMMAND_H

#include "command.h"

/** Функция для настройки окружения исполнения команд */
void setup_executor();

/** Выполнить указанную команду */
void exec_command(command_t* command);

#endif