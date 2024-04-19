#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>

#include "builtin_command.h"

typedef struct builtin_command
{
    /** Название встроенной команды */
    const char *name;
    /** Функция для ее выполнения с переданными аргументами */
    void (*exec)(int argc, const char **argv);
} builtin_command_t;

static void do_exit(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    if (argc == 0)
    {
        exit(0);
    }

    if (argc == 1)
    {
        int code = (int)strtol(argv[0], NULL, 10);
        exit(code);
    }

    dprintf(STDERR_FILENO, "Слишком большое количество аргументов\n");
}

static const char *get_pwd()
{
    const char *path = getenv("HOME");
    if (path != NULL)
    {
        return path;
    }

    struct passwd *pw = getpwuid(geteuid());
    return pw->pw_dir;
}

static void do_cd(int argc, const char **argv)
{
    const char *path;
    if (argc == 0)
    {
        path = get_pwd();
        if (path == NULL)
        {
            dprintf(STDERR_FILENO, "Не удалось получить домашний каталог\n");
            return;
        }
        
    }
    else if (argc == 1)
    {
        path = argv[0];
    }
    else
    {
        dprintf(STDERR_FILENO, "Слишком много директорий указано");
        return;
    }

    if (chdir(path) == -1)
    {
        dprintf(STDERR_FILENO, "Ошибка при смене директорий: %s\n", strerror(errno));
    }
}

static const builtin_command_t builtin_commands[] = {
    {
        .name = "exit",
        .exec = do_exit,
    },
    {
        .name = "cd",
        .exec = do_cd
    },
};

#define BUILTINS_COMMANDS_COUNT (sizeof(builtin_commands) / sizeof(builtin_command_t))

static int find_command(const char *name, const builtin_command_t **command)
{
    for (size_t i = 0; i < BUILTINS_COMMANDS_COUNT; i++)
    {
        if (strcmp(name, builtin_commands[i].name) == 0)
        {
            *command = &builtin_commands[i];
            return 0;
        }
    }
    return -1;
}

int exec_builtin_command(const char *name, const char** argv, int argc)
{
    const builtin_command_t *cmd;
    if (find_command(name, &cmd) == -1)
    {
        return -1;
    }

    cmd->exec(argc, argv);
    return 0;
}