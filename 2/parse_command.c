#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "parse_command.h"

#define ASSERT_IS_COMMAND(expr) (assert((expr)->type == EXPR_TYPE_COMMAND))

typedef struct exe_build_state_t
{
    /** Название исполняемого файла */
    const char *exe;
    /** Аргументы для запуска */
    const char **args;
    /* Количество аргументов для запуска (длина массива args) */
    int args_count;
} exe_state_t;

static void
exe_state_init(exe_state_t *state)
{
    state->args_count = 0;
    state->args = NULL;
    state->exe = NULL;
}

/** Проверить, что указанное состояние проинициализировано значениями */
static bool
exe_state_is_init(exe_state_t *state)
{
    return state->exe != NULL;
}

static void
exe_state_update(exe_state_t *state, struct command_raw *cmd)
{
    state->exe = strdup(cmd->exe);
    if (0 < cmd->arg_count)
    {
        char **args = malloc(sizeof(char *) * cmd->arg_count);
        for (int i = 0; i < cmd->arg_count; i++)
        {
            args[i] = strdup(cmd->args[i]);
        }

        state->args = (const char **)args;
        state->args_count = cmd->arg_count;
    }
    else
    {
        state->args_count = 0;
        state->args = NULL;
    }
}

static void
exe_state_build(exe_state_t *state, exe_t *exe)
{
    /* Эти значения уже скопированы, поэтому просто отдаем */
    exe->name = state->exe;
    exe->args = state->args;
    exe->args_count = state->args_count;
}

static void
exe_state_free(exe_state_t *s)
{
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
    exe_state_t *exes;
    /** Длина массива other */
    int size;
    /** Вместимость массива exes (Capacity) */
    int capacity;
} pipeline_state_t;

static void
pipeline_state_init(pipeline_state_t *state)
{
    state->and = false;
    state->exes = NULL;
    state->capacity = 0;
    state->size = 0;
}

static void
pipeline_state_add_command(pipeline_state_t *state, struct expr *e, bool is_and)
{
    ASSERT_IS_COMMAND(e);

    /* Проверяем место для массива state->other */
    int insert_index = state->size;
    if (state->exes == NULL)
    {
        state->exes = (exe_state_t *)malloc(sizeof(exe_state_t) * 1);
        state->capacity = 1;
    }
    else if (state->capacity == state->size)
    {
        state->exes = (exe_state_t *)realloc(state->exes, state->capacity * 2);
        state->capacity *= 2;
    }

    /* Добавляем новый exe в наш массив */
    exe_state_init(state->exes + insert_index);
    exe_state_update(state->exes + insert_index, &e->cmd);
    state->and = is_and;
    ++state->size;
}

static void
pipeline_state_build_cond(pipeline_state_t *state, pipeline_condition_t *pc)
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

    exe_t *piped = (exe_t *)malloc(sizeof(exe_t) * state->size);
    for (size_t i = 0; i < state->size - 1; i++)
    {
        exe_state_build(state->exes + i, piped + i);
    }
    pc->pipeline.piped = piped;
    pc->pipeline.piped_count = state->size;
}

static void
pipeline_state_build_no_cond(pipeline_state_t *state, pipeline_t *p)
{
    exe_state_build(state->exes + (state->size - 1), &p->last);

    if (state->size == 1)
    {
        p->piped = NULL;
        p->piped_count = 0;
        return;
    }

    exe_t *piped = (exe_t *)malloc(sizeof(exe_t) * state->size);
    for (size_t i = 0; i < state->size - 1; i++)
    {
        exe_state_build(state->exes + i, piped + i);
    }
    p->piped = piped;
    p->piped_count = state->size;
}

static void
pipeline_state_free(pipeline_state_t *ps)
{
    ps->and = false;
    if (ps->exes != NULL)
    {
        for (size_t i = 0; i < ps->size; i++)
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
    const char *filename;
    /** Запись в файл в режиме добавления */
    bool append;
    /** Первый пайплайн */
    pipeline_state_t first;
    /** Пайплайны команд */
    pipeline_state_t *pipelines;
    /** Размер массива pipelines, кол-во элементов в нем */
    int pipelines_count;
    /** Вместимость массива pipelines */
    int pipelines_cap;
} build_state_t;

static void
build_state_init(build_state_t *state, struct command_line *line)
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

    pipeline_state_init(&state->first);
    state->pipelines = NULL;
    state->pipelines_cap = 0;
    state->pipelines_count = 0;
}

