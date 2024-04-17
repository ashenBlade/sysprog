#include <stdlib.h>
#include <string.h>

#include "stack.h"

#define INITIAL_CAPACITY 4
#define STACK_IS_EMPTY(s) ((s)->capacity == 0)
#define STACK_IS_FULL(s) ((s)->capacity == (s)->size)

void stack_init(stack_t *stack)
{
    stack->values = NULL;
    stack->size = 0;
    stack->capacity = 0;
}

void stack_free(stack_t *stack)
{
    free(stack->values);
    stack->size = 0;
    stack->capacity = 0;
}

static void
stack_enlarge(stack_t *stack)
{
    int new_cap = stack->capacity * 2;
    stack->values = (void **)realloc(stack->values, sizeof(void *) * new_cap);
    // void **new_data = malloc(sizeof(void *)*new_cap);
    // memcpy(new_data, stack->values, sizeof(void *) * stack->capacity);
    // free(stack->values);
    // stack->values = new_data;
    stack->capacity = new_cap;
}

void stack_push(stack_t *s, void *value)
{
    if (STACK_IS_EMPTY(s))
    {
        s->values = (void **)malloc(sizeof(void *) * INITIAL_CAPACITY);
        s->values[0] = value;
        s->capacity = INITIAL_CAPACITY;
        s->size = 1;
        return;
    }

    if (STACK_IS_FULL(s))
    {
        stack_enlarge(s);
    }

    s->values[s->size] = value;
    ++s->size;
}

bool stack_try_pop(stack_t *stack, void **value)
{
    if (stack->size == 0)
    {
        return false;
    }

    *value = stack->values[stack->size - 1];
    --stack->size;
    return true;
}
