#include <assert.h>
#include <complex.h>
#include <errno.h>
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

#define RET_CODE_SUCCESS(code) ((code) == 0)
#define RET_CODE_FAILURE(code) (!RET_CODE_SUCCESS(code))

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
	dprintf(STDERR_FILENO, "[Log:%d]: ошибка исполнения: %s", getpid(),
	        argv[0]);
	if (0 < exe->args_count)
	{
		for (int i = 0; i < exe->args_count; i++)
		{
			dprintf(STDERR_FILENO, " %s", exe->args[i]);
		}
	}

	dprintf(STDERR_FILENO, "\n");

	exit(1);
}

static int wait_child(pid_t pid)
{
	/* TODO: код возврата потомка */
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
		/* TODO: что в таких случаях делать надо? Скорее прервать обработку */
		return 1;
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

	return ret_code;
}

static int exec_pipeline(pipeline_t* pp)
{
	if (pp->piped_count == 0)
	{
		/* Короткий путь для единственной команды */
		const builtin_command_t* bc;
		if ((bc = get_builtin_command(pp->last.name)) != NULL)
		{
			return exec_builtin_command(bc, pp->last.args, pp->last.args_count);
		}

		int child_pid;
		if ((child_pid = fork()) == 0)
		{
			exec_exe_child(&pp->last);
			return 1;
		}

		return wait_child(child_pid);
	}

	/*
	 * Если имеется пайплайн (не 1 команда), то ее представление следующее:
	 *
	 * pp->piped[0] | pp->piped[1] | ... | pp->piped[pp->piped_count - 1] |
	 * pp->last
	 *
	 * Таким образом, все организуется в цикл (i - текущий индекс):
	 * - Читаем из предыдущего пайпа
	 * - Создаем следующий и пишем в него
	 * - После создания потомка обмениваем предыдущий и текущий пайпы
	 *
	 * Команды в пайплайне (pp->piped) всегда выполняются в subshell, т.е. в
	 * потомке. Но если последняя команда встроенная (exit, cd), то
	 * - Выполняется в самом шеле
	 * - Надо перенаправить STDIN из предыдущего пайпа и
	 * - Восстановить STDIN после выполнения.
	 *
	 * Но даже если и не встроенная, то не нужно вызывать pipe, т.к. вывод будет
	 * в STDOUT
	 */

	/* Первая команда в пайплайне будет использовать исходные stdin/stdout */
	int saved_stdin = dup(STDIN_FILENO);
	int prev_pipe[2] = {
	    [PIPE_READ] = STDIN_FILENO,
	    [PIPE_WRITE] = STDOUT_FILENO,
	};
	int* child_pids = (int*)malloc(sizeof(int) * pp->piped_count);

	for (int i = 0; i < pp->piped_count; i++)
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
			if (cur_pipe[PIPE_WRITE] != STDOUT_FILENO)
			{
				if (dup2(cur_pipe[PIPE_WRITE], STDOUT_FILENO) == -1)
				{
					dprintf(STDERR_FILENO, "dup2(STDOUT_FILENO): %s\n",
					        strerror(errno));
					exit(1);
				}
				close(cur_pipe[PIPE_WRITE]);
			}

			if (prev_pipe[PIPE_READ] != STDIN_FILENO)
			{
				if (dup2(prev_pipe[PIPE_READ], STDIN_FILENO) == -1)
				{
					dprintf(STDERR_FILENO, "dup2(STDIN_FILENO): %s\n",
					        strerror(errno));
					exit(1);
				}
				close(prev_pipe[PIPE_READ]);
			}

			exec_exe_child(pp->piped + i);
		}

		close(prev_pipe[PIPE_READ]);
		close(cur_pipe[PIPE_WRITE]);

		child_pids[i] = child_pid;
		prev_pipe[PIPE_READ] = cur_pipe[PIPE_READ];
		prev_pipe[PIPE_WRITE] = cur_pipe[PIPE_WRITE];
	}
	/* Результат работы пайплайна - код последней команды в нем */
	int ret_code;

	/* Последняя команда должна выполняться сразу же если это встроенная
	 * (сохранение семантики bash) */
	const builtin_command_t* builtin;
	if ((builtin = get_builtin_command(pp->last.name)) != NULL)
	{
		/* При запуске встроенной команды необходимо читать вывод из
		 * последнего пайпа, но при этом нужно и частить */
		dup2(prev_pipe[PIPE_READ], STDIN_FILENO);
		ret_code =
		    exec_builtin_command(builtin, pp->last.args, pp->last.args_count);
	}
	else
	{
		int last_child_pid;
		if ((last_child_pid = fork()) == 0)
		{
			if (prev_pipe[PIPE_READ != STDIN_FILENO])
			{
				dup2(prev_pipe[PIPE_READ], STDIN_FILENO);
				close(prev_pipe[PIPE_READ]);
			}

			exec_exe_child(&pp->last);
			return 1;
		}

		ret_code = wait_child(last_child_pid);
	}

	for (int i = 0; i < pp->piped_count; i++)
	{
		(void)wait_child(child_pids[i]);
	}
	free(child_pids);

	/* Восстанавливаем STDIN */
	dup2(saved_stdin, STDIN_FILENO);
	close(saved_stdin);
	close(prev_pipe[PIPE_READ]);

	return ret_code;
}

