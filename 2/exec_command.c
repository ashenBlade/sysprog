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

/* Запустить указанную команду в потомке. На этом моменте stdout и stdin должны
 * быть настроены */
__attribute__((noreturn)) static void exec_exe_child(exe_t* exe)
{
	const builtin_command_t* bc;
	if ((bc = get_builtin_command(exe->name)) != NULL)
	{
		exec_builtin_command(bc, exe->args, exe->args_count);
		exit(0);
	}

	char** argv = build_execvp_argv(exe);
	execvp(argv[0], argv);
	perror("execvp");
	exit(1);
}

/* Запустить  */
__attribute__((noreturn)) static void exec_exe_pipe(exe_t* exe,
                                                    int fd_in,
                                                    int fd_out)
{
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
			exec_exe_child(&pp->last);
			return;
		}

		wait_child(child_pid);

		return;
	}

	/* Первая команда в пайплайне будет использовать исходные stdin/stdout */
	int prev_pipe[2] = {
	    [PIPE_READ] = STDIN_FILENO,
	    [PIPE_WRITE] = STDOUT_FILENO,
	};
	/* Массив для всех дескрипторов потомков */
	int* child_pids = (int*)malloc(sizeof(int) * pp->piped_count);

	for (size_t i = 0; i < pp->piped_count; i++)
	{
		int cur_pipe[2];
		if (pipe(cur_pipe) == -1)
		{
			perror("pipe");
			exit(1);
		}

		int child_pid;
		if ((child_pid = fork()) == 0)
		{
			dup2(cur_pipe[PIPE_WRITE], STDOUT_FILENO);
			dup2(prev_pipe[PIPE_READ], STDIN_FILENO);
			close(cur_pipe[PIPE_WRITE]);
			close(prev_pipe[PIPE_READ]);
			exec_exe_child(pp->piped + i);
		}

		if (prev_pipe[PIPE_READ] != STDIN_FILENO)
		{
			/* Осторожно закрываем пайп для чтения предыдущий, так как там может
			 * находиться STDIN */
			close(prev_pipe[PIPE_READ]);
		}
		close(cur_pipe[PIPE_WRITE]);

		child_pids[i] = child_pid;
		prev_pipe[0] = cur_pipe[0];
		prev_pipe[1] = cur_pipe[1];
	}

	/* Последняя команда должна выполняться сразу же если это встроенная
	 * (сохранение семантики bash) */
	const builtin_command_t* builtin;
	if ((builtin = get_builtin_command(pp->last.name)) != NULL)
	{
		/* При запуске встроенной команды необходимо читать вывод из
		 * последнего пайпа, но при этом нужно и частить */
		int saved_stdin = dup(STDIN_FILENO);
		dup2(prev_pipe[PIPE_READ], STDIN_FILENO);
		exec_builtin_command(builtin, pp->last.args, pp->last.args_count);
		dup2(saved_stdin, STDIN_FILENO);
		close(saved_stdin);
	}
	else
	{
		int last_child_pid;
		if ((last_child_pid = fork()) == 0)
		{
			dup2(prev_pipe[PIPE_READ], STDIN_FILENO);
			close(prev_pipe[PIPE_READ]);
			exec_exe_child(&pp->last);
			return;
		}

		wait_child(last_child_pid);
	}

	for (size_t i = 0; i < pp->piped_count; i++)
    {
		wait_child(child_pids[i]);
    }
	
	close(prev_pipe[PIPE_READ]);
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