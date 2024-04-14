#ifndef INSERTION_SORT_H
#define INSERTION_SORT_H

typedef struct insertion_sort_state
{
    int *array;
    int capacity;
    int size;
} insertion_sort_state;

/**
 * @brief Создать новый экземпляр структуры для дальнейшей сортировки
 *
 * @param state Состояние, которое необходимо инициализировать
 */
void insertion_sort_init(insertion_sort_state *state);

/**
 * @brief Очистить состояние
 *
 * @param state Состояние
 */
void insertion_sort_free(insertion_sort_state *state);

/**
 * @brief
 *
 * @param state
 * @return int
 */
int insertion_sort_size(insertion_sort_state *state);
void insertion_sort_insert(insertion_sort_state *state, int number);
const int *insertion_sort_array(insertion_sort_state *state);

#endif