static int get_out_fd(const char* filename, bool is_append, int* fd)
{
	if (filename == NULL)
	{
		*fd = STDOUT_FILENO;
		return 0;
	}

	/* -rw|-r-|-r- */
	const int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int flags = O_CREAT | O_WRONLY;
	flags |= (is_append ? O_APPEND : O_TRUNC);
	if ((*fd = open(filename, flags, mode)) == -1)
	{
		dprintf(STDERR_FILENO, "Ошибка открытия файла %s: %s\n", filename,
		        strerror(errno));
		return -1;
	}
	return 0;
}

static void close_out_fd(int fd)
{
	if (fd != STDOUT_FILENO)
	{
		close(fd);
	}
}

/*
 * Запустить выполнение пайплайна с учетом возможного перенаправления STDOUT.
 * Вызывается последним в цепочке вызовов
 */
static void exec_pipeline_redirect(pipeline_t* pl, command_t* cmd)
{
	int fd;
	if (get_out_fd(cmd->redirect_filename, cmd->append, &fd) == -1)
	{
		return;
	}

	int saved_stdout = dup(STDOUT_FILENO);
	dup2(fd, STDOUT_FILENO);

	exec_pipeline(pl);

	dup2(saved_stdout, STDOUT_FILENO);
	close(saved_stdout);
	close_out_fd(fd);
}

static void exec_command_main(command_t* cmd)
{
	if (0 == cmd->chained_count)
	{
		exec_pipeline_redirect(&cmd->first, cmd);
		return;
	}

	int prev_ret_code = exec_pipeline(&cmd->first);

	pipeline_condition_t* pc;
	for (int i = 0; i < cmd->chained_count; i++)
	{
		/*
		 * Очередной пайплайн выполнится только в 2 случаях:
		 * 1. && + код успешный
		 * 2. || + код НЕ успешный
		 */
		pc = cmd->chained + i;
		if ((pc->is_and && RET_CODE_SUCCESS(prev_ret_code)) ||
		    (!pc->is_and && RET_CODE_FAILURE(prev_ret_code)))
		{
			if (i == (cmd->chained_count - 1))
			{
				/* Обновлять код не нужно, т.к. это последняя итерация */
				exec_pipeline_redirect(
				    &cmd->chained[cmd->chained_count - 1].pipeline, cmd);
			}
			else
			{
				prev_ret_code = exec_pipeline(&pc->pipeline);
			}
		}
	}
}

void exec_command(command_t* cmd)
{
	/*
	 * TODO: echo 123 | cat | cat - "cat: -: Неправильный дескриптор файла"
	 * Завтра:
	 * 1. Тесты
	 * 2. Проверка памяти
	 * 3. Описание решения
	 */

	if (cmd->is_bg)
	{
		int child_pid;
		if ((child_pid = fork()) == 0)
		{
			exec_command_main(cmd);
			kill(getppid(), SIGUSR1);
			exit(0);
		}

		dprintf(STDERR_FILENO, "Запущен потомок %d\n", child_pid);
	}
	else
	{
		exec_command_main(cmd);
	}
}

static void itoa(char* buf, int buf_len, int val, int* len, char** start)
{
	char val_ch;
	int i = buf_len - 1;
	do
	{
		val_ch = (char)(val % 10) + '0';
		buf[i] = val_ch;
		i--;
		val /= 10;
	} while (0 < val && 0 <= i);
	*len = buf_len - i;
	*start = buf + i;
}

static void sigusr1_handler(int signum, siginfo_t* info, void* context_ptr)
{
	(void)context_ptr;
	if (signum != SIGUSR1)
	{
		return;
	}

	pid_t child_pid = info->si_pid;
	int status;

	waitpid(child_pid, &status, 0);

	char buf[12];
	int len;
	char* start;
	itoa(buf, 12, info->si_pid, &len, &start);
	write(STDERR_FILENO, "Потомок ", sizeof("Потомок "));
	write(STDERR_FILENO, start, len);
	write(STDERR_FILENO, " завершился\n", sizeof(" завершился\n"));
}

void setup_executor()
{
	struct sigaction sa = {
	    .sa_flags = SA_SIGINFO,
	    .sa_sigaction = sigusr1_handler,
	};
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGUSR1, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}
}