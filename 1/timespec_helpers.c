#include "timespec_helpers.h"

#define SECONDS_IN_NANOSECS 1000000000

void
timespec_add(struct timespec *left, struct timespec *right, struct timespec *result)
{
    result->tv_sec = left->tv_sec + right->tv_sec;

    /*
     * При сложении наносекунд надо проверить переполнение.
     * Стоит заметить, что при переполнении может добавиться только 1 секунда.
     */
    long ns = left->tv_nsec + right->tv_nsec;
    if (SECONDS_IN_NANOSECS <= ns)
    {
        ++result->tv_sec;
        result->tv_nsec = ns - SECONDS_IN_NANOSECS;
    }
    else
    {
        result->tv_nsec = ns;
    }
}

void
timespec_sub(struct timespec *left, struct timespec *right, struct timespec *result)
{
    /* Предполагаю, что используется монотонное время и левая часть больше правой */
    result->tv_sec = left->tv_sec - right->tv_sec;

    if (left->tv_nsec < right->tv_nsec)
    {
        --result->tv_sec;
        result->tv_nsec = SECONDS_IN_NANOSECS - (right->tv_nsec - left->tv_nsec);
    }
    else
    {
        result->tv_nsec = left->tv_nsec - right->tv_nsec;
    }
}