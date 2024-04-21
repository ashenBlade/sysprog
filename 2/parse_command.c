#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "command.h"

#include "parse_command.h"

#define ASSERT_IS_COMMAND(expr) (assert((expr)->type == EXPR_TYPE_COMMAND))

/* Отображаем названия функций для получения информации о стеке вызовов при
 * утечке (utils/heap_check) */
#ifdef LEAK_CHECK
#define internal
#else
#define internal static
#endif

typedef struct exe_build_state_t
{
	/** Название исполняемого файла */
	const char* exe;
	/** Аргументы для запуска */
	const char** args;
	/* Количество аргументов для запуска (длина массива args) */
	int args_count;
} exe_state_t;

internal void exe_state_init(exe_state_t* state)
{
	state->args_count = 0;
	state->args = NULL;
	state->exe = NULL;
}

internal void exe_state_update(exe_state_t* state, struct command_raw* cmd)
{
	state->exe = strdup(cmd->exe);
	if (0 < cmd->arg_count)
	{
		char** args = malloc(sizeof(char*) * cmd->arg_count);
		for (uint32_t i = 0; i < cmd->arg_count; i++)
		{
			args[i] = strdup(cmd->args[i]);
		}

		state->args = (const char**)args;
		state->args_count = cmd->arg_count;
	}
	else
	{
		state->args_count = 0;
		state->args = NULL;
	}
}

internal void exe_state_build(exe_state_t* state, exe_t* exe)
{
	/* Эти значения уже скопированы, поэтому просто отдаем */
	exe->name = state->exe;
	exe->args = state->args;
	exe->args_count = state->args_count;
}

internal void exe_state_free(exe_state_t* s)
{
    /* Выделенные strdup строки удаляем уже в команде, чтобы уменьшить общую работу */
	s->args = NULL;
	s->args_count = 0;
	s->exe = NULL;
}

typedef struct pipeline_build_state
{
	/**
	 * true - запуск происходит с условием &&, иначе ||.
	 * Для первого пайплайна опускается, т.к. запуск без условия
	 */
	bool and;
	/** Команды, участвующие в пайплайне */
	exe_state_t* exes;
	/** Длина массива other */
	int size;
	/** Вместимость массива exes (Capacity) */
	int capacity;
} pipeline_state_t;

internal void pipeline_state_init(pipeline_state_t* state, bool is_and)
{
	state->and = is_and;
	state->exes = NULL;
	state->capacity = 0;
	state->size = 0;
}

internal void pipeline_state_add_command(pipeline_state_t* state,
                                         struct expr* e)
{
	ASSERT_IS_COMMAND(e);

	if (state->capacity == 0)
	{
		state->exes = (exe_state_t*)malloc(sizeof(exe_state_t) * 1);
		state->capacity = 1;
	}
	else if (state->capacity == state->size)
	{
		state->capacity *= 2;
		state->exes = (exe_state_t*)realloc(
		    state->exes, sizeof(exe_state_t) * state->capacity);
	}

	/* Добавляем новый exe в наш массив */
	exe_state_init(state->exes + state->size);
	exe_state_update(state->exes + state->size, &e->cmd);
	++state->size;
}

internal void pipeline_state_build_cond(pipeline_state_t* state,
                                        pipeline_condition_t* pc)
{
	assert(0 < state->size);
	assert(state->exes != NULL);

	pc->is_and = state->and;

	exe_state_build(state->exes + (state->size - 1), &pc->pipeline.last);

	if (state->size == 1)
	{
		pc->pipeline.piped = NULL;
		pc->pipeline.piped_count = 0;
		return;
	}

	exe_t* piped = (exe_t*)malloc(sizeof(exe_t) * (state->size - 1));
	for (int i = 0; i < state->size - 1; i++)
	{
		exe_state_build(state->exes + i, piped + i);
	}
	pc->pipeline.piped = piped;
	pc->pipeline.piped_count = (state->size - 1);
}

internal void pipeline_state_build_no_cond(pipeline_state_t* state,
                                           pipeline_t* p)
{
	exe_state_build(state->exes + (state->size - 1), &p->last);

	if (state->size == 1)
	{
		p->piped = NULL;
		p->piped_count = 0;
		return;
	}

	int piped_count = state->size - 1;
	exe_t* piped = (exe_t*)malloc(sizeof(exe_t) * piped_count);
	for (int i = 0; i < piped_count; i++)
	{
		exe_state_build(state->exes + i, piped + i);
	}
	p->piped = piped;
	p->piped_count = piped_count;
}

internal void pipeline_state_free(pipeline_state_t* ps)
{
	ps->and = false;
	if (ps->exes != NULL)
	{
		for (int i = 0; i < ps->size; i++)
		{
			exe_state_free(ps->exes + i);
		}
		free(ps->exes);
		ps->exes = NULL;
		ps->capacity = 0;
		ps->size = 0;
	}
}

typedef struct command_build_state
{
	/** Запуск в фоновом режиме */
	bool is_bg;
	/** Название файла, в который необходимо записывать вывод */
	const char* filename;
	/** Запись в файл в режиме добавления */
	bool append;
	/** Первый пайплайн */
	pipeline_state_t first;
	/** Пайплайны команд */
	pipeline_state_t* pipelines;
	/** Размер массива pipelines, кол-во элементов в нем */
	int pipelines_count;
	/** Вместимость массива pipelines */
	int pipelines_cap;
} build_state_t;

