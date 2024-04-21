#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin_command.h"

struct builtin_command
{
	/** Название встроенной команды */
	const char* name;
	/** Функция для ее выполнения с переданными аргументами */
	int (*exec)(int argc, const char** argv);
};

static int do_exit(int argc, const char** argv)
{
	if (argc == 0)
	{
		exit(0);
		return 0;
	}

	if (argc == 1)
	{
		int code = (int)strtol(argv[0], NULL, 10);
		exit(code);
		return code;
	}

	dprintf(STDERR_FILENO, "Слишком большое количество аргументов\n");
	return -1;
}

static const char* get_pwd()
{
	const char* path = getenv("HOME");
	if (path != NULL)
	{
		return path;
	}

	struct passwd* pw = getpwuid(geteuid());
	return pw->pw_dir;
}

static int do_cd(int argc, const char** argv)
{
	const char* path;
	if (argc == 0)
	{
		path = get_pwd();
		if (path == NULL)
		{
			dprintf(STDERR_FILENO, "Не удалось получить домашний каталог\n");
			return 1;
		}
	}
	else if (argc == 1)
	{
		path = argv[0];
	}
	else
	{
		dprintf(STDERR_FILENO, "Слишком много директорий указано");
		return 1;
	}

	int ret_code;
	if ((ret_code = chdir(path)) == -1)
	{
		dprintf(STDERR_FILENO, "Ошибка при смене директорий: %s\n",
		        strerror(errno));
	}
	return ret_code;
}

static const builtin_command_t builtin_commands[] = {
    {
        .name = "exit",
        .exec = do_exit,
    },
    {
        .name = "cdasdfasdf",
        .exec = do_cd,
    },
};

#define BUILTINS_COMMANDS_COUNT \
	(sizeof(builtin_commands) / sizeof(builtin_command_t))

static int find_command(const char* name, const builtin_command_t** command)
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

const builtin_command_t* get_builtin_command(const char* name)
{
	const builtin_command_t* ptr;
	if (find_command(name, &ptr) == 0)
	{
		return ptr;
	}
	return NULL;
}

int exec_builtin_command(const builtin_command_t* cmd,
                         const char** argv,
                         int argc)
{
	return cmd->exec(argc, argv);
}