#include <assert.h>
#include <complex.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtin_command.h"
#include "exec_command.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

static char** build_execvp_argv(exe_t* exe)
{
	int argv_count = exe->args_count + 2 /* Название самой программы + NULL */;
	char** argv = (char**)malloc(sizeof(char*) * argv_count);
	argv[0] = strdup(exe->name);
	argv[argv_count] = NULL;
	for (int i = 0; i < exe->args_count; i++)
	{
		argv[i + 1] = strdup(exe->args[i]);
	}

	return argv;
}

__attribute__((noreturn)) static void exec_exe(exe_t* exe)
{
	char** argv = build_execvp_argv(exe);
	execvp(argv[0], argv);
	perror("execvp");
	exit(1);
}

static void wait_child(pid_t pid)
{
	int status;
	int ret_code;
	int ret_pid = waitpid(pid, &status, 0);
	if (ret_pid == -1)
	{
		perror("waitpid");
		exit(1);
	}

	dprintf(STDERR_FILENO, "[Log:%d]: ", pid);
	if (!WIFEXITED(status))
	{
		dprintf(STDERR_FILENO, "Потомок завершился аварийно: ");
		if (WIFSIGNALED(status))
		{
			dprintf(STDERR_FILENO, "получен сигнал %s\n",
			        strerror(WTERMSIG(status)));
		}
		else
		{
			dprintf(STDERR_FILENO, "неизвестная ошибка\n");
		}
	}
	else if ((ret_code = WEXITSTATUS(status)) != 0)
	{
		dprintf(STDERR_FILENO,
		        "Потомок завершился с неуспешным статус кодом %d\n", ret_code);
	}
	else
	{
		dprintf(STDERR_FILENO, "Потомок завершился успешно\n");
	}
}

static void exec_pipeline(pipeline_t* pp)
{
	if (pp->piped_count == 0)
	{
		const builtin_command_t* bc;
		if ((bc = get_builtin_command(pp->last.name)) != NULL)
		{
			exec_builtin_command(bc, pp->last.args, pp->last.args_count);
			return;
		}

		int child_pid;
		if ((child_pid = fork()) == 0)
		{
			exec_exe(&pp->last);
			return;
		}

		wait_child(child_pid);

		return;
	}

	if (pp->piped_count == 1)
	{
		int fds[2];
		if (pipe(fds) == -1)
		{
			perror("pipe");
			exit(1);
		}

		int first_child_pid;
		if ((first_child_pid = fork()) == 0)
		{
			dup2(fds[PIPE_WRITE], STDOUT_FILENO);
			close(fds[PIPE_WRITE]);
			exec_exe(pp->piped);
		}
		close(fds[PIPE_WRITE]);

		const builtin_command_t* builtin;
		if ((builtin = get_builtin_command(pp->last.name)) != NULL)
		{
			
			// int saved = dup(STDIN_FILENO);
			dup2(fds[PIPE_READ], STDIN_FILENO);
			close(fds[PIPE_READ]);
			exec_builtin_command(builtin, pp->last.args, pp->last.args_count);
			// dup2(saved, STDIN_FILENO);
		}
		else
		{
			int last_child_pid;
			if ((last_child_pid = fork()) == 0)
			{
				dup2(fds[PIPE_READ], STDIN_FILENO);
				close(fds[PIPE_READ]);
				exec_exe(&pp->last);
				return;
			}

			wait_child(last_child_pid);
		}
        close(fds[PIPE_READ]);
		wait_child(first_child_pid);
		return;
	}

	/* Пока не поддерживаю пайплайны, пойдут сюда */
	assert(false);
}

void exec_command(command_t* cmd)
{
	/*
	 * План реализации:
	 * 0. + Встроенные команды
	 * 1. Пайплайн - |
	 * 2. Перенаправление в файл - >, >>
	 * 3. Встроенные команды
	 * 4. Условия - &&, ||
	 * 5. Фоновая работа - &
	 */

	if (0 < cmd->chained_count)
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

	// if (0 < cmd->first.piped_count)
	// {
	// 	dprintf(STDERR_FILENO, "Пайпы пока не поддерживаются\n");
	// 	return;
	// }

	exec_pipeline(&cmd->first);
}