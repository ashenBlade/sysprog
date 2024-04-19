#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

/** Представление запускаемого приложения - приложение и его аргументы */
typedef struct exe
{
    /** Введенное название программы */
    const char *name;
    /** Аргументы, переданные этой программе */
    const char **args;
    /** Количество этих аргументов */
    int args_count;
} exe_t;

/**
 * Группа запускаемых программ, объединенных пайпом.
 * Грубо говоря, это единица запуска: может быть одной командой, либо цепочкой (пайплайном)
 *
 * Расположение следующее:
 * first | piped[0] | piped[1] | ...
 *
 * Таким образом, если пайпов нет, то дополнительной памяти выделять не надо
 */
typedef struct pipeline
{
    /** Первая программа в этом пайплайне */
    exe_t first;
    /**
     * Дополнительные программы, которые должны быть запущены.
     * Если программа одна, то массив пуст (NULL)
     */
    exe_t *piped;
    /** Количество спайпленных команд, т.е. размер массива piped */
    int piped_count;
} pipeline_t;

/**
 * Представление пайплайна вместе с условием запуска.
 * Используется для создания цепочки запуска.
 */
typedef struct pipeline_condition
{
    /** Условие запуска && */
    bool is_and;
    /** Пайплайн, который необходимо запустить */
    pipeline_t pipeline;
} pipeline_condition_t;

/** Полная команда, которая была введена пользователем */
typedef struct command
{
    /** Первая команда, которую необходимо выполнить */
    pipeline_t first;
    /** Команды, которые необходимо выполнять после (с условиями) */
    pipeline_condition_t *chained;
    /** Длина массива chained */
    int chained_count;
    /** Необходимо выполнить на фоне */
    bool is_bg;
    /** Название файла, в который необходимо перенаправлять вывод */
    const char *redirect_filename;
    /** Флаг - если вывод делать в файл, то необходимо выполнять APPEND */
    bool append;
} command_t;

#endif