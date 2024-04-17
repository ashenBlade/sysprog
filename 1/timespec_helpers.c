#include "timespec_helpers.h"

#define NS_IN_S 1000000000LL
#define US_IN_S 1000000

void
timespec_add(struct timespec *left, struct timespec *right, struct timespec *result)
{
    result->tv_sec = left->tv_sec + right->tv_sec;

    /*
     * При сложении наносекунд надо проверить переполнение.
     * Стоит заметить, что при переполнении может добавиться только 1 секунда.
     */
    long ns = left->tv_nsec + right->tv_nsec;
    if (NS_IN_S <= ns)
    {
        ++result->tv_sec;
        result->tv_nsec = ns - NS_IN_S;
    }
    else
    {
        result->tv_nsec = ns;
    }
}

void timespec_div(struct timespec *divisible, int divider, struct timespec *quotient)
{
    quotient->tv_sec = divisible->tv_sec / divider;
    long long nsec = divisible->tv_nsec;
    if (quotient->tv_sec * divider != divisible->tv_sec)
    {
        nsec += NS_IN_S;
    }

    quotient->tv_nsec = nsec / divider;
}

bool timespec_le(struct timespec *left, struct timespec *right)
{
    if (left->tv_sec == right->tv_sec) 
    {
        return left->tv_nsec <= right->tv_nsec;
    }

    return left->tv_sec < right->tv_sec;
}

void us_to_timespec(long long us, struct timespec *ts)
{
    ts->tv_sec = us / US_IN_S;
    ts->tv_nsec = (us % US_IN_S) * (NS_IN_S / US_IN_S);
}

void
timespec_sub(struct timespec *left, struct timespec *right, struct timespec *result)
{
    /* Предполагаю, что используется монотонное время и левая часть больше правой */
    result->tv_sec = left->tv_sec - right->tv_sec;

    if (left->tv_nsec < right->tv_nsec)
    {
        --result->tv_sec;
        result->tv_nsec = NS_IN_S - (right->tv_nsec - left->tv_nsec);
    }
    else
    {
        result->tv_nsec = left->tv_nsec - right->tv_nsec;
    }
}
