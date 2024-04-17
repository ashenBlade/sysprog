#ifndef STACK_H
#define STACK_H

#include <stdbool.h>

typedef struct stack 
{
    void **values;
    int size;
    int capacity;
} stack_t;

/** Инициализировать стек */
void stack_init(stack_t *stack);

/** Удалить содержимое стека и освободить ресурсы */
void stack_free(stack_t *stack);

/** Добавить элемент в стек */
void stack_push(stack_t *stack, void *value);

/** Попробовать взять элемент из стека. В случае успеха возвращается true и значение value указывает на данные */
bool stack_try_pop(stack_t *stack, void **value);

#endif