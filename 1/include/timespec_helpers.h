#ifndef TIMESPEC_HELPERS_H
#define TIMESPEC_HELPERS_H

#include <time.h>

/*
 * Вычесть из left значение right и сохранить результат в result. 
 * Предполагается, что right < left, т.к. функция используется для замера времени 
 */
void timespec_sub(struct timespec *left, struct timespec *right, struct timespec *result);

/** Прибавить время из left к right и сохранить результат в result */
void timespec_add(struct timespec *left, struct timespec *right, struct timespec *result);

#endif