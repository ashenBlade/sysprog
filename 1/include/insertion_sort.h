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
 * @brief Получить размер отсортированного массива
 * 
 * @param state Объект массива
 * @return int Размер массива
 */
int insertion_sort_size(insertion_sort_state *state);

/**
 * @brief Вставить в отсортированный массив указанное число
 * 
 * @param state Объект массива
 * @param number Число, которое нужно вставить
 */
void insertion_sort_insert(insertion_sort_state *state, int number);

/**
 * @brief Получить указатель на отсортированный массив
 * 
 * @param state Объект массива
 * @return const int* Указатель на массив отсортированных чисел
 */
const int *insertion_sort_array(insertion_sort_state *state);

#endif