internal void build_state_init(build_state_t* state, struct command_line* line)
{
	state->is_bg = line->is_background;
	if (line->out_type == OUTPUT_TYPE_STDOUT)
	{
		state->filename = NULL;
		state->append = false;
	}
	else
	{
		state->filename = strdup(line->out_file);
		state->append = line->out_type == OUTPUT_TYPE_FILE_APPEND;
	}

	pipeline_state_init(&state->first, false);
	state->pipelines = NULL;
	state->pipelines_cap = 0;
	state->pipelines_count = 0;
}

internal void build_state_free(build_state_t* bs)
{
	bs->append = false;
	bs->filename = NULL;
	bs->is_bg = false;

	pipeline_state_free(&bs->first);
	if (bs->pipelines != NULL)
	{
		for (int i = 0; i < bs->pipelines_count; i++)
		{
			pipeline_state_free(bs->pipelines + i);
		}

		bs->pipelines_count = 0;
		bs->pipelines_cap = 0;
		free(bs->pipelines);
		bs->pipelines = NULL;
	}
}

/* Начать новый пайплайн, который запускается с указанным условием */
internal void build_state_start_pipeline(build_state_t* state,
                                         struct expr* e,
                                         bool is_and)
{
	ASSERT_IS_COMMAND(e);

	int idx = state->pipelines_count;
	if (state->pipelines == NULL)
	{
		state->pipelines =
		    (pipeline_state_t*)malloc(sizeof(pipeline_state_t) * 1);
		state->pipelines_cap = 1;
	}
	else if (state->pipelines_cap == state->pipelines_count)
	{
		state->pipelines = (pipeline_state_t*)realloc(state->pipelines,
		                                              state->pipelines_cap * 2);
		state->pipelines_cap *= 2;
	}

	pipeline_state_init(state->pipelines + idx, is_and);
	pipeline_state_add_command(state->pipelines + idx, e);
	++state->pipelines_count;
}

/**
 * Добавить команду на прямую - cmd ...
 * Эта функция должна вызываться самой первой в списке выражений
 */
internal void build_state_add_command(build_state_t* state, struct expr* e)
{
	/* Проверяем, что это первый вызов */
	ASSERT_IS_COMMAND(e);
	assert(state->pipelines == NULL);

	pipeline_state_add_command(&state->first, e);
}

/* Добавить новую команду, которая начинается сразу после пайплайна - ... | cmd
 * ... */
internal void build_state_add_pipe(build_state_t* state, struct expr* e)
{
	pipeline_state_t* p = 0 < state->pipelines_count
	                          ? state->pipelines + state->pipelines_count - 1
	                          : &state->first;
	pipeline_state_add_command(p, e);
}

/* Создать команду из собранных выражений */
internal void build_state_build(build_state_t* state, command_t* cmd)
{
	cmd->append = state->append;
	cmd->is_bg = state->is_bg;
	cmd->redirect_filename = state->filename;

	pipeline_state_build_no_cond(&state->first, &cmd->first);
	if (state->pipelines != NULL)
	{
		pipeline_condition_t* pcs = (pipeline_condition_t*)malloc(
		    sizeof(pipeline_condition_t) * state->pipelines_count);
		for (int i = 0; i < state->pipelines_count; i++)
		{
			pipeline_state_build_cond(state->pipelines + i, pcs + i);
		}
		cmd->chained = pcs;
		cmd->chained_count = state->pipelines_count;
	}
	else
	{
		cmd->chained = NULL;
		cmd->chained_count = 0;
	}
}

int parse_command(struct command_line* line, command_t* command)
{
	build_state_t bs;
	build_state_init(&bs, line);
	struct expr* e = line->head;
	while (e != NULL)
	{
		switch (e->type)
		{
			case EXPR_TYPE_COMMAND:
				/* Сюда можем попасть только если это первая команда */
				build_state_add_command(&bs, e);
				break;
			case EXPR_TYPE_AND:
				assert(e->next != NULL);
				build_state_start_pipeline(&bs, e->next, true);
				e = e->next;
				break;
			case EXPR_TYPE_OR:
				assert(e->next != NULL);
				build_state_start_pipeline(&bs, e->next, false);
				e = e->next;
				break;
			case EXPR_TYPE_PIPE:
				build_state_add_pipe(&bs, e->next);
				e = e->next;
				break;
			default:
				assert(false);
				break;
		}

		e = e->next;
	}

	build_state_build(&bs, command);
	build_state_free(&bs);
	return 0;
}

internal void exe_free(exe_t* e)
{
	for (int i = 0; i < e->args_count; i++)
	{
		free((void*)e->args[i]);
	}

	free((void*)e->args);
	e->args = NULL;
	free((void*)e->name);
	e->name = NULL;
}

internal void pipeline_free(pipeline_t* p)
{
	exe_free(&p->last);
	if (p->piped != NULL)
	{
		for (int i = 0; i < p->piped_count; i++)
		{
			exe_free(p->piped + i);
		}
		free(p->piped);
		p->piped = NULL;
	}
}

void free_command(command_t* cmd)
{
	cmd->append = false;
	cmd->is_bg = false;
	free((void*)cmd->redirect_filename);

	pipeline_free(&cmd->first);
	if (cmd->chained != NULL)
	{
		assert(0 < cmd->chained_count);

		for (int i = 0; i < cmd->chained_count; i++)
		{
			pipeline_condition_t* pc = cmd->chained + i;
			pc->is_and = false;
			pipeline_free(&pc->pipeline);
		}
		free(cmd->chained);
		cmd->chained = NULL;
	}
}