static void
build_state_free(build_state_t *bs)
{
    bs->append = false;
    bs->filename = NULL;
    bs->is_bg = false;

    if (bs->pipelines != NULL)
    {
        for (size_t i = 0; i < bs->pipelines_count; i++)
        {
            pipeline_state_t *s = bs->pipelines + i;
            if (s->exes != NULL)
            {
                /* TODO: освобождение ресурсов */
            }
        }

        bs->pipelines_count = 0;
        bs->pipelines_cap = 0;
        free(bs->pipelines);
        bs->pipelines = NULL;
    }
}

static void
build_state_add_cond_expr(build_state_t *state, struct expr *e, bool is_and)
{
    ASSERT_IS_COMMAND(e);
    /* assert(pipeline_state_is_init()); */

    /*
     * Это фукнция для добавления exe к уже существующему пайплайну - новых создавать не нужно
     */
    if (state->pipelines == NULL)
    {
        pipeline_state_add_command(&state->first, e, is_and);
        return;
    }

    assert(state->pipelines != NULL);
    assert(state->pipelines_count != 0);
    pipeline_state_add_command(state->pipelines + state->pipelines_count - 1, e, is_and);
}

/** Добавить команду, запускающуюся с условием && - ... && cmd ...*/
static void
build_state_add_and(build_state_t *state, struct expr *e)
{
    build_state_add_cond_expr(state, e, true);
}

/** Добавить команду, запускающуюся с условием || - ... || cmd ... */
static void
build_state_add_or(build_state_t *state, struct expr *e)
{
    build_state_add_cond_expr(state, e, false);
}

/**
 * Добавить команду на прямую - cmd ...
 * Эта функция должна вызываться самой первой в списке выражений
 */
static void
build_state_add_command(build_state_t *state, struct expr *e)
{
    /* Проверяем, что это первый вызов */
    ASSERT_IS_COMMAND(e);
    assert(state->pipelines == NULL);

    pipeline_state_add_command(&state->first, e, true);
}

/* Добавить новую команду, которая начинается сразу после пайплайна - ... | cmd ... */
static void
build_state_add_pipe(build_state_t *state, struct expr *e)
{
    int pipeline_index = state->pipelines_count;
    if (state->pipelines == NULL)
    {
        state->pipelines = (pipeline_state_t *)malloc(sizeof(pipeline_state_t) * 1);
        state->pipelines_cap = 1;
    }
    else if (state->pipelines_cap == state->pipelines_count)
    {
        state->pipelines = (pipeline_state_t *)realloc(state->pipelines, state->pipelines_cap * 2);
        state->pipelines_cap *= 2;
    }

    pipeline_state_init(state->pipelines + pipeline_index);
    pipeline_state_add_command(state->pipelines + pipeline_index, e, true);
}

/* Создать команду из собранных выражений */
static void
build_state_build(build_state_t *state, command_t *cmd)
{
    cmd->append = state->append;
    cmd->is_bg = state->is_bg;
    cmd->redirect_filename = state->filename;

    pipeline_state_build_no_cond(&state->first, &cmd->first);
    if (state->pipelines != NULL)
    {
        pipeline_condition_t *pcs = (pipeline_condition_t *)malloc(sizeof(pipeline_condition_t) * state->pipelines_count);
        for (size_t i = 0; i < state->pipelines_count; i++)
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

int parse_command(struct command_line *line, command_t *command)
{
    build_state_t bs;
    build_state_init(&bs, line);
    struct expr *e = line->head;
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
            build_state_add_and(&bs, e->next);
            e = e->next;
            break;
        case EXPR_TYPE_OR:
            assert(e->next != NULL);
            build_state_add_or(&bs, e->next);
            e = e->next;
            break;
        case EXPR_TYPE_PIPE:
            build_state_add_pipe(&bs, e->next);
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

static void
exe_free(exe_t *e)
{
    for (size_t i = 0; i < e->args_count; i++)
    {
        free((void*)e->args[i]);
    }

    free((void*)e->args);
    e->args = NULL;
    free((void *)e->name);
    e->name = NULL;
}

static void
pipeline_free(pipeline_t *p)
{
    exe_free(&p->last);
    if (p->piped != NULL)
    {
        for (size_t i = 0; i < p->piped_count; i++)
        {
            exe_free(p->piped + i);
        }
        free(p->piped);
        p->piped = NULL;
    }
}

void free_command(command_t *cmd)
{
    cmd->append = false;
    free((char *)cmd->redirect_filename);
    cmd->is_bg = false;

    pipeline_free(&cmd->first);
    if (cmd->chained != NULL)
    {
        assert(0 < cmd->chained_count);

        for (size_t i = 0; i < cmd->chained_count; i++)
        {
            pipeline_condition_t *pc = cmd->chained + i;
            pc->is_and = false;
            pipeline_free(&pc->pipeline);
        }
        free(cmd->chained);
        cmd->chained = NULL;
    }
}
