#ifndef TIMESPEC_HELPERS_H
#define TIMESPEC_HELPERS_H

#include <time.h>
#include <stdbool.h>

#define TIMESPEC_IS_ZERO(ts) ((ts).tv_sec == 0 && (ts).tv_nsec == 0)

/*
 * Вычесть из diminutive значение deductible и сохранить результат в diff.
 * Предполагается, что deductible < diminutive, т.к. функция используется для замера времени
 */
void timespec_sub(struct timespec *diminutive, struct timespec *deductible, struct timespec *diff);

/** Прибавить время из left_term к right_term и сохранить результат в sum */
void timespec_add(struct timespec *left_term, struct timespec *right_term, struct timespec *sum);

/** Разделить divisible на divider и сохранить частное в quotient */
void timespec_div(struct timespec *divisible, int divider, struct timespec *quotient);

/** Проверить, что right не меньше чем left - left <= right */
bool timespec_le(struct timespec *left, struct timespec *right);

/** Создать новый timespec из переданного количества микросекунд */
void us_to_timespec(long long us, struct timespec *ts);

#endif