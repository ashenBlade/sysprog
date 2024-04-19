#pragma once

#include <stdbool.h>
#include <stdint.h>

struct parser;

enum parser_error
{
    /** Ok */
    PARSER_ERR_NONE = 0,
    /** | arg */
    PARSER_ERR_PIPE_WITH_NO_LEFT_ARG = 1,

    /** asdf && | */
    PARSER_ERR_PIPE_WITH_LEFT_ARG_NOT_A_COMMAND = 2,

    /** && asdf */
    PARSER_ERR_AND_WITH_NO_LEFT_ARG = 3,

    /** asdf | && */
    PARSER_ERR_AND_WITH_LEFT_ARG_NOT_A_COMMAND = 4,

    /** || asdf */
    PARSER_ERR_OR_WITH_NO_LEFT_ARG = 5,

    /** asdf | || */
    PARSER_ERR_OR_WITH_LEFT_ARG_NOT_A_COMMAND = 6,

    /** asdf > > */
    PARSER_ERR_OUTOUT_REDIRECT_BAD_ARG = 7,

    /** asdf > |*/
    PARSER_ERR_TOO_LATE_ARGUMENTS = 8,

    /** asdf | */
    PARSER_ERR_ENDS_NOT_WITH_A_COMMAND = 9,
};

struct command_raw
{
    char *exe;
    char **args;
    uint32_t arg_count;
    uint32_t arg_capacity;
};

enum expr_type
{
    EXPR_TYPE_COMMAND,
    EXPR_TYPE_PIPE,
    EXPR_TYPE_AND,
    EXPR_TYPE_OR,
};

struct expr
{
    enum expr_type type;
    /** Valid if the type is COMMAND. */
    struct command_raw cmd;
    struct expr *next;
};

enum output_type
{
    OUTPUT_TYPE_STDOUT,
    OUTPUT_TYPE_FILE_NEW,
    OUTPUT_TYPE_FILE_APPEND,
};

struct command_line
{
    struct expr *head;
    struct expr *tail;
    enum output_type out_type;
    /** Valid if the out type is FILE. */
    char *out_file;
    bool is_background;
};

void command_line_delete(struct command_line *line);

struct parser *
parser_new(void);

void parser_feed(struct parser *p, const char *str, uint32_t len);

enum parser_error
parser_pop_next(struct parser *p, struct command_line **out);

void parser_delete(struct parser *p);
