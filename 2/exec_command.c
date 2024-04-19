#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "exec_command.h"
#include "builtin_command.h"

#define PIPE_READ 0
#define PIPE_WRITE 1


static char **build_execvp_argv(exe_t *exe)
{
    int argv_count = exe->args_count + 2 /* Название самой программы + NULL */;
    char **argv = (char **)malloc(sizeof(char *) * argv_count);
    argv[0] = strdup(exe->name);
    argv[argv_count] = NULL;
    for (int i = 0; i < exe->args_count; i++)
    {
        argv[i + 1] = strdup(exe->args[i]);
    }

    return argv;
}



void exec_command(command_t *cmd)
{
    /* 
     * План реализации:
     * 0. Встроенные команды
     * 1. Пайплайн - |
     * 2. Перенаправление в файл - >, >>
     * 3. Встроенные команды
     * 4. Условия - &&, ||
     * 5. Фоновая работа - &
     */

    if (cmd->chained_count > 0)
    {
        dprintf(STDERR_FILENO, "Условия пока не поддерживаются\n");
        return;
    }

    if (cmd->is_bg)
    {
        dprintf(STDERR_FILENO, "Фоновая работа пока не поддерживается\n");
        return;
    }

    if (cmd->redirect_filename != 0)
    {
        dprintf(STDERR_FILENO, "Перенаправление не поддерживается\n");
        return;
    }

    if (0 < cmd->first.piped_count)
    {
        dprintf(STDERR_FILENO, "Пайпы пока не поддерживаются\n");
        return;
    }

    if (exec_builtin_command(cmd->first.first.name, cmd->first.first.args, cmd->first.first.args_count) == 0)
    {
        /* Выполнена встроенная команда */
        return;
    }

    int child_pid;
    if ((child_pid = fork()) == 0)
    {
        char **argv = build_execvp_argv(&cmd->first.first);
        exit(execvp(argv[0], argv));
        return;
    }

    int ret_stat;
    int ret_pid = waitpid(child_pid, &ret_stat, 0);
    assert(ret_pid == child_pid);

    if (!WIFEXITED(ret_stat))
    {
        dprintf(STDERR_FILENO, "Приложение завершилось ненормально: ");
        if (WIFSIGNALED(ret_stat))
        {
            dprintf(STDERR_FILENO, "Завершение из-за необработанного сигнала %s\n", strsignal(WTERMSIG(ret_stat)));
        }
    }
    else if (WEXITSTATUS(ret_stat))
    {
        dprintf(STDERR_FILENO, "Приложение завершилось с кодом ошибки %d\n", WEXITSTATUS(ret_stat));
    }
    else
    {
        dprintf(STDERR_FILENO, "Приложение завершилось с успешным кодом возврата %d\n", WEXITSTATUS(ret_stat));
    }
}