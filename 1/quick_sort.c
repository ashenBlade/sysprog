#include "quick_sort.h"

static inline void swap(int *array, int i, int j)
{
    /* Для обмена используется трюк с XOR */
    array[i] ^= array[j];
    array[j] ^= array[i];
    array[i] ^= array[j];
}

static int get_middle_index(int left, int right)
{
    /*
     * Для получения опорного элемента пока просто возьмем середину.
     * Оптимизацией может быть рандомизация, но пока лень
     */
    return left + (right - left) / 2;
}

/**
 * @brief Выполнить разделение участка массива на 2 части - больше и меньше какого-то числа.
 * Это число высчитывается внутри и возвращается обратно из функции
 *
 * @param array Массив, который надо разбить
 * @param left Левая граница (включительно)
 * @param right Правая граница (исключая)
 * @return int Индекс середины
 */
static int do_partition(int *array, int left, int right)
{
    /* Разделение производим с помощью схемы Хоара */
    int pivot = array[get_middle_index(left, right)];
    int l = left;
    int r = right - 1;
    while (l < r)
    {
        while (array[l] < pivot)
        {
            l++;
        }
        while (pivot < array[r])
        {
            r--;
        }

        if (r <= l)
        {
            return r;
        }

        swap(array, l, r);
        l++;
        r--;
    }
    return r;
}

/// @brief Основная логика быстрой сортировки
/// @param array Исходный массив
/// @param left Индекс начала (включительно)
/// @param right Индекс конца (исключая)
static void do_quick_sort(int *array, int left, int right)
{
    if (right - left == 1)
    {
        /* Массив из 1 элемента или пуст - уже отсортирован*/
        return;
    }

    if (right - left == 2)
    {
        /* Для 2 элементов достаточно сделать 1 проверку */
        if (array[right] < array[left])
        {
            swap(array, left, right);
        }
        return;
    }

    int pivot = do_partition(array, left, right);
    do_quick_sort(array, left, pivot);
    do_quick_sort(array, pivot + 1, right);
}

void quick_sort(int *array, int size)
{
    do_quick_sort(array, 0, size);
}