#include "insertion_sort.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define IS_EMPTY(state) ((state)->capacity == 0)
#define INITIAL_CAPACITY 4

void insertion_sort_init(insertion_sort_state *state)
{
    state->array = NULL;
    state->size = 0;
    state->capacity = 0;
}

void insertion_sort_free(insertion_sort_state *state)
{
    free(state->array);
    state->array = NULL;
    state->size = 0;
    state->capacity = 0;
}

int insertion_sort_size(insertion_sort_state *state)
{
    return state->size;
}

static int do_bsearch(int *array, int left, int right, int target)
{
    int mid = left + (right - left) / 2;
    while (left <= right)
    {
        if (left == right)
        {
            if (array[left] <= target)
            {
                return left + 1;
            }

            return left;
        }

        if (right - left == 1)
        {
            int lv = array[left];
            int rv = array[right];
            if (rv <= target)
            {
                return right + 1;
            }
            if (lv <= target)
            {
                return right;
            }

            return left;
        }

        if (array[mid] == target)
        {
            return mid + 1;
        }
        else if (array[mid] < target)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
        mid = left + (right - left) / 2;
    }

    return mid;
}

static int perform_bsearch(insertion_sort_state *state, int target)
{
    return do_bsearch(state->array, 0, state->size - 1, target);
}

static void enlarge_array(insertion_sort_state *state)
{
    assert(0 < state->capacity);

    int new_capacity = state->capacity * 2;
    state->array = realloc(state->array, new_capacity * sizeof(int));
    state->capacity = new_capacity;
}

static void insert_at_index(insertion_sort_state *state, int index, int number)
{
    // assert(index <= state->size);

    if (state->size == index)
    {
        state->array[index] = number;
        return;
    }

    int *copy_start = state->array + index;
    int copy_length = sizeof(int) * (state->size - index);
    memmove(copy_start + 1, copy_start, copy_length);
    state->array[index] = number;
}

void insertion_sort_insert(insertion_sort_state *state, int number)
{
    if (IS_EMPTY(state))
    {
        int *array = malloc(sizeof(int) * INITIAL_CAPACITY);
        array[0] = number;
        state->capacity = INITIAL_CAPACITY;
        state->size = 1;
        state->array = array;
        return;
    }

    if (state->size == state->capacity)
    {
        enlarge_array(state);
    }

    int index_to_insert = perform_bsearch(state, number);
    insert_at_index(state, index_to_insert, number);
    ++state->size;
}

const int *insertion_sort_array(insertion_sort_state *state)
{
    return state->array;
}
